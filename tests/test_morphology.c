/*
 * test_morphology.c
 * Unit tests for morphology.c: noun class detection, prefix stripping,
 * infinitive detection, conjugated verb detection, phonological rules.
 */
#include <string.h>
#include "../include/kinyarwanda.h"
#include "test_framework.h"

/* ── Helpers ──────────────────────────────────────────────────────────────── */
static char vstm[KIN_MAX_STEM];
static int  sc, oc;
static VerbTense tns;
static VerbExtension ext;
static bool neg;

/* ── Noun class detection ─────────────────────────────────────────────────── */
static void test_noun_class_detection(TestResult *r) {
    /* Unambiguous full-prefix forms */
    ASSERT_INT(r, kin_detect_noun_class("abantu"),  2,  "abantu → Nt.2");
    ASSERT_INT(r, kin_detect_noun_class("amazi"),   6,  "amazi → Nt.6");
    ASSERT_INT(r, kin_detect_noun_class("ikigo"),   7,  "ikigo → Nt.7");
    ASSERT_INT(r, kin_detect_noun_class("ibigo"),   8,  "ibigo → Nt.8");
    ASSERT_INT(r, kin_detect_noun_class("urugo"),   11, "urugo → Nt.11");
    ASSERT_INT(r, kin_detect_noun_class("akana"),   12, "akana → Nt.12");
    ASSERT_INT(r, kin_detect_noun_class("utugabo"), 13, "utugabo → Nt.13");
    ASSERT_INT(r, kin_detect_noun_class("uburezi"), 14, "uburezi → Nt.14");
    ASSERT_INT(r, kin_detect_noun_class("ahantu"),  16, "ahantu → Nt.16");

    /* Nt.9: i + nasal prefix */
    ASSERT_INT(r, kin_detect_noun_class("inka"),   9, "inka → Nt.9 (in- prefix)");
    ASSERT_INT(r, kin_detect_noun_class("imvura"), 9, "imvura → Nt.9 (n→m before bilabial)");

    /* Nt.5: i + non-nasal consonant */
    ASSERT_INT(r, kin_detect_noun_class("ibuye"),  5, "ibuye → Nt.5");
    ASSERT_INT(r, kin_detect_noun_class("ishuri"), 5, "ishuri → Nt.5");

    /* Nt.1/3 share prefix; function returns 1 per documented tie-break */
    ASSERT_INT(r, kin_detect_noun_class("umuntu"), 1, "umuntu → Nt.1");
    ASSERT_INT(r, kin_detect_noun_class("umuti"),  1, "umuti → Nt.1 (shared prefix)");

    /* Phonological variants */
    ASSERT_INT(r, kin_detect_noun_class("igihugu"), 7,  "igihugu → Nt.7 (k→g voiced)");
    ASSERT_INT(r, kin_detect_noun_class("ibintu"),  8,  "ibintu → Nt.8");
    ASSERT_INT(r, kin_detect_noun_class("ubwenge"), 14, "ubwenge → Nt.14 (ubw- variant)");
    ASSERT_INT(r, kin_detect_noun_class("urwego"),  11, "urwego → Nt.11 (urw- variant)");

    /* Locative Nt.16 */
    ASSERT_INT(r, kin_detect_noun_class("ahasigarana"), 16, "ahasigarana → Nt.16");
}

