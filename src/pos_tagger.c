/*
 * pos_tagger.c
 * Part-of-speech tagging for Kinyarwanda.
 *
 * Priority order (from most specific to most general):
 *  1. Invariable words  → checked against the complete invariable table
 *  2. Pronouns          → checked against full pronoun table
 *  3. Known full word   → exact-match override for irregular/bare-prefix nouns
 *  4. Verb infinitive   → pattern ku/gu/kw/gw + stem + -a
 *  5. Proper noun       → capital letter mid-sentence (heuristic)
 *  6. Noun              → D+RT prefix detection
 *  7. Adjective         → concordance prefix + known adjective stem
 *  8. Conjugated verb   → subject prefix + stem + -a pattern
 *  9. Foreign/Unknown
 *
 * NOTE: Known full word (step 3) is checked early so that specific lexicon
 * entries (like "mvura" = rain) override the general verb heuristics.
 */

#include <string.h>
#include "../include/kinyarwanda.h"

/* Tag a single token in isolation */
void kin_tag_token(Token *tok) {
    const char *w = tok->lower;

    /* 1. Invariable word? */
    POS inv_pos;
    if (kin_is_invariable(w, &inv_pos)) {
        tok->pos           = inv_pos;
        tok->is_kinyarwanda = true;
        return;
    }

    /* 2. Pronoun? */
    PronounType ptype; int pcls;
    if (kin_is_pronoun(w, &ptype, &pcls)) {
        tok->pos            = POS_PRONOUN;
        tok->pron_type      = ptype;
        tok->noun_class     = pcls;
        tok->is_kinyarwanda = true;
        return;
    }

    /* 3. Known full word (exact-match lexicon override)
     * Checked before verb heuristics so that specific nouns like "mvura"
     * (rain) are not misanalysed as conjugated verbs (mv SP + ur stem).   */
    {
        int kcls = 0;
        char kstem[KIN_MAX_STEM] = "";
        if (kin_is_known_full_word(w, &kcls, kstem)) {
            tok->pos            = POS_NOUN;
            tok->noun_class     = kcls;
            tok->is_kinyarwanda = true;
            strncpy(tok->stem, kstem, KIN_MAX_STEM - 1);
            return;
        }
    }

    /* 4. Verb infinitive (ku/gu/kw/gw + stem + a)? */
    char stem[KIN_MAX_STEM];
    if (kin_is_verb_infinitive(w, stem)) {
        tok->pos            = POS_VERB_INF;
        tok->noun_class     = 15;   /* class 15 = infinitive class         */
        tok->is_kinyarwanda = true;
        strncpy(tok->stem, stem, KIN_MAX_STEM - 1);
        /* Record the prefix */
        size_t stemlen = strlen(stem);
        size_t wordlen = strlen(w);
        size_t pfxlen  = wordlen - stemlen - 1; /* -1 for final 'a'        */
        strncpy(tok->detected_prefix, w, pfxlen);
        tok->detected_prefix[pfxlen] = '\0';
        return;
    }

    /* 5. Proper noun (capitalised mid-sentence)? */
    if (tok->is_proper_noun) {
        tok->pos            = POS_NOUN;
        tok->noun_class     = 0;   /* class unknown for proper nouns       */
        tok->is_kinyarwanda = true; /* may be foreign name; mark true anyway*/
        return;
    }

    /* 6. Noun? — detect D+RT prefix */
    int cls = 0;
    if (kin_strip_noun_prefix(w, stem, &cls)) {
        tok->pos            = POS_NOUN;
        tok->noun_class     = cls;
        tok->is_kinyarwanda = true;
        strncpy(tok->stem, stem, KIN_MAX_STEM - 1);
        /* Record prefix = everything before the stem */
        size_t stemlen = strlen(stem);
        size_t wordlen = strlen(w);
        size_t pfxlen  = (wordlen > stemlen) ? wordlen - stemlen : 0;
        strncpy(tok->detected_prefix, w, pfxlen);
        tok->detected_prefix[pfxlen] = '\0';
        return;
    }

    /* 7. Adjective? — RS prefix + known stem */
    int acls = 0;
    if (kin_strip_adj_prefix(w, stem, &acls)) {
        tok->pos            = POS_ADJECTIVE;
        tok->noun_class     = acls;
        tok->is_kinyarwanda = true;
        strncpy(tok->stem, stem, KIN_MAX_STEM - 1);
        size_t stemlen = strlen(stem);
        size_t wordlen = strlen(w);
        size_t pfxlen  = (wordlen > stemlen) ? wordlen - stemlen : 0;
        strncpy(tok->detected_prefix, w, pfxlen);
        tok->detected_prefix[pfxlen] = '\0';
        return;
    }

    /* 8. Conjugated verb heuristic */
    int scls = 0, obj_cls = 0;
    VerbTense vtense = TENSE_NONE;
    VerbExtension vext = VEXT_NONE;
    bool is_neg = false;
    if (kin_is_verb_conjugated(w, stem, &scls, &vtense, &obj_cls, &vext, &is_neg)) {
        tok->pos            = POS_VERB_CONJ;
        tok->noun_class     = scls;
        tok->verb_tense     = vtense;
        tok->verb_ext       = vext;
        tok->obj_class      = obj_cls;
        tok->is_negative    = is_neg;
        tok->is_kinyarwanda = true;
        strncpy(tok->stem, stem, KIN_MAX_STEM - 1);
        return;
    }

    /* 8b. Locative contraction: kw + (i|a)-initial noun
     * Rule: ku + vowel-initial noun → kw + noun  (phonological fusion).
     *   ku + isi   → kwisi   (on/at earth)
     *   ku + ijuru → kwijuru (in heaven)
     *   ku + amazi → kwamazi (of/with water)
     *   ku + abantu→ kwabantu(of/belonging to people)
     * Step 4 (VERB_INF) already succeeded for kw+verb patterns (kwigisha),
     * so anything reaching here with kw+[ia] is a locative noun phrase.     */
    if (w[0]=='k' && w[1]=='w' && (w[2]=='i' || w[2]=='a') && strlen(w) > 3) {
        const char *rest = w + 2;
        /* Try prefix-based noun detection first */
        if (kin_strip_noun_prefix(rest, stem, &cls)) {
            tok->pos            = POS_NOUN;
            tok->noun_class     = cls;
            tok->is_kinyarwanda = true;
            strncpy(tok->stem, stem, KIN_MAX_STEM - 1);
            strncpy(tok->detected_prefix, w, 3);
            tok->detected_prefix[3] = '\0';
            return;
        }
        /* Fall back to known-word table (e.g. kwisi = ku+isi, isi in KNOWN_WORDS) */
        char kstem[KIN_MAX_STEM] = "";
        int  kcls = 0;
        if (kin_is_known_full_word(rest, &kcls, kstem)) {
            tok->pos            = POS_NOUN;
            tok->noun_class     = kcls;
            tok->is_kinyarwanda = true;
            strncpy(tok->stem, kstem, KIN_MAX_STEM - 1);
            strncpy(tok->detected_prefix, w, 3);
            tok->detected_prefix[3] = '\0';
            return;
        }
    }

    /* 9. Unknown / foreign */
    tok->pos            = POS_FOREIGN;
    tok->is_kinyarwanda = false;
}

/* Tag all tokens in a sentence and detect verbs */
void kin_tag_sentence(SentenceAnalysis *sa) {
    sa->has_verb = false;
    for (int i = 0; i < sa->token_count; i++) {
        kin_tag_token(&sa->tokens[i]);
        if (sa->tokens[i].pos == POS_VERB_INF ||
            sa->tokens[i].pos == POS_VERB_CONJ)
            sa->has_verb = true;
    }
}
