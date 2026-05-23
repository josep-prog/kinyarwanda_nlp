/*
 * test_pipeline.c
 * End-to-end tests for kin_analyze(): tokenization, POS tagging,
 * morpheme assignment, syntax agreement, and error detection.
 *
 * Each test calls kin_analyze() on a known Kinyarwanda sentence and
 * asserts specific properties of the resulting SentenceAnalysis.
 */
#include <string.h>
#include "../include/kinyarwanda.h"
#include "test_framework.h"

/* ── Helpers ──────────────────────────────────────────────────────────────── */

/* Return the POS of the first token whose .lower matches word, or -1 */
static int tok_pos(const SentenceAnalysis *sa, const char *word) {
    for (int i = 0; i < sa->token_count; i++)
        if (strcmp(sa->tokens[i].lower, word) == 0)
            return (int)sa->tokens[i].pos;
    return -1;
}

/* Return the noun_class of the first matching token, or -1 */
static int tok_class(const SentenceAnalysis *sa, const char *word) {
    for (int i = 0; i < sa->token_count; i++)
        if (strcmp(sa->tokens[i].lower, word) == 0)
            return sa->tokens[i].noun_class;
    return -1;
}

/* Return the verb_tense of the first matching token, or TENSE_NONE */
static int tok_tense(const SentenceAnalysis *sa, const char *word) {
    for (int i = 0; i < sa->token_count; i++)
        if (strcmp(sa->tokens[i].lower, word) == 0)
            return (int)sa->tokens[i].verb_tense;
    return (int)TENSE_NONE;
}

/* Count errors of a specific type */
static int count_err(const SentenceAnalysis *sa, ErrorType type) {
    int n = 0;
    for (int i = 0; i < sa->error_count; i++)
        if (sa->errors[i].type == type) n++;
    return n;
}

/* ── POS tagging ──────────────────────────────────────────────────────────── */
static void test_pos_tags(TestResult *r) {
    SentenceAnalysis sa;

    /* Canonical sentence: noun + conjugated verb */
    sa = kin_analyze("Umuntu aragenda.");
    ASSERT_INT(r, tok_pos(&sa, "umuntu"),   POS_NOUN,      "umuntu → POS_NOUN");
    ASSERT_INT(r, tok_pos(&sa, "aragenda"), POS_VERB_CONJ, "aragenda → POS_VERB_CONJ");

    /* Infinitive in subject position */
    sa = kin_analyze("Gusoma ni byiza.");
    ASSERT_INT(r, tok_pos(&sa, "gusoma"), POS_VERB_INF, "gusoma → POS_VERB_INF");

    /* Conjunction */
    sa = kin_analyze("Imana yaremye ijuru n'isi.");
    ASSERT_INT(r, tok_pos(&sa, "yaremye"), POS_VERB_CONJ, "yaremye → POS_VERB_CONJ");
    ASSERT_INT(r, tok_pos(&sa, "ijuru"),   POS_NOUN,      "ijuru → POS_NOUN");

    /* Adjective agrees with noun */
    sa = kin_analyze("Umuntu munini aragenda.");
    ASSERT_INT(r, tok_pos(&sa, "munini"), POS_ADJECTIVE, "munini → POS_ADJECTIVE");

    /* Adverb */
    sa = kin_analyze("Aragenda neza.");
    ASSERT_INT(r, tok_pos(&sa, "neza"), POS_ADVERB, "neza → POS_ADVERB");
    ASSERT_INT(r, tok_pos(&sa, "cyane"), -1, "cyane not in this sentence");

    sa = kin_analyze("Bakora cyane.");
    ASSERT_INT(r, tok_pos(&sa, "cyane"), POS_ADVERB, "cyane → POS_ADVERB");

    /* Conjunction */
    sa = kin_analyze("Arakora ariko ntareba.");
    ASSERT_INT(r, tok_pos(&sa, "ariko"), POS_CONJUNCTION, "ariko → POS_CONJUNCTION");

    /* Locative */
    sa = kin_analyze("Aragenda hano.");
    ASSERT_INT(r, tok_pos(&sa, "hano"), POS_LOCATIVE, "hano → POS_LOCATIVE");
}

/* ── Noun class assignment ────────────────────────────────────────────────── */
static void test_noun_classes(TestResult *r) {
    SentenceAnalysis sa;

    sa = kin_analyze("Umuntu aragenda.");
    ASSERT_INT(r, tok_class(&sa, "umuntu"), 1, "umuntu → Nt.1");

    sa = kin_analyze("Abantu baragenda.");
    ASSERT_INT(r, tok_class(&sa, "abantu"), 2, "abantu → Nt.2");

    sa = kin_analyze("Ikigo kinini kirabaho.");
    ASSERT_INT(r, tok_class(&sa, "ikigo"),  7, "ikigo → Nt.7");

    sa = kin_analyze("Ibigo birabaho.");
    ASSERT_INT(r, tok_class(&sa, "ibigo"),  8, "ibigo → Nt.8");

    sa = kin_analyze("Urugo ruratwaye.");
    ASSERT_INT(r, tok_class(&sa, "urugo"), 11, "urugo → Nt.11");

    sa = kin_analyze("Amazi arangwa.");
    ASSERT_INT(r, tok_class(&sa, "amazi"),  6, "amazi → Nt.6");
}

