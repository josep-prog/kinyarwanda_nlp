/*
 * analysis.c
 * Top-level pipeline: tokenize → tag → check syntax → suggest corrections.
 * Also contains the pretty-print output function.
 */

#include <stdio.h>
#include <string.h>
#include "../include/kinyarwanda.h"

/*
 * kin_resolve_sp_ambiguity() – post-processing pass to fix ambiguous SP classes.
 *
 * In Kinyarwanda the "ya" subject prefix is shared by two grammatical contexts:
 *
 *   1. Nt.6 PRESENT habitual:  ya + stem + a     (yamara, yagenda)
 *   2. Nt.1/3 PAST:            a(SP) + a(past) → ya + stem + ye/tse/aga
 *                               (yagiye, yaremye, yagendaga, yabonye)
 *
 * The verb morphology tagger has no sentence context, so it always assigns
 * class 6 when it sees "ya" as SP.  After syntax checking we know whether
 * the subject noun is Nt.1/3 (human/tree class) or Nt.6 (mass/plural class).
 * This pass corrects the verb's stored noun_class to match the actual subject
 * so the output display and any downstream processing shows the right class.
 *
 * Similarly "i" SP is stored as class 4 but class 9 also uses "i"; we resolve
 * that in context too.
 *
 * Only verbs whose class changed from "ya→Nt.1" or "i→Nt.9" and whose
 * IMMEDIATE predecessor is a noun of the matching class are updated.
 * Verbs with already-correct class, or without a directly preceding noun,
 * are left unchanged.
 */
static void kin_resolve_sp_ambiguity(SentenceAnalysis *sa) {
    for (int i = 1; i < sa->token_count; i++) {
        Token *noun = &sa->tokens[i - 1];
        Token *verb = &sa->tokens[i];

        if (verb->pos != POS_VERB_CONJ) continue;
        if (noun->pos != POS_NOUN)      continue;
        if (noun->noun_class == 0)      continue;

        int nc = noun->noun_class;
        int vc = verb->noun_class;

        /* "ya" SP (stored cls 6) + Nt.1/3/9 subject + PAST tense → reclassify.
         * In Kinyarwanda the "ya" SP is shared by:
         *   Nt.1 past:  umuntu yagiye  (a+past-a → ya)
         *   Nt.3 past:  umuti waguye    (... but also uses 'wa', less common)
         *   Nt.9 past:  Imana yaremye   (inka ya- in past)
         *   Nt.6 pres:  amazu yagenda   (genuine Nt.6 present)
         * We reclassify "ya" to match the subject class when tense is past. */
        if (vc == 6 && (nc == 1 || nc == 3 || nc == 9) &&
            (verb->verb_tense == TENSE_PAST_PERF ||
             verb->verb_tense == TENSE_PAST_IMPF)) {
            verb->noun_class = nc;
        }

        /* "i" SP (stored cls 4) + Nt.9 subject → reclassify to Nt.9 */
        if (vc == 4 && nc == 9) {
            verb->noun_class = 9;    /* e.g. Imana iravuga → Nt.9 (not Nt.4) */
        }
    }
}

SentenceAnalysis kin_analyze(const char *text) {
    SentenceAnalysis sa;
    memset(&sa, 0, sizeof(sa));
    sa.token_count = kin_tokenize(text, sa.tokens, KIN_MAX_TOKENS);
    kin_tag_sentence(&sa);
    kin_check_syntax(&sa);
    kin_suggest_corrections(&sa);
    kin_resolve_sp_ambiguity(&sa);   /* resolve ya/i SP class from context    */
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

        /* Verb details (always shown for conjugated verbs) */
        if (t->pos == POS_VERB_CONJ || t->pos == POS_VERB_INF) {
            if (t->is_negative)
                printf("  └─ INSHINGA Y'UBUNYAGATIFU (Negative verb)\n");
            if (t->verb_tense != TENSE_NONE)
                printf("  └─ %s\n", kin_verb_tense_name(t->verb_tense));
            if (t->obj_class > 0)
                printf("  └─ Indangakinyazina y'inshinga (OM) → Nt.%d: %s\n",
                       t->obj_class, kin_class_name(t->obj_class));
            if (t->verb_ext != VEXT_NONE)
                printf("  └─ %s\n", kin_verb_ext_name(t->verb_ext));
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
