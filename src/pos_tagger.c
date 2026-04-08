/*
 * pos_tagger.c  —  Part-of-speech tagging for Kinyarwanda.
 *
 * This file assigns each token to its word-type "tree":
 *   Tree 5 → POS_PREPOSITION / POS_CONJUNCTION / POS_ADVERB / POS_LOCATIVE
 *             POS_INTERJECTION / POS_VERB_PARTICLE   (amagambo adahinduka)
 *   Tree 4 → POS_PRONOUN   (ikinyazina, any sub-type)
 *   Tree 1 → POS_NOUN      (izina mbonera, D+RT+C)
 *          → POS_RELATIVE_NOUN (izina ntera: noun as qualifier, detected in context pass)
 *   Tree 2 → POS_ADJECTIVE (ntera, RS+C)
 *          → POS_COMPOUND_ADJ (igisantera: compound adj — planned)
 *   Tree 3 → POS_VERB_INF  (inshinga imbundo, PREF+C+FV)
 *          → POS_VERB_CONJ (inshinga itondaguye, SP+TM+C+FV)
 *   ——     → POS_FOREIGN / POS_UNKNOWN  (not matched by any tree)
 *
 * Single-token priority (kin_tag_token, most-specific first):
 *  Step 1. Invariable words    → Tree 5 (amagambo adahinduka)
 *  Step 2. Pronouns            → Tree 4 (ikinyazina)
 *  Step 3. Known full word     → Tree 1 override (lexicon exact-match)
 *  Step 4. Verb infinitive     → Tree 3a (imbundo)
 *  Step 5. Proper noun         → Tree 1 heuristic (capitalised mid-sentence)
 *  Step 6. Noun (D+RT prefix)  → Tree 1, with verb/adj guard before committing
 *  Step 7. Adjective (RS+C)    → Tree 2 (ntera)
 *  Step 8. Conjugated verb     → Tree 3b (itondaguye)
 *  Step 9. Foreign/Unknown
 *
 * Sentence context passes (kin_tag_sentence, after single-token pass):
 *  Pass A. Indomo elision recovery: noun after demonstrative/relative (Tree 1)
 *  Pass B. Izina ntera (POS_RELATIVE_NOUN): noun after possessive connector
 *           that agrees with a preceding head noun → Tree 1→relative_noun
 *
 * NOTE: Known full word (step 3) is checked early so that specific lexicon
 * entries (like "mvura" = rain) override the general verb heuristics.
 */

#include <string.h>
#include "../include/kinyarwanda.h"

/* Primary noun stems that share a stripped form with a known verb root but
 * are NOT deverbative.  These are lexically independent nouns that happen to
 * look like verb-derived forms; the deverbative heuristic must skip them.
 * Add any new collision here rather than changing check_deverbative's logic. */
