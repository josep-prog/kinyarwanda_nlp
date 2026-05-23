/*
 * test_lexicon.c
 * Unit tests for lexicon.c: invariable words, pronouns, adjective stems,
 * verb stem table, and noun plural pair lookup.
 */
#include <string.h>
#include "../include/kinyarwanda.h"
#include "test_framework.h"

/* ── Invariable words ─────────────────────────────────────────────────────── */
static void test_invariables(TestResult *r) {
    POS p;

    /* Conjunctions (icyungo) */
    ASSERT(r, kin_is_invariable("na",    &p) && p == POS_CONJUNCTION, "na → POS_CONJUNCTION");
    ASSERT(r, kin_is_invariable("ariko", &p) && p == POS_CONJUNCTION, "ariko → POS_CONJUNCTION");
    ASSERT(r, kin_is_invariable("kandi", &p) && p == POS_CONJUNCTION, "kandi → POS_CONJUNCTION");
    ASSERT(r, kin_is_invariable("rero",  &p) && p == POS_CONJUNCTION, "rero → POS_CONJUNCTION");

    /* Verb particles (ikegeranshinga) */
    ASSERT(r, kin_is_invariable("ngo",   &p) && p == POS_VERB_PARTICLE, "ngo → POS_VERB_PARTICLE");
    ASSERT(r, kin_is_invariable("ko",    &p) && p == POS_VERB_PARTICLE, "ko → POS_VERB_PARTICLE");

    /* Locative/prepositional words — stored as POS_LOCATIVE in the table
     * (ku/mu/kuri are ambiguous with noun-class prefixes; the tagger
     * resolves them contextually, so they use POS_LOCATIVE as a base tag) */
    ASSERT(r, kin_is_invariable("ku",    &p) && p == POS_LOCATIVE, "ku → POS_LOCATIVE");
    ASSERT(r, kin_is_invariable("mu",    &p) && p == POS_LOCATIVE, "mu → POS_LOCATIVE");
    ASSERT(r, kin_is_invariable("kuri",  &p) && p == POS_LOCATIVE, "kuri → POS_LOCATIVE");

    /* Adverbs (akamamo) */
    ASSERT(r, kin_is_invariable("cyane", &p) && p == POS_ADVERB, "cyane → POS_ADVERB");
    ASSERT(r, kin_is_invariable("neza",  &p) && p == POS_ADVERB, "neza → POS_ADVERB");
    ASSERT(r, kin_is_invariable("gato",  &p) && p == POS_ADVERB, "gato → POS_ADVERB");

    /* Locatives (indangahantu) */
    ASSERT(r, kin_is_invariable("hano",  &p) && p == POS_LOCATIVE, "hano → POS_LOCATIVE");
    ASSERT(r, kin_is_invariable("hasi",  &p) && p == POS_LOCATIVE, "hasi → POS_LOCATIVE");

    /* Words NOT in the invariable table */
    ASSERT(r, !kin_is_invariable("umuntu",  &p), "umuntu is NOT invariable");
    ASSERT(r, !kin_is_invariable("aragenda",&p), "aragenda is NOT invariable");
    ASSERT(r, !kin_is_invariable("nini",    &p), "nini is NOT invariable");
}

/* ── Pronouns ─────────────────────────────────────────────────────────────── */
static void test_pronouns(TestResult *r) {
    PronounType pt;
    int cls;

    /* Demonstrative (ikinyazina nyereka) */
    ASSERT(r, kin_is_pronoun("uyu",  &pt, &cls) && pt == PRON_DEMONSTRATIVE,
           "uyu → PRON_DEMONSTRATIVE");
    ASSERT(r, kin_is_pronoun("aba",  &pt, &cls) && pt == PRON_DEMONSTRATIVE,
           "aba → PRON_DEMONSTRATIVE (these people)");

    /* Personal (ikinyazina ngenga) */
    ASSERT(r, kin_is_pronoun("nge",  &pt, &cls) && pt == PRON_PERSONAL,
           "nge → PRON_PERSONAL (1sg me)");
    ASSERT(r, kin_is_pronoun("we",   &pt, &cls) && pt == PRON_PERSONAL,
           "we → PRON_PERSONAL (2sg you)");

    /* Interrogative (ikinyazina kibaza) */
    ASSERT(r, kin_is_pronoun("nde",  &pt, &cls) && pt == PRON_INTERROGATIVE,
           "nde → PRON_INTERROGATIVE (who?)");
    /* "iki" appears first in the demonstrative table (class 7) before the
     * interrogative entry; the function returns the first match */
    ASSERT(r, kin_is_pronoun("iki",  &pt, &cls) && pt == PRON_DEMONSTRATIVE,
           "iki → PRON_DEMONSTRATIVE (first table match, class 7)");

    /* Reflexive/intensive possessive (ikinyazina ngenera ngenga) */
    ASSERT(r, kin_is_pronoun("wange", &pt, &cls),
           "wange recognized as pronoun");
    ASSERT(r, kin_is_pronoun("wacu",  &pt, &cls),
           "wacu recognized as pronoun");

    /* Possessive connector (ikinyazina ngenera) — some forms */
    ASSERT(r, kin_is_pronoun("cya",  &pt, &cls),
           "cya recognized as pronoun (Nt.7 possessive connector)");

    /* Non-pronoun words */
    ASSERT(r, !kin_is_pronoun("umuntu", &pt, &cls), "umuntu is NOT pronoun");
    ASSERT(r, !kin_is_pronoun("gukora", &pt, &cls), "gukora is NOT pronoun");
}

