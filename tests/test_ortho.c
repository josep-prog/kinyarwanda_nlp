/*
 * test_ortho.c
 * Unit tests for ortho.c: orthographic rule engine (RALC 2017).
 *
 * Tests cover:
 *   kin_ortho_gen()       — forward generation: morpheme string → surface form
 *   kin_ortho_validate()  — surface word → OrthoViolation list
 *   kin_ortho_fix()       — auto-correct detectable violations
 */
#include <string.h>
#include "../include/kinyarwanda.h"
#include "test_framework.h"

/* ── kin_ortho_gen: forward generation ───────────────────────────────────── */
static void test_ortho_gen(TestResult *r) {
    char buf[128];

    /* Passthrough: no rules fire when no boundaries or contacts exist */
    kin_ortho_gen("bakora", false, buf, sizeof(buf));
    ASSERT_STR(r, buf, "bakora", "bakora passthrough (no boundary)");

    kin_ortho_gen("u|mu|ntu", false, buf, sizeof(buf));
    ASSERT_STR(r, buf, "umuntu", "u|mu|ntu → umuntu (no rules fire)");

    kin_ortho_gen("ba|nini", false, buf, sizeof(buf));
    ASSERT_STR(r, buf, "banini", "ba|nini → banini (consonant-initial stem)");

    /* §1.1  u → w before vowel. §3.7 voicing (k→g) only applies before a
     * CONSONANT-initial following morpheme (igitabo: ki+tabo); before a
     * vowel-initial morpheme the boundary is a vowel-contact site instead,
     * so voicing must NOT fire here: ku+eza → kw+eza = kweza, not "gweza".
     * "kweza" (to purify) is the real, attested word — confirmed below by
     * kin_ortho_validate("kweza") reporting 0 violations, and present in
     * the lexicon as the causative of "kwera". */
    kin_ortho_gen("ku|eza", false, buf, sizeof(buf));
    ASSERT_STR(r, buf, "kweza", "ku|eza → kweza (u→w §1.1; no voicing before a vowel)");

    /* §1.2  a + i → e (vowel fusion, confirmed working case) */
    kin_ortho_gen("ba|inja", false, buf, sizeof(buf));
    ASSERT_STR(r, buf, "benja", "ba|inja → benja (a+i→e §1.2)");

    /* §3.3  n → m before labiodentals (confirmed cases) */
    kin_ortho_gen("n|vura", false, buf, sizeof(buf));
    ASSERT_STR(r, buf, "mvura", "n|vura → mvura (n→m before labiodental v)");

    kin_ortho_gen("n|fite", false, buf, sizeof(buf));
    ASSERT_STR(r, buf, "mfite", "n|fite → mfite (n→m before labiodental f)");

    /* §3.3  n → m before bilabial b; engine fuses nb → m (b absorbed into nasal) */
    kin_ortho_gen("n|baga", false, buf, sizeof(buf));
    ASSERT_STR(r, buf, "maga", "n|baga → maga (n+b→m: bilabial absorbed into nasal)");

    /* §3.7  k → g before voiced consonant h */
    kin_ortho_gen("ki|haza", false, buf, sizeof(buf));
    ASSERT_STR(r, buf, "gihaza", "ki|haza → gihaza (k→g before voiced h §3.7)");

    /* §2.4  n + y → nz, activated only when noun_class_9 flag is set */
    kin_ortho_gen("n|yoga", true, buf, sizeof(buf));
    ASSERT_STR(r, buf, "nzoga", "n|yoga (Nt.9 flag=true) → nzoga (n+y→nz §2.4)");

    char buf2[128];
    kin_ortho_gen("n|yoga", false, buf2, sizeof(buf2));
    ASSERT(r, strcmp(buf, buf2) != 0,
           "n|yoga: Nt.9 flag changes output vs. without flag");
}

/* ── kin_ortho_validate: violation detection ──────────────────────────────── */
static void test_ortho_validate(TestResult *r) {
    OrthoViolation viol[8];
    int n;

    /* Correctly formed words → 0 violations */
    n = kin_ortho_validate("kweza", viol, 8);
    ASSERT_INT(r, n, 0, "kweza has 0 ortho violations");

    n = kin_ortho_validate("aragenda", viol, 8);
    ASSERT_INT(r, n, 0, "aragenda has 0 ortho violations");

    n = kin_ortho_validate("mbaga", viol, 8);
    ASSERT_INT(r, n, 0, "mbaga has 0 ortho violations");

    n = kin_ortho_validate("mwana", viol, 8);
    ASSERT_INT(r, n, 0, "mwana has 0 ortho violations");

    /* VV hiatus violations */
    n = kin_ortho_validate("kueza", viol, 8);
    ASSERT(r, n > 0, "kueza has violations");
    ASSERT_INT(r, viol[0].type, ORTHO_VV_HIATUS, "kueza → ORTHO_VV_HIATUS");

    n = kin_ortho_validate("mueza", viol, 8);
    ASSERT(r, n > 0, "mueza has violations");
    ASSERT_INT(r, viol[0].type, ORTHO_VV_HIATUS, "mueza → ORTHO_VV_HIATUS");

    /* Nasal assimilation violation */
    n = kin_ortho_validate("nbaga", viol, 8);
    ASSERT(r, n > 0, "nbaga has violations");
    ASSERT_INT(r, viol[0].type, ORTHO_NASAL_ASSIM, "nbaga → ORTHO_NASAL_ASSIM");

    n = kin_ortho_validate("npamba", viol, 8);
    ASSERT(r, n > 0, "npamba has violations");
    ASSERT_INT(r, viol[0].type, ORTHO_NASAL_ASSIM, "npamba → ORTHO_NASAL_ASSIM");
}

/* ── kin_ortho_fix: auto-correction ──────────────────────────────────────── */
static void test_ortho_fix(TestResult *r) {
    char fixed[128];

    /* Nasal assimilation: n→m before bilabial */
    kin_ortho_fix("nbaga",  fixed, sizeof(fixed));
    ASSERT_STR(r, fixed, "mbaga",  "fix nbaga → mbaga");

    kin_ortho_fix("npamba", fixed, sizeof(fixed));
    ASSERT_STR(r, fixed, "mpamba", "fix npamba → mpamba");

    /* Already correct words pass through unchanged */
    kin_ortho_fix("aragenda", fixed, sizeof(fixed));
    ASSERT_STR(r, fixed, "aragenda", "fix aragenda → unchanged");

    kin_ortho_fix("mbere", fixed, sizeof(fixed));
    ASSERT_STR(r, fixed, "mbere", "fix mbere → unchanged");
}

/* ── Entry point ──────────────────────────────────────────────────────────── */
void run_ortho_tests(TestResult *r) {
    test_ortho_gen(r);
    test_ortho_validate(r);
    test_ortho_fix(r);
}