static const char *PRIMARY_NOUN_STEMS[] = {
    "siga",   /* igisiga/ibisiga – birds of prey (eagles/vultures/large hawks)
               * NOT from gusiga (to leave/anoint); independent lexical item  */
    NULL
};

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
    /* Guard: skip known primary nouns that collide with verb roots */
    for (int pi = 0; PRIMARY_NOUN_STEMS[pi]; pi++)
        if (strcmp(tok->stem, PRIMARY_NOUN_STEMS[pi]) == 0) return;
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
    /* Punctuation tokens are already fully tagged by the tokenizer — preserve. */
    if (tok->pos == POS_PUNCTUATION) return;

    const char *w = tok->lower;

    /* Step 1 → Tree 5 (amagambo adahinduka): invariable word? */
    POS inv_pos;
    if (kin_is_invariable(w, &inv_pos)) {
        tok->pos            = inv_pos;
        tok->is_kinyarwanda = true;
        /* ── -fite stative possessive: set class + stem so morph display is right.
         * Forms: bifite(Nt.8), afite(Nt.1), bafite(Nt.2), gifite(Nt.7), etc.
         * Structure: SP + -fite  (stative of "to have", not a regular tense). */
        if (inv_pos == POS_VERB_CONJ && kin_ends_with(w, "fite")) {
            strncpy(tok->stem, "fit", KIN_MAX_STEM - 1);
            tok->verb_tense = TENSE_STATIVE_POSS;  /* -fite stative possessive */
            size_t wlen = strlen(w);
            static const struct { const char *sp; int cls; } FITE_SP[] = {
                { "bi",  8  }, { "ba",  2  }, { "gi",  7  }, { "zi", 10  },
                { "ru", 11  }, { "ga", 12  }, { "du",  1  }, { "mu",  2  },
                { "bu", 14  }, { "u",   1  }, { "a",   1  }, { "n",   1  },
                { "i",   9  }, { NULL,  0  }
            };
            for (int fi = 0; FITE_SP[fi].sp; fi++) {
                size_t slen = strlen(FITE_SP[fi].sp);
                if (wlen > 4 && wlen - 4 == slen &&
                    strncmp(w, FITE_SP[fi].sp, slen) == 0) {
                    tok->noun_class = FITE_SP[fi].cls;
                    break;
                }
            }
        }
        return;
    }

    /* Step 2 → Tree 4 (ikinyazina): pronoun? */
    PronounType ptype; int pcls;
    if (kin_is_pronoun(w, &ptype, &pcls)) {
        tok->pos            = POS_PRONOUN;
        tok->pron_type      = ptype;
        tok->noun_class     = pcls;
        tok->is_kinyarwanda = true;
        return;
    }

    /* Step 3 → Tree 1 override: known full word (exact-match lexicon).
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

    /* Step 4 → Tree 3a (inshinga imbundo): verb infinitive (ku/gu/kw/gw + C + a)? */
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

    /* Step 5 → Tree 1 heuristic: proper noun (capitalised mid-sentence)?
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

    /* Step 6 → Tree 1 (izina mbonera): noun via D+RT prefix detection.
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

        /* Amategeko y'igenamajwi priority: bare-phon-SP words (by/cy/ry/zy)
         * arise from two distinct patterns:
         *   (a) INSHINGA: SP 'bi' + vowel-initial root  (bi+izer→byizera)
         *   (b) NTERA:    RS 'bi' + vowel-initial adj.stem (bi+iza→byiza)
         *
         * Rule: compound consonants in non-root positions are surface
         * artefacts only.  When the suffix after the RS is a KNOWN
         * adjective stem, the adjective (ntera) interpretation MUST win
         * over the verb interpretation.
         * Example: byiza = bi(RS·Nt.8) + iza(adj.stem) → POS_ADJECTIVE
         *          byizera = bi(SP·Nt.8) + izer(verb root) + a → POS_VERB_CONJ  */
        if (is_bare_phon_sp) {
            char adj_stem_buf[KIN_MAX_STEM] = "";
            int  adj_cls_buf  = 0;
            if (kin_strip_adj_prefix(w, adj_stem_buf, &adj_cls_buf)) {
                tok->pos            = POS_ADJECTIVE;
                tok->noun_class     = adj_cls_buf;
                tok->is_kinyarwanda = true;
                strncpy(tok->stem, adj_stem_buf, KIN_MAX_STEM - 1);
                tok->stem[KIN_MAX_STEM - 1] = '\0';
                size_t astemlen = strlen(adj_stem_buf);
                size_t awordlen = strlen(w);
                size_t apfxlen  = (awordlen > astemlen) ? awordlen - astemlen : 0;
                strncpy(tok->detected_prefix, w, apfxlen);
                tok->detected_prefix[apfxlen] = '\0';
                return;
            }
        }

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

    /* Step 7 → Tree 2 (ntera): adjective via RS (concordance) prefix + known stem. */
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

    /* Step 7b → Known noun stem: the word itself IS a recognised noun stem
     * whose noun-class prefix (i/in/im) was elided — e.g. "nyanja" (inyanja,
     * sea/lake, Nt.9) arrives without its "i-" prefix.  Guard: ≥ 5 chars so
     * that short ambiguous forms (si, zi, gi, ko…) are not affected.
     * Checked before the open-ended verb heuristic (step 8) to prevent
     * legitimate nouns from being reinvented as conjugated-verb readings.   */
    {
        int ns_cls = 0;
        if (strlen(w) >= 5 && kin_is_known_noun_stem(w, &ns_cls)) {
            tok->pos             = POS_NOUN;
            tok->noun_class      = ns_cls;
            tok->is_kinyarwanda  = true;
            strncpy(tok->stem, w, KIN_MAX_STEM - 1);
            tok->stem[KIN_MAX_STEM - 1] = '\0';
            tok->detected_prefix[0] = '\0';   /* indomo was elided */
            check_deverbative(tok);
            return;
        }
    }

    /* Step 8 → Tree 3b (inshinga itondaguye): conjugated verb heuristic. */
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

    /* Step 9 → No tree matched: foreign or unknown word. */
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

    /* ── Context Pass A: Indomo elision recovery (Tree 1) ───────────────── *
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

    /* ── Context Pass A2: Indangahantu (locative) noun rescue ──────────── *
     * Rule: after a locative preposition (mu/ku/i/kuri/muri/kwa) the next  *
     * word refers to a place or thing — prefer noun over verb reading.     *
     * Handles forms where the noun prefix 'i/in' is elided after a         *
     * locative, e.g. "mu nyanja" (in the sea) where "nyanja" = inyanja.   *
     * No length guard here: the locative context is strong enough evidence.*
     * Belt-and-suspenders over step 7b in kin_tag_token.                   */
    for (int i = 1; i < sa->token_count; i++) {
        Token *prev = &sa->tokens[i - 1];
        Token *curr = &sa->tokens[i];
        if (curr->pos != POS_VERB_CONJ) continue;
        if (prev->pos != POS_PREPOSITION) continue;
        const char *plo = prev->lower;
        bool is_loc = (strcmp(plo, "mu")   == 0 ||
                       strcmp(plo, "ku")   == 0 ||
                       strcmp(plo, "i")    == 0 ||
                       strcmp(plo, "kuri") == 0 ||
                       strcmp(plo, "muri") == 0 ||
                       strcmp(plo, "kwa")  == 0);
        if (!is_loc) continue;
        int lcls = 0;
        if (!kin_is_known_noun_stem(curr->lower, &lcls)) continue;
        curr->pos            = POS_NOUN;
        curr->noun_class     = lcls;
        curr->verb_tense     = TENSE_NONE;
        curr->verb_ext       = VEXT_NONE;
        curr->obj_class      = 0;
        curr->is_negative    = false;
        curr->is_kinyarwanda = true;
        strncpy(curr->stem, curr->lower, KIN_MAX_STEM - 1);
        curr->stem[KIN_MAX_STEM - 1] = '\0';
        curr->detected_prefix[0] = '\0';   /* indomo elided after locative */
        check_deverbative(curr);
    }

    /* ── Context Pass B: Izina ntera (POS_RELATIVE_NOUN) detection ──────── *
     * An izina ntera is a noun that functions as a qualifier (adjective)     *
     * of another noun.  It is connected via an ikinyazina ngenera            *
     * (possessive connector) that AGREES with the head noun's class.         *
     *                                                                         *
     * Pattern:  [NOUN Nt.X] + [PRON.POSSESSIVE Nt.X] + [NOUN ?]             *
     *   "igitabo cy'Ikinyarwanda"  → igitabo(Nt.7) + cya(Nt.7) + Ikinyarwanda
     *   "inzu y'abantu"            → inzu(Nt.9)   + ya(Nt.9)   + abantu
     *   "umwana w'umugabo"         → umwana(Nt.1) + wa(Nt.1)   + umugabo
     *                                                                         *
     * When the possessive connector class matches the head noun class,       *
     * the following noun is functioning as izina ntera (relative noun).      *
     * We re-tag it POS_RELATIVE_NOUN to distinguish it from a free noun.    *
     *                                                                         *
     * Guard: we only apply this when we can confirm the poss.connector       *
     * class matches a preceding noun — to avoid false positives on           *
     * sentence-initial possessives (which are standalone pronouns).          */
    for (int i = 2; i < sa->token_count; i++) {
        Token *head  = &sa->tokens[i - 2];  /* potential head noun */
        Token *conn  = &sa->tokens[i - 1];  /* possessive connector */
        Token *qual  = &sa->tokens[i];       /* potential izina ntera */

        /* Connector must be a possessive pronoun */
        if (conn->pos != POS_PRONOUN || conn->pron_type != PRON_POSSESSIVE)
            continue;
        /* Head must be a noun with a known class */
        if (head->pos != POS_NOUN || head->noun_class == 0)
            continue;
        /* Qualifier must currently be a plain noun */
        if (qual->pos != POS_NOUN)
            continue;
        /* Possessive connector class must agree with head noun class.
         * Class 0 on the connector means it applies to multiple classes
         * (e.g. "ya" is used for Nt.4, Nt.6, Nt.9 — all share "ya").
         * Only re-tag when the class match is confirmed.               */
        if (conn->noun_class != 0 && conn->noun_class != head->noun_class)
            continue;

        /* Confirmed: qualifier is an izina ntera (noun as adjective).    *
         * Transition: POS_NOUN → POS_RELATIVE_NOUN                       *
         * The noun class and stem remain unchanged; only the POS changes. */
        qual->pos = POS_RELATIVE_NOUN;
    }

    /* ── Context Pass C: Verbal noun detection (izina ryaturutse ku nshinga) ─ *
     *                                                                           *
     * In Kinyarwanda, a conjugated verb form can function as a noun when:      *
     *   – Its object-marker (OM) class matches the class of the following      *
     *     pronoun or the SP of the following verb.                             *
     *   – There is no prior noun of that class serving as a real subject.     *
     *                                                                           *
     * Example: "ibimera byose byera imbuto"                                    *
     *   ibimera = i(SP·4) + bi(OM·8) + mer + a  → verb analysis               *
     *   byose   = Nt.8 pronoun (directly modifies ibimera)                     *
     *   byera   = bi(SP·8) + era  → SP=8 agrees with OM class of ibimera       *
     *   → ibimera is a Nt.8 verbal noun meaning "crops / plants"              *
     *                                                                           *
     * Detection: VERB(obj_class=Y) immediately followed by PRON(class=Y)      *
     * is the primary signal.  VERB(obj_class=Y) → VERB(sp=Y) is secondary     *
     * (requires no prior Nt.Y noun that could be the real subject).           */
    for (int i = 0; i < sa->token_count; i++) {
        Token *cur = &sa->tokens[i];
        if (cur->pos != POS_VERB_CONJ) continue;
        if (cur->obj_class == 0) continue;       /* no OM = not a candidate   */
        int om_cls = cur->obj_class;

        bool nominal_context = false;

        /* Primary: next token is a pronoun of the same class as OM */
        if (i + 1 < sa->token_count) {
            Token *nxt = &sa->tokens[i + 1];
            if (nxt->pos == POS_PRONOUN && nxt->noun_class == om_cls)
                nominal_context = true;
        }

        /* Secondary: one of the next 2 tokens is a verb whose SP = OM class,
         * AND no prior noun of that class can serve as the subject.           */
        if (!nominal_context) {
            for (int j = i + 1; j < sa->token_count && j <= i + 2; j++) {
                Token *nxt = &sa->tokens[j];
                if (nxt->pos != POS_VERB_CONJ) continue;
                if (nxt->noun_class != om_cls) continue;
                /* Guard: scan back for a prior noun of om_cls */
                bool prior_subject = false;
                for (int k = 0; k < i; k++) {
                    if (sa->tokens[k].pos == POS_NOUN &&
                        sa->tokens[k].noun_class == om_cls)
                        { prior_subject = true; break; }
                }
                if (!prior_subject) { nominal_context = true; break; }
            }
        }

        if (!nominal_context) continue;

        cur->gram_role    = GRAM_ROLE_VERBAL_NOUN;
        cur->is_deverbative = true;
        strncpy(cur->verb_root, cur->stem, KIN_MAX_STEM - 1);
        cur->verb_root[KIN_MAX_STEM - 1] = '\0';
    }

    /* Re-check has_verb after all context passes.
     * Verbal nouns (gram_role == GRAM_ROLE_VERBAL_NOUN) are syntactically
     * nouns; they must not satisfy the "sentence has a verb" requirement.    */
    sa->has_verb = false;
    for (int i = 0; i < sa->token_count; i++) {
        Token *t = &sa->tokens[i];
        if (t->pos == POS_VERB_INF) { sa->has_verb = true; continue; }
        if (t->pos == POS_VERB_CONJ &&
            t->gram_role != GRAM_ROLE_VERBAL_NOUN)
            sa->has_verb = true;
    }
}