/* ── Adjective stems ──────────────────────────────────────────────────────── */
static void test_adj_stems(TestResult *r) {
    /* Known stems from the closed set (REB S4 p.66-67) */
    ASSERT(r, kin_is_adj_stem("nini"),   "nini is adj stem (large)");
    ASSERT(r, kin_is_adj_stem("bi"),     "bi is adj stem (bad)");
    ASSERT(r, kin_is_adj_stem("re"),     "re is adj stem (tall/long)");
    ASSERT(r, kin_is_adj_stem("inshi"),  "inshi is adj stem (many)");
    ASSERT(r, kin_is_adj_stem("iza"),    "iza is adj stem (good/beautiful)");
    ASSERT(r, kin_is_adj_stem("to"),     "to is adj stem (small)");
    ASSERT(r, kin_is_adj_stem("bisi"),   "bisi is adj stem (raw/fresh)");

    /* Non-adjective stems */
    ASSERT(r, !kin_is_adj_stem("gend"),  "gend is NOT adj stem (verb root)");
    ASSERT(r, !kin_is_adj_stem("ntu"),   "ntu is NOT adj stem (noun root)");
    ASSERT(r, !kin_is_adj_stem("fite"),  "fite is NOT adj stem (verb form)");
    ASSERT(r, !kin_is_adj_stem("go"),    "go is NOT adj stem");
}

/* ── Known verb stems ─────────────────────────────────────────────────────── */
static void test_known_verb_stems(TestResult *r) {
    ASSERT(r, kin_is_known_verb_stem("gend"),  "gend is known stem (kugenda)");
    ASSERT(r, kin_is_known_verb_stem("kor"),   "kor is known stem (gukora)");
    ASSERT(r, kin_is_known_verb_stem("rem"),   "rem is known stem (kurema)");
    ASSERT(r, kin_is_known_verb_stem("bon"),   "bon is known stem (kubona)");
    ASSERT(r, kin_is_known_verb_stem("som"),   "som is known stem (gusoma)");
    ASSERT(r, kin_is_known_verb_stem("fash"),  "fash is known stem (gufasha)");

    /* Stems that should NOT be in the verb table */
    ASSERT(r, !kin_is_known_verb_stem("nini"), "nini is NOT verb stem");
    ASSERT(r, !kin_is_known_verb_stem("ntu"),  "ntu is NOT verb stem");
    ASSERT(r, !kin_is_known_verb_stem("xyz"),  "xyz is NOT verb stem");
}

/* ── Noun class accessor ──────────────────────────────────────────────────── */
static void test_noun_class_table(TestResult *r) {
    const NounClass *nc;

    nc = kin_get_noun_class(1);
    ASSERT(r, nc != NULL,              "class 1 entry exists");
    ASSERT_STR(r, nc->prefix, "umu",   "Nt.1 prefix = umu");
    ASSERT_STR(r, nc->subj_prefix, "a", "Nt.1 subj prefix = a");

    nc = kin_get_noun_class(2);
    ASSERT(r, nc != NULL,              "class 2 entry exists");
    ASSERT_STR(r, nc->prefix, "aba",   "Nt.2 prefix = aba");
    ASSERT_STR(r, nc->subj_prefix, "ba", "Nt.2 subj prefix = ba");

    nc = kin_get_noun_class(7);
    ASSERT(r, nc != NULL,              "class 7 entry exists");
    ASSERT_STR(r, nc->prefix, "iki",   "Nt.7 prefix = iki");
    ASSERT_STR(r, nc->concordance_adj, "ki", "Nt.7 adj concordance = ki");

    nc = kin_get_noun_class(11);
    ASSERT(r, nc != NULL,               "class 11 entry exists");
    ASSERT_STR(r, nc->concordance_poss, "rwa", "Nt.11 poss connector = rwa");

    ASSERT(r, kin_get_noun_class(0)  == NULL, "class 0 → NULL");
    ASSERT(r, kin_get_noun_class(17) == NULL, "class 17 → NULL");
}

/* ── Entry point ──────────────────────────────────────────────────────────── */
void run_lexicon_tests(TestResult *r) {
    test_invariables(r);
    test_pronouns(r);
    test_adj_stems(r);
    test_known_verb_stems(r);
    test_noun_class_table(r);
}
