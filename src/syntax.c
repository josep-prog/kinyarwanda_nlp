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
    /* RULE 3: Must have a verb */
    if (!sa->has_verb && sa->token_count > 0) {
        add_error(sa, ERR_NO_VERB, 0,
            "Iri nteruro ntirigira inshinga (umuvugo) / "
            "This sentence has no verb.",
            "Ongeraho inshinga (Add a verb).");
    }

    /* RULE 1 & 2: Agreement between adjacent noun → adjective / possessive */
    for (int i = 0; i < sa->token_count - 1; i++) {
        Token *cur  = &sa->tokens[i];
        Token *next = &sa->tokens[i + 1];

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
            if (cur->noun_class != next->noun_class) {
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
        Token *verb = &sa->tokens[i + 1];

        if (noun->pos != POS_NOUN)      continue;
        if (verb->pos != POS_VERB_CONJ) continue;
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
        /* "ya" SP (stored as cls 6) is ambiguous: it is ALSO the Nt.1 past
         * tense form (a-subject + past 'a' marker → "ya").  Do not flag   *
         * agreement errors when verb SP class is 6 and noun is Nt.1/3/9.  *
         * e.g.  "Imana yaremye ijuru" – Nt.9 noun + ya (Nt.1-past) verb.  */
        if (vc == 6 && (nc == 1 || nc == 3 || nc == 9)) continue;

        if (nc != vc) {
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
        Token *next = &sa->tokens[i + 1];

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

    sa->is_complete = sa->has_verb && (sa->error_count == 0);
}
