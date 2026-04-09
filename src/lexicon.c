/*
 * lexicon.c
 * Static linguistic tables directly encoded from the REB textbook.
 *
 * Sources (all from "Ikinyarwanda Amashuri Nderabarezi TTC" – REB 2020):
 *   Noun classes     → p.61-63
 *   Adjective stems  → p.66-67
 *   Pronoun tables   → p.90-97
 *   Invariable words → p.89 (amagambo adahinduka)
 *   Verb stems       → examples throughout the book
 */

#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include "../include/kinyarwanda.h"

/* ══════════════════════════════════════════════════════════════════════════
 * TREE 1 — IZINA MBONERA (Common Noun)
 * Section 1: INTEKO (Noun Classes) – 16 classes   Source: REB S4 p.61-62
 *
 * Each class entry stores:
 *   num           – class number 1-16
 *   prefix (D+RT) – combined surface prefix (e.g. "umu" = u+mu)
 *   rt            – indanganteko alone (e.g. "mu")
 *   concordance_adj  – indangasano for ntera (adjective agreement)
 *   concordance_poss – possessive connector (ikinyazina ngenera)
 *   subj_prefix   – verb subject agreement prefix (SP)
 *   description   – label in English
 *
 * Nt.1/Nt.2 = human nouns  (umuntu / abantu)
 * Nt.3/Nt.4 = tree/thing nouns (umuti / imiti)  — SAME prefix as Nt.1/2
 * ══════════════════════════════════════════════════════════════════════════ */
static const NounClass NOUN_CLASSES[] = {
    /* num  prefix   rt    adj-RS  poss-conn  subj  description */
    {  1, "umu",  "mu",  "mu",   "wa",  "a",   "Nt.1 – human singular (umuntu)"       },
    {  2, "aba",  "ba",  "ba",   "ba",  "ba",  "Nt.2 – human plural (abantu)"         },
    {  3, "umu",  "mu",  "mu",   "wa",  "u",   "Nt.3 – tree/thing singular (umuti)"   },
    {  4, "imi",  "mi",  "mi",   "ya",  "i",   "Nt.4 – tree/thing plural (imiti)"     },
    {  5, "i",    "ri",  "ri",   "rya", "ri",  "Nt.5 – singular (ibuye/iryango)"      },
    {  6, "ama",  "ma",  "ma",   "ya",  "a",   "Nt.6 – plural/mass (amabuye/amazi)"   },
    {  7, "iki",  "ki",  "ki",   "cya", "ki",  "Nt.7 – thing singular (ikigo)"        },
    {  8, "ibi",  "bi",  "bi",   "bya", "bi",  "Nt.8 – thing plural (ibigo)"          },
    {  9, "in",   "n",   "n",    "ya",  "i",   "Nt.9 – animal/thing sing. (inka)"     },
    { 10, "in",   "n",   "zi",   "za",  "zi",  "Nt.10 – animal/thing plur. (inka)"    },
    { 11, "uru",  "ru",  "ru",   "rwa", "ru",  "Nt.11 – long/thin (urugo)"            },
    { 12, "aka",  "ka",  "ka",   "ka",  "ka",  "Nt.12 – diminutive sing. (akana)"     },
    { 13, "utu",  "tu",  "tu",   "twa", "tu",  "Nt.13 – diminutive plur. (utugabo)"   },
    { 14, "ubu",  "bu",  "bu",   "bwa", "bu",  "Nt.14 – abstract (uburezi)"           },
    { 15, "uku",  "ku",  "ku",   "kwa", "ku",  "Nt.15 – infinitive/verbal noun"       },
    { 16, "aha",  "ha",  "ha",   "ha",  "ha",  "Nt.16 – locative (ahantu)"            },
};
#define NOUN_CLASS_COUNT 16

const NounClass *kin_get_noun_class(int num) {
    if (num < 1 || num > NOUN_CLASS_COUNT) return NULL;
    return &NOUN_CLASSES[num - 1];
}

/* ══════════════════════════════════════════════════════════════════════════
 * TREE 2 — NTERA (Adjective)   Formula: RS + C
 * Section 2: IBICUMBI BY'INTERA (Adjective stems)   Source: REB S4 p.66-67
 *
 * These stems form the CLOSED SET of adjective roots in Kinyarwanda.
 * Any word built as:  concordance_prefix (RS) + one of these stems = ntera.
 * RS must agree with the noun class it modifies (indangasano rule).
 *
 * Transition → izina ntera (POS_RELATIVE_NOUN): when a noun plays the
 *   qualifier role for another noun via an ikinyazina ngenera connector.
 *   e.g. "igitabo cy'Ikinyarwanda" — the noun Ikinyarwanda is izina ntera.
 *   Detected in pos_tagger.c Context Pass B.
 *
 * Planned → igisantera (POS_COMPOUND_ADJ): noun pair as compound adjective.
 *   Not yet implemented. Tag POS_COMPOUND_ADJ exists in the header.
 * ══════════════════════════════════════════════════════════════════════════ */
static const char *ADJ_STEMS[] = {
    "nini",         /* 1.  large / adult                          */
    "inshi",        /* 2.  many                                   */
    "bi",           /* 3.  bad                                    */
    "tindi",        /* 4.  another kind (different)               */
    "gari",         /* 5.  wide                                   */
    "iza",          /* 6.  good / beautiful                       */
    "sa",           /* 7.  like (similar), also "sa-sa"           */
    "sa-sa",
    "zima",         /* 8.  whole / healthy                        */
    "to",           /* 9.  small, also "toto", "to-to", "toya"    */
    "toto",
    "to-to",
    "toya",
    "ke",           /* 10. few, also "keya", "ke-ke"              */
    "keya",
    "ke-ke",
    "kuru",         /* 11. old / big                              */
    "bisi",         /* 12. raw / unripe                           */
    "shya",         /* 13. new / fresh, also "shyashya"           */
    "shyashya",
    "gufi",         /* 14. short, also "gufiya"                   */
    "gufiya",
    "re",           /* 15. long / tall, also "-re-re"             */
    "re-re",
    "tagatifu",     /* 16. holy / sacred                          */
    "hire",         /* 17. fast / quick                           */
    "taraga",       /* 18. old (age)                              */
    /* 19. big / many (augmentative forms): */
    "nzinya", "nzunyu", "nuya", "niniya", "nzuzunya",
    "nunuya", "niniriya", "nziginya", "nzugurunyu",
    /* Stems from REB textbooks and Bible corpus not in the original list */
    "nzima",        /* 20. heavy / difficult / sick (munzima, binzima)   */
    "ogo",          /* 21. deep / wide / immense   (rugo=long, bugo=...)  */
    "eru",          /* 22. white / clean / pure    (mweru, byeru, keru)   */
    "rimbwa",       /* 23. lovable / beloved       (murimbwa, birimbwa)   */
    "nkuru",        /* 24. great / senior / big (variant of -kuru)        */
    "meze",         /* 25. resembling / having the condition of (from kumera, RS+meze = bumeze/numeze) */
    NULL
};

bool kin_is_adj_stem(const char *stem) {
    for (int i = 0; ADJ_STEMS[i]; i++)
        if (strcmp(stem, ADJ_STEMS[i]) == 0) return true;
    return false;
}

/*
 * kin_is_adj_reduplicated()
 *
 * Reduplication (gukoresha indorerezi) of adjectives emphasises the quality:
 *   -re  (long)  → mu+re+mu+re = muremure  (very tall/long, Nt.1)
 *              → ba+re+ba+re = barebare  (Nt.2)
 *              → ru+re+ru+re = rurerure  (Nt.11)
 * Structure of the reduplicated form: RS + stem + RS + stem
 * After stripping the initial RS prefix in kin_strip_adj_prefix, the
 * remainder (sfx) should be:  stem + pfx + stem
 * e.g.  sfx="rebare", pfx="ba"  →  "re" + "ba" + "re" ✓
 * Returns true and fills stem_out with the base stem if matched.
 */
bool kin_is_adj_reduplicated(const char *sfx, const char *pfx, char *stem_out) {
    size_t pfxlen = strlen(pfx);
    for (int i = 0; ADJ_STEMS[i]; i++) {
        size_t slen = strlen(ADJ_STEMS[i]);
        /* sfx must be at least  slen + pfxlen + slen  chars */
        if (strlen(sfx) != slen + pfxlen + slen) continue;
        /* Check: sfx starts with stem, then pfx, then stem again */
        if (strncmp(sfx,                    ADJ_STEMS[i], slen)   == 0 &&
            strncmp(sfx + slen,             pfx,          pfxlen) == 0 &&
            strncmp(sfx + slen + pfxlen,    ADJ_STEMS[i], slen)   == 0) {
            if (stem_out) strncpy(stem_out, ADJ_STEMS[i], KIN_MAX_STEM - 1);
            return true;
        }
    }
    return false;
}

/* ══════════════════════════════════════════════════════════════════════════
 * TREE 4 — IKINYAZINA (Pronoun)   Nine sub-types
 * Section 3: PRONOUNS table   Source: REB S4/S5/S6 p.89-121
 *
 * Each entry: { surface form, PronounType, noun_class }
 *   class 0 = applies to multiple classes or is class-independent
 *
 * Sub-type order in this table (matching PronounType enum):
 *   PRON_PERSONAL       Ikinyazina ngenga        (nge, we, bo...)
 *   PRON_DEMONSTRATIVE  Ikinyazina nyereka       (uyu, uwo, uno... × 3 proximities)
 *   PRON_POSSESSIVE     Ikinyazina ngenera       (wa, ya, cya... × 16 classes)
 *   PRON_REFLEXIVE      Ikinyazina ngenera ngenga (wange, wacu, wawe, wabo...)
 *   PRON_RELATIVE       Ikinyazina mbanziriza    (uwo, abo, icyo... with -ô tone)
 *   PRON_INTERROGATIVE  Ikinyazina kibaza        (nde, iki, iyihe... + -he/-ngahe)
 *   PRON_INDEFINITE     Ikinyazina ndafutura     (umwe, bamwe, undi, bandi...)
 *   PRON_NUMERICAL      Ikinyazina nyamubaro     (umwe...-rindwi per class)
 *   PRON_VOCATIVE       Ikinyazina mpamagazi     (wa — O! address)
 *
 * Gap: demonstrative proximities 3-6 (further from speaker) partially missing.
 * ══════════════════════════════════════════════════════════════════════════ */
typedef struct { const char *word; PronounType type; int class; } PronounEntry;

