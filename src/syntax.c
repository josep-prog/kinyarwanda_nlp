/*
 * syntax.c
 * Syntax checking for Kinyarwanda sentences.
 *
 * Rules applied (derived from the book):
 *
 * RULE 1 – Noun-adjective class agreement (p.65-68)
 *   An adjective (ntera) must bear the concordance prefix (indangasano)
 *   that matches the noun class it modifies.
 *   e.g.  umuntu munini  (nt.1 noun + nt.1 adjective prefix mu-)  ✓
 *         umuntu binini  (nt.1 noun + nt.8 adjective prefix bi-)  ✗
 *
 * RULE 2 – Possessive connector class agreement (p.91-92)
 *   A possessive pronoun (ikinyazina ngenera) must agree with the class
 *   of the noun it attaches to.
 *   e.g.  urugo rwacu  (nt.11: rwa + cu)  ✓
 *         urugo wacu   (nt.1 connector on nt.11 noun)  ✗
 *
 * RULE 3 – Sentence completeness
 *   A sentence (umuvugo) must contain at least one verb (inshinga).
 *   The book (p.57) defines umuvugo as requiring a verb.
 *
 * RULE 4 – Unknown words
 *   Words tagged POS_FOREIGN that are not proper nouns are flagged.
 */

#include <string.h>
#include <stdio.h>
#include "../include/kinyarwanda.h"

static bool is_vowel_ch(char c) {
    return c=='a'||c=='e'||c=='i'||c=='o'||c=='u';
}

/* Expected concordance prefix for adjective, keyed by noun class */
static const char *adj_concordance[17] = {
    "",     /* 0 = unused     */
    "mu",   /* Nt.1           */
    "ba",   /* Nt.2           */
    "mu",   /* Nt.3           */
    "mi",   /* Nt.4           */
    "ri",   /* Nt.5           */
    "ma",   /* Nt.6           */
    "ki",   /* Nt.7           */
    "bi",   /* Nt.8           */
    "n",    /* Nt.9           */
    "zi",   /* Nt.10          */
    "ru",   /* Nt.11          */
    "ka",   /* Nt.12          */
    "tu",   /* Nt.13          */
    "bu",   /* Nt.14          */
    "ku",   /* Nt.15          */
    "ha",   /* Nt.16          */
};

/* Expected possessive connector for each noun class */
static const char *poss_connector[17] = {
    "",
    "wa",  "ba",  "wa",  "ya",  "rya", "ya",  "cya",
    "bya", "ya",  "za",  "rwa", "ka",  "twa", "bwa",
    "kwa", "ha",
};

static void add_error(SentenceAnalysis *sa, ErrorType type, int idx,
                      const char *msg, const char *suggestion) {
    if (sa->error_count >= KIN_MAX_ERRORS) return;
    Error *e = &sa->errors[sa->error_count++];
    e->type         = type;
    e->token_index  = idx;
    strncpy(e->message,    msg,        KIN_MAX_MSG - 1);
    strncpy(e->suggestion, suggestion, KIN_MAX_MSG - 1);
    sa->tokens[idx].error_count++;
}