/* ── Noun prefix stripping ────────────────────────────────────────────────── */
static void test_noun_prefix_strip(TestResult *r) {
    char stem[KIN_MAX_STEM];
    int  cls = 0;

    /* umuntu → Nt.1, stem "ntu" */
    cls = 0; memset(stem, 0, sizeof(stem));
    ASSERT(r, kin_strip_noun_prefix("umuntu", stem, &cls), "strip umuntu returns true");
    ASSERT_INT(r, cls, 1, "umuntu stripped class = 1");
    ASSERT_STR(r, stem, "ntu", "umuntu stem = ntu");

    /* abantu → Nt.2, stem "ntu" */
    cls = 0; memset(stem, 0, sizeof(stem));
    ASSERT(r, kin_strip_noun_prefix("abantu", stem, &cls), "strip abantu returns true");
    ASSERT_INT(r, cls, 2, "abantu stripped class = 2");
    ASSERT_STR(r, stem, "ntu", "abantu stem = ntu");

    /* ikigo → Nt.7, stem "go" */
    cls = 0; memset(stem, 0, sizeof(stem));
    ASSERT(r, kin_strip_noun_prefix("ikigo", stem, &cls), "strip ikigo returns true");
    ASSERT_INT(r, cls, 7, "ikigo stripped class = 7");
    ASSERT_STR(r, stem, "go", "ikigo stem = go");

    /* ibigo → Nt.8, stem "go" */
    cls = 0; memset(stem, 0, sizeof(stem));
    ASSERT(r, kin_strip_noun_prefix("ibigo", stem, &cls), "strip ibigo returns true");
    ASSERT_INT(r, cls, 8, "ibigo stripped class = 8");
    ASSERT_STR(r, stem, "go", "ibigo stem = go");

    /* urugo → Nt.11, stem "go" */
    cls = 0; memset(stem, 0, sizeof(stem));
    ASSERT(r, kin_strip_noun_prefix("urugo", stem, &cls), "strip urugo returns true");
    ASSERT_INT(r, cls, 11, "urugo stripped class = 11");
    ASSERT_STR(r, stem, "go", "urugo stem = go");

    /* uburezi → Nt.14, stem "rezi" */
    cls = 0; memset(stem, 0, sizeof(stem));
    ASSERT(r, kin_strip_noun_prefix("uburezi", stem, &cls), "strip uburezi returns true");
    ASSERT_INT(r, cls, 14, "uburezi stripped class = 14");
    ASSERT_STR(r, stem, "rezi", "uburezi stem = rezi");
}

/* ── Verb infinitive detection ────────────────────────────────────────────── */
static void test_verb_infinitive(TestResult *r) {
    /* Standard gu- prefix */
    memset(vstm, 0, sizeof(vstm));
    ASSERT(r, kin_is_verb_infinitive("gukora", vstm), "gukora is infinitive");
    ASSERT_STR(r, vstm, "kor", "gukora stem = kor");

    memset(vstm, 0, sizeof(vstm));
    ASSERT(r, kin_is_verb_infinitive("gutura", vstm), "gutura is infinitive");
    ASSERT_STR(r, vstm, "tur", "gutura stem = tur");

    /* Standard ku- prefix */
    memset(vstm, 0, sizeof(vstm));
    ASSERT(r, kin_is_verb_infinitive("kugenda", vstm), "kugenda is infinitive");
    ASSERT_STR(r, vstm, "gend", "kugenda stem = gend");

    memset(vstm, 0, sizeof(vstm));
    ASSERT(r, kin_is_verb_infinitive("kubara", vstm), "kubara is infinitive");
    ASSERT_STR(r, vstm, "bar", "kubara stem = bar");

    /* kw- prefix (vowel-initial stem) */
    memset(vstm, 0, sizeof(vstm));
    ASSERT(r, kin_is_verb_infinitive("kwiga", vstm), "kwiga is infinitive");
    ASSERT_STR(r, vstm, "ig", "kwiga stem = ig");

    /* Verb with extension */
    memset(vstm, 0, sizeof(vstm));
    ASSERT(r, kin_is_verb_infinitive("gukorwa", vstm), "gukorwa (passive) is infinitive");

    memset(vstm, 0, sizeof(vstm));
    ASSERT(r, kin_is_verb_infinitive("gukorera", vstm), "gukorera (applicative) is infinitive");

    memset(vstm, 0, sizeof(vstm));
    ASSERT(r, kin_is_verb_infinitive("gufungura", vstm), "gufungura (reversive) is infinitive");

    /* Non-infinitives */
    ASSERT(r, !kin_is_verb_infinitive("umuntu", vstm),   "umuntu is NOT infinitive");
    ASSERT(r, !kin_is_verb_infinitive("aragenda", vstm), "aragenda is NOT infinitive");
    ASSERT(r, !kin_is_verb_infinitive("neza", vstm),     "neza is NOT infinitive");
}