/* ── Verb tense detection ────────────────────────────────────────────────── */
static void test_verb_tenses(TestResult *r) {
    SentenceAnalysis sa;

    /* Present immediate (ara-) */
    sa = kin_analyze("Aragenda buhoro.");
    ASSERT_INT(r, tok_tense(&sa, "aragenda"), TENSE_PRESENT, "aragenda → TENSE_PRESENT");

    /* Past perfect (-ye) */
    sa = kin_analyze("Imana yaremye ijuru.");
    ASSERT_INT(r, tok_tense(&sa, "yaremye"), TENSE_PAST_PERF, "yaremye → TENSE_PAST_PERF");

    /* Past imperfect (-aga) */
    sa = kin_analyze("Yagendaga buri munsi.");
    ASSERT_INT(r, tok_tense(&sa, "yagendaga"), TENSE_PAST_IMPF, "yagendaga → TENSE_PAST_IMPF");

    /* Future (za-) */
    sa = kin_analyze("Azagenda vuba cyane.");
    ASSERT_INT(r, tok_tense(&sa, "azagenda"), TENSE_FUTURE, "azagenda → TENSE_FUTURE");

    /* Subjunctive (-e) */
    sa = kin_analyze("Bamusaba agende.");
    ASSERT_INT(r, tok_tense(&sa, "agende"), TENSE_SUBJUNCTIVE, "agende → TENSE_SUBJUNCTIVE");

    /* Narrative sequential (ka-) */
    sa = kin_analyze("Akagenda agasoma.");
    ASSERT_INT(r, tok_tense(&sa, "akagenda"), TENSE_NARRATIVE, "akagenda → TENSE_NARRATIVE");
}

/* ── Sentence completeness ────────────────────────────────────────────────── */
static void test_has_verb(TestResult *r) {
    SentenceAnalysis sa;

    sa = kin_analyze("Umuntu aragenda.");
    ASSERT(r,  sa.has_verb, "Umuntu aragenda → has_verb");

    sa = kin_analyze("Imana yaremye ijuru n'isi.");
    ASSERT(r,  sa.has_verb, "Imana yaremye → has_verb");

    sa = kin_analyze("Gukora ni byiza.");
    ASSERT(r,  sa.has_verb, "Gukora ni byiza → has_verb");

    sa = kin_analyze("Azagenda vuba.");
    ASSERT(r,  sa.has_verb, "Azagenda vuba → has_verb");

    /* Nominal predicate: noun + adj — copula implied, no ERR_NO_VERB */
    sa = kin_analyze("Umuntu mwiza.");
    ASSERT(r, count_err(&sa, ERR_NO_VERB) == 0,
           "Umuntu mwiza (nominal pred) → no ERR_NO_VERB");
}

/* ── Adjective agreement errors ──────────────────────────────────────────── */
static void test_adj_agreement(TestResult *r) {
    SentenceAnalysis sa;

    /* Correct: Nt.1 + mu-nini */
    sa = kin_analyze("Umuntu munini aragenda.");
    ASSERT_INT(r, count_err(&sa, ERR_ADJ_AGREEMENT), 0,
               "umuntu munini: correct Nt.1 agreement, no error");

    /* Correct: Nt.2 + ba-nini */
    sa = kin_analyze("Abantu banini baragenda.");
    ASSERT_INT(r, count_err(&sa, ERR_ADJ_AGREEMENT), 0,
               "abantu banini: correct Nt.2 agreement, no error");

    /* Correct: Nt.7 + ki-nini */
    sa = kin_analyze("Ikigo kinini kirabaho.");
    ASSERT_INT(r, count_err(&sa, ERR_ADJ_AGREEMENT), 0,
               "ikigo kinini: correct Nt.7 agreement, no error");

    /* Correct: Nt.8 + bi-nini */
    sa = kin_analyze("Ibigo binini birabaho.");
    ASSERT_INT(r, count_err(&sa, ERR_ADJ_AGREEMENT), 0,
               "ibigo binini: correct Nt.8 agreement, no error");

    /* Error: Nt.2 noun + Nt.1 adjective prefix (mu- instead of ba-) */
    sa = kin_analyze("Abantu munini baragenda.");
    ASSERT(r, count_err(&sa, ERR_ADJ_AGREEMENT) >= 1,
           "abantu munini: Nt.2 noun + mu-nini → ERR_ADJ_AGREEMENT");

    /* Error: Nt.7 noun + Nt.8 adjective prefix (bi- instead of ki-) */
    sa = kin_analyze("Ikigo binini kirabaho.");
    ASSERT(r, count_err(&sa, ERR_ADJ_AGREEMENT) >= 1,
           "ikigo binini: Nt.7 noun + bi-nini → ERR_ADJ_AGREEMENT");

    /* Error: Nt.8 noun + Nt.1 adjective prefix (mu- instead of bi-) */
    sa = kin_analyze("Ibigo munini birabaho.");
    ASSERT(r, count_err(&sa, ERR_ADJ_AGREEMENT) >= 1,
           "ibigo munini: Nt.8 noun + mu-nini → ERR_ADJ_AGREEMENT");
}

