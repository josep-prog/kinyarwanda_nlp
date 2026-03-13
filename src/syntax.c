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

    sa->is_complete = sa->has_verb && (sa->error_count == 0);
}