/* ── Verb conjugation detection ───────────────────────────────────────────── */
static void test_verb_conjugated(TestResult *r) {
    /* Present immediate (ara-SP, TENSE_PRESENT, cls=1) */
    ASSERT(r, kin_is_verb_conjugated("aragenda", vstm, &sc, &tns, &oc, &ext, &neg),
           "aragenda recognized as conjugated");
    ASSERT_INT(r, tns, TENSE_PRESENT, "aragenda → TENSE_PRESENT");
    ASSERT_INT(r, sc,  1,             "aragenda → SP class 1 (ara-)");
    ASSERT(r, !neg,                   "aragenda is not negative");

    /* Present, Nt.2 SP "ba" */
    ASSERT(r, kin_is_verb_conjugated("baragenda", vstm, &sc, &tns, &oc, &ext, &neg),
           "baragenda recognized as conjugated");
    ASSERT_INT(r, tns, TENSE_PRESENT, "baragenda → TENSE_PRESENT");
    ASSERT_INT(r, sc,  2,             "baragenda → SP class 2 (ba-)");

    /* Past perfect (-ye FV) */
    ASSERT(r, kin_is_verb_conjugated("yaremye", vstm, &sc, &tns, &oc, &ext, &neg),
           "yaremye recognized as conjugated");
    ASSERT_INT(r, tns, TENSE_PAST_PERF, "yaremye → TENSE_PAST_PERF");
    ASSERT(r, !neg,                     "yaremye is not negative");

    /* Future (za- TM) */
    ASSERT(r, kin_is_verb_conjugated("azagenda", vstm, &sc, &tns, &oc, &ext, &neg),
           "azagenda recognized as conjugated");
    ASSERT_INT(r, tns, TENSE_FUTURE,  "azagenda → TENSE_FUTURE");
    ASSERT_INT(r, sc,  1,             "azagenda → SP class 1");

    /* Past imperfect (-aga FV) */
    ASSERT(r, kin_is_verb_conjugated("yagendaga", vstm, &sc, &tns, &oc, &ext, &neg),
           "yagendaga recognized as conjugated");
    ASSERT_INT(r, tns, TENSE_PAST_IMPF, "yagendaga → TENSE_PAST_IMPF");

    /* Subjunctive (-e FV) */
    ASSERT(r, kin_is_verb_conjugated("agende", vstm, &sc, &tns, &oc, &ext, &neg),
           "agende recognized as conjugated");
    ASSERT_INT(r, tns, TENSE_SUBJUNCTIVE, "agende → TENSE_SUBJUNCTIVE");

    /* Narrative sequential (ka- TM) */
    ASSERT(r, kin_is_verb_conjugated("akagenda", vstm, &sc, &tns, &oc, &ext, &neg),
           "akagenda recognized as conjugated");
    ASSERT_INT(r, tns, TENSE_NARRATIVE, "akagenda → TENSE_NARRATIVE");

    /* Negative (nt- prefix sets neg flag) */
    ASSERT(r, kin_is_verb_conjugated("ntiragenda", vstm, &sc, &tns, &oc, &ext, &neg),
           "ntiragenda recognized as conjugated");
    ASSERT(r, neg, "ntiragenda is negative");

    /* Verb with extension — passive (-w-) */
    ASSERT(r, kin_is_verb_conjugated("yakorwaga", vstm, &sc, &tns, &oc, &ext, &neg),
           "yakorwaga recognized as conjugated");
    ASSERT_INT(r, ext, VEXT_PASSIVE, "yakorwaga → VEXT_PASSIVE");
    ASSERT_INT(r, tns, TENSE_PAST_IMPF, "yakorwaga → TENSE_PAST_IMPF");

    /* Non-conjugated words */
    ASSERT(r, !kin_is_verb_conjugated("umuntu", vstm, &sc, &tns, &oc, &ext, &neg),
           "umuntu is NOT conjugated verb");
    ASSERT(r, !kin_is_verb_conjugated("gukora", vstm, &sc, &tns, &oc, &ext, &neg),
           "gukora (infinitive) is NOT conjugated");
}

