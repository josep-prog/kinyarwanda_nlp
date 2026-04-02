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

/* Check if a noun token is a deverbative (izina rivuye mu nshinga).
 * Pattern: strip the final vowel from the noun stem; if the result is a
 * known verb stem (≥ 2 chars), the noun was derived from that verb.
 * Example: umucyo (stem "cyo") → strip 'o' → "cy" → kugcya (to shine). */
static void check_deverbative(Token *tok) {
    if (tok->pos != POS_NOUN) return;
    size_t slen = tok->stem[0] ? strlen(tok->stem) : 0;
    if (slen < 3) return;                    /* stem must be ≥ 3 chars      */
    char last = tok->stem[slen - 1];
    bool ends_vowel = (last=='a'||last=='e'||last=='i'||last=='o'||last=='u');
    if (!ends_vowel) return;                 /* nominalizer is always a vowel*/
    char root[KIN_MAX_STEM];
    strncpy(root, tok->stem, slen - 1);
    root[slen - 1] = '\0';
    if (strlen(root) >= 2 && kin_is_known_verb_stem(root)) {
        tok->is_deverbative = true;
        strncpy(tok->verb_root, root, KIN_MAX_STEM - 1);
        tok->verb_root[KIN_MAX_STEM - 1] = '\0';
    }
}

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
            check_deverbative(tok);
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
        /* Record the prefix.
         * Formula: pfxlen = wordlen - stemlen - 1 (for final 'a').
         * Adjust for locative suffixes (-ho/-mo/-yo) which are 2 extra chars
         * appended after the final 'a': guturaho = gu+tur+a+ho.
         * Without adjustment: 8-3-1=4 → "gutu" (wrong); with: 8-3-3=2 → "gu". */
        size_t stemlen = strlen(stem);
        size_t wordlen = strlen(w);
        size_t pfxlen  = wordlen - stemlen - 1; /* -1 for final 'a'        */
        if (wordlen > 5 && wordlen >= 3 && w[wordlen - 3] == 'a' &&
            (kin_ends_with(w, "ho") || kin_ends_with(w, "mo") ||
             kin_ends_with(w, "yo"))) {
            pfxlen -= 2;   /* additional -2 for the 2-char locative suffix   */
        }
        strncpy(tok->detected_prefix, w, pfxlen);
        tok->detected_prefix[pfxlen] = '\0';
        return;
    }

    /* 5. Proper noun (capitalised mid-sentence)?
     * Try verb analysis first — capitalised verb forms occur at the start of
     * quoted speech (e.g. "Habeho" = ha(SP16)+b+e+ho, SUBJUNCTIVE+LOC).
     * Only accept the verb reading for distinctive tense markers; PRESENT_NORA
     * and plain SUBJUNCTIVE are too permissive without additional context.   */
    if (tok->is_proper_noun) {
        char      pn_stem[KIN_MAX_STEM] = "";
        int       pn_cls = 0, pn_obj = 0;
        VerbTense pn_tense = TENSE_NONE;
        VerbExtension pn_ext = VEXT_NONE;
        bool      pn_neg = false;
        if (kin_is_verb_conjugated(tok->lower, pn_stem, &pn_cls, &pn_tense,
                                   &pn_obj, &pn_ext, &pn_neg)
            && pn_tense != TENSE_PRESENT_NORA
            && pn_tense != TENSE_SUBJUNCTIVE
            && pn_tense != TENSE_IMPERATIVE
            && (pn_tense == TENSE_SUBJUNCTIVE_LOC
                || pn_tense == TENSE_PAST_PERF
                || pn_tense == TENSE_PAST_IMPF
                || pn_tense == TENSE_NEG_RELATIVE
                || pn_tense == TENSE_FUTURE
                || pn_tense == TENSE_NARRATIVE
                || pn_tense == TENSE_PRESENT
                || pn_tense == TENSE_COPULA_PAST
                || pn_tense == TENSE_COPULA_PRES
                || kin_is_known_verb_stem(pn_stem))) {
            tok->pos            = POS_VERB_CONJ;
            tok->noun_class     = pn_cls;
            tok->verb_tense     = pn_tense;
            tok->verb_ext       = pn_ext;
            tok->obj_class      = pn_obj;
            tok->is_negative    = pn_neg;
            tok->is_kinyarwanda = true;
            strncpy(tok->stem, pn_stem, KIN_MAX_STEM - 1);
            tok->stem[KIN_MAX_STEM - 1] = '\0';
            return;
        }
        tok->pos            = POS_NOUN;
        tok->noun_class     = 0;   /* class unknown for proper nouns       */
        tok->is_kinyarwanda = true; /* may be foreign name; mark true anyway*/
        return;
    }

    /* 6. Noun? — detect D+RT prefix
     *
     * Disambiguation guard: in Kinyarwanda the noun-class prefix (D+RT) is
     * identical to the verb subject prefix for many classes (ru=Nt.11/SP11,
     * ki=Nt.7/SP7, bi=Nt.8/SP8, bu=Nt.14/SP14, etc.).  If a word matches
     * both a noun prefix AND a conjugated-verb pattern with a KNOWN stem and
     * an unambiguous tense marker, we prefer the verb interpretation.
     *
     * We only override noun→verb when the tense is explicitly marked (not
     * TENSE_PRESENT_NORA which is too permissive) to minimise false positives.
     * Examples this fixes:
     *   "rurabaho"  → Nt.11 present of kuba (ru+ra+b+a+ho), not an Nt.11 noun
     *   "kirabaho"  → Nt.7  present of kuba
     *   "burabaho"  → Nt.14 present of kuba
     */
    int cls = 0;
    if (kin_strip_noun_prefix(w, stem, &cls)) {
        /* Try verb detection before committing to noun */
        char      v_stem[KIN_MAX_STEM] = "";
        int       v_cls  = 0, v_obj = 0;
        VerbTense v_tense = TENSE_NONE;
        VerbExtension v_ext = VEXT_NONE;
        bool      v_neg  = false;

        /* Bare phonological-mutation SPs (cy/by/ry/zy without leading vowel
         * D-prefix) are VERB markers — they arise only from the i→y rule on
         * verb subject prefixes (ki→cy, bi→by, ri→ry, zi→zy before vowel-
         * initial stems).  Nouns in those classes always keep the full prefix
         * (iki-, ibi-, iri-, izi-) or at minimum drop only the D, yielding
         * "ki-", "bi-" etc., never bare "cy-" or "by-".  When we see such a
         * prefix, trust the verb analysis even for PRESENT_NORA with an
         * unknown stem — the phonological pattern alone is strong evidence.   */
        bool is_bare_phon_sp = (
            (w[0]=='c' && w[1]=='y') ||   /* cy = ki + vowel-initial stem */
            (w[0]=='b' && w[1]=='y') ||   /* by = bi + vowel-initial stem */
            (w[0]=='r' && w[1]=='y') ||   /* ry = ri + vowel-initial stem */
            (w[0]=='z' && w[1]=='y')      /* zy = zi + vowel-initial stem */
        );

        if (kin_is_verb_conjugated(w, v_stem, &v_cls, &v_tense,
                                   &v_obj, &v_ext, &v_neg)
            /* PRESENT_NORA: require known stem OR phon-mutation SP (bare_phon_sp). */
            && (v_tense != TENSE_PRESENT_NORA
                || kin_is_known_verb_stem(v_stem)
                || (is_bare_phon_sp && kin_is_valid_verb_stem_shape(v_stem)))
            && v_tense != TENSE_SUBJUNCTIVE
            && v_tense != TENSE_IMPERATIVE
            /* Tenses with unambiguous morphological markers bypass stem check:
             * PAST_PERF  – surface differs from citation stem (murakoze→koz)
             * PAST_IMPF  – -aga suffix is highly distinctive
             * NEG_RELATIVE – -ta- marker uniquely identifies this form
             * FUTURE / NARRATIVE / COPULA – markers are unambiguous enough
             * bare_phon_sp – phonological mutation alone is sufficient signal */
            && (v_tense == TENSE_PAST_PERF
                || v_tense == TENSE_PAST_IMPF
                || v_tense == TENSE_NEG_RELATIVE
                || v_tense == TENSE_FUTURE
                || v_tense == TENSE_NARRATIVE
                || v_tense == TENSE_COPULA_PAST
                || v_tense == TENSE_COPULA_PRES
                || v_tense == TENSE_SUBJUNCTIVE_LOC
                || kin_is_known_verb_stem(v_stem)
                || (is_bare_phon_sp && kin_is_valid_verb_stem_shape(v_stem)))) {
            /* Verb interpretation wins */
            tok->pos            = POS_VERB_CONJ;
            tok->noun_class     = v_cls;
            tok->verb_tense     = v_tense;
            tok->verb_ext       = v_ext;
            tok->obj_class      = v_obj;
            tok->is_negative    = v_neg;
            tok->is_kinyarwanda = true;
            strncpy(tok->stem, v_stem, KIN_MAX_STEM - 1);
            tok->stem[KIN_MAX_STEM - 1] = '\0';
            return;
        }
        /* Before committing to noun, check if this is an adjective.
         * Adjective concordance prefixes (RS) are identical to noun-class
         * prefixes, so adjective detection at step 7 is never reached for
         * words caught here first.  e.g. "munini" = mu(RS Nt.1)+nini → adj,
         * NOT an Nt.1 noun with stem "nini". */
        {
            char a_stem[KIN_MAX_STEM] = "";
            int  a_cls = 0;
            if (kin_strip_adj_prefix(w, a_stem, &a_cls)) {
                tok->pos            = POS_ADJECTIVE;
                tok->noun_class     = a_cls;
                tok->is_kinyarwanda = true;
                strncpy(tok->stem, a_stem, KIN_MAX_STEM - 1);
                tok->stem[KIN_MAX_STEM - 1] = '\0';
                size_t astemlen = strlen(a_stem);
                size_t awordlen = strlen(w);
                size_t apfxlen  = (awordlen > astemlen) ? awordlen - astemlen : 0;
                strncpy(tok->detected_prefix, w, apfxlen);
                tok->detected_prefix[apfxlen] = '\0';
                return;
            }
        }
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
        check_deverbative(tok);
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
            check_deverbative(tok);
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
            check_deverbative(tok);
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

    /* ── Context pass: indomo (initial vowel) elision recovery ─────────── *
     * In Kinyarwanda, the initial vowel (indomo) of a noun is elided when   *
     * the noun follows a demonstrative or relative pronoun that ends in a   *
     * vowel (VV contact rule / Iranya ry'impanvu).                          *
     * e.g. "iryo isanzure" → "iryo sanzure"  (i- of isanzure elided)       *
     * The first pass tagged "sanzure" as verb (bare subj.); here we correct *
     * it to noun when the preceding token is a demonstrative/relative.      */
    for (int i = 1; i < sa->token_count; i++) {
        Token *prev = &sa->tokens[i-1];
        Token *curr = &sa->tokens[i];
        /* Only re-tag if current is NOT already a noun */
        if (curr->pos == POS_NOUN) continue;
        /* Only when previous token is demonstrative or relative pronoun */
        if (prev->pos != POS_PRONOUN ||
            (prev->pron_type != PRON_DEMONSTRATIVE &&
             prev->pron_type != PRON_RELATIVE)) continue;
        /* Guard: word must be ≥ 5 chars to avoid short ambiguous stems */
        if (strlen(curr->lower) < 5) continue;
        int icls = 0;
        if (!kin_is_known_noun_stem(curr->lower, &icls)) continue;
        /* Re-tag as noun with elided indomo */
        curr->pos               = POS_NOUN;
        curr->noun_class        = icls;
        curr->verb_tense        = TENSE_NONE;
        curr->verb_ext          = VEXT_NONE;
        curr->obj_class         = 0;
        curr->is_negative       = false;
        curr->is_kinyarwanda    = true;
        strncpy(curr->stem, curr->lower, KIN_MAX_STEM - 1);
        curr->stem[KIN_MAX_STEM - 1] = '\0';
        curr->detected_prefix[0] = '\0';  /* indomo was elided */
        check_deverbative(curr);
    }

    /* Re-check has_verb after context pass */
    sa->has_verb = false;
    for (int i = 0; i < sa->token_count; i++) {
        if (sa->tokens[i].pos == POS_VERB_INF ||
            sa->tokens[i].pos == POS_VERB_CONJ)
            sa->has_verb = true;
    }
}