/* ── Possessive connector agreement errors ───────────────────────────────── */
static void test_poss_agreement(TestResult *r) {
    SentenceAnalysis sa;

    /* Correct: Nt.11 + rwa-cu */
    sa = kin_analyze("Urugo rwacu rugenda.");
    ASSERT_INT(r, count_err(&sa, ERR_POSS_AGREEMENT), 0,
               "urugo rwacu: correct Nt.11 possessive, no error");

    /* Correct: Nt.1 + wa-cu */
    sa = kin_analyze("Umuntu wacu aragenda.");
    ASSERT_INT(r, count_err(&sa, ERR_POSS_AGREEMENT), 0,
               "umuntu wacu: correct Nt.1 possessive, no error");

    /* Correct: Nt.7 + cya-cu */
    sa = kin_analyze("Ikigo cyacu kirabaho.");
    ASSERT_INT(r, count_err(&sa, ERR_POSS_AGREEMENT), 0,
               "ikigo cyacu: correct Nt.7 possessive, no error");

    /* Error: Nt.11 + Nt.1 connector (wa- instead of rwa-) */
    sa = kin_analyze("Urugo wacu rugenda.");
    ASSERT(r, count_err(&sa, ERR_POSS_AGREEMENT) >= 1,
           "urugo wacu: Nt.11 noun + wa-cu → ERR_POSS_AGREEMENT");

    /* Error: Nt.7 + Nt.1 connector (wa- instead of cya-) */
    sa = kin_analyze("Ikigo wacu kirabaho.");
    ASSERT(r, count_err(&sa, ERR_POSS_AGREEMENT) >= 1,
           "ikigo wacu: Nt.7 noun + wa-cu → ERR_POSS_AGREEMENT");
}

/* ── Token count sanity ───────────────────────────────────────────────────── */
static void test_tokenization(TestResult *r) {
    SentenceAnalysis sa;

    /* Single word */
    sa = kin_analyze("Aragenda");
    ASSERT(r, sa.token_count >= 1, "aragenda: at least 1 token");

    /* Two content words + period */
    sa = kin_analyze("Umuntu aragenda.");
    ASSERT(r, sa.token_count >= 2, "Umuntu aragenda.: at least 2 tokens");

    /* Longer sentence */
    sa = kin_analyze("Imana yaremye ijuru n'isi.");
    ASSERT(r, sa.token_count >= 4, "Imana yaremye ijuru n'isi: at least 4 tokens");
}

/* ── Regression: specific sentences from the existing demo suite ─────────── */
static void test_regressions(TestResult *r) {
    SentenceAnalysis sa;

    /* Reduplicated adjective: muremure */
    sa = kin_analyze("Umuntu muremure aragenda.");
    ASSERT(r, sa.has_verb, "muremure sentence has verb");
    ASSERT_INT(r, count_err(&sa, ERR_ADJ_AGREEMENT), 0,
               "umuntu muremure: correct reduplicated adj, no error");

    /* Greeting recognized (should not produce ERR_NO_VERB) */
    sa = kin_analyze("Murakoze cyane.");
    ASSERT(r, count_err(&sa, ERR_NO_VERB) == 0, "Murakoze cyane: no ERR_NO_VERB");

    /* Personal pronoun 'we' tagged correctly */
    sa = kin_analyze("We uragenda.");
    ASSERT_INT(r, tok_pos(&sa, "we"), POS_PRONOUN, "we → POS_PRONOUN");

    /* Stative 'afite' */
    sa = kin_analyze("Afite inzu nziza.");
    ASSERT(r, sa.has_verb, "Afite inzu nziza → has_verb");

    /* 1sg verb form */
    sa = kin_analyze("Ndabizi neza.");
    ASSERT(r, sa.has_verb, "Ndabizi neza → has_verb");

    /* Sentence-internal conjunction na */
    sa = kin_analyze("Aragenda na we.");
    ASSERT_INT(r, tok_pos(&sa, "na"), POS_CONJUNCTION, "na → POS_CONJUNCTION");
}

/* ── Entry point ──────────────────────────────────────────────────────────── */
void run_pipeline_tests(TestResult *r) {
    test_pos_tags(r);
    test_noun_classes(r);
    test_verb_tenses(r);
    test_has_verb(r);
    test_adj_agreement(r);
    test_poss_agreement(r);
    test_tokenization(r);
    test_regressions(r);
}
