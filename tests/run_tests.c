/*
 * run_tests.c — test entry point
 *
 * Runs all test suites, prints a per-suite summary, and exits with
 * status 0 (all passed) or 1 (any failure).
 */
#include <stdio.h>
#include "test_framework.h"

void run_morphology_tests(TestResult *r);
void run_lexicon_tests(TestResult *r);
void run_ortho_tests(TestResult *r);
void run_pipeline_tests(TestResult *r);

int main(void) {
    int any_fail = 0;
    TestResult r;

    printf("Kinyarwanda NLP — test suite\n");
    printf("─────────────────────────────────────────────────────────────────\n");

    r = (TestResult){0, 0};
    run_morphology_tests(&r);
    any_fail |= suite_report(&r, "morphology");

    r = (TestResult){0, 0};
    run_lexicon_tests(&r);
    any_fail |= suite_report(&r, "lexicon");

    r = (TestResult){0, 0};
    run_ortho_tests(&r);
    any_fail |= suite_report(&r, "ortho");

    r = (TestResult){0, 0};
    run_pipeline_tests(&r);
    any_fail |= suite_report(&r, "pipeline");

    printf("─────────────────────────────────────────────────────────────────\n");
    if (any_fail)
        printf("RESULT: FAIL — see lines above marked FAIL\n");
    else
        printf("RESULT: PASS\n");

    return any_fail ? 1 : 0;
}