static const PronounEntry PRONOUNS[] = {
    /* ── Ikinyazina ngenga (personal pronouns) ─────────────────────────── */
    { "nge",   PRON_PERSONAL, 0 },   /* I / me (1st person sing.)          */
    { "mwe",   PRON_PERSONAL, 0 },   /* you (2nd person plur.)             */
    { "we",    PRON_PERSONAL, 0 },   /* you (2nd person sing.) / it (nt.1) */
    { "bo",    PRON_PERSONAL, 2 },   /* they (human plural)                */
    { "wo",    PRON_PERSONAL, 3 },   /* it (nt.3)                          */
    { "yo",    PRON_PERSONAL, 4 },   /* they (nt.4)                        */
    { "ryo",   PRON_PERSONAL, 5 },   /* it (nt.5)                          */
    { "yo",    PRON_PERSONAL, 6 },   /* they (nt.6)  – shared form         */
    { "cyo",   PRON_PERSONAL, 7 },   /* it (nt.7)                          */
    { "byo",   PRON_PERSONAL, 8 },   /* they (nt.8)                        */
    { "zo",    PRON_PERSONAL,10 },   /* they (nt.10)                       */
    { "rwo",   PRON_PERSONAL,11 },   /* it (nt.11)                         */
    { "ko",    PRON_PERSONAL,12 },   /* it (nt.12)                         */
    { "two",   PRON_PERSONAL,13 },   /* they (nt.13)                       */
    { "bwo",   PRON_PERSONAL,14 },   /* it (nt.14)                         */
    { "ku",    PRON_PERSONAL,15 },   /* it (nt.15 / infinitive)            */
    { "ho",    PRON_PERSONAL,16 },   /* there (nt.16)                      */

    /* ── Ikinyazina nyereka (demonstrative pronouns) ──────────────────── */
    /* proximity 1 (here): uyu/aba/uyu/iyi/iri/aya/iki/ibi/iyi/izi/uru/aka/utu/ubu/uku/aha */
    { "uyu",   PRON_DEMONSTRATIVE,  1 },
    { "aba",   PRON_DEMONSTRATIVE,  2 },
    { "uyu",   PRON_DEMONSTRATIVE,  3 },
    { "iyi",   PRON_DEMONSTRATIVE,  4 },
    { "iri",   PRON_DEMONSTRATIVE,  5 },
    { "aya",   PRON_DEMONSTRATIVE,  6 },
    { "iki",   PRON_DEMONSTRATIVE,  7 },
    { "ibi",   PRON_DEMONSTRATIVE,  8 },
    { "iyi",   PRON_DEMONSTRATIVE,  9 },
    { "izi",   PRON_DEMONSTRATIVE, 10 },
    { "uru",   PRON_DEMONSTRATIVE, 11 },
    { "aka",   PRON_DEMONSTRATIVE, 12 },
    { "utu",   PRON_DEMONSTRATIVE, 13 },
    { "ubu",   PRON_DEMONSTRATIVE, 14 },
    { "uku",   PRON_DEMONSTRATIVE, 15 },
    { "aha",   PRON_DEMONSTRATIVE, 16 },
    /* proximity 2 (there): uwo/abo/uwo/iyo/iryo/ayo/icyo/ibyo/iyo/izo/urwo/ako/utwo/ubwo/ukwo/aho */
    { "uwo",   PRON_DEMONSTRATIVE,  1 },
    { "abo",   PRON_DEMONSTRATIVE,  2 },
    { "iyo",   PRON_DEMONSTRATIVE,  4 },
    { "iryo",  PRON_DEMONSTRATIVE,  5 },
    { "ayo",   PRON_DEMONSTRATIVE,  6 },
    { "icyo",  PRON_DEMONSTRATIVE,  7 },
    { "ibyo",  PRON_DEMONSTRATIVE,  8 },
    { "izo",   PRON_DEMONSTRATIVE, 10 },
    { "urwo",  PRON_DEMONSTRATIVE, 11 },
    { "ako",   PRON_DEMONSTRATIVE, 12 },
    { "ubwo",  PRON_DEMONSTRATIVE, 14 },
    { "aho",   PRON_DEMONSTRATIVE, 16 },

    /* ── Ikinyazina ngenera (possessive/relative connectors) ───────────── */
    { "wa",    PRON_POSSESSIVE,  1 },
    { "ba",    PRON_POSSESSIVE,  2 },
    { "wa",    PRON_POSSESSIVE,  3 },
    { "ya",    PRON_POSSESSIVE,  4 },
    { "rya",   PRON_POSSESSIVE,  5 },
    { "ya",    PRON_POSSESSIVE,  6 },
    { "cya",   PRON_POSSESSIVE,  7 },
    { "bya",   PRON_POSSESSIVE,  8 },
    { "ya",    PRON_POSSESSIVE,  9 },
    { "za",    PRON_POSSESSIVE, 10 },
    { "rwa",   PRON_POSSESSIVE, 11 },
    { "ka",    PRON_POSSESSIVE, 12 },
    { "twa",   PRON_POSSESSIVE, 13 },
    { "bwa",   PRON_POSSESSIVE, 14 },
    { "kwa",   PRON_POSSESSIVE, 15 },
    { "ha",    PRON_POSSESSIVE, 16 },
    /* combined with vowel (gifite indomo): uwa/aba/uwa/iya/irya/aya/icya/ibya/iza/urwa/aka/utwa/ubwa/ukwa/aha */
    { "uwa",   PRON_POSSESSIVE,  1 },
    { "aba",   PRON_POSSESSIVE,  2 },
    { "iya",   PRON_POSSESSIVE,  4 },
    { "irya",  PRON_POSSESSIVE,  5 },
    { "icya",  PRON_POSSESSIVE,  7 },
    { "ibya",  PRON_POSSESSIVE,  8 },
    { "iza",   PRON_POSSESSIVE, 10 },
    { "urwa",  PRON_POSSESSIVE, 11 },
    { "utwa",  PRON_POSSESSIVE, 13 },
    { "ubwa",  PRON_POSSESSIVE, 14 },
    { "ukwa",  PRON_POSSESSIVE, 15 },

    /* ── Ikinyazina ngenera ngenga (reflexive/inclusive possessives) ───── */
    /* 1st singular (-nge): wange, bange, wange, yange... */
    { "wange",  PRON_REFLEXIVE,  1 }, { "bange",  PRON_REFLEXIVE,  2 },
    { "yange",  PRON_REFLEXIVE,  4 }, { "ryange", PRON_REFLEXIVE,  5 },
    { "yange",  PRON_REFLEXIVE,  6 }, { "cyange", PRON_REFLEXIVE,  7 },
    { "byange", PRON_REFLEXIVE,  8 }, { "zange",  PRON_REFLEXIVE, 10 },
    { "rwange", PRON_REFLEXIVE, 11 }, { "kange",  PRON_REFLEXIVE, 12 },
    { "twange", PRON_REFLEXIVE, 13 }, { "bwange", PRON_REFLEXIVE, 14 },
    { "kwange", PRON_REFLEXIVE, 15 }, { "hange",  PRON_REFLEXIVE, 16 },
    /* 1st plural (-cu): wacu, bacu, wacu, yacu... */
    { "wacu",   PRON_REFLEXIVE,  1 }, { "bacu",   PRON_REFLEXIVE,  2 },
    { "yacu",   PRON_REFLEXIVE,  4 }, { "ryacu",  PRON_REFLEXIVE,  5 },
    { "yacu",   PRON_REFLEXIVE,  6 }, { "cyacu",  PRON_REFLEXIVE,  7 },
    { "byacu",  PRON_REFLEXIVE,  8 }, { "zacu",   PRON_REFLEXIVE, 10 },
    { "rwacu",  PRON_REFLEXIVE, 11 }, { "kacu",   PRON_REFLEXIVE, 12 },
    { "twacu",  PRON_REFLEXIVE, 13 }, { "bwacu",  PRON_REFLEXIVE, 14 },
    { "kwacu",  PRON_REFLEXIVE, 15 }, { "hacu",   PRON_REFLEXIVE, 16 },
    /* 2nd singular (-we): wawe, bawe, wawe, yawe... */
    { "wawe",   PRON_REFLEXIVE,  1 }, { "bawe",   PRON_REFLEXIVE,  2 },
    { "yawe",   PRON_REFLEXIVE,  4 }, { "ryawe",  PRON_REFLEXIVE,  5 },
    { "yawe",   PRON_REFLEXIVE,  6 }, { "cyawe",  PRON_REFLEXIVE,  7 },
    { "byawe",  PRON_REFLEXIVE,  8 }, { "zawe",   PRON_REFLEXIVE, 10 },
    { "rwawe",  PRON_REFLEXIVE, 11 }, { "kawe",   PRON_REFLEXIVE, 12 },
    { "twawe",  PRON_REFLEXIVE, 13 }, { "bwawe",  PRON_REFLEXIVE, 14 },
    { "kwawe",  PRON_REFLEXIVE, 15 }, { "hawe",   PRON_REFLEXIVE, 16 },
    /* 3rd plural (-bo): wabo, babo, wabo, yabo... */
    { "wabo",   PRON_REFLEXIVE,  1 }, { "babo",   PRON_REFLEXIVE,  2 },
    { "yabo",   PRON_REFLEXIVE,  4 }, { "ryabo",  PRON_REFLEXIVE,  5 },
    { "yabo",   PRON_REFLEXIVE,  6 }, { "cyabo",  PRON_REFLEXIVE,  7 },
    { "byabo",  PRON_REFLEXIVE,  8 }, { "zabo",   PRON_REFLEXIVE, 10 },
    { "rwabo",  PRON_REFLEXIVE, 11 }, { "kabo",   PRON_REFLEXIVE, 12 },
    { "twabo",  PRON_REFLEXIVE, 13 }, { "bwabo",  PRON_REFLEXIVE, 14 },
    { "kwabo",  PRON_REFLEXIVE, 15 }, { "habo",   PRON_REFLEXIVE, 16 },
    /* Nt.8 class-referent possessives (-byo): connector + byo             *
     * e.g. "twabyo" = twa(Nt.13 conn.) + byo(Nt.8 ref.) = their Nt.8   *
     *      "yabyo"  = ya(Nt.6 conn.)   + byo(Nt.8 ref.) = their Nt.8   */
    { "wabyo",  PRON_REFLEXIVE,  1 }, { "babyo",  PRON_REFLEXIVE,  2 },
    { "yabyo",  PRON_REFLEXIVE,  4 }, { "ryabyo", PRON_REFLEXIVE,  5 },
    { "yabyo",  PRON_REFLEXIVE,  6 }, { "cyabyo", PRON_REFLEXIVE,  7 },
    { "byabyo", PRON_REFLEXIVE,  8 }, { "zabyo",  PRON_REFLEXIVE, 10 },
    { "rwabyo", PRON_REFLEXIVE, 11 }, { "kabyo",  PRON_REFLEXIVE, 12 },
    { "twabyo", PRON_REFLEXIVE, 13 }, { "bwabyo", PRON_REFLEXIVE, 14 },
    { "kwabyo", PRON_REFLEXIVE, 15 }, { "habyo",  PRON_REFLEXIVE, 16 },
    /* Nt.3 class-referent possessives (-wo): connector + wo                 */
    { "wawo",  PRON_REFLEXIVE,  1 }, { "bawo",  PRON_REFLEXIVE,  2 },
    { "yawo",  PRON_REFLEXIVE,  4 }, { "ryawo", PRON_REFLEXIVE,  5 },
    { "yawo",  PRON_REFLEXIVE,  6 }, { "cyawo", PRON_REFLEXIVE,  7 },
    { "byawo", PRON_REFLEXIVE,  8 }, { "zawo",  PRON_REFLEXIVE, 10 },
    { "rwawo", PRON_REFLEXIVE, 11 }, { "kawo",  PRON_REFLEXIVE, 12 },
    { "twawo", PRON_REFLEXIVE, 13 }, { "bwawo", PRON_REFLEXIVE, 14 },
    { "kwawo", PRON_REFLEXIVE, 15 }, { "hawo",  PRON_REFLEXIVE, 16 },
    /* Nt.4/6/9 class-referent possessives (-yo): connector + yo             *
     * e.g. "yayo" = ya(Nt.6 conn.) + yo(Nt.4/6/9 ref.) = "its/their"      *
     *      "zazo" already covered; "yayo"/"yazo" were missing here          */
    { "wayo",  PRON_REFLEXIVE,  1 }, { "bayo",  PRON_REFLEXIVE,  2 },
    { "yayo",  PRON_REFLEXIVE,  4 }, { "ryayo", PRON_REFLEXIVE,  5 },
    { "yayo",  PRON_REFLEXIVE,  6 }, { "cyayo", PRON_REFLEXIVE,  7 },
    { "byayo", PRON_REFLEXIVE,  8 }, { "zayo",  PRON_REFLEXIVE, 10 },
    { "rwayo", PRON_REFLEXIVE, 11 }, { "kayo",  PRON_REFLEXIVE, 12 },
    { "twayo", PRON_REFLEXIVE, 13 }, { "bwayo", PRON_REFLEXIVE, 14 },
    { "kwayo", PRON_REFLEXIVE, 15 }, { "hayo",  PRON_REFLEXIVE, 16 },
    /* Nt.5 class-referent possessives (-ryo): connector + ryo               */
    { "waryo", PRON_REFLEXIVE,  1 }, { "baryo", PRON_REFLEXIVE,  2 },
    { "yaryo", PRON_REFLEXIVE,  4 }, { "ryaryo",PRON_REFLEXIVE,  5 },
    { "yaryo", PRON_REFLEXIVE,  6 }, { "cyaryo",PRON_REFLEXIVE,  7 },
    { "byaryo",PRON_REFLEXIVE,  8 }, { "zaryo", PRON_REFLEXIVE, 10 },
    { "rwaryo",PRON_REFLEXIVE, 11 }, { "karyo", PRON_REFLEXIVE, 12 },
    { "twaryo",PRON_REFLEXIVE, 13 }, { "bwaryo",PRON_REFLEXIVE, 14 },
    { "kwaryo",PRON_REFLEXIVE, 15 }, { "haryo", PRON_REFLEXIVE, 16 },
    /* Nt.7 class-referent possessives (-cyo): connector + cyo               */
    { "wacyo", PRON_REFLEXIVE,  1 }, { "bacyo", PRON_REFLEXIVE,  2 },
    { "yacyo", PRON_REFLEXIVE,  4 }, { "ryacyo",PRON_REFLEXIVE,  5 },
    { "yacyo", PRON_REFLEXIVE,  6 }, { "cyacyo",PRON_REFLEXIVE,  7 },
    { "byacyo",PRON_REFLEXIVE,  8 }, { "zacyo", PRON_REFLEXIVE, 10 },
    { "rwacyo",PRON_REFLEXIVE, 11 }, { "kacyo", PRON_REFLEXIVE, 12 },
    { "twacyo",PRON_REFLEXIVE, 13 }, { "bwacyo",PRON_REFLEXIVE, 14 },
    { "kwacyo",PRON_REFLEXIVE, 15 }, { "hacyo", PRON_REFLEXIVE, 16 },
    /* Nt.10 class-referent possessives (-zo): connector + zo                *
     * e.g. "yazo" = ya(Nt.6 conn.) + zo(Nt.10 ref.)                        */
    { "wazo",  PRON_REFLEXIVE,  1 }, { "bazo",  PRON_REFLEXIVE,  2 },
    { "yazo",  PRON_REFLEXIVE,  4 }, { "ryazo", PRON_REFLEXIVE,  5 },
    { "yazo",  PRON_REFLEXIVE,  6 }, { "cyazo", PRON_REFLEXIVE,  7 },
    { "byazo", PRON_REFLEXIVE,  8 }, { "zazo",  PRON_REFLEXIVE, 10 },
    { "rwazo", PRON_REFLEXIVE, 11 }, { "kazo",  PRON_REFLEXIVE, 12 },
    { "twazo", PRON_REFLEXIVE, 13 }, { "bwazo", PRON_REFLEXIVE, 14 },
    { "kwazo", PRON_REFLEXIVE, 15 }, { "hazo",  PRON_REFLEXIVE, 16 },
    /* Nt.11 class-referent possessives (-rwo): connector + rwo              */
    { "warwo", PRON_REFLEXIVE,  1 }, { "barwo", PRON_REFLEXIVE,  2 },
    { "yarwo", PRON_REFLEXIVE,  4 }, { "ryarwo",PRON_REFLEXIVE,  5 },
    { "yarwo", PRON_REFLEXIVE,  6 }, { "cyarwo",PRON_REFLEXIVE,  7 },
    { "byarwo",PRON_REFLEXIVE,  8 }, { "zarwo", PRON_REFLEXIVE, 10 },
    { "rwarwo",PRON_REFLEXIVE, 11 }, { "karwo", PRON_REFLEXIVE, 12 },
    { "twarwo",PRON_REFLEXIVE, 13 }, { "bwarwo",PRON_REFLEXIVE, 14 },
    { "kwarwo",PRON_REFLEXIVE, 15 }, { "harwo", PRON_REFLEXIVE, 16 },
    /* Nt.12 class-referent possessives (-ko): connector + ko                */
    { "wako",  PRON_REFLEXIVE,  1 }, { "bako",  PRON_REFLEXIVE,  2 },
    { "yako",  PRON_REFLEXIVE,  4 }, { "ryako", PRON_REFLEXIVE,  5 },
    { "yako",  PRON_REFLEXIVE,  6 }, { "cyako", PRON_REFLEXIVE,  7 },
    { "byako", PRON_REFLEXIVE,  8 }, { "zako",  PRON_REFLEXIVE, 10 },
    { "rwako", PRON_REFLEXIVE, 11 }, { "kako",  PRON_REFLEXIVE, 12 },
    { "twako", PRON_REFLEXIVE, 13 }, { "bwako", PRON_REFLEXIVE, 14 },
    { "kwako", PRON_REFLEXIVE, 15 }, { "hako",  PRON_REFLEXIVE, 16 },
    /* Nt.13 class-referent possessives (-two): connector + two              */
    { "watwo", PRON_REFLEXIVE,  1 }, { "batwo", PRON_REFLEXIVE,  2 },
    { "yatwo", PRON_REFLEXIVE,  4 }, { "ryatwo",PRON_REFLEXIVE,  5 },
    { "yatwo", PRON_REFLEXIVE,  6 }, { "cyatwo",PRON_REFLEXIVE,  7 },
    { "byatwo",PRON_REFLEXIVE,  8 }, { "zatwo", PRON_REFLEXIVE, 10 },
    { "rwatwo",PRON_REFLEXIVE, 11 }, { "katwo", PRON_REFLEXIVE, 12 },
    { "twatwo",PRON_REFLEXIVE, 13 }, { "bwatwo",PRON_REFLEXIVE, 14 },
    { "kwatwo",PRON_REFLEXIVE, 15 }, { "hatwo", PRON_REFLEXIVE, 16 },
    /* Nt.14 class-referent possessives (-bwo): connector + bwo              */
    { "wabwo", PRON_REFLEXIVE,  1 }, { "babwo", PRON_REFLEXIVE,  2 },
    { "yabwo", PRON_REFLEXIVE,  4 }, { "ryabwo",PRON_REFLEXIVE,  5 },
    { "yabwo", PRON_REFLEXIVE,  6 }, { "cyabwo",PRON_REFLEXIVE,  7 },
    { "byabwo",PRON_REFLEXIVE,  8 }, { "zabwo", PRON_REFLEXIVE, 10 },
    { "rwabwo",PRON_REFLEXIVE, 11 }, { "kabwo", PRON_REFLEXIVE, 12 },
    { "twabwo",PRON_REFLEXIVE, 13 }, { "bwabwo",PRON_REFLEXIVE, 14 },
    { "kwabwo",PRON_REFLEXIVE, 15 }, { "habwo", PRON_REFLEXIVE, 16 },
    /* Nt.15 class-referent possessives (-kwo): connector + kwo              */
    { "wakwo", PRON_REFLEXIVE,  1 }, { "bakwo", PRON_REFLEXIVE,  2 },
    { "yakwo", PRON_REFLEXIVE,  4 }, { "ryakwo",PRON_REFLEXIVE,  5 },
    { "yakwo", PRON_REFLEXIVE,  6 }, { "cyakwo",PRON_REFLEXIVE,  7 },
    { "byakwo",PRON_REFLEXIVE,  8 }, { "zakwo", PRON_REFLEXIVE, 10 },
    { "rwakwo",PRON_REFLEXIVE, 11 }, { "kakwo", PRON_REFLEXIVE, 12 },
    { "twakwo",PRON_REFLEXIVE, 13 }, { "bwakwo",PRON_REFLEXIVE, 14 },
    { "kwakwo",PRON_REFLEXIVE, 15 }, { "hakwo", PRON_REFLEXIVE, 16 },
    /* Nt.16 class-referent possessives (-ho): connector + ho                *
     * Note: "baho/waho" may overlap with common locative forms; pronoun     *
     * lookup runs before verb analysis so these are preferred as pronouns.  */
    { "waho",  PRON_REFLEXIVE,  1 }, { "yaho",  PRON_REFLEXIVE,  4 },
    { "ryaho", PRON_REFLEXIVE,  5 }, { "yaho",  PRON_REFLEXIVE,  6 },
    { "cyaho", PRON_REFLEXIVE,  7 }, { "byaho", PRON_REFLEXIVE,  8 },
    { "zaho",  PRON_REFLEXIVE, 10 }, { "rwaho", PRON_REFLEXIVE, 11 },
    { "kaho",  PRON_REFLEXIVE, 12 }, { "twaho", PRON_REFLEXIVE, 13 },
    { "bwaho", PRON_REFLEXIVE, 14 }, { "kwaho", PRON_REFLEXIVE, 15 },
    { "haho",  PRON_REFLEXIVE, 16 },
    /* 3rd singular (-e): we, be, rye, ye, cye, bye, ze, rwe, ke, twe, bwe...
     * connector + ye → fusion: rya+ye → rye, cya+ye → cye, etc.         */
    { "we",    PRON_REFLEXIVE,  1 },  /* his/her/its (Nt.1)                */
    { "rye",   PRON_REFLEXIVE,  5 },  /* its (Nt.5) — izina rye            */
    { "cye",   PRON_REFLEXIVE,  7 },  /* its (Nt.7)                        */
    { "bye",   PRON_REFLEXIVE,  8 },  /* their (Nt.8)                      */
    { "ze",    PRON_REFLEXIVE, 10 },  /* their (Nt.10)                     */
    { "rwe",   PRON_REFLEXIVE, 11 },  /* its (Nt.11)                       */
    { "ke",    PRON_REFLEXIVE, 12 },  /* its (Nt.12)                       */
    { "twe",   PRON_REFLEXIVE, 13 },  /* their (Nt.13)                     */
    { "bwe",   PRON_REFLEXIVE, 14 },  /* its (Nt.14)                       */
    { "kwe",   PRON_REFLEXIVE, 15 },  /* its (Nt.15)                       */
    { "he",    PRON_REFLEXIVE, 16 },  /* its (Nt.16)                       */
    /* 2nd plural (-nyu): wanyu, banyu, ryanyu, yanyu, cyanyu... */
    { "wanyu",  PRON_REFLEXIVE,  1 }, { "banyu",  PRON_REFLEXIVE,  2 },
    { "yanyu",  PRON_REFLEXIVE,  4 }, { "ryanyu", PRON_REFLEXIVE,  5 },
    { "yanyu",  PRON_REFLEXIVE,  6 }, { "cyanyu", PRON_REFLEXIVE,  7 },
    { "byanyu", PRON_REFLEXIVE,  8 }, { "zanyu",  PRON_REFLEXIVE, 10 },
    { "rwanyu", PRON_REFLEXIVE, 11 }, { "kanyu",  PRON_REFLEXIVE, 12 },
    { "twanyu", PRON_REFLEXIVE, 13 }, { "bwanyu", PRON_REFLEXIVE, 14 },
    { "kwanyu", PRON_REFLEXIVE, 15 }, { "hanyu",  PRON_REFLEXIVE, 16 },

    /* ── 1sg possessive with emphatic -njye (e.g. ryanjye=mine Nt.5) ─────  */
    { "ryanjye", PRON_REFLEXIVE,  5 },  /* mine (Nt.5) = rya + njye        */
    { "cyanjye", PRON_REFLEXIVE,  7 },  /* mine (Nt.7) = cya + njye        */
    { "byanjye", PRON_REFLEXIVE,  8 },  /* mine (Nt.8) = bya + njye        */
    { "bwanjye", PRON_REFLEXIVE, 14 },  /* mine (Nt.14) = bwa + njye       */
    { "rwanjye", PRON_REFLEXIVE, 11 },  /* mine (Nt.11) = rwa + njye       */
    { "wanjye",  PRON_REFLEXIVE,  1 },  /* mine (Nt.1/3) = wa + njye       */
    { "yanjye",  PRON_REFLEXIVE,  4 },  /* mine (Nt.4/6) = ya + njye       */
    { "kanjye",  PRON_REFLEXIVE, 12 },  /* mine (Nt.12) = ka + njye        */
    { "twanjye", PRON_REFLEXIVE, 13 },  /* mine (Nt.13) = twa + njye       */
    { "hanjye",  PRON_REFLEXIVE, 16 },  /* mine (Nt.16) = ha + njye        */
    /* na + pronoun compounds: "and X / with X" ─────────────────────────── */
    { "nanjye",  PRON_PERSONAL,   0 },  /* and I / with me (emph. 1sg)     */
    { "nawe",    PRON_PERSONAL,   0 },  /* and you / with you (2sg)        */
    { "natwe",   PRON_PERSONAL,   0 },  /* and us / with us (1pl)          */
    { "namwe",   PRON_PERSONAL,   0 },  /* and you / with you (2pl)        */
    { "nabo",    PRON_PERSONAL,   2 },  /* and them / with them (human)    */
    { "nayo",    PRON_PERSONAL,   4 },  /* and them/it (Nt.4/6/9)         */
    { "naryo",   PRON_PERSONAL,   5 },  /* and it (Nt.5)                  */
    { "nacyo",   PRON_PERSONAL,   7 },  /* and it (Nt.7)                  */
    { "nabyo",   PRON_PERSONAL,   8 },  /* and them (Nt.8)                */
    { "nazo",    PRON_PERSONAL,  10 },  /* and them (Nt.10)               */
    { "narwo",   PRON_PERSONAL,  11 },  /* and it (Nt.11)                 */
    { "nako",    PRON_PERSONAL,  12 },  /* and it (Nt.12)                 */
    { "natwo",   PRON_PERSONAL,  13 },  /* and them (Nt.13)               */
    { "nabwo",   PRON_PERSONAL,  14 },  /* and it (Nt.14)                 */
    { "nakwo",   PRON_PERSONAL,  15 },  /* and it (Nt.15)                 */
    { "naho",    PRON_PERSONAL,  16 },  /* and there (Nt.16)              */

    /* ── "nka" + 1st/2nd person pronouns — written together ───────────── *
     * Official Orthography Rules §1.2.2d: "nka" (like/as) joined to 1st  *
     * and 2nd person pronouns forms a single written word.                 *
     * 3rd-person forms (nka bo, nka byo …) are written separately.        */
    { "nkanje",  PRON_PERSONAL,   0 },  /* like/as me (nka + nje/nge)     */
    { "nkange",  PRON_PERSONAL,   0 },  /* like/as me (nka + nge, variant)*/
    { "nkawe",   PRON_PERSONAL,   0 },  /* like/as you (2sg)              */
    { "nkatwe",  PRON_PERSONAL,   0 },  /* like/as us (1pl)               */
    { "nkamwe",  PRON_PERSONAL,   0 },  /* like/as you (2pl)              */

    /* ── Ikinyazina kibaza (interrogative pronouns) ─────────────────────  */
    { "nde",    PRON_INTERROGATIVE, 0 },  /* who?                          */
    { "iki",    PRON_INTERROGATIVE, 0 },  /* what? (thing)                 */
    { "iyihe",  PRON_INTERROGATIVE, 0 },  /* which?                        */
    { "ryari",  PRON_INTERROGATIVE, 0 },  /* when?                         */
    { "hehe",   PRON_INTERROGATIVE, 0 },  /* where?                        */
    { "bite",   PRON_INTERROGATIVE, 0 },  /* how?                          */
    { "bangahe",PRON_INTERROGATIVE, 0 },  /* how many?                     */
    { "kuki",   PRON_INTERROGATIVE, 0 },  /* why?                          */

    /* ── Ikinyazina ndafutura (indefinite pronouns) p.115-116 ──────────  */
    /* Short form (kigufi): undi, indi, andi... */
    { "undi",   PRON_INDEFINITE,  1 }, /* another (cls1/3 sg)               */
    { "indi",   PRON_INDEFINITE,  4 }, /* another (cls4/9 sg)               */
    { "andi",   PRON_INDEFINITE,  6 }, /* other (cls6 pl)                   */
    /* Long form (ikirekire) with indomo: uwundi, iyindi, ayandi... */
    { "uwundi",  PRON_INDEFINITE,  1 }, { "uwundiwundi", PRON_INDEFINITE, 1 },
    { "abandi",  PRON_INDEFINITE,  2 }, { "abandibandi",  PRON_INDEFINITE, 2 },
    { "iyindi",  PRON_INDEFINITE,  4 }, { "iyindiyindi",  PRON_INDEFINITE, 4 },
    { "irindi",  PRON_INDEFINITE,  5 }, { "irindirindi",  PRON_INDEFINITE, 5 },
    { "ayandi",  PRON_INDEFINITE,  6 }, { "ayandiyandi",  PRON_INDEFINITE, 6 },
    { "ikindi",  PRON_INDEFINITE,  7 }, { "ikindikindi",  PRON_INDEFINITE, 7 },
    { "ibindi",  PRON_INDEFINITE,  8 }, { "ibindibindi",  PRON_INDEFINITE, 8 },
    { "izindi",  PRON_INDEFINITE, 10 }, { "izindizindi",  PRON_INDEFINITE,10 },
    { "urundi",  PRON_INDEFINITE, 11 }, { "urundirundi",  PRON_INDEFINITE,11 },
    { "akandi",  PRON_INDEFINITE, 12 }, { "akandikandi",  PRON_INDEFINITE,12 },
    { "utundi",  PRON_INDEFINITE, 13 }, { "utunditundi",  PRON_INDEFINITE,13 },
    { "ubundi",  PRON_INDEFINITE, 14 }, { "ubundibundi",  PRON_INDEFINITE,14 },
    { "ukundi",  PRON_INDEFINITE, 15 }, { "ukundikundi",  PRON_INDEFINITE,15 },
    { "ahandi",  PRON_INDEFINITE, 16 }, { "ahandihandi",  PRON_INDEFINITE,16 },
    /* Single-form indefinites used across classes */
    { "umwe",   PRON_INDEFINITE,  1 }, { "bamwe",  PRON_INDEFINITE,  2 },
    { "kimwe",  PRON_INDEFINITE,  7 }, { "bimwe",  PRON_INDEFINITE,  8 },
    { "rimwe",  PRON_INDEFINITE,  5 }, { "bumwe",  PRON_INDEFINITE, 14 },
    { "rumwe",  PRON_INDEFINITE, 11 }, { "kamwe",  PRON_INDEFINITE, 12 },
    { "tumwe",  PRON_INDEFINITE, 13 }, { "imwe",   PRON_INDEFINITE,  4 },
    { "amwe",   PRON_INDEFINITE,  6 }, { "zimwe",  PRON_INDEFINITE, 10 },

    /* ── Ikinyazina nyamubaro (numerical pronouns) p.119-121 ────────── */
    /* Nt.1/3 singular */
    { "umwe",    PRON_NUMERICAL,  1 },
    /* Nt.2 (human pl) */
    { "babiri",  PRON_NUMERICAL,  2 }, { "batatu",   PRON_NUMERICAL,  2 },
    { "bane",    PRON_NUMERICAL,  2 }, { "batanu",   PRON_NUMERICAL,  2 },
    { "batandatu",PRON_NUMERICAL, 2 }, { "barindwi", PRON_NUMERICAL,  2 },
    { "bamwe",   PRON_NUMERICAL,  2 },
    /* Nt.4 (imiti pl) */
    { "imwe",    PRON_NUMERICAL,  4 }, { "ibiri",    PRON_NUMERICAL,  4 },
    { "itatu",   PRON_NUMERICAL,  4 }, { "ine",      PRON_NUMERICAL,  4 },
    { "itanu",   PRON_NUMERICAL,  4 }, { "itandatu", PRON_NUMERICAL,  4 },
    { "irindwi", PRON_NUMERICAL,  4 },
    /* Nt.5 singular */
    { "rimwe",   PRON_NUMERICAL,  5 },
    /* Nt.6 (ama- pl) */
    { "amwe",    PRON_NUMERICAL,  6 }, { "abiri",    PRON_NUMERICAL,  6 },
    { "atatu",   PRON_NUMERICAL,  6 }, { "ane",      PRON_NUMERICAL,  6 },
    { "atanu",   PRON_NUMERICAL,  6 }, { "atandatu", PRON_NUMERICAL,  6 },
    { "arindwi", PRON_NUMERICAL,  6 },
    /* Nt.7 singular */
    { "kimwe",   PRON_NUMERICAL,  7 },
    /* Nt.8 (ibi- pl) */
    { "bimwe",   PRON_NUMERICAL,  8 }, { "bibiri",   PRON_NUMERICAL,  8 },
    { "bitatu",  PRON_NUMERICAL,  8 }, { "bine",     PRON_NUMERICAL,  8 },
    { "bitanu",  PRON_NUMERICAL,  8 }, { "bitandatu",PRON_NUMERICAL,  8 },
    { "birindwi",PRON_NUMERICAL,  8 },
    /* Nt.9 singular */
    { "imwe",    PRON_NUMERICAL,  9 },
    /* Nt.10 (zi- pl) – special forms: ebyiri/eshatu/enye/eshanu/esheshatu */
    { "zimwe",   PRON_NUMERICAL, 10 }, { "ebyiri",   PRON_NUMERICAL, 10 },
    { "eshatu",  PRON_NUMERICAL, 10 }, { "enye",     PRON_NUMERICAL, 10 },
    { "eshanu",  PRON_NUMERICAL, 10 }, { "esheshatu",PRON_NUMERICAL, 10 },
    { "zirindwi",PRON_NUMERICAL, 10 },
    /* Nt.11 singular ONLY — Nt.11 nouns (uru-) have no plural number forms; */
    /* their plural class is Nt.10, which uses ebyiri/eshatu/zirindwi etc.  */
    { "rumwe",   PRON_NUMERICAL, 11 },
    /* Nt.12 singular ONLY — Nt.12 nouns (aka-) have no plural number forms;*/
    /* their plural class is Nt.13, which uses tubiri/dutatu etc.           */
    { "kamwe",   PRON_NUMERICAL, 12 },
    /* Nt.13 (utu- pl) — note: 3/5/6 use du- prefix (u→voiced before GR)  */
    { "tumwe",   PRON_NUMERICAL, 13 }, { "tubiri",   PRON_NUMERICAL, 13 },
    { "dutatu",  PRON_NUMERICAL, 13 }, { "tune",     PRON_NUMERICAL, 13 },
    { "dutanu",  PRON_NUMERICAL, 13 }, { "dutandatu",PRON_NUMERICAL, 13 },
    { "turindwi",PRON_NUMERICAL, 13 },
    /* Nt.14 (ubu-) */
    { "bumwe",   PRON_NUMERICAL, 14 }, { "bubiri",   PRON_NUMERICAL, 14 },
    { "butatu",  PRON_NUMERICAL, 14 }, { "bune",     PRON_NUMERICAL, 14 },
    { "butanu",  PRON_NUMERICAL, 14 }, { "butandatu",PRON_NUMERICAL, 14 },
    { "burindwi",PRON_NUMERICAL, 14 },
    /* Nt.15 singular ONLY — infinitive/verbal noun class                   */
    { "kumwe",   PRON_NUMERICAL, 15 },
    /* Nt.16 (aha- locative) */
    { "hamwe",   PRON_NUMERICAL, 16 }, { "habiri",   PRON_NUMERICAL, 16 },
    { "hatatu",  PRON_NUMERICAL, 16 }, { "hane",     PRON_NUMERICAL, 16 },
    { "hatanu",  PRON_NUMERICAL, 16 }, { "hatandatu",PRON_NUMERICAL, 16 },
    { "harindwi",PRON_NUMERICAL, 16 },
    /* Plural-form numerical nouns (>7) – these behave like amazina nyamubaro */
    { "icumi",   PRON_NUMERICAL,  0 }, /* ten                               */
    { "ijana",   PRON_NUMERICAL,  0 }, /* hundred                           */
    { "igihumbi",PRON_NUMERICAL,  0 }, /* thousand                          */

    /* ── Ikinyazina mbanziriza (relative pronouns) p.114 ──────────────  */
    /* These carry circumflex accent (isaku nyejuru -ô) vs demonstrative -o */
    /* We normalize without accent; context distinguishes from demonstratives */
    { "uwo",    PRON_RELATIVE,  1 },  /* whom/which (cls1)                  */
    { "abo",    PRON_RELATIVE,  2 },  /* whom/which (cls2)                  */
    { "iyo",    PRON_RELATIVE,  4 },  /* which (cls4/9)                     */
    { "iryo",   PRON_RELATIVE,  5 },  /* which (cls5)                       */
    { "ayo",    PRON_RELATIVE,  6 },  /* which (cls6)                       */
    { "icyo",   PRON_RELATIVE,  7 },  /* which (cls7)                       */
    { "ibyo",   PRON_RELATIVE,  8 },  /* which (cls8)                       */
    { "izo",    PRON_RELATIVE, 10 },  /* which (cls10)                      */
    { "urwo",   PRON_RELATIVE, 11 },  /* which (cls11)                      */
    { "ako",    PRON_RELATIVE, 12 },  /* which (cls12)                      */
    { "utwo",   PRON_RELATIVE, 13 },  /* which (cls13)                      */
    { "ubwo",   PRON_RELATIVE, 14 },  /* which (cls14)                      */
    { "uko",    PRON_RELATIVE, 15 },  /* which (cls15)                      */
    { "aho",    PRON_RELATIVE, 16 },  /* where/which place                  */

    /* ── Ikinyazina kibaza – complete table (p.117-119) ────────────────  */
    /* Igicumbi -he? forms for each class */
    { "wuhe",    PRON_INTERROGATIVE,  1 }, { "uwuhe",   PRON_INTERROGATIVE,  1 },
    { "bahe",    PRON_INTERROGATIVE,  2 }, { "abahe",   PRON_INTERROGATIVE,  2 },
    { "yihe",    PRON_INTERROGATIVE,  4 }, { "iyihe",   PRON_INTERROGATIVE,  4 },
    { "rihe",    PRON_INTERROGATIVE,  5 }, { "irihe",   PRON_INTERROGATIVE,  5 },
    { "yahe",    PRON_INTERROGATIVE,  6 }, { "ayahe",   PRON_INTERROGATIVE,  6 },
    { "kihe",    PRON_INTERROGATIVE,  7 }, { "ikihe",   PRON_INTERROGATIVE,  7 },
    { "bihe",    PRON_INTERROGATIVE,  8 }, { "ibihe",   PRON_INTERROGATIVE,  8 },
    { "zihe",    PRON_INTERROGATIVE, 10 }, { "izihe",   PRON_INTERROGATIVE, 10 },
    { "ruhe",    PRON_INTERROGATIVE, 11 }, { "uruhe",   PRON_INTERROGATIVE, 11 },
    { "kahe",    PRON_INTERROGATIVE, 12 }, { "akahe",   PRON_INTERROGATIVE, 12 },
    { "tuhe",    PRON_INTERROGATIVE, 13 }, { "utuhe",   PRON_INTERROGATIVE, 13 },
    { "buhe",    PRON_INTERROGATIVE, 14 }, { "ubuhe",   PRON_INTERROGATIVE, 14 },
    { "kuhe",    PRON_INTERROGATIVE, 15 }, { "ukuhe",   PRON_INTERROGATIVE, 15 },
    { "hahe",    PRON_INTERROGATIVE, 16 }, { "ahahe",   PRON_INTERROGATIVE, 16 },
    /* Igicumbi -ngahe? (how many?) – plural classes only */
    { "bangahe",  PRON_INTERROGATIVE, 2 }, { "ingahe",   PRON_INTERROGATIVE, 4 },
    { "angahe",   PRON_INTERROGATIVE, 6 }, { "bingahe",  PRON_INTERROGATIVE, 8 },
    { "zingahe",  PRON_INTERROGATIVE,10 }, { "tungahe",  PRON_INTERROGATIVE,13 },
    { "bungahe",  PRON_INTERROGATIVE,14 }, { "hangahe",  PRON_INTERROGATIVE,16 },
    /* Igicumbi -e? (where? with locative) */
    { "he",      PRON_INTERROGATIVE,  0 }, /* where? (locative)              */
    /* Other interrogatives */
    { "nde",     PRON_INTERROGATIVE,  0 }, /* who?                           */
    { "nini",    PRON_INTERROGATIVE,  0 }, /* what? / which?                 */
    { "iki",     PRON_INTERROGATIVE,  0 }, /* what?                          */
    { "ryari",   PRON_INTERROGATIVE,  0 }, /* when?                          */
    { "hehe",    PRON_INTERROGATIVE,  0 }, /* where? (informal)              */
    { "bite",    PRON_INTERROGATIVE,  0 }, /* how?                           */
    { "kuki",    PRON_INTERROGATIVE,  0 }, /* why?                           */
    { "ese",     PRON_INTERROGATIVE,  0 }, /* yes/no question marker         */
    { "mbese",   PRON_INTERROGATIVE,  0 }, /* so? / is it that?              */

    /* ── Pronoun suffixes: -nyine (alone/only), -mbi (both), -se (all) ──  */
    /* (imbonerahamwe p.93-94) these are pronouns with umusuma attached    */
    { "wenyine",   PRON_PERSONAL,  1 }, { "bonyine",   PRON_PERSONAL,  2 },
    { "wonyine",   PRON_PERSONAL,  3 }, { "yonyine",   PRON_PERSONAL,  4 },
    { "ryonyine",  PRON_PERSONAL,  5 }, { "cyonyine",  PRON_PERSONAL,  7 },
    { "byonyine",  PRON_PERSONAL,  8 }, { "zonyine",   PRON_PERSONAL, 10 },
    { "rwonyine",  PRON_PERSONAL, 11 }, { "konyine",   PRON_PERSONAL, 12 },
    { "twonyine",  PRON_PERSONAL, 13 }, { "bwonyine",  PRON_PERSONAL, 14 },
    { "honyine",   PRON_PERSONAL, 16 },
    { "twenyine",  PRON_PERSONAL,  0 }, { "mwenyine",  PRON_PERSONAL,  0 },
    /* -mbi (both) */
    { "yombi",    PRON_PERSONAL,  4 }, { "bombi",    PRON_PERSONAL,  2 },
    { "byombi",   PRON_PERSONAL,  8 }, { "zombi",    PRON_PERSONAL, 10 },
    { "twombi",   PRON_PERSONAL, 13 }, { "bwombi",   PRON_PERSONAL, 14 },
    { "hombi",    PRON_PERSONAL, 16 }, { "mwembi",   PRON_PERSONAL,  0 },
    { "twembi",   PRON_PERSONAL,  0 },
    /* -se (all/every) */
    { "bose",     PRON_PERSONAL,  2 }, { "yose",     PRON_PERSONAL,  4 },
    { "cyose",    PRON_PERSONAL,  7 }, { "byose",    PRON_PERSONAL,  8 },
    { "zose",     PRON_PERSONAL, 10 }, { "rwose",    PRON_PERSONAL, 11 },
    { "kose",     PRON_PERSONAL, 12 }, { "twose",    PRON_PERSONAL, 13 },
    { "bwose",    PRON_PERSONAL, 14 }, { "hose",     PRON_PERSONAL, 16 },
    { "wose",     PRON_PERSONAL,  3 }, { "ryose",    PRON_PERSONAL,  5 },
    { "mwese",    PRON_PERSONAL,  0 }, { "twese",    PRON_PERSONAL,  0 },

    /* ── Personal pronoun: emphatic 1sg (jye = I/me emphatic) ──────────── */
    { "jye",    PRON_PERSONAL,  0 },  /* emphatic I / me (also: nanjye)     */
    { "jyeho",  PRON_PERSONAL,  0 },  /* emphatic I / me (jye + locative)   */
    { "jyewe",  PRON_PERSONAL,  0 },  /* emphatic I myself (stronger form)  */
    { "jyeweho",PRON_PERSONAL,  0 },  /* I myself (jyewe + locative -ho)    */

    /* ── Wese: all / every (indefinite, all classes) ────────────────────── */
    { "wese",  PRON_INDEFINITE,  0 },  /* all / every / everyone            */

    /* ── Indefinite: another/other ──────────────────────────────────────── */
    /* -ndi (another/other) short forms: match the concordance prefix        *
     * Nt.1/3: wundi (u→w before vowel: u+ndi); Nt.2: bandi; Nt.4: yindi;  *
     * Nt.5: rindi; Nt.6: andi; Nt.7: kindi; Nt.8: bindi; Nt.9/10: zindi;  *
     * Nt.11: rundi; Nt.12: kandi; Nt.13: tundi; Nt.14: bundi; Nt.15: kundi*/
    { "wundi",  PRON_INDEFINITE,  1 },  /* another (Nt.1/3: u→w, umuntu wundi)*/
    /* "yundi" does NOT exist – removed (native speaker correction)         */
    { "rundi",  PRON_INDEFINITE, 11 },  /* another (Nt.11: urugendo rundi)  */
    { "kundi",  PRON_INDEFINITE, 15 },  /* another (Nt.15)                  */
    { "kindi",  PRON_INDEFINITE,  7 },  /* another thing (Nt.7: ikintu kindi)*/
    { "bundi",  PRON_INDEFINITE, 14 },  /* another (Nt.14: ubundi)          */
    { "bandi",  PRON_INDEFINITE,  2 },  /* others (Nt.2: abantu bandi)      */
    { "yindi",  PRON_INDEFINITE,  4 },  /* other (Nt.4: imiti yindi)        */
    { "zindi",  PRON_INDEFINITE, 10 },  /* other (Nt.10: inka zindi)        */
    { "bindi",  PRON_INDEFINITE,  8 },  /* other (Nt.8: ibintu bindi)       */
    { "rindi",  PRON_INDEFINITE,  5 },  /* another (Nt.5: ijambo rindi)     */
    { "andi",   PRON_INDEFINITE,  6 },  /* another (Nt.6: amabuye andi)     */
    { "kandi",  PRON_INDEFINITE, 12 },  /* another (Nt.12: akana kandi) — also conj*/
    { "tundi",  PRON_INDEFINITE, 13 },  /* another (Nt.13: utugabo tundi)   */

    /* ── Apostrophe-elided possessive connectors (2-char fragments) ──────  */
    /* These appear when possessive connector + apostrophe elides before     */
    /* a vowel-initial word: ry'amazi=rya amazi, cy'igihe=cya igihe etc.   */
    { "ry",    PRON_POSSESSIVE,  5 },  /* truncated rya (Nt.5)              */
    { "cy",    PRON_POSSESSIVE,  7 },  /* truncated cya (Nt.7)              */
    { "by",    PRON_POSSESSIVE,  8 },  /* truncated bya (Nt.8)              */
    { "bw",    PRON_POSSESSIVE, 14 },  /* truncated bwa (Nt.14)             */
    { "rw",    PRON_POSSESSIVE, 11 },  /* truncated rwa (Nt.11)             */
    { "tw",    PRON_POSSESSIVE, 13 },  /* truncated twa (Nt.13)             */
    { "my",    PRON_POSSESSIVE,  4 },  /* truncated mya (Nt.4)              */

    /* ── Apostrophe-elided demonstratives ───────────────────────────────── */
    { "iby",   PRON_DEMONSTRATIVE,  8 },  /* truncated ibyo (Nt.8)          */
    { "ab",    PRON_DEMONSTRATIVE,  2 },  /* truncated abo (Nt.2)           */

    /* -bwe (reflexive-intensive: yourself, himself) */
    { "wowe",     PRON_PERSONAL,  0 }, /* you yourself                       */
    { "twebwe",   PRON_PERSONAL,  0 }, /* we ourselves                       */
    { "mwebwe",   PRON_PERSONAL,  0 }, /* you yourselves                     */

    /* ── Ikinyazina mpamagazi (vocative pronouns) ─────────────────────  */
    { "wa",      PRON_VOCATIVE,  0 }, /* O! (to a person)                   */

    { NULL, PRON_NONE, 0 }
};

