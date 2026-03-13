/*
 * analysis.c
 * Top-level pipeline: tokenize → tag → check syntax → suggest corrections.
 * Also contains the pretty-print output function.
 */

#include <stdio.h>
#include <string.h>
#include "../include/kinyarwanda.h"

SentenceAnalysis kin_analyze(const char *text) {
    SentenceAnalysis sa;
    memset(&sa, 0, sizeof(sa));
    sa.token_count = kin_tokenize(text, sa.tokens, KIN_MAX_TOKENS);
    kin_tag_sentence(&sa);
    kin_check_syntax(&sa);
    kin_suggest_corrections(&sa);
    return sa;
}

/* ── Pretty-print helpers ──────────────────────────────────────────────────*/
static void print_separator(char c, int width) {
    for (int i = 0; i < width; i++) putchar(c);
    putchar('\n');
}

void kin_print_analysis(const SentenceAnalysis *sa, bool verbose) {
    print_separator('=', 72);
    printf("ISESENGURA RY'URURIMI / LANGUAGE ANALYSIS\n");
    print_separator('=', 72);

    printf("\nAmagambo / Tokens:  %d\n", sa->token_count);
    printf("Inshinga iboneka / Has verb: %s\n",
           sa->has_verb ? "Yego (Yes)" : "Oya (No)");
    printf("Makosa / Errors:    %d\n\n", sa->error_count);

    /* Token table */
    printf("%-20s  %-30s  %-8s  %-8s\n",
           "Ijambo/Word", "Ubwoko/Type", "Inteko/Class", "Igicumbi/Stem");
    print_separator('-', 72);

    for (int i = 0; i < sa->token_count; i++) {
        const Token *t = &sa->tokens[i];
        char class_str[16] = "";
        if (t->noun_class > 0)
            snprintf(class_str, sizeof(class_str), "Nt.%d", t->noun_class);

        /* Mark tokens with errors */
        char marker = (t->error_count > 0) ? '!' : ' ';
        printf("%c%-19s  %-30s  %-8s  %-8s\n",
               marker,
               t->surface,
               kin_pos_name(t->pos),
               class_str,
               t->stem);

        /* Verb tense (always shown for conjugated verbs) */
        if (t->pos == POS_VERB_CONJ && t->verb_tense != TENSE_NONE) {
            printf("  └─ %s\n", kin_verb_tense_name(t->verb_tense));
        }
        /* Extra pronoun info in verbose mode */
        if (verbose && t->pos == POS_PRONOUN && t->pron_type != PRON_NONE) {
            printf("  └─ %s\n", kin_pron_type_name(t->pron_type));
        }
    }

    /* Errors section */
    if (sa->error_count > 0) {
        print_separator('-', 72);
        printf("\nMAKOSA / ERRORS DETECTED:\n\n");
        for (int i = 0; i < sa->error_count; i++) {
            const Error *e = &sa->errors[i];
            printf("[%d] %s\n", i + 1, e->message);
            if (e->suggestion[0])
                printf("    → %s\n", e->suggestion);
            putchar('\n');
        }
    } else {
        printf("\nNta makosa aboneka / No errors detected.\n");
    }

    print_separator('=', 72);
}
