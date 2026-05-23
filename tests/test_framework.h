/*
 * test_framework.h — minimal C99 test framework (no external dependencies)
 *
 * Usage: each test file defines run_X_tests(TestResult *r).
 * Macros write to *r; run_tests.c aggregates and reports totals.
 */
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

typedef struct { int pass; int fail; } TestResult;

/* Assert a boolean condition */
#define ASSERT(r, cond, label) do {                                     \
    if (cond) {                                                         \
        (r)->pass++;                                                    \
    } else {                                                            \
        fprintf(stderr, "  FAIL  %-55s  [%s:%d]\n",                   \
                (label), __FILE__, __LINE__);                           \
        (r)->fail++;                                                    \
    }                                                                   \
} while (0)

/* Assert two ints are equal */
#define ASSERT_INT(r, got, want, label) do {                            \
    int _g = (int)(got), _w = (int)(want);                             \
    if (_g == _w) {                                                     \
        (r)->pass++;                                                    \
    } else {                                                            \
        fprintf(stderr, "  FAIL  %-55s  got=%d want=%d  [%s:%d]\n",  \
                (label), _g, _w, __FILE__, __LINE__);                  \
        (r)->fail++;                                                    \
    }                                                                   \
} while (0)

/* Assert two strings are equal */
#define ASSERT_STR(r, got, want, label) do {                            \
    const char *_g = (got), *_w = (want);                              \
    if (_g && _w && strcmp(_g, _w) == 0) {                            \
        (r)->pass++;                                                    \
    } else {                                                            \
        fprintf(stderr, "  FAIL  %-55s  got=\"%s\" want=\"%s\"  [%s:%d]\n", \
                (label), _g ? _g : "(null)", _w ? _w : "(null)",      \
                __FILE__, __LINE__);                                    \
        (r)->fail++;                                                    \
    }                                                                   \
} while (0)

/* Print per-suite summary, return 0 if all passed */
static inline int suite_report(TestResult *r, const char *name) {
    printf("  [%-18s]  %3d passed  %3d failed\n",
           name, r->pass, r->fail);
    return r->fail > 0 ? 1 : 0;
}

#endif /* TEST_FRAMEWORK_H */