bool kin_is_pronoun(const char *word, PronounType *type_out, int *class_out) {
    for (int i = 0; PRONOUNS[i].word; i++) {
        if (strcmp(word, PRONOUNS[i].word) == 0) {
            if (type_out)  *type_out  = PRONOUNS[i].type;
            if (class_out) *class_out = PRONOUNS[i].class;
            return true;
        }
    }
    return false;
}

/* ══════════════════════════════════════════════════════════════════════════
 * TREE 5 — AMAGAMBO ADAHINDUKA (Invariable Words)
 * Section 4: INVARIABLES table   Source: REB S4 p.89 + corpus extension
 *
 * These words never change form regardless of context.
 * Checked at POS priority Step 1 — BEFORE all morphological analysis.
 *
 * Sub-categories (POS values in this table):
 *   POS_LOCATIVE       Indangahantu            ku, mu, i, kuri, muri, kwa, hasi, hano...
 *   POS_PREPOSITION    Umugereka               nka, bwa, nyiri (comparison/ownership/time)
 *   POS_CONJUNCTION    Icyungo                 na, kandi, ariko, rero, nuko...
 *   POS_ADVERB         Akamamo                 cyane, neza, gato, kenshi...
 *   POS_INTERJECTION   Irangamutima            yee, ahaa, asyi, wee...
 *   POS_VERB_PARTICLE  Ikegeranshinga          ngo, ko, dore, ngwino...
 *   POS_VERB_CONJ      Frozen/suppletive forms ni, si, ndi, ati, bati...
 *
 * Note: Elided possessive connectors (cy, ry, bw...) are in PRONOUNS table
 * but also duplicated here to catch apostrophe-stripped forms early.
 * ══════════════════════════════════════════════════════════════════════════ */
typedef struct { const char *word; POS pos; } InvEntry;

