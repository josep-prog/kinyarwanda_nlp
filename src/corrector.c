/*
 * corrector.c
 * Error correction suggestions for Kinyarwanda text.
 *
 * For each error detected by syntax.c, this module:
 *  - Explains the rule that was broken (in both Kinyarwanda and English)
 *  - Offers a corrected form of the offending token where possible
 *  - Applies the agreement rules from the book (p.61-68)
 */

#include <string.h>
#include <stdio.h>
#include "../include/kinyarwanda.h"

/*
 * Build the correct adjective form by prepending the right concordance
 * prefix (indangasano) for the given noun class onto the stem.
 *
 * The phonological rules from book p.67 apply:
 *   u → w before vowel-initial stems (class 1/3: mu + iza → mwiza)
 *   i → y before vowel-initial stems (class 4: mi + iza → myiza)
 *   k → g before voiced C (class 7/15: ki+zima → gizima? no – ki stays)
 *
 * Note: The RS prefix is the class marker (indanganteko), not D+RT.
 */
static void build_adj(int noun_class, const char *stem, char *out, size_t outsz) {
    static const char *RS[] = {
        "", "mu","ba","mu","mi","ri","ma","ki","bi",
        "n","zi","ru","ka","tu","bu","ku","ha"
    };
    if (noun_class < 1 || noun_class > 16) {
        strncpy(out, stem, outsz - 1); return;
    }
    const char *pfx = RS[noun_class];
    char vowels[] = "aeiou";
    bool stem_vowel = (stem[0] && strchr(vowels, stem[0]) != NULL);

    /* Apply phonological changes (igenamajwi) before vowel-initial stems */
    if (stem_vowel) {
        switch (noun_class) {
            case 1: case 3:  snprintf(out, outsz, "mw%s",  stem); return;
            case 4:          snprintf(out, outsz, "my%s",  stem); return;
            case 7:          snprintf(out, outsz, "cy%s",  stem); return;
            case 8:          snprintf(out, outsz, "by%s",  stem); return;
            case 11:         snprintf(out, outsz, "rw%s",  stem); return;
            case 13:         snprintf(out, outsz, "tw%s",  stem); return;
            case 14:         snprintf(out, outsz, "bw%s",  stem); return;
            case 15:         snprintf(out, outsz, "kw%s",  stem); return;
            default: break;
        }
    }
    snprintf(out, outsz, "%s%s", pfx, stem);
}

void kin_suggest_corrections(SentenceAnalysis *sa) {
    for (int e = 0; e < sa->error_count; e++) {
        Error *err = &sa->errors[e];

        if (err->type == ERR_ADJ_AGREEMENT) {
            Token *adj_tok = &sa->tokens[err->token_index];
            /* Find the preceding noun to know its class */
            if (err->token_index > 0) {
                Token *noun_tok = &sa->tokens[err->token_index - 1];
                if (noun_tok->pos == POS_NOUN && noun_tok->noun_class > 0 &&
                    adj_tok->stem[0]) {
                    char corrected[KIN_MAX_WORD];
                    build_adj(noun_tok->noun_class, adj_tok->stem,
                              corrected, sizeof(corrected));
                    /* Store the literal replacement so kin_correct() can apply it */
                    strncpy(err->corrected_word, corrected, KIN_MAX_WORD - 1);
                    char sug[KIN_MAX_MSG * 2];
                    snprintf(sug, sizeof(sug),
                        "Hindura '%s' ugakoresheje '%s' kugira ngo ishyikire "
                        "inteko %d (%s). / "
                        "Replace '%s' with '%s' to agree with class %d (%s).",
                        adj_tok->surface, corrected,
                        noun_tok->noun_class,
                        kin_class_name(noun_tok->noun_class),
                        adj_tok->surface, corrected,
                        noun_tok->noun_class,
                        kin_class_name(noun_tok->noun_class));
                    strncpy(err->suggestion, sug, KIN_MAX_MSG - 1);
                }
            }
        }

        if (err->type == ERR_POSS_AGREEMENT) {
            Token *poss_tok = &sa->tokens[err->token_index];
            if (err->token_index > 0) {
                Token *noun_tok = &sa->tokens[err->token_index - 1];
                if (noun_tok->pos == POS_NOUN && noun_tok->noun_class > 0) {
                    const NounClass *nc = kin_get_noun_class(noun_tok->noun_class);
                    char sug[KIN_MAX_MSG];
                    if (nc) {
                        snprintf(sug, sizeof(sug),
                            "Hindura '%s' ugakoresheje '%s...' (ikinyazina ngenera "
                            "cy'inteko %d). / "
                            "Replace '%s' with '%s...' (possessive for class %d).",
                            poss_tok->surface, nc->concordance_poss,
                            noun_tok->noun_class,
                            poss_tok->surface, nc->concordance_poss,
                            noun_tok->noun_class);
                        strncpy(err->suggestion, sug, KIN_MAX_MSG - 1);
                    }
                }
            }
        }
    }
}