/* ── Phonological rule: VV hiatus ─────────────────────────────────────────── */
static void test_vv_hiatus(TestResult *r) {
    /* Forms that have hiatus (adjacent vowels within a word) */
    ASSERT(r,  kin_has_vowel_hiatus("kuiga"),   "kuiga has hiatus (u+i)");
    ASSERT(r,  kin_has_vowel_hiatus("baeza"),   "baeza has hiatus (a+e)");
    ASSERT(r,  kin_has_vowel_hiatus("mueza"),   "mueza has hiatus (u+e)");
    ASSERT(r,  kin_has_vowel_hiatus("baita"),   "baita has hiatus (a+i)");

    /* Correctly resolved forms — no hiatus */
    ASSERT(r, !kin_has_vowel_hiatus("kwiga"),    "kwiga no hiatus (w not vowel)");
    ASSERT(r, !kin_has_vowel_hiatus("beza"),     "beza no hiatus");
    ASSERT(r, !kin_has_vowel_hiatus("mweza"),    "mweza no hiatus");
    ASSERT(r, !kin_has_vowel_hiatus("aragenda"), "aragenda no hiatus");
    ASSERT(r, !kin_has_vowel_hiatus("umuntu"),   "umuntu no hiatus");
    ASSERT(r, !kin_has_vowel_hiatus("abantu"),   "abantu no hiatus");
    ASSERT(r, !kin_has_vowel_hiatus("mbere"),    "mbere no hiatus");
}

/* ── Phonological rule: consonant cluster ─────────────────────────────────── */
static void test_invalid_cluster(TestResult *r) {
    /* Valid native clusters (nasal-initial, digraphs) */
    ASSERT(r, !kin_has_invalid_cluster("mbere"),    "mbere: mb valid");
    ASSERT(r, !kin_has_invalid_cluster("ntabwo"),   "ntabwo: nt valid, bw valid");
    ASSERT(r, !kin_has_invalid_cluster("nshuti"),   "nshuti: nsh valid");
    ASSERT(r, !kin_has_invalid_cluster("ishuri"),   "ishuri: sh valid (digraph)");
    ASSERT(r, !kin_has_invalid_cluster("aragenda"), "aragenda: no CC");
    ASSERT(r, !kin_has_invalid_cluster("mpanga"),   "mpanga: mp valid");

    /* Invalid foreign-shaped clusters */
    ASSERT(r,  kin_has_invalid_cluster("straba"), "straba: st invalid");
    ASSERT(r,  kin_has_invalid_cluster("blayi"),  "blayi: bl invalid");
    ASSERT(r,  kin_has_invalid_cluster("flanga"), "flanga: fl invalid");
}

/* ── VV join (prefix + stem resolver) ────────────────────────────────────── */
static void test_vv_join(TestResult *r) {
    char out[64];

    kin_vv_join("mu",  "ana",   out, sizeof(out));
    ASSERT_STR(r, out, "mwana",  "mu+ana → mwana (u→w)");

    kin_vv_join("ba",  "ana",   out, sizeof(out));
    ASSERT_STR(r, out, "bana",   "ba+ana → bana (a→∅)");

    kin_vv_join("ba",  "inja",  out, sizeof(out));
    ASSERT_STR(r, out, "benja",  "ba+inja → benja (a+i→e)");

    kin_vv_join("ya",  "iga",   out, sizeof(out));
    ASSERT_STR(r, out, "yiga",   "ya+iga → yiga (a→∅ after glide)");

    kin_vv_join("ku",  "oma",   out, sizeof(out));
    ASSERT_STR(r, out, "koma",   "ku+oma → koma (w dropped before round vowel)");

    kin_vv_join("ku",  "iga",   out, sizeof(out));
    ASSERT_STR(r, out, "kwiga",  "ku+iga → kwiga (u→w before front vowel)");
}

/* ── Entry point ──────────────────────────────────────────────────────────── */
void run_morphology_tests(TestResult *r) {
    test_noun_class_detection(r);
    test_noun_prefix_strip(r);
    test_verb_infinitive(r);
    test_verb_conjugated(r);
    test_vv_hiatus(r);
    test_invalid_cluster(r);
    test_vv_join(r);
}