static const InvEntry INVARIABLES[] = {
    /* ══ Indangahantu (Locative markers – Nt.17/18/19 class prefixes) ══════ */
    /* These are NOT prepositions; they are locative class markers:           *
     *   ku = Nt.17 (surfaces, destinations)                                  *
     *   mu = Nt.18 (interiors, enclosed spaces)                              *
     *   i  = Nt.19 (proper place names)                                      *
     * kuri/muri = their forms before pronouns; kwa = ku+wa (someone's place) *
     * ava/kuva  = ablative/source marker ("from")                            */
    { "mu",      POS_LOCATIVE },     /* in / at (Nt.18 locative)            */
    { "ku",      POS_LOCATIVE },     /* on / at / to (Nt.17 locative)       */
    { "i",       POS_LOCATIVE },     /* at / in (Nt.19 – proper nouns)      */
    { "kuri",    POS_LOCATIVE },     /* to / towards (ku before pronouns)   */
    { "muri",    POS_LOCATIVE },     /* in / among (mu before pronouns)     */
    { "kwa",     POS_LOCATIVE },     /* at / to someone's place (ku+wa)     */
    { "ava",     POS_LOCATIVE },     /* from (ablative, contraction of kuva)*/
    { "kuva",    POS_LOCATIVE },     /* from / since (ablative source)      */

    /* ══ Umugereka (True Prepositions – relationship/comparison markers) ════ */
    /* These establish non-locative relations: comparison, ownership, time.   */
    { "nka",     POS_PREPOSITION },  /* like / as (ingereranya – comparison)*/
    { "bwa",     POS_PREPOSITION },  /* at the time of (temporal)           */
    { "nyiri",   POS_PREPOSITION },  /* owner of / possessor (nyiri inzu)   */

    /* ══ Icyungo (conjunctions) ════════════════════════════════════════════ */
    /* Ibyungo ngombwa (necessary – remove changes meaning) */
    { "na",      POS_CONJUNCTION },  /* and / with                         */
    { "nka",     POS_CONJUNCTION },  /* like / as (also ingereranya)       */
    { "cyangwa", POS_CONJUNCTION },  /* or                                 */
    { "yuko",    POS_CONJUNCTION },  /* that (factual complement) – inyumane*/
    { "kuko",    POS_CONJUNCTION },  /* because – inyumane                 */
    /* Ibyungo ntagombwa (removable without changing meaning) */
    { "kandi",   POS_CONJUNCTION },  /* and also / moreover                */
    { "naho",    POS_CONJUNCTION },  /* whereas / while / even if          */
    { "ariko",   POS_CONJUNCTION },  /* but / however                      */
    { "nyamara", POS_CONJUNCTION },  /* however / yet                      */
    { "keretse", POS_CONJUNCTION },  /* except / unless                    */
    { "kuko",    POS_CONJUNCTION },  /* because                            */
    { "kubera",  POS_CONJUNCTION },  /* because of                         */
    { "none",    POS_CONJUNCTION },  /* now / then / so                    */
    { "maze",    POS_CONJUNCTION },  /* and then (sequence)                */
    { "erega",   POS_CONJUNCTION },  /* indeed / well then / you see       */
    { "icyo",    POS_CONJUNCTION },  /* that is why (icyo bikora...)       */
    { "bityo",   POS_CONJUNCTION },  /* thus / in that way                 */
    { "nanone",  POS_CONJUNCTION },  /* also / and also                    */
    { "ndetse",  POS_CONJUNCTION },  /* even / furthermore                 */
    { "rero",    POS_CONJUNCTION },  /* therefore / so                     */
    { "cyakora", POS_CONJUNCTION },  /* however / but                      */
    { "nubwo",   POS_CONJUNCTION },  /* although / even though             */
    { "n'iyo",   POS_CONJUNCTION },  /* even if / although                 */
    { "niyo",    POS_CONJUNCTION },  /* even if / although                 */
    /* NOTE: "kugira ngo" (in order to) is a conjunction phrase, but "kugira"
     * alone is a verb infinitive (ku+gir+a = to have/do/be).  Removing it
     * from the invariables table lets the POS tagger handle it as INF.      */
    /* { "kugira",  POS_CONJUNCTION }, */ /* moved to phrase-level handling  */
    { "ngo",     POS_VERB_PARTICLE },/* that / in order to (ikegeranshinga)*/
    { "ko",      POS_VERB_PARTICLE },/* that (complementizer)              */
    { "no",      POS_CONJUNCTION  }, /* and also / plus (na + o fusion)    */
    { "bene",    POS_ADVERB       }, /* own / themselves (intensifier)     */
    { "gituma",  POS_CONJUNCTION  }, /* that is why / the reason being     */
    { "niba",    POS_CONJUNCTION  }, /* if / whether / in case (conditional)*/
    { "nibyo",   POS_CONJUNCTION  }, /* if that is so / if it is true       */

    /* ══ Copula (inshinga nkene) ════════════════════════════════════════════ */
    /* ni/si are equative verbs (copulas); POS_VERB_CONJ makes has_verb work */
    { "ni",      POS_VERB_CONJ   }, /* copula / is (equative verb)         */
    { "si",      POS_VERB_CONJ   }, /* negative copula: is not             */
    /* Conjugated copula forms of kuba */
    /* ── Copula (inshinga nkene) plain forms ─────────────────────────────  *
     * Most SP+ri forms are now handled by verb_match_inner Pattern D/E,     *
     * which correctly fills tense (COPULA_PAST/PRES), class and stem "b".  *
     * Only forms that cannot be reached by verb_match_inner stay here:      *
     *   ndi  – 1sg SP with no inner content (len==plen, no room for "ri")  *
     *   tari / zwari – SPs not in the SP table; keep as fallback            *
     *   atari / ntari – negative forms that interact with nt- negation      */
    { "ndi",     POS_VERB_CONJ   }, /* 1sg copula: I am                    */
    { "tari",    POS_VERB_CONJ   }, /* 1pl? past copula (exceptional SP)   */
    { "zwari",   POS_VERB_CONJ   }, /* nt.10 past copula (exceptional SP)  */
    { "atari",   POS_VERB_CONJ   }, /* nt.1  neg past copula: he was not   */
    { "ntari",   POS_VERB_CONJ   }, /* 1sg   neg past copula: I was not    */
    { "amen",    POS_INTERJECTION}, /* Amen (Hebrew loanword, invariable)   */
    /* ── "kuzizi" (to know) conjugated forms ─────────────────────────────  */
    /* These end in -i (not standard -a/-e), caught here as special forms   */
    { "nzi",     POS_VERB_CONJ   }, /* 1sg: I know                          */
    { "uzi",     POS_VERB_CONJ   }, /* 2sg: you know                        */
    { "bazi",    POS_VERB_CONJ   }, /* nt.2: they know                      */
    { "tuzi",    POS_VERB_CONJ   }, /* 1pl: we know                         */
    { "sinzi",   POS_VERB_CONJ   }, /* 1sg neg kuzizi: I don't know         */
    /* "muzi" (2pl: you know) OMITTED — conflicts with umuzi (homestead) in KNOWN_WORDS */
    /* ── Short verb forms too small for morphological detection ──────────  */
    { "mpa",     POS_VERB_CONJ   }, /* 1sg: I give (guha, 1sg n→m before p) */
    { "mbe",     POS_VERB_CONJ   }, /* 1sg subj/request: let me (be)        */
    { "be",      POS_VERB_CONJ   }, /* nt.2 subj: let them be               */
    /* ── Common adverbs not yet in the table ──────────────────────────────  */
    { "nyakuri",  POS_ADVERB     }, /* truly / certainly / indeed           */
    { "kongera",  POS_ADVERB     }, /* again / also / furthermore           */
    { "utari",    POS_VERB_CONJ  }, /* nt.3/2sg neg copula: you/it was not  */
    { "uwari",    POS_VERB_CONJ  }, /* relative copula: the one who was     */
    { "ne",       POS_CONJUNCTION}, /* and (variant of 'na' before vowel)   */
    /* ── Negative forms with inner verbs ending in 'i' ───────────────────  */
    { "ntuzi",    POS_VERB_CONJ  }, /* nt.1sg neg: we don't know (kuzizi)   */
    { "ntiyari",  POS_VERB_CONJ  }, /* neg copula Nt.1 past: was not        */
    { "sinzongera",POS_VERB_CONJ }, /* neg: will not add again (si+zongera) */
    { "ntabwo",  POS_ADVERB      }, /* not at all / absolutely not         */
    { "ntiho",   POS_VERB_CONJ   }, /* there was not (locative neg)        */
    /* ── "akiri" copula compounds ─────────────────────────────────────────  *
     * "akiri" = a + kiri = "he/she is still". Used in age clauses, etc.    */
    { "akiri",   POS_VERB_CONJ   }, /* Nt.1/3 copula: (s)he is still        */
    { "ukiri",   POS_VERB_CONJ   }, /* 2sg copula: you are still            */
    { "tukiri",  POS_VERB_CONJ   }, /* 1pl copula: we are still             */
    { "bakiri",  POS_VERB_CONJ   }, /* Nt.2 copula: they are still          */
    { "bikiri",  POS_VERB_CONJ   }, /* Nt.8 copula: they (things) are still */
    { "rukiri",  POS_VERB_CONJ   }, /* Nt.11 copula: it (long) is still     */
    /* ── kuzizwa (passive of kuzizi): to be known ─────────────────────────  *
     * "-zwi" forms = contracted passive present of kuzizi                  */
    { "zwi",     POS_VERB_CONJ   }, /* known (passive abs. form)            */
    { "azwi",    POS_VERB_CONJ   }, /* 3sg: it/he is known                  */
    { "bazwi",   POS_VERB_CONJ   }, /* 3pl: they are known                  */
    { "arazwi",  POS_VERB_CONJ   }, /* 3sg present prog: he is known        */
    { "ntazwi",  POS_VERB_CONJ   }, /* negative: not known                  */
    { "atazi",   POS_VERB_CONJ   }, /* 3sg neg: he does not know            */
    { "atabizi", POS_VERB_CONJ   }, /* 3sg neg with OM: he doesn't know them*/
    { "atanazwi",POS_VERB_CONJ   }, /* neg past: was not known              */
    { "awuzi",   POS_VERB_CONJ   }, /* 3sg + OM: he knows it (Nt.3)         */
    /* ── Adverb compounds with 'a' augment ────────────────────────────────  */
    { "akenshi", POS_ADVERB      }, /* often / frequently (a + kenshi)      */
    { "ahenshi", POS_ADVERB      }, /* sometimes / often (variant)          */
    { "amenshi", POS_ADVERB      }, /* often (wa + kenshi variant?)         */
    /* ── Locative adverbs: va + aho/ayo (from there) ─────────────────────  */
    { "vaho",    POS_ADVERB      }, /* from there (va + aho, ablative)       */
    { "vayo",    POS_ADVERB      }, /* from there/it (va + ayo, Nt.6)        */
    /* ── Compound time adverbs ────────────────────────────────────────────  *
     * Official Orthography Rules §1.2.2k: words indicating time or parts of *
     * the day are written as a single word.                                  */
    { "ejobundi", POS_ADVERB     }, /* the day before/after yesterday        */
    { "ejuru",    POS_ADVERB     }, /* a long time ago / way back            */
    { "nijoro",   POS_ADVERB     }, /* at night / tonight                    */
    { "ninjoro",  POS_ADVERB     }, /* at night (variant of nijoro)          */
    { "nimunsi",  POS_ADVERB     }, /* during the day / by day               */
    { "nimugoroba",POS_ADVERB    }, /* in the evening / this evening         */

    /* ══ Quotative & complementizer particles ══════════════════════════════ */
    { "iti",     POS_VERB_PARTICLE}, /* quotative: (s)he said "..."        */
    { "ati",     POS_VERB_PARTICLE}, /* quotative: he/she said "..." (cls1)*/
    { "bati",    POS_VERB_CONJ   }, /* suppletive: they said (ba+ti)       */
    { "uti",     POS_VERB_CONJ   }, /* suppletive: you said (u+ti)         */
    { "nti",     POS_VERB_CONJ   }, /* suppletive: I say/said (n+ti)       */
    { "yati",    POS_VERB_CONJ   }, /* suppletive: he/she said (ya+ti)     */
    { "mwati",   POS_VERB_CONJ   }, /* suppletive: you-pl said (mwa+ti)    */
    { "twati",   POS_VERB_CONJ   }, /* suppletive: we said (twa+ti)        */
    { "rwati",   POS_VERB_CONJ   }, /* suppletive: Nt.11 said (rwa+ti)     */
    /* ── Complete -ti quotative paradigm (S6 textbook Ingirwanshinga) ──── */
    /* Classes missing from the table above (p.141 of Kinyarwanda S6 SB)   */
    { "kuti",    POS_VERB_PARTICLE}, /* Nt.15 quotative: ku+ti (also manner)*/
    { "ruti",    POS_VERB_PARTICLE}, /* Nt.11 quotative: ru+ti              */
    { "hati",    POS_VERB_PARTICLE}, /* Nt.16 quotative: ha+ti              */
    { "kiti",    POS_VERB_PARTICLE}, /* Nt.7  quotative: ki+ti              */
    { "tuti",    POS_VERB_PARTICLE}, /* Nt.13 quotative: tu+ti              */
    { "buti",    POS_VERB_PARTICLE}, /* Nt.14 quotative: bu+ti              */
    { "ziti",    POS_VERB_PARTICLE}, /* Nt.10 quotative: zi+ti              */

    /* ══ Elided forms (before apostrophe) ══════════════════════════════════ */
    { "n",       POS_CONJUNCTION  }, /* elided 'na' before vowel           */
    { "y",       POS_CONJUNCTION  }, /* elided 'ya' possessive connector   */
    { "k",       POS_LOCATIVE     }, /* elided 'ku' before vowel           */
    { "b",       POS_CONJUNCTION  }, /* elided 'ba' before vowel           */
    { "w",       POS_CONJUNCTION  }, /* elided 'wa' before vowel           */
    { "r",       POS_CONJUNCTION  }, /* elided 'rya' before vowel          */
    { "c",       POS_CONJUNCTION  }, /* elided 'cya' before vowel          */
    { "nk",      POS_PREPOSITION  }, /* elided 'nka' (like/as) before vowel*/
    { "cy",      POS_CONJUNCTION  }, /* elided 'cya' Nt.7 possessive       */
    { "ry",      POS_CONJUNCTION  }, /* elided 'rya' Nt.5 possessive       */
    { "bw",      POS_CONJUNCTION  }, /* elided 'bwa' Nt.14 possessive      */
    { "rw",      POS_CONJUNCTION  }, /* elided 'rwa' Nt.11 possessive      */
    { "by",      POS_CONJUNCTION  }, /* elided 'bya' Nt.8 possessive       */
    { "tw",      POS_CONJUNCTION  }, /* elided 'twa' Nt.13 possessive      */
    { "my",      POS_CONJUNCTION  }, /* elided 'mya' Nt.4 possessive       */
    { "iby",     POS_CONJUNCTION  }, /* elided 'ibyo/ibya' Nt.8 demonstr.  */
    { "icy",     POS_CONJUNCTION  }, /* elided 'icya' Nt.7 possessive/rel. */
    { "ab",      POS_CONJUNCTION  }, /* elided 'abo' Nt.2 demonstrative    */
    { "abe",     POS_CONJUNCTION  }, /* elided 'abe' Nt.2 / let them be    */
    { "kw",      POS_LOCATIVE     }, /* elided 'kwa' before vowel           */
    { "uw",      POS_PRONOUN      }, /* elided 'uwa' Nt.1 possessive        */
    { "iy",      POS_PRONOUN      }, /* elided 'iya' Nt.4 possessive        */
    { "ubw",     POS_PRONOUN      }, /* elided 'ubwa' Nt.14 possessive      */
    { "mwa",     POS_CONJUNCTION  }, /* elided 'mwa' Nt.1 past SP fragment  */
    { "urw",     POS_PRONOUN      }, /* elided 'urwa' Nt.11 possessive      */

    /* ══ Irangamutima (interjections) – p.103-104 ══════════════════════════ */
    /* Kwemeza (affirm) */
    { "yee",     POS_INTERJECTION },
    { "yego",    POS_ADVERB       }, /* yes                                */
    { "oya",     POS_ADVERB       }, /* no                                 */
    /* Gutangara (surprise / amazement) */
    { "ahahaa",  POS_INTERJECTION }, /* amazement!                         */
    { "ahaa",    POS_INTERJECTION }, /* surprise!                          */
    { "ah",      POS_INTERJECTION },
    { "aye",     POS_INTERJECTION }, /* well / so                          */
    { "eeh",     POS_INTERJECTION },
    { "hee",     POS_INTERJECTION },
    { "ooh",     POS_INTERJECTION },
    { "asyi",    POS_INTERJECTION }, /* wow / unbelievable!                */
    { "ashwi",   POS_INTERJECTION }, /* expression of frustration/dismissal*/
    { "yooo",    POS_INTERJECTION }, /* wow / oh no!                       */
    { "yuuu",    POS_INTERJECTION }, /* expression of distress / doubt     */
    { "ayi",     POS_INTERJECTION }, /* no! / stop! / surprise             */
    { "apu",     POS_INTERJECTION }, /* disgust / contempt                 */
    /* Kubabara (grief) */
    { "ararara",  POS_INTERJECTION}, /* grief / lamentation                */
    { "orororooo",POS_INTERJECTION}, /* grief / crying out                 */
    { "ahiii",   POS_INTERJECTION }, /* pain / sorrow                      */
    { "ahwiii",  POS_INTERJECTION }, /* relief / exhaustion                */
    { "ahuuu",   POS_INTERJECTION }, /* relief                             */
    /* Gushima/gucecekesha/kwikanga */
    { "ashyiii", POS_INTERJECTION }, /* admiration                         */
    { "shiii",   POS_INTERJECTION }, /* silence! / shhh!                   */
    { "yebaba",  POS_INTERJECTION }, /* surprise / exclamation             */
    { "dore",    POS_INTERJECTION }, /* look! / behold! (also ikegeranshinga)*/
    { "pyo",     POS_INTERJECTION }, /* thief/swift movement sound         */
    { "bwe",     POS_INTERJECTION }, /* sound of hitting                   */
    /* Generic exclamatives */
    { "wee",     POS_INTERJECTION },
    /* "we" removed here — it is the emphatic 2sg personal pronoun and is
     * correctly listed in the pronouns table (PRON_PERSONAL).  Keeping it
     * here as INTERJECTION caused step-1 to shadow the pronouns check.    */
    { "yewe",    POS_INTERJECTION }, /* hey! (attention – informal)        */
    { "yewe",    POS_ADVERB       },

    /* ══ Akamamo particles (p.102-103) ═════════════════════════════════════ */
    /* Note: da=male, ma=female – follows commands/suggestions              */
    { "da",      POS_ADVERB       }, /* (male) come on / please / relax    */
    { "ma",      POS_ADVERB       }, /* (female) come on / please          */
    { "ga",      POS_ADVERB       }, /* emphasis / go ahead                */
    { "ye",      POS_ADVERB       }, /* listen / you hear?                 */
    { "ra",      POS_ADVERB       }, /* still / already (emphasis)         */
    { "re",      POS_ADVERB       }, /* emphasis particle (dore re)        */
    { "sha",     POS_ADVERB       }, /* come on / already                  */
    { "ngo",     POS_ADVERB       }, /* they say / apparently (hearsay)    */

    /* ══ Ikegeranshinga (particles / imperatives) – p.101-102 ══════════════ */
    { "cyono",   POS_VERB_PARTICLE}, /* come! / let's go (attention)       */
    { "ngwino",  POS_VERB_PARTICLE}, /* come here!                         */
    { "ngaho",   POS_VERB_PARTICLE}, /* go on / let's go (directional)     */
    { "mpano",   POS_VERB_PARTICLE}, /* bring here!                        */
    { "enda",    POS_VERB_PARTICLE}, /* go ahead                           */
    { "mbiswa",  POS_VERB_PARTICLE}, /* let me be shown / help me          */
    { "hinga",   POS_VERB_PARTICLE}, /* come on (motivation)               */
    { "have",    POS_VERB_PARTICLE}, /* come away from there               */
    { "hoshi",   POS_VERB_PARTICLE}, /* come out!                          */
    { "gira",    POS_VERB_PARTICLE}, /* take / do (imperative)             */
    { "ishi",    POS_VERB_PARTICLE}, /* here / take this                   */

    /* ══ Negative particles ═════════════════════════════════════════════════ */
    { "nta",     POS_ADVERB      }, /* there is no / none (nta muntu)     */
    { "ntacyo",  POS_ADVERB      }, /* nothing / it doesn't matter         */
    { "ntaho",   POS_ADVERB      }, /* nowhere                             */
    { "nta na",  POS_ADVERB      }, /* not even                            */

    /* ══ Umugereka w'igihe (time adverbs) ══════════════════════════════════ */
    { "ubu",        POS_ADVERB }, /* now / currently                       */
    { "none",       POS_ADVERB }, /* now / currently / then                */
    { "ejo",        POS_ADVERB }, /* yesterday / tomorrow                  */
    { "ejo hashize",POS_ADVERB }, /* yesterday                             */
    { "kera",       POS_ADVERB }, /* long ago / in the old days            */
    { "rimwe",      POS_ADVERB }, /* sometimes / once (also numerical)     */
    { "buri gihe",  POS_ADVERB }, /* always / every time                   */
    { "ntibigera",  POS_ADVERB }, /* never                                 */
    { "ryari",      POS_ADVERB }, /* when? (as time adverb)                */
    { "hanyuma",    POS_ADVERB }, /* afterwards / then                     */
    { "mbere",      POS_ADVERB }, /* first / before                        */
    { "bukeye",     POS_ADVERB }, /* the next day / the following day      */
    { "uyu munsi",  POS_ADVERB }, /* today                                 */
    { "vuba",       POS_ADVERB }, /* soon / quickly                        */
    { "noneho",     POS_ADVERB }, /* right now / just now                  */
    { "naho",       POS_ADVERB }, /* even / yet / still (also conjunction) */

    /* ══ Umugereka w'uburyo (manner adverbs) ═══════════════════════════════ */
    { "cyane",      POS_ADVERB }, /* very much / a lot                     */
    { "gato",       POS_ADVERB }, /* a little / slightly                   */
    { "neza",       POS_ADVERB }, /* well / nicely / properly              */
    { "guhoro",     POS_ADVERB }, /* slowly / gently                       */
    { "buhorobuhoro",POS_ADVERB}, /* slowly / gradually                    */
    { "buhoro",     POS_ADVERB }, /* slowly (short form)                   */
    { "gihoro",     POS_ADVERB }, /* quietly / calmly                      */
    { "bucece",     POS_ADVERB }, /* silently / quietly                    */
    { "hafi",       POS_ADVERB }, /* nearly / almost                       */
    { "cyane cyane",POS_ADVERB }, /* especially / particularly             */
    { "cyanecyane", POS_ADVERB }, /* especially                            */
    { "gusa",       POS_ADVERB }, /* only / just                           */
    { "rwose",      POS_ADVERB }, /* completely / totally / really         */
    { "bidasanzwe", POS_ADVERB }, /* unusually / surprisingly              */
    { "hamwe",      POS_ADVERB }, /* together / in the same place          */
    { "ubwira",     POS_ADVERB }, /* alone / by oneself                    */
    { "ubwabyo",    POS_ADVERB }, /* by itself / automatically             */
    { "kenshi",     POS_ADVERB }, /* often / frequently                    */
    { "inshuro",    POS_ADVERB }, /* times / frequency                     */
    { "gatatu",     POS_ADVERB }, /* three times / thirdly                 */
    { "incuro",     POS_ADVERB }, /* times / repetitions                   */

    /* ══ Umugereka w'ahantu (place adverbs / indangahantu) ═════════════════ */
    { "hasi",    POS_LOCATIVE }, /* below / down / on the ground           */
    { "hano",    POS_LOCATIVE }, /* here / right here                      */
    { "aho",     POS_LOCATIVE }, /* there (relative / general)             */
    { "hariya",  POS_LOCATIVE }, /* over there (distant)                   */
    { "hejuru",  POS_LOCATIVE }, /* above / up there                       */
    { "haruguru",POS_LOCATIVE }, /* above / up there (formal/biblical)     */
    { "hepfo",   POS_LOCATIVE }, /* below / southward                      */
    { "munsi",   POS_LOCATIVE }, /* below / down                           */
    { "hanze",   POS_LOCATIVE }, /* outside                                */
    { "imbere",  POS_LOCATIVE }, /* in front / forward                     */
    { "inyuma",  POS_LOCATIVE }, /* behind / backward                      */
    { "hagati",  POS_LOCATIVE }, /* in the middle / between                */
    { "hafi",    POS_LOCATIVE }, /* near / close by                        */
    { "hose",    POS_LOCATIVE }, /* everywhere / all over                  */
    { "hahandi", POS_LOCATIVE }, /* elsewhere / somewhere else             */
    { "epfo",    POS_LOCATIVE }, /* in the south / below (directional)     */
    { "ruguru",  POS_LOCATIVE }, /* in the north / above (directional)     */
    { "hirya",   POS_LOCATIVE }, /* over there / on that side              */
    { "hino",    POS_LOCATIVE }, /* on this side / here                    */
    { "iruhande", POS_LOCATIVE}, /* to the side / sideways                 */
    { "hambavu",  POS_LOCATIVE}, /* beside / next to                       */

    /* ══ Other common invariables ═══════════════════════════════════════════ */
    { "koko",       POS_ADVERB }, /* indeed / truly / really               */
    { "buri",       POS_ADVERB }, /* every / each                          */
    { "se",         POS_ADVERB }, /* father / (question tag)               */
    { "yego",       POS_ADVERB }, /* yes / indeed                          */
    { "oya",        POS_ADVERB }, /* no / not                              */
    { "mbese",      POS_ADVERB }, /* is it that / so / therefore           */
    { "gake",       POS_ADVERB }, /* rarely / seldom / few (akamamo)       */
    { "kenshi",     POS_ADVERB }, /* often / frequently                    */
    { "make",       POS_ADVERB }, /* little / few / a bit                  */
    { "cyane",      POS_ADVERB }, /* very / much / a lot                   */
    { "hanyuma",    POS_ADVERB }, /* then / afterwards / later             */
    { "nanone",     POS_ADVERB }, /* also / again / moreover               */
    { "ariko",      POS_CONJUNCTION }, /* but / however / yet               */
    /* ── Interrogative adverbs (adverbes interrogatifs) ─────────────────  */
    { "gute",       POS_ADVERB }, /* how? / in what way?                   */
    { "gutyo",      POS_ADVERB }, /* like that / in that manner            */
    { "gutya",      POS_ADVERB }, /* like this / in this manner            */
    { "guhe",       POS_ADVERB }, /* which / where (used in "ni guhe")     */
    { "handi",      POS_LOCATIVE}, /* elsewhere / somewhere else           */
    { "kati",       POS_ADVERB }, /* just / at that moment / right now     */
    /* ── Additional copula compounds ─────────────────────────────────────  */
    { "hakiri",     POS_VERB_CONJ }, /* Nt.16: there is still / it still is*/
    { "hazwi",      POS_VERB_CONJ }, /* Nt.16: it is known (locative)      */
    { "hatari",     POS_VERB_CONJ }, /* there is not (negative locative)   */

    /* ── High-frequency narrative & discourse particles (corpus-derived) ──  *
     * These were previously misanalysed as conjugated verbs because their    *
     * surface form accidentally matches an SP + tense + stem pattern.        *
     * Adding them here ensures they are caught at step 1 (invariables),      *
     * well before the verb heuristic.                                        */
    { "nuko",    POS_CONJUNCTION }, /* narrative connector: "so / and then / *
                                     * it came to pass that" – the most       *
                                     * common sentence-initial particle in     *
                                     * the Bible corpus (~2954 occurrences).  *
                                     * Previously parsed as SP "nu" + stem    *
                                     * "k" + FV "a" (wrong).                 */
    { "ahubwo",  POS_ADVERB     }, /* contrastive adverb: "rather / instead / *
                                     * on the contrary" (~1024 occurrences).   *
                                     * Previously parsed as Nt.1 passive verb. */
    { "wati",    POS_VERB_CONJ  }, /* quotative 2sg: "you said" (wa+ti)       *
                                     * Completes the quotative paradigm:        *
                                     * nti uti wati yati ati bati mwati twati  */

    /* ── kuba + locative forms: "to be/exist there/in/from" ──────────────  *
     * These are SP + ra(present) + b(kuba) + a + locative-suffix forms.      *
     * They fall victim to the noun-before-verb priority: the "SP" prefix is  *
     * also a noun class marker so the word is grabbed as a noun first.       *
     * Listing them here as POS_VERB_CONJ short-circuits that ambiguity.      */
    { "arabaho",   POS_VERB_CONJ }, /* Nt.1 sg: he/she exists / is there     */
    { "rurabaho",  POS_VERB_CONJ }, /* Nt.11: it (urugo etc.) exists there   */
    { "kirabaho",  POS_VERB_CONJ }, /* Nt.7: it (ikintu etc.) is there       */
    { "birabaho",  POS_VERB_CONJ }, /* Nt.8: they (ibintu) are there         */
    { "irabaho",   POS_VERB_CONJ }, /* Nt.5: it (ijambo etc.) is there       */
    { "zirabaho",  POS_VERB_CONJ }, /* Nt.10: they (inka) are there          */
    { "burabaho",  POS_VERB_CONJ }, /* Nt.14: it (ubuzima etc.) is there     */
    { "turabaho",  POS_VERB_CONJ }, /* 1pl: we are there                     */
    { "murabaho",  POS_VERB_CONJ }, /* 2pl: you all are there                */
    { "barabaho",  POS_VERB_CONJ }, /* Nt.2: they (abantu) are there         */
    /* No-ra present forms of kubaho (SP+b+a+ho without -ra- tense marker):   *
     * These don't have -ri- copular marker so verb_match_inner can't detect   *
     * them via the copula pattern; list here to prevent kubaha misparse.     */
    { "kabaho",    POS_VERB_CONJ }, /* Nt.12: it (aka-class) is there        */
    { "cyibaho",   POS_VERB_CONJ }, /* Nt.7: it (iki-class) is there (cy+ibaho)*/
    { "ryibaho",   POS_VERB_CONJ }, /* Nt.5: it (iri-class) is there         */
    { "yibaho",    POS_VERB_CONJ }, /* Nt.1/6: he/she/it is there (no-ra)   */
    /* kuba + -mo (in) */
    { "arabamo",   POS_VERB_CONJ }, /* Nt.1: he/she is in it                 */
    { "birabamo",  POS_VERB_CONJ }, /* Nt.8: they are in it                  */
    /* kuba + -yo (Nt.6 locative "from/in there") */
    { "arababayo", POS_VERB_CONJ }, /* Nt.1: he/she is among them (cls2 OM)  */

    /* ── Negative existential "nta" ────────────────────────────────────────  *
     * "nta" = "there is no / no / without" — used before noun phrases.       *
     * Distinct from the conjunction "na" and pronoun uses.                   */
    { "nta",     POS_ADVERB     }, /* negative existential: no / there is no */
    { "ntaho",   POS_VERB_CONJ  }, /* locative neg: there is nothing there   */

    /* ── Common temporal & conditional conjunctions ─────────────────────── */
    { "igihe",   POS_CONJUNCTION}, /* temporal conj: when / at the time (also*
                                     * Nt.7 noun "time" – dual use; conj form  *
                                     * used sentence-initially as "igihe cyo…")*/
    { "kugeza",  POS_CONJUNCTION}, /* until / up to (kugeza aho…)            */
    { "ubwo",    POS_CONJUNCTION}, /* temporal: when / while / at that moment *
                                     * (also used as Nt.14 pronoun – keep both)*/

    /* ── Greetings / social formulae (frozen verb/interjection forms) ──────── *
     * These are lexically frozen utterances.  Listing them here prevents the   *
     * verb heuristic from mis-tagging them or from triggering ERR_NO_VERB.     */
    { "muraho",     POS_INTERJECTION }, /* hello / good day (lit. "you are there")*/
    { "mwiriwe",    POS_INTERJECTION }, /* good afternoon/evening                  */
    { "amakuru",    POS_INTERJECTION }, /* greetings / how are you? (lit. "news")  */
    { "ni meza",    POS_INTERJECTION }, /* it's fine / I'm well                    */
    { "nimeza",     POS_INTERJECTION }, /* it's fine (contracted)                  */

    /* ── Stative "kuba na" (to have) conjugations ──────────────────────────── *
     * Formed from kuba (to be) + na (with) contracted: SP + -fite / -fit-.     *
     * Surface form: SP + fit + e.  Stem "fit" is not in VERB_STEMS so the verb *
     * heuristic misses it.  Listed here so has_verb is set correctly.          */
    { "nfite",   POS_VERB_CONJ }, /* 1sg: I have (n + fite)                       */
    { "ufite",   POS_VERB_CONJ }, /* 2sg/Nt.3: you have / it has                  */
    { "afite",   POS_VERB_CONJ }, /* Nt.1 3sg: he/she has                         */
    { "bafite",  POS_VERB_CONJ }, /* Nt.2: they (human) have                      */
    { "gifite",  POS_VERB_CONJ }, /* Nt.7: it has                                 */
    { "bifite",  POS_VERB_CONJ }, /* Nt.8: they have                              */
    { "ifite",   POS_VERB_CONJ }, /* Nt.4/9: it has                               */
    { "zifite",  POS_VERB_CONJ }, /* Nt.10: they have                             */
    { "rufite",  POS_VERB_CONJ }, /* Nt.11: it has                                */
    { "gafite",  POS_VERB_CONJ }, /* Nt.12: it has (ga- variant before voiced)    */
    { "dufite",  POS_VERB_CONJ }, /* 1pl: we have                                 */
    { "mufite",  POS_VERB_CONJ }, /* 2pl: you all have                            */
    { "bufite",  POS_VERB_CONJ }, /* Nt.14: it has                                */

    /* ── 1sg knowledge / common fixed forms ────────────────────────────────── */
    { "ndabizi", POS_VERB_CONJ }, /* I know it (1sg pres of kubizi, lit n+da+bizi)*/
    { "mbizi",   POS_VERB_CONJ }, /* I know it (alt 1sg before labial)            */
    { "nzi",     POS_VERB_CONJ }, /* I know (1sg of kumenya — short form)         */

    /* ── Common adverb / manner fixes ──────────────────────────────────────── */
    { "nabi",    POS_ADVERB },    /* badly / poorly (manner adverb)              */

    /* ── Standalone numerals (used with saa / temporal / quantifier phrases) ─ */
    { "tatu",      POS_ADVERB }, /* three (also in "saa tatu" = 9 AM)            */
    { "kane",      POS_ADVERB }, /* four                                         */
    { "gatanu",    POS_ADVERB }, /* five                                         */
    { "gatandatu", POS_ADVERB }, /* six                                          */
    { "karindwi",  POS_ADVERB }, /* seven                                        */
    { "umunani",   POS_ADVERB }, /* eight                                        */
    { "icyenda",   POS_ADVERB }, /* nine                                         */
    { "icumi",     POS_ADVERB }, /* ten                                          */
    { "kabiri",    POS_ADVERB }, /* two / twice (also used with saa)             */

    { NULL, POS_UNKNOWN }
};

