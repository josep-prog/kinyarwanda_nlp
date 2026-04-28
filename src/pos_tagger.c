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

#include <ctype.h>
#include <string.h>
#include "../include/kinyarwanda.h"

/* Primary noun stems that share a stripped form with a known verb root but
 * are NOT deverbative.  These are lexically independent nouns that happen to
 * look like verb-derived forms; the deverbative heuristic must skip them.
 * Add any new collision here rather than changing check_deverbative's logic. */
static const char *PRIMARY_NOUN_STEMS[] = {
    "siga",   /* igisiga/ibisiga – birds of prey (eagles/vultures/large hawks)
               * NOT from gusiga (to leave/anoint); independent lexical item  */
    "gore",   /* umugore – woman; primary lexical noun, NOT deverbative from
               * kugora (to be difficult/hard); independent root item         */
    "gabo",   /* umugabo – man;  primary lexical noun, NOT deverbative from
               * kugaba (to give lavishly/distribute gifts); independent item */
    "taka",   /* ubutaka – soil/land/earth; primary lexical noun, NOT from
               * gutaka (to shout/cry out); unrelated independent root       */
    "tungo",  /* itungo/amatungo – domestic animal; primary lexical noun, NOT
               * deverbative from gutunga (to acquire/possess wealth);
               * umutunzi/abatunzi are the true deverbatives of gutunga     */
    "hungu",  /* umuhungu – boy; primary lexical noun meaning 'boy/son',
               * NOT deverbative from guhunga (to flee); independent root   */
    "kuru",   /* agakuru/impamvu – reason/matter; NOT from gukura (to grow) */
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
        return;
    }
    /* Privative/completive 'ti-' prefix on vowel-initial verb roots.
     * In some abstract nouns (Nt.14 ubu- class), the 'ti-' prefix combines
     * with a vowel-initial verb root: ti + icur → ticura (ubuticura).
     * The 'ti' is a privative/completive prefix (cf. nti- negation marker);
     * the underlying root is the vowel-initial verb (kwicura = kwi+icur+a).
     * Condition: stem starts 'ti' + vowel, remainder minus FV is known root.
     * e.g. ticura → strip 'ti' → icura → strip 'a' → icur = known (kwicura). */
    if (slen >= 5 && tok->stem[0] == 't' && tok->stem[1] == 'i') {
        char ti_fv = tok->stem[slen - 1];
        bool ti_ends_v = (ti_fv=='a'||ti_fv=='e'||ti_fv=='i'
                          ||ti_fv=='o'||ti_fv=='u');
        if (ti_ends_v) {
            size_t ti_slen = slen - 2;   /* length of part after 'ti' */
            char _s2 = tok->stem[2];
            bool _v2 = (_s2=='a'||_s2=='e'||_s2=='i'||_s2=='o'||_s2=='u');
            /* Case A: ti + vowel-initial root still visible (no elision).
             * e.g. if root were 'abur' → stem 'tiabura' → after ti: 'abura'
             * strip FV → 'abur'. */
            if (_v2 && ti_slen >= 3) {
                char ti_root[KIN_MAX_STEM];
                strncpy(ti_root, tok->stem + 2, ti_slen - 1);
                ti_root[ti_slen - 1] = '\0';
                if (strlen(ti_root) >= 2 && kin_is_known_verb_stem(ti_root)) {
                    tok->is_deverbative = true;
                    strncpy(tok->verb_root, ti_root, KIN_MAX_STEM - 1);
                    tok->verb_root[KIN_MAX_STEM - 1] = '\0';
                    return;
                }
            }
            /* Case B: ti + vowel-initial root, but VV contact elided root's
             * initial 'i': ti + icur + a → ticura (i elides).
             * Recover: strip 'ti', prepend 'i', strip FV → check lexicon.
             * e.g. ticura → after ti: cura → strip FV: cur → prepend i: icur */
            if (!_v2 && ti_slen >= 3) {
                char ti_root[KIN_MAX_STEM];
                /* Reconstruct vowel-initial root: prepend 'i', copy (ti_slen-1)
                 * chars (strips FV), terminate. e.g. ticura: ti_slen=4,
                 * copy 3 chars "cur" → ti_root = "icur". */
                ti_root[0] = 'i';
                strncpy(ti_root + 1, tok->stem + 2, ti_slen - 1);
                ti_root[ti_slen] = '\0';
                if (strlen(ti_root) >= 2 && kin_is_known_verb_stem(ti_root)) {
                    tok->is_deverbative = true;
                    strncpy(tok->verb_root, ti_root, KIN_MAX_STEM - 1);
                    tok->verb_root[KIN_MAX_STEM - 1] = '\0';
                    return;
                }
            }
        }
    }
    /* Privative 'da-' prefix (dependent-negative variant -da-, Zorc & Nibagwire
     * §2007): attaches to consonant-initial verb stems without VV elision.
     * e.g. dafatika → strip 'da' → fatika → strip FV → fatik (verb root).
     * Attested: ubudahwema (non-stop), ubudasiba (without-ceasing),
     *           ubudafatika (instability). Condition: 'da' + consonant. */
    if (slen >= 5 && tok->stem[0] == 'd' && tok->stem[1] == 'a') {
        char da_s2 = tok->stem[2];
        bool da_v2 = (da_s2=='a'||da_s2=='e'||da_s2=='i'||da_s2=='o'||da_s2=='u');
        if (!da_v2) {
            size_t da_slen = slen - 2;
            if (da_slen >= 3) {
                char da_root[KIN_MAX_STEM];
                strncpy(da_root, tok->stem + 2, da_slen - 1);
                da_root[da_slen - 1] = '\0';
                if (strlen(da_root) >= 2 && kin_is_known_verb_stem(da_root)) {
                    tok->is_deverbative = true;
                    strncpy(tok->verb_root, da_root, KIN_MAX_STEM - 1);
                    tok->verb_root[KIN_MAX_STEM - 1] = '\0';
                    return;
                }
            }
        }
    }
    /* Agentive -nzi suffix: C ends in 'zi' with underlying g→z before -i.
     * e.g. "tunzi" → strip 'zi' → "tun", reverse z→g on last consonant → "tung"
     * cf. umutunzi (one who has wealth) ← gutunga (-tung- root, g→z before -i).
     * Rule: strip 'i' (FV) → "tunz"; final 'z' was 'g' before -i → "tung". */
    if (slen >= 4 && tok->stem[slen-2] == 'z' && tok->stem[slen-1] == 'i') {
        /* root = stem minus final 'i', then replace trailing 'z' with 'g' */
        char root_gz[KIN_MAX_STEM];
        strncpy(root_gz, tok->stem, slen - 1);
        root_gz[slen - 1] = '\0';                 /* "tunz"  */
        root_gz[slen - 2] = 'g';                  /* "tung"  */
        if (strlen(root_gz) >= 2 && kin_is_known_verb_stem(root_gz)) {
            tok->is_deverbative = true;
            strncpy(tok->verb_root, root_gz, KIN_MAX_STEM - 1);
            tok->verb_root[KIN_MAX_STEM - 1] = '\0';
            return;
        }
        /* r→z before agentive -i: root-final 'r' palatalizes to 'z' before -i.
         * e.g. "cuzi" → "cuz" → z was 'r': "cur" ← gucura (to forge).
         * Try same -zi stem with z reverted to 'r' instead of 'g'. */
        char root_rz[KIN_MAX_STEM];
        strncpy(root_rz, tok->stem, slen - 1);
        root_rz[slen - 1] = '\0';                 /* "cuz"   */
        root_rz[slen - 2] = 'r';                  /* "cur"   */
        if (strlen(root_rz) >= 2 && kin_is_known_verb_stem(root_rz)) {
            tok->is_deverbative = true;
            strncpy(tok->verb_root, root_rz, KIN_MAX_STEM - 1);
            tok->verb_root[KIN_MAX_STEM - 1] = '\0';
        }
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
         * "abafite" = a(AUG) + ba(SP·Nt.2) + fit + e: participial "those who have".
         * Structure: SP + -fite  (stative of gufata, fat→fit vowel shift). */
        if (inv_pos == POS_VERB_CONJ && kin_ends_with(w, "fite")) {
            strncpy(tok->stem, "fit", KIN_MAX_STEM - 1);
            tok->verb_tense = TENSE_STATIVE_POSS;  /* -fite stative possessive */
            size_t wlen = strlen(w);
            static const struct { const char *sp; int cls; } FITE_SP[] = {
                { "bi",  8  }, { "aba", 2  }, { "ba",  2  }, { "gi",  7  },
                { "zi", 10  }, { "ru", 11  }, { "ga", 12  }, { "du",  1  },
                { "mu",  2  }, { "bu", 14  }, { "u",   1  }, { "a",   1  },
                { "n",   1  }, { "i",   9  }, { NULL,  0  }
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
            /* PRESENT_NORA is normally too permissive, but allow it when the
             * extracted stem is confirmed in the verb lexicon (e.g. azitwa
             * from kwitwa: stem="tw" is a known reflexive root).           */
            && (pn_tense != TENSE_PRESENT_NORA || kin_is_known_verb_stem(pn_stem))
            /* Subjunctive: allow when stem is a known verb root.
             * e.g. Mwororoke = mw(2pl SP) + ororok(kororoka) + e(SUBJ).
             * Bare subjunctive forms at sentence-start (quoted speech) must be
             * admitted when the stem is confirmed in the verb lexicon.          */
            && (pn_tense != TENSE_SUBJUNCTIVE || kin_is_known_verb_stem(pn_stem))
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
                || pn_tense == TENSE_SUBJUNCTIVE
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
            /* PRESENT_NORA: require known stem.  Suppress when the stripped OM
             * class equals the noun class — that means the "OM" is actually the
             * noun-class prefix, not a real object marker.
             * e.g. ubu-riganya: v_obj=14, cls=14 → noun prefix, not OM → noun wins.
             * e.g. bu-riganya (SP=bu, no OM): v_obj=0 → guard doesn't fire → ok. */
            && (v_tense != TENSE_PRESENT_NORA
                || (kin_is_known_verb_stem(v_stem) && !(v_obj > 0 && v_obj == cls))
                || (is_bare_phon_sp && kin_is_valid_verb_stem_shape(v_stem)))
            /* SUBJUNCTIVE: allow when stem is known AND the OM is not just the
             * noun-class prefix in disguise.
             *
             * Key guard: !(v_obj > 0 && v_obj == cls)
             *   When the detected object-marker class equals the noun class from
             *   kin_strip_noun_prefix, the "OM" is actually the noun's own class
             *   prefix being misread as an object marker.  The word is a noun.
             *
             *   e.g. "abagore" Nt.2: cls=2, verb analysis gives v_obj=2 (ba as OM).
             *        But 'ba' here IS the Nt.2 noun prefix (aba-gore = women), so
             *        it cannot also be an OM.  Noun wins.  Without this guard the
             *        verb reading a(SP)+ba(OM·Nt.2)+gor+e(SUBJ) wins over the noun
             *        reading, which then gets promoted to a deverbative noun of
             *        kugora (to be difficult) — a completely wrong analysis.
             *
             * bare_phon_sp (cy/by/ry/zy) bypasses the OM guard: those prefixes
             * can only arise from verb SP i→y mutation, never from a noun prefix. */
            && (v_tense != TENSE_SUBJUNCTIVE
                || (kin_is_known_verb_stem(v_stem) && !(v_obj > 0 && v_obj == cls))
                || is_bare_phon_sp)
            && v_tense != TENSE_IMPERATIVE
            /* Tenses with unambiguous morphological markers bypass stem check:
             * PAST_PERF  – surface differs from citation stem (murakoze→koz)
             * PAST_IMPF  – -aga suffix is highly distinctive
             * NEG_RELATIVE – -ta- marker uniquely identifies this form
             * FUTURE / NARRATIVE / COPULA – markers are unambiguous enough
             * bare_phon_sp – phonological mutation alone is sufficient signal.
             *
             * Same OM-equals-noun-class guard applied here for kin_is_known_verb_stem
             * path: a known stem does not override a noun when the OM matches the
             * noun class prefix. */
            && (v_tense == TENSE_PAST_PERF
                || v_tense == TENSE_PAST_IMPF
                || v_tense == TENSE_NEG_RELATIVE
                || v_tense == TENSE_FUTURE
                || v_tense == TENSE_NARRATIVE
                || v_tense == TENSE_COPULA_PAST
                || v_tense == TENSE_COPULA_PRES
                || v_tense == TENSE_SUBJUNCTIVE_LOC
                || v_tense == TENSE_SUBJUNCTIVE
                || (kin_is_known_verb_stem(v_stem) && !(v_obj > 0 && v_obj == cls))
                || kin_is_causative_y_surface(v_stem)  /* r+y→z §1.3 surface root */
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

    /* Step 8 → Tree 3b (inshinga itondaguye): conjugated verb heuristic.
     *
     * Quality guard: kin_is_verb_conjugated() always succeeds for any word
     * whose length and ending vowel permit a structural SP+stem+FV parse —
     * including words with single-char or completely unknown stems (fallback
     * path in verb_match_inner).  Without a guard, proper names like "Ada"
     * at sentence start (capital is orthographic, not a proper-noun marker)
     * would be labelled as conjugated verbs with fabricated roots ("kuda").
     *
     * The guard mirrors the one applied in Step 5 (proper-noun path):
     *   • PRESENT_NORA tense is the most permissive (any word SP+C+a).
     *     Accept only when the extracted stem is a KNOWN verb root.
     *   • SUBJUNCTIVE is similarly permissive (SP+C+e).
     *     Accept only when stem is known.
     *   • All other tenses carry unambiguous morphological markers (ra, aga,
     *     za, ye, ta-, ka-) that are strong enough evidence on their own.
     *
     * Additionally, single-char stems ("d", "a", "e", etc.) are accepted
     * only for the small set of confirmed monosyllabic Kinyarwanda verb
     * roots ("b"=kuba, "h"=guha, "z"=kuza, "v"=kuva) — never for unknown
     * single chars that arise from a fallback parse of a short name.        */
    /* ── Hortative ni- pre-check ────────────────────────────────────────── *
     * nimutegere = ni(HORT) + mu(2pl SP) + teg + er + e                    *
     * nimwumve   = ni(HORT) + mw(2pl SP) + umv + e                         *
     * The particle 'ni' precedes the full conjugated form (SP+TM+root+FV). *
     * Guard: len > 4, starts "ni", third char is a consonant (not vowel),  *
     * and the suffix starting at +2 is itself a valid conjugated verb.     */
    bool is_hort = false;
    const char *verb_w = w;
    {
        size_t hlen = strlen(w);
        unsigned char c3 = (unsigned char)(hlen > 2 ? w[2] : '\0');
        bool c3_vowel = (c3=='a'||c3=='e'||c3=='i'||c3=='o'||c3=='u');
        if (hlen > 4 && w[0]=='n' && w[1]=='i' && !c3_vowel) {
            char hort_stem[KIN_MAX_STEM] = "";
            int hort_cls = 0;
            VerbTense hort_tense = TENSE_NONE;
            int hort_obj = 0;
            VerbExtension hort_ext = VEXT_NONE;
            bool hort_neg = false;
            if (kin_is_verb_conjugated(w + 2, hort_stem, &hort_cls, &hort_tense,
                                       &hort_obj, &hort_ext, &hort_neg)
                && (hort_tense != TENSE_PRESENT_NORA || kin_is_known_verb_stem(hort_stem))
                && (hort_tense != TENSE_SUBJUNCTIVE   || kin_is_known_verb_stem(hort_stem))) {
                is_hort = true;
                verb_w  = w + 2;
            }
        }
    }

    int scls = 0, obj_cls = 0;
    VerbTense vtense = TENSE_NONE;
    VerbExtension vext = VEXT_NONE;
    bool is_neg = false;
    if (kin_is_verb_conjugated(verb_w, stem, &scls, &vtense, &obj_cls, &vext, &is_neg)
        /* PRESENT_NORA: require confirmed stem to avoid naming proper nouns
         * or short words (e.g. "ada", "izi", "uko") as conjugated verbs. */
        && (vtense != TENSE_PRESENT_NORA || kin_is_known_verb_stem(stem))
        /* SUBJUNCTIVE: same gate — prevents "abe", "ize" etc. from being
         * wrongly labelled as verbs when they are actually names or nouns. */
        && (vtense != TENSE_SUBJUNCTIVE  || kin_is_known_verb_stem(stem))
        ) {
        tok->pos            = POS_VERB_CONJ;
        tok->noun_class     = scls;
        tok->verb_tense     = vtense;
        tok->verb_ext       = vext;
        tok->obj_class      = obj_cls;
        tok->is_negative    = is_neg;
        tok->is_hortative   = is_hort;
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

    /* Step 9 → No tree matched: foreign or unknown word.
     *
     * Proper-noun phonotactics heuristic (RULE before FALLBACK):
     *   Before declaring the word foreign, check whether it is a capitalised
     *   word that follows Kinyarwanda phonotactics.  Such a word is almost
     *   certainly a proper name written in Kinyarwanda orthography — either a
     *   Kinyarwanda name not yet in the lexicon, or a foreign name adapted to
     *   Kinyarwanda writing (e.g. Henoki, Iradi, Metushayeli, Nyagasaro).
     *
     *   The phonotactics test asks two questions that any genuine Kinyarwanda
     *   (or Kinyarwanda-adapted) word must pass:
     *     (a) No vowel hiatus: no two vowels appear adjacent without a
     *         consonant between them (§4: VV is illegal in Kinyarwanda).
     *     (b) No invalid consonant cluster: no combination of consonants that
     *         cannot appear in native or adapted Kinyarwanda words.
     *
     *   Truly foreign words that violate these rules (e.g. "Smith" with initial
     *   sm-cluster, or "idea" with VV "ea") stay POS_FOREIGN so that the
     *   syntax checker can flag them as genuinely unrecognised.
     *
     *   This is NOT memorisation: no lookup table of specific names is used.
     *   Any capital-letter word that passes the phonotactic filter is accepted
     *   as a proper noun regardless of whether it is in the lexicon.          */
    if (isupper((unsigned char)tok->surface[0]) &&
        !kin_has_vowel_hiatus(tok->lower) &&
        !kin_has_invalid_cluster(tok->lower)) {
        tok->pos            = POS_NOUN;
        tok->noun_class     = 0;       /* class undetermined for proper nouns  */
        tok->is_proper_noun = true;
        tok->is_kinyarwanda = true;
        return;
    }

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
     * After a locative marker (mu/ku/i/kuri/muri/kwa/ava/kuva) the next   *
     * word refers to a place or thing — prefer noun over verb/locative.    *
     *                                                                        *
     * Case 1 (POS_VERB_CONJ): word mistagged as verb but is a noun with    *
     *   elided prefix, e.g. "mu nyanja" where "nyanja" = inyanja.          *
     *                                                                        *
     * Case 2 (POS_LOCATIVE): word is a locative invariable that is ALSO a  *
     *   common noun with elided augment, e.g. "ku munsi wa karindwi" where  *
     *   "munsi" = umunsi (day, Nt.3) NOT the locative "below".             *
     *   Extra guard: the next token must be a possessive connector —        *
     *   that disambiguates "ku munsi wa X" (on day X) from standalone       *
     *   "ku munsi" (below/down, which should stay as a locative).           */
    for (int i = 1; i < sa->token_count; i++) {
        Token *prev = &sa->tokens[i - 1];
        Token *curr = &sa->tokens[i];
        bool is_verb_cj   = (curr->pos == POS_VERB_CONJ);
        bool is_loc_invar = (curr->pos == POS_LOCATIVE);
        if (!is_verb_cj && !is_loc_invar) continue;
        if (prev->pos != POS_LOCATIVE) continue;
        const char *plo = prev->lower;
        bool is_loc = (strcmp(plo, "mu")   == 0 ||
                       strcmp(plo, "ku")   == 0 ||
                       strcmp(plo, "i")    == 0 ||
                       strcmp(plo, "kuri") == 0 ||
                       strcmp(plo, "muri") == 0 ||
                       strcmp(plo, "kwa")  == 0 ||
                       strcmp(plo, "ava")  == 0 ||
                       strcmp(plo, "kuva") == 0);
        if (!is_loc) continue;
        /* Case 2 guard: locative invariable needs a following possessive    *
         * connector to confirm it is really an elided noun, not a direction.*/
        if (is_loc_invar) {
            if (i + 1 >= sa->token_count) continue;
            Token *nxt = &sa->tokens[i + 1];
            if (nxt->pos != POS_PRONOUN || nxt->pron_type != PRON_POSSESSIVE) continue;
        }
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

    /* ── Context Pass A3: Numeral after mirongo/magana ──────────────────── *
     * When mirongo or magana is followed by a word that was tagged as verb  *
     * or foreign but whose surface matches a numeral pattern, retag it as   *
     * PRON_NUMERICAL.  This handles the phonological form where the class   *
     * prefix vowel 'i-' is elided after the preceding vowel:               *
     *   magana cyenda = 900  (icyenda with i- elided after magana -a)      *
     *   magana nani   = 800  (inani with i-/a- form variant)               *
     * kin_numerical_value() returns > 0 for these surface forms.           */
    for (int i = 1; i < sa->token_count; i++) {
        Token *curr = &sa->tokens[i];
        if (curr->pos == POS_PUNCTUATION) continue;
        if (curr->pos == POS_PRONOUN && curr->pron_type == PRON_NUMERICAL)
            continue;  /* already correct */
        /* Find previous non-punct token */
        int pi = i - 1;
        while (pi >= 0 && sa->tokens[pi].pos == POS_PUNCTUATION) pi--;
        if (pi < 0) continue;
        Token *prev = &sa->tokens[pi];
        if (strcmp(prev->lower, "mirongo") != 0 && strcmp(prev->lower, "magana") != 0)
            continue;
        int v = kin_numerical_value(curr->lower);
        if (v <= 0) continue;
        /* Retag as numeral */
        curr->pos         = POS_PRONOUN;
        curr->pron_type   = PRON_NUMERICAL;
        curr->noun_class  = 0;   /* class undetermined for elided form */
        curr->verb_tense  = TENSE_NONE;
        curr->verb_ext    = VEXT_NONE;
        curr->obj_class   = 0;
        curr->is_negative = false;
        curr->is_kinyarwanda = true;
        curr->stem[0] = '\0';
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