void kin_check_syntax(SentenceAnalysis *sa) {
    /* RULE 3: Must have a verb
     * Exception: a sentence composed entirely of interjections, particles,
     * or adverbs is a valid elliptical utterance (e.g. greetings: "Muraho",
     * "Amakuru", "Murakoze", "Yego", "Oya").  Do not flag these as errors. */
    if (!sa->has_verb && sa->token_count > 0) {
        bool all_interj = true;
        for (int ii = 0; ii < sa->token_count; ii++) {
            POS p = sa->tokens[ii].pos;
            if (p != POS_INTERJECTION && p != POS_ADVERB &&
                p != POS_VERB_PARTICLE && p != POS_CONJUNCTION &&
                p != POS_PUNCTUATION) {
                all_interj = false;
                break;
            }
        }
        if (!all_interj) {
            add_error(sa, ERR_NO_VERB, 0,
                "Iri nteruro ntirigira inshinga (umuvugo) / "
                "This sentence has no verb.",
                "Ongeraho inshinga (Add a verb).");
        }
    }

    /* RULE 1 & 2: Agreement between adjacent noun → adjective / possessive */
    for (int i = 0; i < sa->token_count - 1; i++) {
        Token *cur  = &sa->tokens[i];
        Token *next = &sa->tokens[i + 1];
        if (cur->pos  == POS_PUNCTUATION) continue;
        if (next->pos == POS_PUNCTUATION) continue;

        /* RULE 1: Noun followed immediately by adjective */
        if (cur->pos == POS_NOUN && cur->noun_class > 0 &&
            next->pos == POS_ADJECTIVE && next->noun_class > 0) {
            if (cur->noun_class != next->noun_class) {
                char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
                const char *expected_pfx = adj_concordance[cur->noun_class];
                snprintf(msg, sizeof(msg),
                    "Gushyira hamwe nabi: '%s' (inteko %d) na '%s' (inteko %d). "
                    "Agreement error: '%s' (class %d) with '%s' (class %d).",
                    cur->surface, cur->noun_class,
                    next->surface, next->noun_class,
                    cur->surface, cur->noun_class,
                    next->surface, next->noun_class);
                snprintf(sug, sizeof(sug),
                    "Indangasano y'intera igomba kuba '%s' kugira ngo ishyikire "
                    "inteko %d. / "
                    "The adjective concordance prefix should be '%s' for class %d.",
                    expected_pfx, cur->noun_class,
                    expected_pfx, cur->noun_class);
                add_error(sa, ERR_ADJ_AGREEMENT, i + 1, msg, sug);
            }
        }

        /* RULE 2: Noun followed by possessive or reflexive possessive pronoun.
         * Covers both ikinyazina ngenera (wa/ya/rya...) and
         * ikinyazina ngenera ngenga (wange/wacu/wabo...).        p.91-96 */
        if (cur->pos == POS_NOUN && cur->noun_class > 0 &&
            next->pos == POS_PRONOUN &&
            (next->pron_type == PRON_POSSESSIVE ||
             next->pron_type == PRON_REFLEXIVE) &&
            next->noun_class > 0) {
            /* Two classes may share the same connector (e.g. Nt.1 and Nt.3
             * both use "wa").  Compare connector strings, not class numbers,
             * to avoid false positives like "umunsi wa mbere".           */
            if (strcmp(poss_connector[cur->noun_class],
                       poss_connector[next->noun_class]) != 0) {
                char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
                const char *expected = poss_connector[cur->noun_class];
                snprintf(msg, sizeof(msg),
                    "Ikinyazina ngenera '%s' ntigishyikira izina '%s' (inteko %d). "
                    "Possessive '%s' does not agree with noun '%s' (class %d).",
                    next->surface, cur->surface, cur->noun_class,
                    next->surface, cur->surface, cur->noun_class);
                snprintf(sug, sizeof(sug),
                    "Ikinyazina ngenera gikwiye ni '%s' (inteko %d). "
                    "The correct possessive connector for class %d is '%s'.",
                    expected, cur->noun_class, cur->noun_class, expected);
                add_error(sa, ERR_POSS_AGREEMENT, i + 1, msg, sug);
            }
        }
    }

    /* RULE 4: Flag unrecognised non-proper-noun words */
    for (int i = 0; i < sa->token_count; i++) {
        Token *t = &sa->tokens[i];
        if (t->pos == POS_PUNCTUATION) continue;  /* punctuation is not a word */
        if (t->pos == POS_FOREIGN && !t->is_proper_noun) {
            char msg[KIN_MAX_MSG];
            snprintf(msg, sizeof(msg),
                "Ijambo '%s' ntirizwi mu Kinyarwanda. "
                "Word '%s' is not recognised as Kinyarwanda.",
                t->surface, t->surface);
            add_error(sa, ERR_UNKNOWN_WORD, i, msg,
                "Suzuma imyandikire cyangwa ijambo ry'amahanga. "
                "Check spelling or this may be a foreign word.");
        }
    }

    /* RULE 5: Subject-verb agreement (indangasubizi y'inshinga na izina)  *
     * A conjugated verb's subject prefix (SP) must match the class of the  *
     * subject noun that precedes it.  Book p.88 + year-4 p.102+.           *
     * We only check adjacent noun → verb pairs where both classes are known */
    for (int i = 0; i < sa->token_count - 1; i++) {
        Token *noun = &sa->tokens[i];
        /* Skip punctuation in the outer scan */
        if (noun->pos == POS_PUNCTUATION) continue;
        /* Find the next non-punctuation token as the verb candidate */
        int vi = i + 1;
        while (vi < sa->token_count && sa->tokens[vi].pos == POS_PUNCTUATION) vi++;
        if (vi >= sa->token_count) continue;
        Token *verb = &sa->tokens[vi];

        if (noun->pos != POS_NOUN)      continue;
        if (verb->pos != POS_VERB_CONJ) continue;
        /* Do not check agreement across a clause/sentence boundary.
         * A comma or period between noun and verb means they belong to
         * different clauses — subject-verb agreement does not apply.       */
        {
            bool boundary_between = false;
            for (int k = i + 1; k < vi; k++) {
                if (sa->tokens[k].pos == POS_PUNCTUATION &&
                    (sa->tokens[k].is_clause_boundary ||
                     sa->tokens[k].is_sent_boundary)) {
                    boundary_between = true;
                    break;
                }
            }
            if (boundary_between) continue;
        }
        /* Verbal nouns (izina ryaturutse ku nshinga) are syntactically nouns
         * even though they carry POS_VERB_CONJ morphology.  They do not
         * agree with a preceding subject noun — they ARE the subject noun. */
        if (verb->gram_role == GRAM_ROLE_VERBAL_NOUN) continue;
        /* Skip when either side has an unknown or ambiguous class */
        if (noun->noun_class == 0 || verb->noun_class == 0) continue;
        /* Nt.1 and Nt.3 both use the same SP "a/u"; treat as compatible */
        int nc = noun->noun_class;
        int vc = verb->noun_class;
        if ((nc == 1 || nc == 3) && (vc == 1 || vc == 3)) continue;
        /* Nt.4 and Nt.9 both use the same SP "i"; treat as compatible.
         * e.g. "Imana iravuga" — Imana is Nt.9 but iravuga's SP "i" is
         * stored as Nt.4 (first-match in the SP table).              */
        if ((nc == 4 || nc == 9) && (vc == 4 || vc == 9)) continue;
        /* Nt.9 (singular) and Nt.10 (plural) are a paired noun class.
         * Class 9 nouns used with plural reference take class 10 agreement
         * e.g. "imbuto zikwiriye" — imbuto is Nt.9 but plural agreement
         * uses SP "zi" (Nt.10). These are grammatically correct forms.  */
        if ((nc == 9 || nc == 10) && (vc == 9 || vc == 10)) continue;
        /* "ya" SP (stored as cls 6) is ambiguous: it is ALSO the Nt.1 past
         * tense form (a-subject + past 'a' marker → "ya").  Do not flag   *
         * agreement errors when verb SP class is 6 and noun is Nt.1/3/9.  *
         * e.g.  "Imana yaremye ijuru" – Nt.9 noun + ya (Nt.1-past) verb.  */
        if (vc == 6 && (nc == 1 || nc == 3 || nc == 9)) continue;
        /* Copula + locative forms (TENSE_COPULA_PAST/PRES) are existential   *
         * constructions and do not follow strict subject-verb agreement.      *
         * "yariho ubusa busa" = "there was emptiness" – yariho is impersonal.*
         * "hari" (existential) similarly carries no agreement obligation.     *
         * Skip agreement checking for all copula locative tenses.            */
        if (verb->verb_tense == TENSE_COPULA_PAST ||
            verb->verb_tense == TENSE_COPULA_PRES) continue;

        if (nc != vc) {
            /* Before flagging, scan further back: if an earlier noun in the
             * sentence has a class that matches the verb's SP (vc), then the
             * immediate predecessor is an object, not the subject.  Suppress
             * the error — the true subject is that earlier noun.
             * Also treat Nt.4/9 as equivalent (same SP "i") when scanning. */
            bool has_remote_subject = false;
            for (int j = i - 1; j >= 0; j--) {
                const Token *t = &sa->tokens[j];
                /* Stop scanning back at sentence boundaries               */
                if (t->pos == POS_PUNCTUATION && t->is_sent_boundary) break;
                int tc = 0;
                if (t->pos == POS_NOUN && t->noun_class > 0) {
                    tc = t->noun_class;
                } else if (t->pos == POS_VERB_CONJ && t->noun_class > 0
                           && t->gram_role != GRAM_ROLE_VERBAL_NOUN) {
                    /* A preceding conjugated verb with the same SP class means
                     * this is a coordinate clause sharing the same implicit
                     * subject.  e.g. "bitegeke ... , bitandukanye ..." — both
                     * verbs are class 8; the subject (lights) is established
                     * by the earlier verb's SP, not by any intervening noun.  */
                    tc = t->noun_class;
                } else {
                    continue;
                }
                if (tc == vc) { has_remote_subject = true; break; }
                if ((tc == 4 || tc == 9) && (vc == 4 || vc == 9))
                    { has_remote_subject = true; break; }
                if ((tc == 1 || tc == 3) && (vc == 1 || vc == 3))
                    { has_remote_subject = true; break; }
                /* Nt.9 (singular) / Nt.10 (plural) are a paired class.
                 * imbuto (Nt.9) is a valid remote subject for a Nt.10 verb. */
                if ((tc == 9 || tc == 10) && (vc == 9 || vc == 10))
                    { has_remote_subject = true; break; }
                /* "ya" SP (stored as class 6) is also the Nt.1/3/9 past-tense
                 * form (a + past-TM-a → ya).  A remote Nt.1/3/9 noun or verb
                 * with the same implicit subject is a valid match for ya- verbs.
                 * e.g. "Imana irangiza imirimo yakoze" — imirimo is the object
                 * of the relative clause; yakoze's subject is Imana (Nt.9).    */
                if (vc == 6 && (tc == 1 || tc == 3 || tc == 9))
                    { has_remote_subject = true; break; }
            }
            if (has_remote_subject) continue;

            char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
            snprintf(msg, sizeof(msg),
                "Inshinga '%s' ntishyikira izina '%s': "
                "inteko y'inshinga=%d ariko inteko y'izina=%d. "
                "Verb '%s' subject prefix (class %d) doesn't agree "
                "with noun '%s' (class %d).",
                verb->surface, noun->surface, vc, nc,
                verb->surface, vc, noun->surface, nc);
            const NounClass *ncls = kin_get_noun_class(nc);
            snprintf(sug, sizeof(sug),
                "Indangasubizi igomba kuba '%s' (inteko %d). "
                "The subject prefix for class %d should be '%s'.",
                ncls ? ncls->subj_prefix : "?", nc,
                nc, ncls ? ncls->subj_prefix : "?");
            add_error(sa, ERR_SUBJ_VERB_AGREEMENT, i + 1, msg, sug);
        }
    }

    /* RULE 6: Vowel contact (iranya ry'impanvu) – Amategeko y'igenamajwi    *
     * No two vowels may appear adjacent in a Kinyarwanda word.             *
     * When VV contact occurs at a morpheme boundary one of these must apply:*
     *   u→w  (mweza ← mu+iza),  i→y (cyeza ← ki+iza),                    *
     *   a→Ø  elision (beza ← ba+iza),  a+i→e fusion (benja ← ba+inja).   *
     * We flag words that contain VV hiatus and are tagged as Kinyarwanda.  */
    for (int i = 0; i < sa->token_count; i++) {
        Token *t = &sa->tokens[i];
        /* Only check words we believe are Kinyarwanda and not interjections */
        if (t->pos == POS_PUNCTUATION) continue;
        if (!t->is_kinyarwanda) continue;
        if (t->pos == POS_INTERJECTION) continue;
        if (t->is_proper_noun) continue;
        if (!kin_has_vowel_hiatus(t->lower)) continue;

        /* Determine which corrective rule should apply based on the pair    */
        char pair[3] = {0};
        const char *w = t->lower;
        for (int j = 0; w[j] && w[j+1]; j++) {
            if (is_vowel_ch(w[j]) && is_vowel_ch(w[j+1])) {
                pair[0] = w[j]; pair[1] = w[j+1]; break;
            }
        }
        char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
        const char *rule = "a→Ø (elision)";
        if (pair[0] == 'u') rule = "u→w (glide: uX → wX)";
        else if (pair[0] == 'i') rule = "i→y (glide: iX → yX)";
        else if (pair[0] == 'a' && pair[1] == 'i') rule = "a+i→e (ubumwe)";
        snprintf(msg, sizeof(msg),
            "Iranya ry'impanvu: '%s' irimo impanvu ebyiri zisubiranya ('%c%c'). "
            "Vowel hiatus in '%s': vowels '%c' and '%c' are adjacent.",
            t->surface, pair[0], pair[1],
            t->surface, pair[0], pair[1]);
        snprintf(sug, sizeof(sug),
            "Amategeko y'igenamajwi agomba gukurikizwa: %s. "
            "Apply phonological rule: %s.",
            rule, rule);
        add_error(sa, ERR_VOWEL_HIATUS, i, msg, sug);
    }

    /* RULE 6b: Letter 'l' in native Kinyarwanda words
     * Official Orthography Rules §2.3: 'l' exists only in proper names
     * (Kigali, Repubulika, Leta) and foreign words.  Any 'l' in a recognised
     * Kinyarwanda word that is NOT a proper noun is an orthographic error.  */
    for (int i = 0; i < sa->token_count; i++) {
        Token *t = &sa->tokens[i];
        if (t->pos == POS_PUNCTUATION) continue;
        if (!t->is_kinyarwanda) continue;
        if (t->is_proper_noun) continue;
        if (t->pos == POS_FOREIGN) continue;
        if (!strchr(t->lower, 'l')) continue;
        char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
        snprintf(msg, sizeof(msg),
            "Inyuguti §2.3: '%s' irimo inyuguti 'l' itavugwa mu Kinyarwanda. "
            "Letter 'l' in '%s' is not a native Kinyarwanda letter.",
            t->surface, t->surface);
        snprintf(sug, sizeof(sug),
            "Suzuma niba ari ijambo ry'amahanga cyangwa izina bwite. "
            "Check if this is a foreign word or proper name.");
        add_error(sa, ERR_SPELLING, i, msg, sug);
    }

    /* RULE 7: kugenda vs kujya — directional-motion verb selection
     *
     * In standard Kinyarwanda there are two distinct "to go" verbs:
     *   kugenda  = manner/non-directional: "to walk / to travel / to move"
     *              Takes manner adverbs:  "aragenda buhoro" (walks slowly) ✓
     *              Takes path phrases:   "aragenda ku muhanda" (on the road) ✓
     *              Does NOT take bare destination nouns.
     *   kujya    = directional:          "to go TO (a destination)"
     *              Takes destination NP: "ajya ishuri"  (goes to school) ✓
     *                                    "azajya i Kigali" (will go to Kigali) ✓
     *
     * When a "gend"-stem verb is immediately followed by a bare destination
     * noun (no intervening preposition), flag the error and suggest kujya.
     *
     * Source: native-speaker correction (2026-03-21).
     */
    for (int i = 0; i < sa->token_count - 1; i++) {
        Token *verb = &sa->tokens[i];
        if (verb->pos == POS_PUNCTUATION) continue;
        /* Find next non-punctuation token */
        int ni = i + 1;
        while (ni < sa->token_count && sa->tokens[ni].pos == POS_PUNCTUATION) ni++;
        if (ni >= sa->token_count) continue;
        Token *next = &sa->tokens[ni];

        if (verb->pos != POS_VERB_CONJ) continue;
        /* Only flag when the verb stem is "gend" or "end" (both map to kugenda) */
        if (strcmp(verb->stem, "gend") != 0 && strcmp(verb->stem, "end") != 0)
            continue;
        /* Only flag when immediately followed by a bare noun (no preposition) */
        if (next->pos != POS_NOUN) continue;
        /* Skip if the noun is a proper noun (place names used with i/ku
         * preposition are fine; mid-sentence capitals are proper nouns) */
        if (next->is_proper_noun) continue;

        char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
        snprintf(msg, sizeof(msg),
            "Guhitamo inshinga nabi: '%s' (kugenda) ikurikirwa n'izina '%s' "
            "nta mugereka wo hagati. "
            "Wrong verb: 'kugenda' (manner of movement) followed directly by "
            "destination noun '%s' without a preposition. "
            "Use 'kujya' for going TO a destination.",
            verb->surface, next->surface, next->surface);
        snprintf(sug, sizeof(sug),
            "Jyana inshinga 'kugenda' n'inshinga 'kujya' iyo ushaka kuvuga "
            "kujya ahantu. Urugero: 'ajya ishuri' (not 'aragenda ishuri'). "
            "Replace 'kugenda' with 'kujya' when going TO a place. "
            "E.g. 'ajya ishuri' / 'yajya ishuri' / 'azajya ishuri'.");
        add_error(sa, ERR_VERB_SELECTION, i, msg, sug);
    }

    /* RULE 8: Conditional tense marking (Inziganyo)                           *
     *                                                                           *
     * When a conditional conjunction ("niba", "nibyo") precedes a conjugated   *
     * verb, that verb is inside a conditional clause.  We upgrade its tense    *
     * to TENSE_CONDITIONAL so the output names the clause type correctly.      *
     *                                                                           *
     * The verb is not necessarily the immediately next token — a subject noun  *
     * may intervene ("niba umwana aragenda"). We look up to 3 tokens ahead.    *
     *                                                                           *
     * "iyo" as conditional connector (sentence-initial): handled when it       *
     * appears as the first token or immediately after a conjunction.  In all   *
     * other positions it is a demonstrative/relative pronoun and is left alone.*/
    static const char *COND_MARKERS[] = { "niba", "nibyo", NULL };

    for (int i = 0; i < sa->token_count; i++) {
        Token *t = &sa->tokens[i];
        bool is_cond = false;

        /* Lexically-marked conditional conjunctions */
        if (t->pos == POS_CONJUNCTION) {
            for (int k = 0; COND_MARKERS[k]; k++) {
                if (strcmp(t->lower, COND_MARKERS[k]) == 0) { is_cond = true; break; }
            }
        }

        /* "iyo" as conditional: only sentence-initial or after a conjunction   *
         * (e.g. "...ariko iyo...").  Elsewhere it is a demonstrative pronoun.  */
        if (!is_cond && strcmp(t->lower, "iyo") == 0) {
            if (i == 0)                                          is_cond = true;
            else if (sa->tokens[i-1].pos == POS_CONJUNCTION)    is_cond = true;
        }

        if (!is_cond) continue;

        /* Find the first conjugated verb within the next 3 tokens and mark it */
        for (int j = i + 1; j < sa->token_count && j <= i + 3; j++) {
            if (sa->tokens[j].pos == POS_VERB_CONJ) {
                sa->tokens[j].verb_tense = TENSE_CONDITIONAL;
                break;
            }
        }
    }

    sa->is_complete = sa->has_verb && (sa->error_count == 0);
}