bool kin_is_invariable(const char *word, POS *pos_out) {
    for (int i = 0; INVARIABLES[i].word; i++) {
        if (strcmp(word, INVARIABLES[i].word) == 0) {
            if (pos_out) *pos_out = INVARIABLES[i].pos;
            return true;
        }
    }
    return false;
}

/* ══════════════════════════════════════════════════════════════════════════
 * TREE 3 — INSHINGA (Verb)   Formula: PREF+C+FV (imbundo) | SP+(TM)+(OM)+C+(EXT)+FV (itondaguye)
 *
 * 5. KNOWN VERB STEMS (Imizi y'inshinga izwi)
 *    Sources:
 *      – "Ikinyarwanda Amashuri Nderabarezi TTC" (REB 2020)
 *      – Bibiliya Yera 2001 corpus (top-frequency infinitives/conjugations)
 *    Format: bare stem (what follows ku/gu/kw/gw prefix before final -a)
 *
 *    Transitions (Tree 3 → other trees):
 *      Inshinga → Izina mbonera:  verb root becomes noun via derivation
 *        e.g. gucya (to shine) → umucyo (light); kwijima → umwijima (darkness)
 *      Inshinga → Ntera:          verb root used as adjective stem
 *        e.g. gusaza (to grow old) → usaza (one who grows old / old)
 * ══════════════════════════════════════════════════════════════════════════ */
static const char *VERB_STEMS[] = {
    /* ── From REB textbook ────────────────────────────────────────────── */
    "som",      /* gusoma      – to read                                  */
    "bon",      /* kubona      – to see / get                             */
    "ig",       /* kwiga       – to study / learn                         */
    "andik",    /* kwandika    – to write                                 */
    "kund",     /* gukunda     – to love                                  */
    "end",      /* kugenda     – to go                                    */
    "reb",      /* kureba      – to look at                               */
    "tur",      /* gutura      – to live / reside                         */
    "fat",      /* gufata      – to take / hold                           */
    "vug",      /* kuvuga      – to speak / say                           */
    "baz",      /* kubaza      – to ask                                   */
    "subiz",    /* gusubiza    – to answer                                */
    "kor",      /* gukora      – to work / do; gukoresha (kor+esh) = to use */
    "mar",      /* kumara      – to finish / run out / be exhausted       */
    "ram",      /* gurama      – to grow crops                            */
    "bik",      /* kubika      – to store                                 */
    "hind",     /* guhinda     – to plant                                 */
    "hindur",   /* guhindura   – to change / transform                    */
    "himbir",   /* guhimbira   – to compose for                           */
    "ambuk",    /* kwambuka    – to cross                                 */
    "himb",     /* guhimba     – to compose / invent                      */
    "er",       /* kwera – to shine / be holy / be pure (ku+er+a, u→w §1.1).
                 * Note: gutera (to plant/strike) root is "ter" — distinct.      */
    "rez",      /* kureza – to clean / purify / make pure                        */
    "wez",      /* guweza – consonant-initial surface form (gu+wez+a).
                 * Phonological note: kweza (ku+ez+a, root "ez") is the
                 * causative-y form of kwera (er+y→ez §1.3); "wez" is the
                 * independent guweza root (w is stem-initial, not a glide).
                 * Both "ez" (detect_ext_in_stem causative-y) and "wez"
                 * (direct lexicon entry) are valid surface roots.               */
    "um",       /* guuma       – to harden / stay firm                    */
    "tung",     /* gutunga     – to possess / maintain                    */
    "gaban",    /* kugabana    – to share                                 */
    "sek",      /* guseka      – to laugh                                 */
    "rir",      /* kurira      – to cry                                   */
    "ririmb",   /* kuririmba   – to sing                                  */
    "byar",     /* kubyara     – to give birth                            */
    "bur",      /* kubura      – to lack / lose                           */
    "emez",     /* kwemeza     – to confirm                               */
    "erek",     /* kwerekana   – to show / demonstrate                    */
    "itabir",   /* kwitabira   – to attend                                */
    "yemr",     /* kwiyemera   – to accept / agree                        */
    "bwir",     /* kubwira     – to tell                                  */
    "bw",       /* kubwa       – to fall                                  */
    "rw",       /* kurwa       – to fight                                 */
    "shak",     /* gushaka     – to want / look for                       */
    "sur",      /* gusura      – to visit                                 */
    /* ── High-frequency stems from Bibiliya Yera 2001 corpus ─────────── */
    "gir",      /* kugira      – to have / do / be (most common verb!)    */
    "gez",      /* kugeza      – to reach / finish                        */
    "kiranuk",  /* gukiranuka  – to be righteous / upright                */
    "izer",     /* kwizera     – to believe / trust / have faith          */
    "kiraniirw",/* gukiranirwa – to be saved / redeemed                   */
    "meny",     /* kumenya     – to know / understand                     */
    "seng",     /* gusenga     – to pray / worship                        */
    "komer",    /* gukomera    – to be strong / difficult                  */
    "rwan",     /* kurwana     – to fight / wage war                      */
    "rut",      /* kuruta      – to surpass / exceed                      */
    "rimbuk",   /* kurimbuka   – to perish / be destroyed                 */
    "tang",     /* gutanga     – to give / offer / present                */
    "kiz",      /* gukiza      – to save / heal / rescue                  */
    "bah",      /* kubaha      – to respect / honor                       */
    "hamya",    /* guhamya     – to testify / witness                      */
    "bak",      /* kubaka      – to build / construct                     */
    "ger",      /* kugera      – to arrive / reach                        */
    "tak",      /* gutaka      – to shout / cry out                       */
    "zuk",      /* kuzuka      – to rise / resurrect                      */
    "injir",    /* kwinjira    – to enter / go in                         */
    "ihangan",  /* kwihangana  – to be patient / endure                   */
    "twar",     /* gutwara     – to carry / transport / lead              */
    "mvir",     /* kumvira     – to obey / hear / listen                  */
    "zan",      /* kuzana      – to bring                                 */
    "vuk",      /* kuvuka      – to be born / come out                    */
    "ifuz",     /* kwifuza     – to desire / wish                         */
    "zamuk",    /* kuzamuka    – to go up / ascend / rise                 */
    "sohok",    /* gusohooka   – to go out / exit                         */
    "hung",     /* guhunga     – to flee / escape                         */
    "byuk",     /* gubyuka     – to get up / wake up                      */
    "hagarar",  /* guhagarara  – to stop / stand still                    */
    "tiny",     /* gutinya     – to fear / be afraid                      */
    "haguruk",  /* guhaguruka  – to stand up / rise                       */
    "kir",      /* gukira      – to heal / recover / be better than       */
    "rik",      /* gurika      – to slaughter / sacrifice                  */
    "manuk",    /* gumanuka    – to go down / descend                      */
    "rem",      /* guremwa     – to be created / formed                   */
    "it",       /* kwita       – to call / name                           */
    "wit",      /* kwita (surface w-form) – i(SP)+wit+a=iwita when root-initial
                 * vowel 'i' is realised as epenthetic glide sequence iw+it  */
    "tandukany",/* gutandukanya – to separate / distinguish               */
    "tegur",    /* gutegura    – to prepare                               */
    "shyir",    /* gushyira    – to put / place                           */
    "fash",     /* gufasha     – to help                                  */
    "ganir",    /* kuganira    – to talk / converse                       */
    "hiriy",    /* guhiriya    – to bless / prosper                        */
    "tegek",    /* gutegeka    – to command / order                       */
    "gorob",    /* kugororoba  – to be evening / get dark                 */
    "cy",       /* kucya       – to dawn / be morning                     */
    "ham",      /* guhamya (short) – to confirm / be firm                 */
    "nywa",     /* kunywa      – to drink                                 */
    "rug",      /* kuruga      – to cook / prepare food                   */
    "ruk",      /* kuruka      – to vomit (modern); to sprout/grow [crops] (archaic/Bible) */
    "jyan",     /* kujyana     – to take / go together                    */
    "iruk",     /* kwiruka     – to run                                   */
    "irukan",   /* kwirukana   – to chase away                            */
    "inginga",  /* kwinginga   – to beg / plead                           */
    "tonder",   /* kwitondera  – to be careful / pay attention            */
    "rushy",    /* kurusha     – to surpass / be more than                */
    "tabur",    /* gutabara    – to rescue / save                         */
    "ic",       /* kwica       – to kill                                  */
    "jyen",     /* kujyena     – to walk along                            */
    /* ── Additional high-frequency stems (Year 4 book + corpus) ─────── */
    "jy",       /* kujya       – to go to (directional: bajya i Kigali)   */
    "jye",      /* kujya (past stem: yajyeye, alternate conjugated root)  */
    "pfa",      /* gupfa       – to die (short/contracted form)           */
    "gend",     /* kugenda     – to go/travel (general movement)          */
    "giy",      /* kugenda past stem: bagiye (they went/have gone)        */
    "shob",     /* gushobora   – to be able / can                        */
    "subit",    /* gusubira    – to return / go back                     */
    "yumv",     /* kumva       – to hear / understand                    */
    "tah",      /* kutaha      – to go home                              */
    "tum",      /* gutuma      – to send / cause                         */
    "tor",      /* gutora      – to choose / vote                        */
    "mer",      /* kumera      – to grow / sprout; causative kumeza via r+y→z rule */
    "ban",      /* kubana      – to live together / cohabit              */
    "bar",      /* kubara      – to count                                */
    "imb",      /* kwimba      – to dig                                  */
    "tuk",      /* gutuka      – to insult / curse                       */
    "zirik",    /* kuzirikan   – to meditate / contemplate               */
    "shemez",   /* gushemeza   – to praise / glorify                     */
    "onger",    /* kwongera    – to add / do again / increase            */
    "pang",     /* gupanga     – to plan / arrange                       */
    "kang",     /* gukanguka   – to wake up / be vigilant               */
    "ish",      /* kwisha      – to finish / end                         */
    "v",        /* kuva        – to come from / leave from / depart (root=v, like kuba→b) */
    "toh",      /* gutoha      – to get wet / to water (h→s before FV 'a' in conjugated forms: kigatosa) */
    "van",      /* guvana      – to part from / separate                 */
    "tan",      /* gutana      – to part ways / separate (base verb; gutanduka ← tan+uk, gutandukana ← tan+uk+an) */
    "nyur",     /* kunyura     – to pass through / traverse              */
    "zam",      /* kuzamura    – to lift / raise up                      */
    "vunik",    /* guvunika    – to break / fracture                     */
    "hur",      /* guhura      – to meet / encounter                     */
    "rong",     /* gurongora   – to marry (male perspective)             */
    "ron",      /* gurona      – to find / discover                      */
    "fung",     /* gufungura   – to open (a door/bottle/etc.)            */
    "hem",      /* guhema      – to breathe with difficulty              */
    "humek",    /* guhumeka    – to breathe (imuhumekera: i·mu·humek·er·a
                   SP=i(Nt.9) + OM=mu(Nt.1) + root=humek + APPL=er + FV=a
                   "She breathed into him": Gen 2:7 imuhumekera mu mazuru) */
    "gor",      /* kugora      – to be difficult / tough                 */
    "shirom",   /* gushiroma   – to be ashamed / embarrassed            */
    "senyur",   /* gusenya     – to demolish / break down               */
    "ting",     /* gutinga     – to forbid / prevent                     */
    "kwib",     /* kwibuka     – to remember / recall                    */
    "bwir",     /* kubwira  – already present; keep as kwibw variant     */
    "umvikan",  /* kumvikana   – to be understood / reach agreement     */
    "tond",     /* kwitondera  – to be careful / attentive              */
    "rir",      /* kurira  – to cry (already present as rir)             */
    "sangir",   /* gusenga – to worship for someone                      */
    "sukum",    /* gusukuma    – to push                                 */
    "tuhuk",    /* gutuhuka    – to rest / have a break                  */
    "jyanir",   /* kujyanira   – to take for / bring to                  */
    "fungurir",  /* gufungurira – to open for                            */
    /* ── Additional high-frequency Bible corpus stems ─────────────────── */
    "koherez",  /* gukohereza  – to send (on an errand)                  */
    "komez",    /* gukomeza    – to continue / strengthen / keep going   */
    "emer",     /* kwemera     – to believe / accept / agree             */
    "eger",     /* kwegera     – to approach / come near                 */
    "cungur",   /* gucungura   – to redeem / save / ransom               */
    "cecek",    /* guceceka    – to be quiet / silent                    */
    "cebur",    /* gucebura    – to humiliate / abase / put to shame     */
    "himbaz",   /* guhimbaza   – to celebrate / honor / praise           */
    "hish",     /* guhisha     – to hide / conceal                       */
    "humur",    /* guhumura    – to comfort / console / reassure         */
    "geragez",  /* gugerageza  – to try / attempt / make an effort      */
    "erekez",   /* kwerekezeza – to direct toward / point at             */
    "hinduk",   /* guhinduka   – to turn / change / transform            */
    "hembur",   /* guhembura   – to console / wipe away tears            */
    "honger",   /* kwongera    – to add / congratulate (ho+onger)        */
    "hongez",   /* guhoreza    – to calm / pacify                        */
    "hugur",    /* guhugura    – to prepare / train / instruct           */
    "hungur",   /* guhungura   – to liberate / free / release            */
    "humuk",    /* guhumuka    – to be freed / loosed / released         */
    "humat",    /* guhumata    – to stick / adhere / cling               */
    "humek",    /* guhumeka    – to breathe / respire                    */
    "gendar",   /* kugendera   – to live / walk / behave                 */
    "hengam",   /* guhenga     – to add beyond / go beyond              */
    "hindir",   /* guhindira   – to pour out / shed                      */
    "hishimir", /* guhishimira – to rejoice for / be happy for          */
    "rek",      /* kureka      – to leave / let go / allow               */
    "sab",      /* gusaba      – to ask / request / pray                 */
    "pf",       /* gupfa       – to die (very short stem form)           */
    "sim",      /* gusimba     – to replace / substitute                 */
    "rob",      /* kuroha      – to sink / drown                        */
    "boh",      /* guboha      – to tie / bind                          */
    "hug",      /* guhuga – to be busy/occupied; guhugurika – to be destroyed */
    "cur",      /* kucura      – to forge / shape                       */
    "bat",      /* kubata      – to press / oppress                     */
    "hit",      /* guhita      – to pass / go through                   */
    "nyw",      /* kunywa      – to drink (vowel-initial form)           */
    "tur",      /* gutura      – to live (already present)               */
    "ton",      /* gutona      – to drip / trickle                      */
    "nyz",      /* kunyaza     – to urinate                              */
    "honh",     /* guhonha     – to wipe / clean                        */
    "gor",      /* kugora      – to be difficult (already present)      */
    "teg",      /* gutega      – to set a trap / prepare                */
    "tamb",     /* gutamba     – to work hard / be zealous              */
    "gor",      /* kugora  (dup: keep for coverage)                     */
    /* ── Additional stems from this session's corpus analysis ─────────── */
    "ter",      /* gutera      – to plant / cause / do to (very common)  */
    "tek",      /* guteka      – to cook                                 */
    "men",      /* kumena      – to break / shatter                      */
    "teger",    /* gutegera    – to wait for / expect                    */
    "tekerez",  /* gutekereza  – to think / reflect / meditate           */
    "tonderez", /* gutondereza – to be careful / attend to               */
    "tondek",   /* gutondeka   – to arrange / organize                   */
    "tontoy",   /* gutontoya   – to be naive / careless                  */
    "terur",    /* guterura    – to measure / compare                    */
    "terany",   /* guteranya   – to mix / combine                        */
    "teranir",  /* guteraniriza – to unify / bring together              */
    "teran",    /* guterana    – to gather / be united                   */
    "tesh",     /* guteshwa    – to lose (passive of guteza)             */
    "rot",      /* kurota      – to dream                                */
    "roter",    /* kurotera    – to dream about / of                     */
    "her",      /* guherana    – to make white / reconcile               */
    "heng",     /* guhenga     – to migrate / go beyond / exceed         */
    "twaz",     /* gutwaza     – to arm / equip / provide weapons        */
    "nyag",     /* gunyaga     – to steal / plunder / loot               */
    "kaz",      /* gukaza      – to tighten / strengthen                 */
    "sengw",    /* gusengerwa  – to be prayed for (passive of gusenga)  */
    "kubit",    /* gukubita    – to strike / hit / beat                  */
    "pir",      /* kupira      – to blow / puff / breathe out            */
    "zaner",    /* guzanirira  – to bring for (applicative of kuzana)   */
    "bam",      /* kubama      – to be flat / spread out                 */
    "gur",      /* kugura      – to buy / purchase                       */
    "sig",      /* gusiga      – to leave / abandon / anoint             */
    "ragir",    /* kuragira    – to advise / counsel / guide             */
    "sezer",    /* gusezerera  – to say goodbye / dismiss                */
    "ror",      /* kurora      – to look at / watch / examine            */
    "konger",   /* gukongerera – to add more / give more to (applic.)   */
    "vangur",   /* guvangura   – to mix / stir / blend                   */
    "vom",      /* guvoma      – to fetch water / draw water             */
    "tsind",    /* gutsinda    – to conquer / win / prevail              */
    "geran",    /* kugereranya – to compare / equate                     */
    "gereran",  /* kugereranya – full extended stem (ger+er+an) for imper.*/
    "ganir",    /* kuganira    – to talk / discuss / converse             */
    "ganirir",  /* kuganirira  – to talk to / converse with (applicative) */
    /* ── High-frequency stems derived from Bible corpus analysis ─────── */
    /* These were missing and caused verb forms to be mis-tagged or       *
     * flagged with false object-marker errors.                           */
    "z",        /* kuza        – to come / arrive (very common!)         */
    "b",        /* kuba        – to be / exist (copular: araba, rurabaho)*/
    "h",        /* guha        – to give (e.g. ibiha=i+bi(OM.8)+h+a)    */
    "rakar",    /* gurakarira  – to be angry / furious                   */
    "rakaz",    /* gurakariza  – to make angry / enrage                  */
    "mer",      /* kumera      – to grow / thrive (already "mer" added?) */
    "nyag",     /* gunyaga     – to steal / plunder (already present)    */
    "nywer",    /* kunywereza  – to water / make drink (causative base)  */
    "vaner",    /* kuvana      – to separate / split from (also: van)    */
    "nywish",   /* kunywesha   – to make drink / give to drink           */
    "barik",    /* kubarika    – to bless (also: VERB: gutumabarika)      */
    "yimb",     /* kwiyimba    – to swell / inflate                      */
    "nyuran",   /* kunyurana   – to cross one another / pass each other  */
    "seng",     /* gusenga     – to pray (already present; keep dup ok)  */
    "shak",     /* gushaka     – to want / seek (already present)        */
    "reber",    /* kureberera  – to look at / watch over (applicative)   */
    "taber",    /* gutabarana  – to fight each other                     */
    "rumir",    /* kurumira    – to swallow                              */
    "rumb",     /* kurumba     – to be first-born / excel                */
    "rog",      /* kuroga      – to bewitch / poison                     */
    "rang",     /* kuranga     – to lead / be in front / precede         */
    "renguk",   /* kurenguka   – to be light / easy / float              */
    "rangam",   /* gurangama   – to be straight / upright                */
    "giz",      /* kugiza      – to make good / fix / improve            */
    "zor",      /* kuzora      – to be full / satisfied                  */
    "rumuk",    /* kurumuka    – to loosen / untie                       */
    "fot",      /* gufota      – to photograph / take a picture          */
    "shor",     /* gushorera   – to urinate on (applicative)             */
    "shon",     /* gushona     – to set (of sun) / sink / go under       */
    "boher",    /* gubohereza  – to tighten / bind up                    */
    "kub",      /* gukuba      – to fold / multiply                      */
    "gob",      /* kugoba      – to surround / encircle                  */
    "koter",    /* gukotera    – to work for / towards (applicative)      */
    "witer",    /* kwitera     – to cause oneself / self-initiate        */
    "yemrer",   /* kwiyemera   – already: yemr; add extended form        */
    "imburan",  /* kwimburana  – to compete / contend with              */
    "zunguran", /* kuzungurana – to alternate / take turns               */
    "hurumban", /* guhuruza    – to separate / scatter                   */
    "shish",    /* gushisha    – to be late / delayed                    */
    "rangir",   /* kurangira   – to finish / complete / end              */
    "berek",    /* kubereka    – to show / demonstrate to               */
    "bimb",     /* kubimba     – to bury / cover with soil               */
    /* ── Corpus-confirmed high-frequency stems (Bible analysis 2026) ── */
    "hamagal",  /* guhamagara  – to call / summon / name (aramuhamagara) */
    "sobanur",  /* gusobanura  – to explain / clarify (arabisobanurira)  */
    "hanuzan",  /* guhanuriza  – to interpret / prophesy                 */
    "sezeran",  /* gusezerana  – to make covenant / promise each other   */
    "mbwir",    /* kumbwira    – to tell me (1sg OM mbwira: a+ra+m+bwir) */
    "nyuran",   /* kunyurana   – to pass each other                      */
    "gabany",   /* kugabanya   – to divide / reduce                      */
    "vuzan",    /* guvuzana    – to sound together / harmonise           */
    "rang",     /* kuranga     – to lead / be at front (already "rang"?) */
    "tsindw",   /* gutsindwa   – to be defeated (passive of gutsinda)    */
    /* ── Stems from S6 Kinyarwanda textbook (2026 analysis) ────────────── */
    "rwany",    /* kurwanya    – to fight against / combat / resist       */
    "ruhuk",    /* kuruhuka    – to rest / take a break / relax          */
    "sarur",    /* gusarura    – to harvest / reap / select               */
    "shishikariz", /* gushishikariza – to encourage / motivate / inspire  */
    "kum",      /* gukumira    – to prevent / stop / block the spread     */
    "kumur",    /* gukumura    – to vaccinate / treat (medical)           */
    "sagamb",   /* gusagamba   – to stride / march / walk proudly        */
    "riber",    /* guribirira  – to wait for eagerly                      */
    "garagaz",  /* kugaragaza  – to show / demonstrate / reveal          */
    "hangayik", /* guhangayika – to worry / be anxious                   */
    "b",        /* kuba        – to be / to exist (copula; kubaho/kubamo) */
    /* ── Deverbative noun roots (verb stems underlying derived nouns) ─────── */
    "cy",       /* gucya       – to shine / be bright  (→ umucyo = light)     */
    "wijim",    /* kwijima     – to be/become dark      (→ umwijima = darkness)*/
    "wambar",   /* kwambara    – to wear / dress        (→ umwambaro = clothing)*/
    "sanzur",   /* gusanzura   – to spread/stretch out (sky, cloth);
                   ALSO kwisanzura – to free oneself (→ ubwisanzure = freedom) */
    "kwir",     /* gukwira     – to fit / be appropriate / suitable (zikwiriye) */
    /* ── Genesis 1 corpus + Biblical subjunctive forms ─────────────────── */
    "ororok",   /* kororoka    – to be fruitful / reproduce / multiply
                   Infinitive: ku+ororok+a → kororoka (u→∅/_o §1.2)
                   Conjugated: Mwororoke (mw+ororok+e, 2pl SUBJ),
                               byororoke (by+ororok+e, Nt.8 SUBJ)          */
    "gwir",     /* kugwira     – to multiply / increase / be numerous
                   Conjugated: mugwire (mu+gwir+e, 2pl SUBJ)                */
    "uzur",     /* kuzura      – to fill / be full
                   Infinitive: ku+uzur+a → kuzura (u+u→u §1.2)
                   Conjugated: mwuzure (mw+uzur+e, 2pl SUBJ, u→w/_u)       */
    /* Roots of vowel-initial verbs that produce an euphonic-z connector
     * between the SP and the verb root in conjugated forms:             */
    "it",       /* kwita       – to call / name (root -it-)
                   Passive: kwitwa = kw+it+w+a
                   3sg pres passive: a+z(euph.)+it+w+a = azitwa          */
    "kur",      /* gukura      – to remove / separate / take out
                   Passive: gukurwa; phonol. rule r→∅/_w: kur+w → kuw
                   3sg past passive: ya+kur+w+e → yakuwe (r-drop surface)
                   cf. Gen 2:23 "yakuwe mu Mugabo" = was taken from Man   */
    "kurur",    /* gukurura    – to pull / drag / tow
                   Stative: gukururuka (ki→gi §3.7.1: gikururuka = it is pullable)
                   3sg: akurura (he/she pulls), bakururuka (they are pullable)  */
    NULL
};

bool kin_is_known_verb_stem(const char *stem) {
    for (int i = 0; VERB_STEMS[i]; i++)
        if (strcmp(stem, VERB_STEMS[i]) == 0) return true;
    return false;
}

/*
 * kin_is_causative_y_surface()
 *
 * Returns true if `stem` is the SURFACE form of a causative-y (r+y→z §1.3)
 * derivation whose underlying base root is a known verb stem.
 *
 * Pattern: the causative -y- morpheme fuses with a stem-final 'r':
 *   r + y → z  (§1.3)
 * Surface stem ends in 'z'; restoring 'z'→'r' yields the base root.
 *
 * Examples:
 *   "ez"   → "er"   (kwera → kweza:  to be pure → to purify)
 *   "mez"  → "mer"  (kumera → kumeza: to germinate → to cause to germinate)
 *   "gez"  → "ger"  (kugera → kugeza: to reach → to try/put to test)
 *
 * Used in conjugated-verb detection to recognise forms like "ireza":
 *   i(SP·Nt.4) + r(TM ra, a→∅ §1.1) + ez(root) + a(FV) = ireza
 * without adding "ez" to VERB_STEMS (which would block causative-y detection
 * in detect_ext_in_stem via its known-stem early-exit guard).
 */
bool kin_is_causative_y_surface(const char *stem) {
    if (!stem) return false;
    size_t len = strlen(stem);
    if (len < 2 || stem[len-1] != 'z') return false;
    if (len >= KIN_MAX_STEM) return false;
    char try_r[KIN_MAX_STEM];
    strncpy(try_r, stem, len - 1);
    try_r[len-1] = 'r';
    try_r[len]   = '\0';
    return kin_is_known_verb_stem(try_r);
}

/* ══════════════════════════════════════════════════════════════════════════
 * TREE 3 — INSHINGA (continued)
 * 5b. REFLEXIVE VERB STEMS (Inshinga z'imbundo ya kwikora)
 *
 * These stems have a primary reflexive form with prefix kwi- (imbundo ya
 * kwikora).  When a bare subjunctive (no SP, no i-) is detected from one
 * of these stems, the reflexive marker i- was elided — the form functions
 * as an authorization/command to act upon oneself.
 *
 * Rule: kwi- + stem + a  →  bare subj without i-: stem + e
 *   kwisanzura → sanzure  (be free! / you are authorized to be free)
 *   kwimuka    → imuke    (get up! — note: i- kept here, listed separately)
 *
 * Only stems where the bare-e form is the primary attested reflexive usage
 * are listed here.  Stems with obligatory i- retention are excluded.
 * ══════════════════════════════════════════════════════════════════════════ */
static const char *REFLEXIVE_VERB_STEMS[] = {
    "sanzur",   /* kwisanzura  – to free oneself (→ sanzure: be free!)         */
    "gir",      /* kwigira     – to do for oneself / to become (→ igire)        */
    "ger",      /* kwigeraho   – to achieve/attain for oneself                  */
    "menyekan", /* kwimenyekana – to make oneself known (→ imenyekane)          */
    "beshw",    /* kwibeshywa  – root: kubeshya (beshy+w→beshw y+w fusion)
                   bare subj: i+beshw+e = ibeshwe (reflexive passive)        */
    NULL
};

bool kin_is_reflexive_verb_stem(const char *stem) {
    if (!stem) return false;
    for (int i = 0; REFLEXIVE_VERB_STEMS[i]; i++)
        if (strcmp(stem, REFLEXIVE_VERB_STEMS[i]) == 0) return true;
    return false;
}

/* ══════════════════════════════════════════════════════════════════════════
 * TREE 1 — IZINA MBONERA (continued)
 * 6. KNOWN NOUN STEMS (Imizi y'amazina izwi) — fallback for ambiguous morphology
 * ══════════════════════════════════════════════════════════════════════════ */
typedef struct { const char *stem; int class; } NounStem;

static const NounStem NOUN_STEMS[] = {
    /* class 1/2 (human) */
    { "nt",       1 }, /* muntu / abantu – person/people    */
    { "kobwa",    1 }, /* mukobwa – girl                    */
    { "hungu",    1 }, /* umuhungu – boy                    */
    { "gore",     1 }, /* umugore – woman                   */
    { "gabo",     1 }, /* umugabo – man                     */
    { "nyeshuri",1  }, /* umunyeshuri – student             */
    { "rimu",     1 }, /* umurimu – farmer                  */
    { "rimyi",    1 }, /* umurimyi – farmer                 */
    { "bwirwa",   1 }, /* person (formal)                   */
    /* class 3/4 (trees/things) */
    { "ti",       3 }, /* umuti – tree/medicine             */
    { "go",       3 }, /* umugore → no; umugo = broom (nt.3)*/
    { "ezi",      3 }, /* umuezi – month                    */
    { "ka",       3 }, /* umuka – smoke (nt.3)              */
    /* class 5/6 */
    { "ye",       5 }, /* ibuye – stone (i+Ø+buye)         */
    { "zi",       5 }, /* amazi – water (nt.6)              */
    { "hore",     5 }, /* amahoro – peace (nt.6)            */
    { "shuri",    5 }, /* ishuri – school (nt.5)            */
    { "gi",       6 }, /* amagi – eggs (nt.6)               */
    { "tara",     5 }, /* itara – lamp                      */
    { "tabo",     7 }, /* ikitabo – book (nt.7)             */
    { "go",       7 }, /* ikigo – institution (nt.7)        */
    { "baho",    11 }, /* urubaho – board (nt.11)           */
    { "go",      11 }, /* urugo – home (nt.11)              */  /* duplicate 'go' handled by class check */
    { "zi",      14 }, /* uburezi – education (nt.14)       */
    { "mero",    14 }, /* ubumero – number (nt.14)          */
    { "enge",    14 }, /* ubwenge – wisdom (nt.14)          */
    { "ntu",     16 }, /* ahantu – place (nt.16)            */
    /* ── Common nouns from Bibiliya Yera 2001 corpus ──────────────── */
    { "juru",     5 }, /* ijuru – sky / heaven (Nt.5)       */
    { "si",       9 }, /* isi – earth / world (Nt.9)        */
    { "mana",     9 }, /* Imana – God (Nt.9)                */
    { "cyo",      7 }, /* umucyo – light (Nt.1/3)           */
    { "wijima",   9 }, /* umwijima – darkness (Nt.3)        */
    { "hengeri",  9 }, /* imuhengeri – abyss (Nt.3)        */
    { "sanzure",  5 }, /* isanzure – expanse/sky (Nt.5)    */
    { "nyanja",   9 }, /* inyanja – sea / lake (Nt.9)      */
    { "butaka",  14 }, /* ubutaka – ground / land (Nt.14)  */
    { "atsi",    14 }, /* ubwatsi – grass / vegetation (Nt.14) */
    { "miro",     4 }, /* imirimo – works / tasks (Nt.4)   */
    { "ko",      12 }, /* amoko – kinds / types (Nt.6)     */
    { "gibo",    14 }, /* ubugibo – blessing                */
    { "bi",       9 }, /* ibikorwa – deeds                  */
    /* ── Nouns that are ALSO locative invariables (ambiguous forms) ──────── *
     * These appear both as standalone locatives (in INVARIABLES[]) AND as   *
     * elided nouns after ku/mu/i.  Context Pass A2 uses this table to rescue *
     * the noun reading when a possessive connector follows.                  */
    { "munsi",    3 }, /* umunsi – day (elided after ku: "ku munsi wa X")    */
    { NULL, 0 }
};

bool kin_is_known_noun_stem(const char *stem, int *class_out) {
    for (int i = 0; NOUN_STEMS[i].stem; i++) {
        if (strcmp(stem, NOUN_STEMS[i].stem) == 0) {
            if (class_out) *class_out = NOUN_STEMS[i].class;
            return true;
        }
    }
    return false;
}

/* ══════════════════════════════════════════════════════════════════════════
 * TREE 1 — IZINA MBONERA (continued)
 * 6b. FULL KNOWN WORDS – irregular nouns/words whose class prefix is elided
 *     or whose morphology doesn't match standard patterns.
 *     Sourced primarily from Bibiliya Yera 2001 corpus.
 * ══════════════════════════════════════════════════════════════════════════ */
typedef struct { const char *word; int class; char stem[KIN_MAX_STEM]; } KnownWord;

static const KnownWord KNOWN_WORDS[] = {
    /* Nt.5 nouns with elided 'ri' marker */
    { "ijuru",    5,  "juru"   },  /* sky / heaven                        */
    { "irembo",   5,  "rembo"  },  /* gate / entrance                     */
    { "isuka",    9,  "suka"   },  /* hoe / farming tool                  */
    { "iribo",    5,  "ribo"   },  /* fishing net                         */
    /* Nt.9 nouns where n- is elided before certain consonants */
    { "isi",      9,  "si"     },  /* earth / land / world                */
    { "ifu",      9,  "fu"     },  /* flour                               */
    { "igi",      9,  "gi"     },  /* egg (short form)                    */
    /* Nt.9 nouns where n→∅ before sh (palato-alveolar sibilant)           *
     * e.g. ishusho = i + ∅(n) + shusho; confirmed by connector "ya":      *
     *   "ishusho yacu" (Gen 1:26) — not *ryacu (Nt.5)                     */
    { "ishusho",  9,  "shusho" },  /* image / likeness / face / shape     */
    { "isura",    9,  "sura"   },  /* likeness / resemblance / face       */
    /* Common Nt.6 mass nouns */
    { "amazi",    6,  "zi"     },  /* water                               */
    { "amahoro",  6,  "horo"   },  /* peace / greetings                   */
    { "amafaranga",6, "faranga"},  /* money                               */
    { "amata",    6,  "ta"     },  /* milk                                */
    /* Other common irregulars / frequently misanalysed words */
    { "ejo",      0,  ""       },  /* yesterday / tomorrow (context-dep.) */
    /* Nt.5 words whose 'ri' class marker is elided (bare i- prefix) */
    { "ishuri",   5,  "shuri"  },  /* school (also ishule)                */
    { "isoko",    5,  "soko"   },  /* market                              */
    { "ibuye",    5,  "buye"   },  /* stone / rock                        */
    { "izina",    5,  "zina"   },  /* name                                */
    { "itara",    5,  "tara"   },  /* lamp / light / fire (dim)           */
    { "ifarasi",  5,  "farasi" },  /* horse (loanword)                    */
    { "itonde",   5,  "tonde"  },  /* care / caution                      */
    /* Common Nt.3 nouns (umw- before vowel: umwaka, umwami) */
    { "umwaka",   3,  "waka"   },  /* year                                */
    { "umwami",   1,  "wami"   },  /* king / lord / master (Nt.1)         */
    { "umuriro",  3,  "riro"   },  /* fire                                */
    { "umunsi",   3,  "nsi"    },  /* day                                 */
    /* Common Nt.11 nouns */
    { "urukundo", 11, "kundo"  },  /* love                                */
    { "urugendo", 11, "gendo"  },  /* journey / trip                      */
    { "urugo",    11, "go"     },  /* home / household (also nt.7 urugo)  */
    /* Common Nt.14 abstract nouns */
    { "ubugingo", 14, "gingo"  },  /* life / soul / breath                */
    { "ubuzima",  14, "zima"   },  /* health / life                       */
    { "ubwami",   14, "ami"    },  /* kingdom / reign                     */
    { "ubwoba",   14, "oba"    },  /* fear / fright                       */
    /* Common Nt.6 body/nature nouns */
    { "amaraso",  6,  "raso"   },  /* blood                               */
    { "amaboko",  6,  "boko"   },  /* arms / hands                        */
    { "amaguru",  6,  "guru"   },  /* legs / feet                         */
    { "amajwi",   6,  "jwi"    },  /* voices / words / sounds             */
    { "amazina",  6,  "zina"   },  /* names (pl of izina)                 */
    /* Common Nt.7 nouns */
    { "icyaha",   7,  "yaha"   },  /* sin / fault / guilt                 */
    { "igihe",    7,  "gihe"   },  /* time / moment / period              */
    { "igihugu",  7,  "hugu"   },  /* country / nation                    */
    { "igisiga",  7,  "siga"   },  /* bird of prey: eagle/vulture/hawk    */
    { "ibisiga",  8,  "siga"   },  /* birds of prey (pl. of igisiga)      */
    /* ikirere: sky/air/atmosphere — often written without indomo "i" as    *
     * "kirere" (especially after prepositions like "mu kirere").           *
     * Without this entry the verb parser reads ki(SP)+rer(root)+e(FV).    */
    { "ikirere",  7,  "rere"   },  /* sky / air / atmosphere              */
    { "kirere",   7,  "rere"   },  /* sky / air (elided form of ikirere)  */
    /* Common Nt.9 nouns with n+y→nz (§2.4.1) prefix rule */
    { "inzira",   9,  "nzira"  },  /* path / road                         */
    { "inzara",   9,  "nzara"  },  /* hunger / famine                     */
    { "inzoga",   9,  "nzoga"  },  /* beer / alcohol                      */
    { "inzoka",   9,  "nzoka"  },  /* snake                               */
    { "inzuzi",   9,  "nzuzi"  },  /* flies (insects)                     */
    { "inzuki",   9,  "nzuki"  },  /* bees                                */
    { "inzobe",   9,  "nzobe"  },  /* inzobe (plant/food)                 */
    /* Nt.1/3 nouns whose D vowel 'u' is dropped in informal writing       */
    { "musomyi",  1,  "somyi"  },  /* reader / student (= umusomyi)        */
    { "muntu",    1,  "ntu"    },  /* person (= umuntu)                    */
    { "umugore",  1,  "gore"   },  /* woman (full form; guards vs verb subj)*/
    { "umugabo",  1,  "gabo"   },  /* man   (full form; guards vs verb subj)*/
    { "mugore",   1,  "gore"   },  /* woman (= umugore)                    */
    { "mugabo",   1,  "gabo"   },  /* man (= umugabo)                      */
    { "mwana",    1,  "wana"   },  /* child (= umwana)                     */
    { "mwami",    1,  "wami"   },  /* king/lord (= umwami)                 */
    /* Dropped D-vowel: Nt.7 words where prefix 'i' is elided in fast speech */
    { "gihe",     7,  "gihe"   },  /* = igihe (time / moment)             */
    { "gihugu",   7,  "hugu"   },  /* = igihugu (country / nation)        */
    { "gice",     7,  "ce"     },  /* = igice (portion / part)            */
    { "giciro",   7,  "ciro"   },  /* = igiciro (price)                   */
    { "gicaniro", 7,  "caniro" },  /* = igicaniro (fireplace / altar)     */
    { "gicumuro", 7,  "cumuro" },  /* = igicumuro (sin / offense)         */
    /* Dropped D-vowel: Nt.4 (imiti) words where prefix 'i' is elided     */
    { "mirongo",  4,  "rongo"  },  /* = imirongo (tens; counting 10s)     */
    { "mirimo",   4,  "rimo"   },  /* = imirimo (works / tasks)           */
    { "mibiri",   4,  "biri"   },  /* = imibiri (bodies)                  */
    { "mibare",   4,  "bare"   },  /* = imibare (numbers / figures)       */
    { "migabane", 4,  "gabane" },  /* = imigabane (shares / portions)     */
    /* Dropped D-vowel: Nt.6 (ama-) words where prefix 'a' is elided      */
    { "maso",     6,  "maso"   },  /* = amaso (eyes)                      */
    { "magana",   6,  "gana"   },  /* = amagana (hundreds)                */
    { "maboko",   6,  "boko"   },  /* = amaboko (arms / hands)            */
    { "mabuye",   6,  "buye"   },  /* = amabuye (stones)                  */
    { "mafi",     6,  "fi"     },  /* = amafi (fish)                      */
    { "maraso",   6,  "raso"   },  /* = amaraso (blood)                   */
    { "makosa",   6,  "kosa"   },  /* = amakosa (errors / faults)         */
    { "majwi",    6,  "jwi"    },  /* = amajwi (voices / sounds)          */
    { "masomo",   6,  "somo"   },  /* = amasomo (lessons)                 */
    { "mateka",   6,  "teka"   },  /* = amateka (history)                 */
    { "moko",     6,  "oko"    },  /* = amoko dropped-D: m+oko (C=-oko-)  */
    { "amoko",    6,  "oko"    },  /* amoko: a+m+oko (a→∅ §1.1 before 'o')*/
    /* Dropped D-vowel: Nt.9 words where prefix 'i' is elided             */
    { "nzu",      9,  "nzu"    },  /* = inzu (house)                      */
    { "nzira",    9,  "nzira"  },  /* = inzira (path / road)              */
    { "nzoga",    9,  "nzoga"  },  /* = inzoga (beer)                     */
    /* Number words */
    { "cumi",     7,  "cumi"   },  /* = icumi (ten)                       */
    /* Informal / fast-speech noun forms */
    { "imana",    9,  "mana"   },  /* = Imana (God — full form with i-prefix; mid-sentence) */
    { "mana",     9,  "mana"   },  /* = Imana (God — informal/elided)     */
    { "data",     1,  "data"   },  /* father (informal: data = dada)      */
    { "nyina",    1,  "nyina"  },  /* mother / her mother                 */
    { "shebuja",  1,  "shebuja"},  /* master / lord (title)               */
    /* Common Nt.14 nouns */
    { "bwenge",  14,  "enge"   },  /* = ubwenge (wisdom / intelligence)   */
    { "bwami",   14,  "ami"    },  /* = ubwami (kingdom — alternate form) */
    /* Common Nt.9 nouns from Bible corpus */
    { "nteko",    9,  "nteko"  },  /* = inteko (class / group / council)  */
    { "nyota",    9,  "nyota"  },  /* = inyota (thirst)                   */
    /* Common Nt.11 nouns */
    { "rugero",  11,  "gero"   },  /* = urugero (example / measure)       */
    /* Nt.1/3 common nouns with dropped D-vowel 'u' from "umu" */
    { "mudugudu",  3,  "dugudu" }, /* = umudugudu (village / neighborhood)*/
    { "musozi",    3,  "sozi"   }, /* = umusozi (hill / mountain)          */
    { "mugenzi",   1,  "genzi"  }, /* = umugenzi (friend / companion)      */
    { "murimyi",   1,  "rimyi"  }, /* = umurimyi (farmer)                  */
    { "muhana",    3,  "hana"   }, /* = umuhana (valley / stream)          */
    { "mugisha",   3,  "gisha"  }, /* = umugisha (blessing)                */
    { "musaza",    1,  "saza"   }, /* = umusaza (old man / elder)          */
    { "musikari",  1,  "sikari" }, /* = umusikari (soldier / warrior)      */
    { "mubyeyi",   1,  "byeyi"  }, /* = umubyeyi (parent)                 */
    { "mukuru",    1,  "kuru"   }, /* = umukuru (elder / chief / senior)   */
    /* Nt.14 common nouns with dropped D-vowel 'u' from "ubu" */
    { "butayu",   14,  "tayu"   }, /* = ubutayu (desert / wilderness)      */
    { "burenganzira",14,"renganzira"}, /* = uburenganzira (right/authority)*/
    { "busabe",   14,  "sabe"   }, /* = ubusabe (prayer / request)         */
    { "buhemu",   14,  "hemu"   }, /* = ubuhemu (betrayal / treason)       */
    /* Nt.15 common nouns with dropped D-vowel 'u' from "uku" */
    { "kwezi",    15,  "wezi"   }, /* = ukwezi (moon / month)              */
    /* Nt.2 common nouns with dropped D-vowel 'a' from "aba" */
    { "bantu",     2,  "ntu"    }, /* = abantu (people)                    */
    /* Nt.9 common nouns with dropped D-vowel 'i' */
    { "ntebe",     9,  "ntebe"  }, /* = intebe (chair / throne / seat)     */
    /* "nabi" removed from here: it is the manner adverb "badly/poorly" and
     * is listed in INVARIABLES (step 1) to prevent misanalysis as Nt.9 noun. */
    /* Nt.12 with dropped D-vowel 'a' from "aka" */
    { "akara",    12,  "ra"     }, /* flesh / sinew / cartilage (Nt.12)    */
    { "gakondo",  12,  "kondo"  }, /* = agakondo (ancestral land/heritage) */
    /* Common titles and family terms */
    { "databuja",  1,  "databuja"},/* master / lord / employer (title)     */
    { "sogokuru",  1,  "sogokuru"},/* grandfather / ancestor (Nt.1)        */
    { "sekuruza",  1,  "sekuruza"},/* forefather / ancestor                */
    { "sekuru",    1,  "sekuru"  },/* grandfather / uncle (paternal)       */
    { "nyogokuru", 1,  "nyogokuru"},/* grandmother (Nt.1)                  */
    /* Common biblical/religious terms */
    { "intumwa",   9,  "ntumwa"  },/* = intumwa (apostle / messenger)      */
    { "umwuka",    3,  "wuka"    },/* = umwuka (spirit / breath)           */
    { "izahabu",   9,  "zahabu"  },/* = izahabu (gold)                     */
    { "pasika",    9,  "pasika"  },/* Passover / Easter (loanword)          */
    /* Nt.3 nouns with dropped D-vowel */
    { "muti",      3,  "ti"      },/* = umuti (tree / medicine / herb)     */
    /* Nt.16 nouns with dropped D-vowel */
    { "hantu",    16,  "ntu"     },/* = ahantu (place / location)          */
    /* Nt.6 nouns with dropped D-vowel (wlen=4, below normal threshold)    */
    { "mazi",      6,  "zi"      },/* = amazi (water — short drop form)    */
    { "mari",      6,  "ri"      },/* = amari (intestines / guts)          */
    { "amezi",     6,  "ezi"     },/* = amezi (months, pl of ukwezi)        */
    /* Nt.8 nouns with dropped D-vowel */
    { "bihugu",    8,  "hugu"    },/* = ibihugu (countries, pl of igihugu) */
    { "bimwe",     8,  "mwe"     },/* = ibimwe (some things, pl)           */
    { "bintu",     8,  "ntu"     },/* = ibintu (things, pl of ikintu)       */
    /* Nt.7 nouns with dropped D-vowel (ki- prefix, voiceless) */
    { "kintu",     7,  "ntu"     },/* = ikintu (thing / something)          */
    /* Nt.12 nouns with dropped D-vowel (ga- variant) */
    { "gasozi",   12,  "sozi"    },/* = agasozi (small hill / hillock)     */
    { "gatabo",   12,  "tabo"    },/* = agatabo (small book)               */
    { "gati",     12,  "ti"      },/* = igati (bridge) — k→g variant       */
    /* Nt.2 nouns with dropped 'a' from "aba" → bare "ba" prefix          */
    { "bami",      2,  "mi"      },/* = abami (kings, pl of umwami)         */
    { "bagaragu",  2,  "garagu"  },/* = abagaragu (servants, pl)           */
    { "bagenzi",   2,  "genzi"   },/* = abagenzi (friends, pl)             */
    { "bahungu",   2,  "hungu"   },/* = abahungu (sons, pl)                */
    { "bahanuzi",  2,  "hanuzi"  },/* = ababanuzi (interpreters, pl)       */
    { "batambyi",  2,  "tambyi"  },/* = abatambyi (worshippers, pl)        */
    /* Nt.9/10 nouns with fully dropped D+N prefix */
    { "nzozi",     9,  "nzozi"   },/* = inzozi (dreams)                    */
    { "ntoki",     9,  "ntoki"   },/* = intoki (finger / toe)              */
    /* Nt.5 nouns with dropped 'i' prefix (starting with consonant) */
    { "joro",      5,  "joro"    },/* = ijoro (night / evening)             */
    { "tegeko",    5,  "tegeko"  },/* = itegeko (law / commandment)         */
    { "jambo",     5,  "jambo"   },/* = ijambo (word / speech)              */
    /* Nt.5 nouns too short for bare-i threshold */
    { "ivu",       5,  "vu"      },/* = ivu (ash / ashes)                  */
    /* Nt.11 nouns with dropped 'u' prefix */
    { "ruzi",     11,  "zi"      },/* = uruzi (river / stream)             */
    { "rugwi",    11,  "gwi"     },/* = urugwi (necklace / string)          */
    /* Nt.1/3 nouns with dropped 'u' prefix (wlen≤4, below threshold) */
    { "muzi",      1,  "zi"      },/* = umuzi (homestead / village)         */
    /* Common nouns that appear without D-vowel */
    { "bwato",    14,  "wato"    },/* = ubwato (boat / canoe)              */
    { "ruganda",  11,  "ganda"   },/* = uruganda (factory / workshop)      */
    { "rutoke",   11,  "toke"    },/* = urutoke (banana / finger)          */
    /* Common Nt.13 nouns */
    { "ugutwi",   13,  "gwi"     },/* = ututwi (ears, pl dim) / ugutwi     */
    /* Common Kinyarwanda title words */
    { "nyagasani", 1,  "nyagasani"},/* Lord / Sir (honorific, = Nyagasani)  */
    { "sogokuruza",1,  "sogokuruza"},/* grandfather / forefather              */
    { "sekurume",  1,  "sekurume" },/* uncle / elder male relative          */
    { "vino",      9,  "vino"    },/* wine (loanword from Portuguese/French)*/
    { "efa",       9,  "fa"      },/* ephah (unit of measure, biblical)    */
    { "inzoga",    9,  "nzoga"   },/* beer / alcohol (standard form)        */
    /* Common Nt.6 mass nouns not caught by prefix */
    { "amavuta",   6,  "vuta"    },/* = amavuta (oil / fat / anointing oil) */
    { "amarozi",   6,  "rozi"    },/* = amarozi (poison / venom)            */
    { "amavunjwa", 6,  "vunjwa"  },/* = amavunjwa (ruins / rubble)          */
    { "amagambo",  6,  "gambo"   },/* = amagambo (words / sentences)        */
    /* Common Nt.9 nouns */
    { "ndebe",     9,  "ndebe"   },/* = indebe (cup / calabash)             */
    { "ngabo",     9,  "ngabo"   },/* = ingabo (army / shield / soldiers)   */
    /* Nt.5 nouns with fully dropped prefix (no 'i' prefix at all) */
    { "sezerano",  5,  "sezerano"},/* = isezerano (covenant / agreement)    */
    /* Nt.11 short nouns with dropped D */
    { "rugi",     11,  "gi"      },/* = urugi (door / gate — short form)   */
    /* Common Nt.9 nouns from Bible corpus */
    { "ntumbi",    9,  "ntumbi"  },/* = intumbi (dead body / corpse)        */
    /* Nt.11 nouns with dropped D-vowel 'u' from "uru" */
    { "rupfu",    11,  "pfu"     },/* = urupfu (death / mortality)          */
    { "rugo",     11,  "go"      },/* = urugo (household — alt form)        */
    /* Nt.8 nouns with dropped D-vowel */
    { "biti",      8,  "ti"      },/* = ibiti (trees / plants)              */
    /* Nt.2 nouns with dropped 'a' from "aba" → bare "ba" prefix          */
    { "balewi",    2,  "lewi"    },/* = abalewi (Levites, tribe of Levi)    */
    { "bafumu",    2,  "fumu"    },/* = ababafumu (diviners / soothsayers)  */
    /* ── Bare Nt.9 nouns (n-assimilation, conflict with verb SPs) ─────── */
    { "mvura",     9,  "vura"   }, /* = imvura (rain) — mv SP conflict      */
    { "mbuzi",     9,  "buzi"   }, /* = imbuzi (goat)                       */
    { "mbabazi",   9,  "babazi" }, /* = imbabazi (mercy / compassion)       */
    { "mboni",     9,  "boni"   }, /* = imboni (prophet / seer)             */
    { "mbisi",     9,  "bisi"   }, /* = imbisi (raw/uncooked)               */
    { "mbabazi",   9,  "babazi" }, /* = imbabazi (mercy)                    */
    { "mbuni",     9,  "buni"   }, /* = imbuni (coffee plant)               */
    { "mpamvu",    9,  "pamvu"  }, /* = impamvu (reason / cause)            */
    { "mpiri",     9,  "piri"   }, /* = impiri (muscle / fibre)             */
    { "mpore",     9,  "pore"   }, /* = impore (bean / legume)              */
    { "nguku",     9,  "guku"   }, /* = inguku (hen / chicken)              */
    { "ngazi",     9,  "gazi"   }, /* = ingazi (ladder / staircase)         */
    { "ngubu",     9,  "gubu"   }, /* = ingubu (blanket / covering)         */
    { "nguyu",     9,  "guyu"   }, /* = inguyu (fig / fig tree)             */
    { "nguru",     9,  "guru"   }, /* = inguru (tribute / offering)         */
    { "ngunzu",    9,  "gunzu"  }, /* = ingunzu (piglet / young pig)        */
    { "ngenzi",    9,  "genzi"  }, /* = ingenzi (hero / worthy person)      */
    { "ngobyi",    9,  "gobyi"  }, /* = ingobyi (calabash / gourd)          */
    { "ngoyi",     9,  "goyi"   }, /* = ingoyi (chain / fetter)             */
    { "nkumi",     9,  "kumi"   }, /* = inkumi (young woman)                */
    { "nkwi",      9,  "kwi"    }, /* = inkwi (firewood)                    */
    { "njiji",     9,  "jiji"   }, /* = injiji (cricket / grasshopper)      */
    { "mbibi",     9,  "bibi"   }, /* = imbibi (boundary / border)          */
    { "mbizi",     9,  "bizi"   }, /* = imbizi (wild animal)                */
    { "nkozi",     9,  "kozi"   }, /* = inkozi (worker / hired hand)        */
    { "nkomyi",    9,  "komyi"  }, /* = inkomyi (praise / flattery)         */
    /* Nt.9 nouns that conflict with verb SP patterns */
    { "ngoma",     9,  "ngoma"  }, /* = ingoma (drum / kingdom)             */
    { "ngufu",     9,  "ngufu"  }, /* = ingufu (strength / power)           */
    { "ngeri",     9,  "ngeri"  }, /* = ingeri (kind / type / variety)      */
    { "nkabi",     9,  "nkabi"  }, /* = inkabi (ox / bull)                  */
    { "ngali",     9,  "ngali"  }, /* = ingali (cassava leaves)             */
    { "ngagi",     9,  "ngagi"  }, /* = ingagi (gorilla)                    */
    { "ngibi",     9,  "gibi"   }, /* = ingibi (toad / frog)                */
    { "ngiri",     9,  "giri"   }, /* = ingiri (warthog)                    */
    /* ── Bible merger forms (preposition + noun, no apostrophe in source) */
    { "yumwami",   1,  "wami"   }, /* = ya umwami (of the king)            */
    { "numwami",   1,  "wami"   }, /* = na umwami (with the king)          */
    { "nabantu",   2,  "ntu"    }, /* = na abantu (with the people)        */
    { "nibihumbi", 8,  "humbi"  }, /* = na ibihumbi (with thousands)       */
    { "nibindi",   8,  "ndi"    }, /* = na ibindi (and other things)       */
    { "nibintu",   8,  "ntu"    }, /* = na ibintu (and things)             */
    { "nubwami",  14,  "wami"   }, /* = na ubwami (with the kingdom)       */
    { "yabisirayeli",2,"bisirayeli"},/* = ya Abisirayeli (of the Israelites)*/
    { "wabisirayeli",2,"bisirayeli"},/* = wa Abisirayeli                    */
    { "nabisirayeli",2,"bisirayeli"},/* = na Abisirayeli                    */
    { "yumwuka",   3,  "wuka"   }, /* = ya umwuka (of the spirit)          */
    { "numwuka",   3,  "wuka"   }, /* = na umwuka (with the spirit)        */
    { "nabandi",   2,  "bandi"  }, /* = na abandi (with others)            */
    { "yabantu",   2,  "ntu"    }, /* = ya abantu (of the people)          */
    { "yabami",    2,  "mi"     }, /* = ya abami (of the kings)            */
    { "yabana",    2,  "ana"    }, /* = ya abana (of the children)         */
    { "yabagaragu",2,  "garagu" }, /* = ya abagaragu (of the servants)     */
    { "nabami",    2,  "mi"     }, /* = na abami (with the kings)          */
    { "nabagenzi", 2,  "genzi"  }, /* = na abagenzi (with friends)         */
    { "shekeli",   9,  "shekeli"}, /* shekel (unit of weight, biblical)    */
    { "riti",      5,  "riti"   }, /* = iriti (rite/measure, biblical)     */
    /* ── Short nouns below normal detection threshold ─────────────────── */
    { "buyu",     14,  "yu"     }, /* = ubuyu (baobab fruit / tree)        */
    { "kafu",     12,  "fu"     }, /* = akafu (small cup / calabash)       */
    { "kato",     12,  "to"     }, /* = akato (small canoe / boat)         */
    { "biru",     14,  "ru"     }, /* = ubwiru (royal secret / ritual)     */
    { "sinzi",     0,  ""       }, /* 1sg neg of kuzizi: I don't know      */
    /* ── Nouns with dropped D-vowel (too short for threshold or irregular) */
    { "gore",      1,  "gore"   }, /* = mugore (woman) — dropped mu-       */
    { "hungu",     1,  "hungu"  }, /* = umuhungu (boy/son) — dropped umu-  */
    { "cumu",      5,  "cumu"   }, /* = icumu (spear) — dropped i-         */
    { "funguro",   5,  "funguro"}, /* = ifunguro (breakfast/meal)           */
    { "feza",      5,  "feza"   }, /* = ifeza (silver) — dropped i-        */
    { "fumbire",   5,  "fumbire"}, /* = ifumbire (fertilizer) — dropped i- */
    { "fumbwe",   14,  "fumbwe" }, /* = ubufumbwe (secret/private matter)  */

    /* ── Religious / biblical titles (very high frequency in corpus) ────── */
    /* "Uwiteka" = THE LORD (divine title, ~5178 occurrences).               *
     * Without this entry it is mis-parsed as verb "u+witeka" (SP Nt.3 +     *
     * stem witek + a).  Listed as Nt.1 human noun (divine person).          */
    { "uwiteka",   1,  "witeka" }, /* The LORD (Yahweh — Kinyarwanda title)  */

    /* Common high-frequency nouns missed by prefix rules or too short */
    { "umutima",   1,  "tima"   }, /* heart / mind / conscience (Nt.1)       */
    { "amahoro",   6,  "horo"   }, /* peace / greetings (Nt.6 mass)          */
    { "ubwami",   14,  "wami"   }, /* kingdom / reign (Nt.14)                */
    { "imfura",    9,  "fura"   }, /* firstborn / noble (Nt.9)               */
    { "inzira",    9,  "nzira"  }, /* path / road / way (Nt.9)               */
    { "ubuhanga",  14, "hanga"  }, /* skill / art / capability (Nt.14)       */
    { "ubwiza",    14, "wiza"   }, /* beauty / goodness / grace (Nt.14)      */
    { "ubutegetsi",14, "tegetsi"}, /* power / authority / government (Nt.14) */
    { "ubuhamya",  14, "hamya"  }, /* testimony / witness (Nt.14)            */
    { "itegeko",    5, "tegeko"  }, /* law / commandment (Nt.5)               */
    { "iterambere", 5, "terambere"},/* progress / development (Nt.5)         */
    { "amasezerano",6, "sezerano"},/* covenants / agreements (Nt.6 pl)       */
    { "ubukiro",   14, "kiro"   }, /* salvation / redemption (Nt.14)         */
    { "inzoga",    9,  "nzoga"  }, /* beer / alcoholic drink (Nt.9, dup ok)  */
    { "inyumba",   9,  "nyumba" }, /* room / apartment (Nt.9)                */
    { "urwego",   11,  "ego"    }, /* level / rank / tier (Nt.11)            */
    { "uruhande",  11, "hande"  }, /* side / direction (Nt.11)               */
    { "akazi",    12,  "zi"     }, /* work / job / task (Nt.12)              */
    { "agaciro",  12,  "ciro"   }, /* value / dignity / worth (Nt.12)        */
    { "agahe",    12,  "he"     }, /* a short time / while (Nt.12 dim.)      */

    /* ── Nouns from S6 textbook vocabulary (2026 analysis) ─────────────── */
    { "leta",      9,  "leta"   }, /* state / government (loanword fr. l'État)*/
    { "ndimi",     4,  "dimi"   }, /* languages (pl. of ururimi, dropped 'i') */
    { "mpamagazi", 1,  "pamagazi"},/* one who calls/summons; also grammar term*/
    { "intore",    9,  "ntore"  }, /* warriors / trained youth (Nt.9)         */
    { "imbwirwaruhame",9,"bwirwaruhame"},/* narrative poetry / praise poem    */

    /* ── Nouns false-positived as verbs due to SP+OM+stem collision ─────── */
    /* These words match SP+OM+known-verb-stem but are nouns, not verbs.     */
    { "umucyo",    3,  "cyo"    }, /* light / beam of light (Nt.3)            */
    { "icyizero",  7,  "izero"  }, /* hope / expectation (Nt.7)               */
    { "ibihimba",  8,  "himba"  }, /* body members / created things (Nt.8)    */
    /* "ubutaka": u(SP·Nt.3)+bu(OM·Nt.14)+tak(gutaka=shout)+a → misread as verb.
     * Correct reading: D=u + RT=bu (Nt.14) + C=taka → land / soil / earth.  */
    { "ubutaka",  14,  "taka"   }, /* land / soil / earth (Nt.14 abstract)    */

    /* ── Time-expression nouns ──────────────────────────────────────────────── */
    { "saa",       9,  "saa"    }, /* hour / o'clock (Swahili loanword, Nt.9)  */
    { "isaa",      9,  "saa"    }, /* hour (with i- prefix form)               */

    /* ── Common biblical proper nouns (high frequency in corpus) ──────────── */
    { "yohan",     1,  "yohan"  }, /* John (apostle name, Nt.1)                */
    { "petero",    1,  "petero" }, /* Peter (apostle name, Nt.1)               */
    { "pawulo",    1,  "pawulo" }, /* Paul (apostle name, Nt.1)                */
    { "yakobo",    1,  "yakobo" }, /* James/Jacob (Nt.1)                       */
    { "dawidi",    1,  "dawidi" }, /* David (Nt.1)                             */
    { "yozefu",    1,  "yozefu" }, /* Joseph (Nt.1)                            */
    { "mose",      1,  "mose"   }, /* Moses (Nt.1)                             */
    { "abrahamu",  1,  "brahamu"}, /* Abraham (Nt.1)                           */
    { "isaka",     1,  "isaka"  }, /* Isaac (Nt.1)                             */
    { "yakobe",    1,  "yakobe" }, /* Jacob (alt spelling, Nt.1)               */

    { NULL, 0, "" }
};

bool kin_is_known_full_word(const char *word, int *class_out, char *stem_out) {
    for (int i = 0; KNOWN_WORDS[i].word; i++) {
        if (strcmp(word, KNOWN_WORDS[i].word) == 0) {
            if (class_out) *class_out = KNOWN_WORDS[i].class;
            if (stem_out)  strncpy(stem_out, KNOWN_WORDS[i].stem, KIN_MAX_STEM - 1);
            return true;
        }
    }
    return false;
}

/* ══════════════════════════════════════════════════════════════════════════
 * TREE 1 — IZINA MBONERA (continued)
 * 7. NOUN PLURAL PAIRS (Uturemajambo tw'amazina – igicumbi method)
 *
 * Each entry stores a singular/plural pair and the shared igicumbi (C)
 * derived by the plurality method taught in REB textbooks:
 *
 *   Step 1: D = first vowel of the word
 *   Step 2: C = the suffix shared between singular AND plural
 *   Step 3: RT = the part between D and C  (RT = RS on corresponding ntera)
 *
 * When plural == NULL the word is invariant (class 9/10 same-form nouns,
 * or words with no grammatical plural).  In those cases C is confirmed by
 * passing the word through all inteko or by inspecting ntera agreement.
 *
 * Rule: RT ≡ RS  (the noun's indanganteko equals the adjective's indangasano)
 * ══════════════════════════════════════════════════════════════════════════ */
static const NounPluralPair NOUN_PLURAL_PAIRS[] = {
    /* singular           plural             C           sg  pl */
    /* ── Class 1 ↔ 2 (human singular / human plural) ─────────────────────── */
    { "umuntu",        "abantu",          "ntu",        1,  2  },
    { "umugabo",       "abagabo",         "gabo",       1,  2  },
    { "umugore",       "abagore",         "gore",       1,  2  },
    { "umwana",        "abana",           "ana",        1,  2  },  /* u→w (§1.1), a→∅ (§1.1) */
    { "umuhungu",      "abahungu",        "hungu",      1,  2  },
    { "umukobwa",      "abakobwa",        "kobwa",      1,  2  },
    { "umunyeshuri",   "abanyeshuri",     "nyeshuri",   1,  2  },
    { "umurimyi",      "abarimyi",        "rimyi",      1,  2  },
    { "umubyeyi",      "ababyeyi",        "byeyi",      1,  2  },
    { "umutegetsi",    "abategetsi",      "tegetsi",    1,  2  },
    { "umugenzi",      "abagenzi",        "genzi",      1,  2  },
    { "umukuru",       "abakuru",         "kuru",       1,  2  },
    { "umusaza",       "abasaza",         "saza",       1,  2  },
    { "umugaragu",     "abagaragu",       "garagu",     1,  2  },
    { "umwami",        "abami",           "mi",         1,  2  },  /* u→w (§1.1); abami=a+ba+mi */
    { "umunyamuryango","abanyamuryango",  "nyamuryango",1,  2  },
    { "umwigisha",     "abigisha",        "igisha",     1,  2  },  /* u→w, a→∅ */
    { "umusomyi",      "abasomyi",        "somyi",      1,  2  },
    /* ── Class 3 ↔ 4 (tree/thing singular / plural) ────────────────────────── */
    { "umuti",         "imiti",           "ti",         3,  4  },
    { "umugezi",       "imigezi",         "gezi",       3,  4  },
    { "umugozi",       "imigozi",         "gozi",       3,  4  },  /* user's textbook example */
    { "umuriro",       "imiriro",         "riro",       3,  4  },
    { "umunsi",        "iminsi",          "nsi",        3,  4  },
    { "umuryango",     "imiryango",       "ryango",     3,  4  },
    { "umuhana",       "imihana",         "hana",       3,  4  },
    { "umusozi",       "imisozi",         "sozi",       3,  4  },
    { "umudugudu",     "imidugudu",       "dugudu",     3,  4  },
    { "umugisha",      "imigisha",        "gisha",      3,  4  },
    { "umurimo",       "imirimo",         "rimo",       3,  4  },
    { "umubiri",       "imibiri",         "biri",       3,  4  },
    { "umubare",       "imibare",         "bare",       3,  4  },
    { "umugabane",     "imigabane",       "gabane",     3,  4  },
    { "umutima",       "imitima",         "tima",       3,  4  },
    { "umurongo",      "imirongo",        "rongo",      3,  4  },
    { "umutsima",      "imutsima",        "tsima",      3,  4  },
    /* ── Class 5 ↔ 6 (singular / mass-plural) ──────────────────────────────── */
    { "itegeko",       "amategeko",       "tegeko",     5,  6  },  /* user's main example */
    { "izina",         "amazina",         "zina",       5,  6  },
    { "itara",         "amatara",         "tara",       5,  6  },
    { "ibuye",         "amabuye",         "buye",       5,  6  },
    { "isoko",         "amasoko",         "soko",       5,  6  },
    { "isomo",         "amasomo",         "somo",       5,  6  },
    { "irembo",        "amarembo",        "rembo",      5,  6  },
    { "irari",         "amarari",         "rari",       5,  6  },
    { "isezerano",     "amasezerano",     "sezerano",   5,  6  },
    { "ifarasi",       "amafarasi",       "farasi",     5,  6  },
    { "itabu",         "amatabu",         "tabu",       5,  6  },
    { "igitabo",       "ibitabo",         "tabo",       7,  8  },  /* alias via 7/8 */
    { "iterambere",    "amaterambere",    "terambere",  5,  6  },
    { "ifunguro",      "amafunguro",      "funguro",    5,  6  },
    /* ── Class 7 ↔ 8 (thing singular / plural) ─────────────────────────────── */
    { "igisiga",       "ibisiga",         "siga",       7,  8  },  /* bird of prey: eagle/vulture/large hawk */
    { "ikitabo",       "ibitabo",         "tabo",       7,  8  },
    { "ikigo",         "ibigo",           "go",         7,  8  },
    { "ikintu",        "ibintu",          "ntu",        7,  8  },
    { "igihe",         "ibihe",           "he",         7,  8  },  /* k→g§3.7: ki+he→gi+he */
    { "igihugu",       "ibihugu",         "hugu",       7,  8  },
    { "igicaniro",     "ibicaniro",       "caniro",     7,  8  },
    { "ikigaba",       "ibigaba",         "gaba",       7,  8  },
    { "ikigero",       "ibigero",         "gero",       7,  8  },
    { "ikigomba",      "ibigomba",        "gomba",      7,  8  },
    { "ikibondo",      "ibibondo",        "bondo",      7,  8  },
    { "ikirenge",      "ibirenge",        "renge",      7,  8  },
    { "ikiremwa",      "ibiremwa",        "remwa",      7,  8  },
    /* ── Class 9 ↔ 10 (invariant-form nouns; RT confirmed via ntera) ────────── */
    /* plural == NULL means singular=plural surface; class 9 sg / 10 pl        */
    { "inka",          NULL,              "ka",         9,  10 },
    { "inkoko",        NULL,              "koko",       9,  10 },
    { "inzu",          NULL,              "zu",         9,  10 },  /* D=i,RT=n,C=zu */
    { "imvura",        NULL,              "vura",       9,  10 },  /* n→m §3.3 */
    { "imana",         NULL,              "mana",       9,  10 },  /* n+m→m geminate; C=mana */
    { "inyoni",        NULL,              "nyoni",      9,  10 },  /* ny phoneme; C=nyoni */
    { "inyama",        NULL,              "nyama",      9,  10 },
    { "inzira",        NULL,              "yira",       9,  10 },  /* n+y→nz §2.4.1: C=yira */
    { "inzara",        NULL,              "yara",       9,  10 },  /* n+y→nz §2.4.1: C=yara (hunger) */
    { "inzoga",        NULL,              "yoga",       9,  10 },  /* n+y→nz §2.4.1: C=yoga (beer) */
    { "inzoka",        NULL,              "yoka",       9,  10 },  /* n+y→nz §2.4.1: C=yoka (snake) */
    { "inzuzi",        NULL,              "yuzi",       9,  10 },  /* n+y→nz §2.4.1: C=yuzi (flies) */
    { "inzuki",        NULL,              "yuki",       9,  10 },  /* n+y→nz §2.4.1: C=yuki (bees) */
    { "inzobe",        NULL,              "yobe",       9,  10 },  /* n+y→nz §2.4.1: C=yobe */
    { "imbabazi",      NULL,              "babazi",     9,  10 },  /* n→m§3.3 before b */
    { "impamvu",       NULL,              "pamvu",      9,  10 },  /* n→m§3.3 before p */
    { "ingabo",        NULL,              "gabo",       9,  10 },
    { "inkumi",        NULL,              "kumi",       9,  10 },
    { "intumwa",       NULL,              "ntumwa",     9,  10 },  /* C=ntumwa: n+nt cluster */
    { "inyumba",       NULL,              "nyumba",     9,  10 },
    /* ── Class 11 (uru- singular; no standard plural for most) ─────────────── */
    { "urugo",         NULL,              "go",        11,  0  },
    { "urukundo",      NULL,              "kundo",     11,  0  },
    { "urugendo",      NULL,              "gendo",     11,  0  },
    { "uruzi",         NULL,              "zi",        11,  0  },
    { "urupfu",        NULL,              "pfu",       11,  0  },
    { "uruhande",      NULL,              "hande",     11,  0  },
    { "urugwi",        NULL,              "gwi",       11,  0  },
    { "urwego",        NULL,              "ego",       11,  0  },  /* u→w§1.1 */
    /* ── Class 12 ↔ 13 (diminutive singular / diminutive plural) ───────────── */
    { "akabaho",       "utubaho",         "baho",      12, 13  },
    { "akagabo",       "utugabo",         "gabo",      12, 13  },
    { "akarima",       "uturima",         "rima",      12, 13  },
    { "akabari",       "utubari",         "bari",      12, 13  },
    { "akabati",       "utubati",         "bati",      12, 13  },
    { "akazi",         "uduzi",           "zi",        12, 13  },
    /* ── Class 14 (abstract; no plural) ────────────────────────────────────── */
    { "ubutaka",       NULL,              "taka",      14,  0  },
    { "ubuzima",       NULL,              "zima",      14,  0  },
    { "ubwenge",       NULL,              "enge",      14,  0  },  /* u→w§1.1 */
    { "ubwami",        NULL,              "ami",       14,  0  },
    { "ubugingo",      NULL,              "gingo",     14,  0  },
    { "uburezi",       NULL,              "rezi",      14,  0  },
    { "ubwoba",        NULL,              "oba",       14,  0  },
    { "ubwatsi",       NULL,              "atsi",      14,  0  },  /* u→w§1.1 */
    { "ubwoko",        "amoko",           "oko",       14,  6  },  /* u→w§1.1 (bu+oko→bw), a→∅§1.1 (ma+oko→m) */
    { "ubutayu",       NULL,              "tayu",      14,  0  },
    { "ubuhemu",       NULL,              "hemu",      14,  0  },
    { "ubusabe",       NULL,              "sabe",      14,  0  },
    { "uburenganzira", NULL,              "renganzira",14,  0  },
    { "ubuhanga",      NULL,              "hanga",     14,  0  },
    { "ubwiza",        NULL,              "iza",       14,  0  },  /* u→w§1.1 */
    { "ubutegetsi",    NULL,              "tegetsi",   14,  0  },
    { "ubuhamya",      NULL,              "hamya",     14,  0  },
    /* ── Class 15 (verbal noun / infinitive; no plural) ────────────────────── */
    { "ukwezi",        NULL,              "wezi",      15,  0  },  /* u→w§1.1 */
    /* ── Class 16 (locative; no plural) ────────────────────────────────────── */
    { "ahantu",        NULL,              "ntu",       16,  0  },
    { NULL, NULL, NULL, 0, 0 }
};

bool kin_lookup_igicumbi(const char *word, char *igicumbi_out, int *class_out) {
    if (!word || !word[0]) return false;
    for (int i = 0; NOUN_PLURAL_PAIRS[i].singular; i++) {
        /* Match against singular form */
        if (strcmp(word, NOUN_PLURAL_PAIRS[i].singular) == 0) {
            if (igicumbi_out)
                strncpy(igicumbi_out, NOUN_PLURAL_PAIRS[i].igicumbi, KIN_MAX_STEM - 1);
            if (class_out) *class_out = NOUN_PLURAL_PAIRS[i].sg_class;
            return true;
        }
        /* Match against plural form (when distinct) */
        if (NOUN_PLURAL_PAIRS[i].plural &&
            strcmp(word, NOUN_PLURAL_PAIRS[i].plural) == 0) {
            if (igicumbi_out)
                strncpy(igicumbi_out, NOUN_PLURAL_PAIRS[i].igicumbi, KIN_MAX_STEM - 1);
            if (class_out) *class_out = NOUN_PLURAL_PAIRS[i].pl_class;
            return true;
        }
    }
    return false;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 8. String helpers for display
 * ══════════════════════════════════════════════════════════════════════════ */
const char *kin_pos_name(POS pos) {
    switch (pos) {
        case POS_NOUN:         return "Izina mbonera (Noun)";
        case POS_ADJECTIVE:    return "Ntera (Adjective)";
        case POS_RELATIVE_NOUN:return "Izina ntera (Relative noun)";
        case POS_COMPOUND_ADJ: return "Igisantera (Compound adj.)";
        case POS_VERB_INF:     return "Inshinga-infinitive (Verb inf.)";
        case POS_VERB_CONJ:    return "Inshinga-conjugated (Verb conj.)";
        case POS_PRONOUN:      return "Ikinyazina (Pronoun)";
        case POS_PREPOSITION:  return "Umugereka (Preposition)";
        case POS_CONJUNCTION:  return "Icyungo (Conjunction)";
        case POS_INTERJECTION: return "Irangamutima (Interjection)";
        case POS_ADVERB:       return "Akamamo (Adverb)";
        case POS_LOCATIVE:     return "Indangahantu (Locative)";
        case POS_VERB_PARTICLE:return "Ikegeranshinga (Verb particle)";
        case POS_FOREIGN:      return "Ijambo ry'amahanga (Foreign/Unknown)";
        case POS_PUNCTUATION:  return "Ibirango (Punctuation)";
        default:               return "Ntizwi (Unknown)";
    }
}

const char *kin_class_name(int c) {
    if (c < 1 || c > NOUN_CLASS_COUNT) return "N/A";
    return NOUN_CLASSES[c - 1].description;
}

const char *kin_pron_type_name(PronounType t) {
    switch (t) {
        case PRON_DEMONSTRATIVE: return "Ikinyazina nyereka (Demonstrative)";
        case PRON_PERSONAL:      return "Ikinyazina ngenga (Personal)";
        case PRON_POSSESSIVE:    return "Ikinyazina ngenera (Possessive connector)";
        case PRON_REFLEXIVE:     return "Ikinyazina ngenera ngenga (Associative possessive)";
        case PRON_RELATIVE:      return "Ikinyazina ngenera (Relative)";
        case PRON_INTERROGATIVE: return "Ikinyazina kibaza (Interrogative)";
        case PRON_INDEFINITE:    return "Ikinyazina ndafutura (Indefinite)";
        case PRON_CONCORDANCE:   return "Ikinyazina mboneranteko (Concordance)";
        case PRON_NUMERICAL:     return "Ikinyazina nyamubaro (Numerical)";
        case PRON_VOCATIVE:      return "Ikinyazina mpamagazi (Vocative)";
        default:                 return "Unknown pronoun type";
    }
}

const char *kin_verb_tense_name(VerbTense t) {
    switch (t) {
        case TENSE_PRESENT:      return "Indagihe y'ako kanya (Present – immediate)";
        case TENSE_PRESENT_NORA: return "Indagihe y'ubusanzwe (Present – habitual)";
        case TENSE_PAST_PERF:    return "Impitakare (Impitagihe – recent past)";
        case TENSE_PAST_IMPF:    return "Impitakera (Impitagihe – remote/habitual past)";
        case TENSE_FUTURE:       return "Inzagihe (Future)";
        case TENSE_SUBJUNCTIVE:  return "Ikigombero (Subjunctive: SP+stem+e)";
        case TENSE_NARRATIVE:    return "Inkurikizo (Narrative/Sequential: SP+ka+stem+a)";
        case TENSE_OPTATIVE:     return "Inyifurizo (Optative: SP+ra+ka+stem+a)";
        case TENSE_IMPERATIVE:   return "Integeko (Imperative: bare stem+a)";
        case TENSE_CONDITIONAL:  return "Inziganyo (Conditional: SP+a+stem+a)";
        /* Copula forms of kuba (to be/exist) with locative suffix -ho/-mo/-yo *
         * Underlying morphology: ku-b-a-ho (INF+root+FV+post-final locative)  *
         * The root is 'b'; final vowel 'a' is retained before consonant 'h'.  */
        case TENSE_COPULA_PAST:  return "Impitagihe y'inshinga nkene (Past copula: yariho, wariho…)";
        case TENSE_COPULA_PRES:  return "Indagihe y'inshinga nkene (Present copula: ariho, ndiho, iriho…)";
        case TENSE_NEG_RELATIVE:    return "Inshinga nkurikije y'ubunyagatifu (Neg. participial: itagira, utagira\xe2\x80\xa6)";
        case TENSE_NEG_ANTERIOR:    return "Inshinga y'ubunyagatifu bw'imbere (Neg. anterior \xe2\x80\x93 \"not yet\": kataraba, itaravuba\xe2\x80\xa6)";
        case TENSE_SUBJUNCTIVE_LOC: return "Ikigombero + ahantu (Subjunctive+locative: habeho, abeho, mubemo…)";
        case TENSE_STATIVE_POSS:    return "Indagihe y'ugutunga (Stative possessive: bifite, afite, nfite…)";
        default:                 return "";
    }
}

const char *kin_verb_ext_name(VerbExtension e) {
    switch (e) {
        case VEXT_PASSIVE:     return "Imbundo (Passive: -w-)";
        case VEXT_CAUSATIVE:   return "Integeko (Causative: -ish-/-esh-)";
        case VEXT_APPLICATIVE: return "Ikirango (Applicative/Benefactive: -ir-/-er-)";
        case VEXT_RECIPROCAL:  return "Igisubizo (Reciprocal: -an-)";
        case VEXT_REFLEXIVE:   return "Imbundo yo kwisanzura (Reflexive: i- elided in bare form)";
        case VEXT_STATIVE:     return "Ngirika (Stative/Potential: -ik-/-ek-)";
        case VEXT_REVERSIVE:   return "Ngiruka/Ngirura (Reversive: -uk-/-ur-)";
        case VEXT_CAUSATIVE_Y: return "Ngiza (Causative-y: r+y→z, §1.3)";
        default:               return "";
    }
}

const char *kin_gram_role_name(GramRole r) {
    switch (r) {
        case GRAM_ROLE_MAIN_VERB:   return "Inshinga nkuru (Main verb)";
        case GRAM_ROLE_AUXILIARY:   return "Inshinga nkene (Auxiliary/Copula)";
        case GRAM_ROLE_RELATIVE:    return "Inshinga nkurikije (Relative clause)";
        case GRAM_ROLE_PARTICIPIAL: return "Inshinga nkurikije y'ubunyagatifu (Neg. participial)";
        case GRAM_ROLE_COMPLEMENT:  return "Igisangizo (Complement clause: ngo/ko/nuko)";
        case GRAM_ROLE_SEQUENTIAL:  return "Inkurikizo (Sequential narrative: ka)";
        case GRAM_ROLE_VERBAL_NOUN: return "Izina ryaturutse ku nshinga (Verbal noun)";
        default:                    return "";
    }
}

/* Returns the surface object-marker (OM) prefix string for a noun class.
 * Based on OM_TABLE in morphology.c (book p.60).
 * Returns "" for class 0 (unknown/personal) or out-of-range classes. */
const char *kin_om_str(int cls) {
    switch (cls) {
        case  1: return "mu";   case  2: return "ba";
        case  3: return "wu";   case  4: return "yi";
        case  5: return "ri";   case  6: return "ya";
        case  7: return "ki";   case  8: return "bi";
        case  9: return "n";    case 10: return "zi";
        case 11: return "ru";   case 12: return "ka";
        case 13: return "tu";   case 14: return "bu";
        case 15: return "ku";   case 16: return "ha";
        default: return "";
    }
}
