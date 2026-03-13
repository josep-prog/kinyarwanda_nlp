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
 * 1. NOUN CLASSES (Inteko) – 16 classes, p.61-62
 *
 * Columns:
 *  num  | prefix (D+RT) | RT    | adj concordance | poss connector | subj prefix | description
 *
 * Note: classes 1/3 and 2/4 share prefixes but differ in semantics:
 *   Nt.1/Nt.2 = human nouns  (umuntu / abantu)
 *   Nt.3/Nt.4 = tree/thing nouns (umuti / imiti)
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
 * 2. ADJECTIVE STEMS (Ibicumbi by'intera) – p.66-67
 *
 * These 19+ stems are the complete list from the book (section 3.4.3).
 * Any word built as: concordance_prefix + one of these stems = adjective.
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
    NULL
};

bool kin_is_adj_stem(const char *stem) {
    for (int i = 0; ADJ_STEMS[i]; i++)
        if (strcmp(stem, ADJ_STEMS[i]) == 0) return true;
    return false;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 3. PRONOUNS (Ibinyazina) – p.89-97
 *
 * Each entry: { surface form, PronounType, noun_class }
 * class 0 = applies to multiple / not class-specific
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

    /* ── Ikinyazina kibaza (interrogative pronouns) ─────────────────────  */
    { "nde",    PRON_INTERROGATIVE, 0 },  /* who?                          */
    { "iki",    PRON_INTERROGATIVE, 0 },  /* what? (thing)                 */
    { "iyihe",  PRON_INTERROGATIVE, 0 },  /* which?                        */
    { "ryari",  PRON_INTERROGATIVE, 0 },  /* when?                         */
    { "hehe",   PRON_INTERROGATIVE, 0 },  /* where?                        */
    { "bite",   PRON_INTERROGATIVE, 0 },  /* how?                          */
    { "bangahe",PRON_INTERROGATIVE, 0 },  /* how many?                     */
    { "kuki",   PRON_INTERROGATIVE, 0 },  /* why?                          */

    /* ── Ikinyazina ndafutura (indefinite pronouns) ─────────────────────  */
    { "umwe",   PRON_INDEFINITE, 1 },
    /* "umuntu" is a noun, not listed here — context decides              */
    { "bamwe",  PRON_INDEFINITE, 2 },
    { "kimwe",  PRON_INDEFINITE, 7 },
    { "bimwe",  PRON_INDEFINITE, 8 },
    { "rimwe",  PRON_INDEFINITE, 5 },
    { "bumwe",  PRON_INDEFINITE, 14 },
    { "rumwe",  PRON_INDEFINITE, 11 },
    { "kamwe",  PRON_INDEFINITE, 12 },
    { "tumwe",  PRON_INDEFINITE, 13 },

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
 * 4. INVARIABLE WORDS (Amagambo adahinduka) – p.89
 *    Umugereka (ingera), Icyungo, Irangamutima, Akamamo, Indangahantu,
 *    Ikegeranshinga
 * ══════════════════════════════════════════════════════════════════════════ */
typedef struct { const char *word; POS pos; } InvEntry;

static const InvEntry INVARIABLES[] = {
    /* ── Umugereka / Ingera (prepositions) ─────────────────────────────── */
    { "mu",      POS_PREPOSITION },
    { "ku",      POS_PREPOSITION },
    { "i",       POS_PREPOSITION },
    { "kuri",    POS_PREPOSITION },
    { "nka",     POS_PREPOSITION },  /* like / as                          */
    { "bwa",     POS_PREPOSITION },  /* at the time of                     */
    /* ── Icyungo (conjunctions) ──────────────────────────────────────────  */
    { "na",      POS_CONJUNCTION },  /* and / with                         */
    { "kandi",   POS_CONJUNCTION },  /* and also                           */
    { "cyangwa", POS_CONJUNCTION },  /* or                                 */
    { "naho",    POS_CONJUNCTION },  /* whereas / while                    */
    { "ariko",   POS_CONJUNCTION },  /* but                                */
    { "nyamara", POS_CONJUNCTION },  /* however                            */
    { "keretse", POS_CONJUNCTION },  /* except / unless                    */
    { "mbese",   POS_CONJUNCTION },  /* so / therefore                     */
    { "kuko",    POS_CONJUNCTION },  /* because                            */
    { "kubera",  POS_CONJUNCTION },  /* because of                         */
    { "none",    POS_CONJUNCTION },  /* and then / now                     */
    { "maze",    POS_CONJUNCTION },  /* and then (sequence)                */
    { "ngo",     POS_VERB_PARTICLE },/* that / in order to (ikegeranshinga)*/
    { "ko",      POS_VERB_PARTICLE },/* that (complementizer)              */
    /* ── Irangamutima (interjections) ──────────────────────────────────── */
    { "ah",      POS_INTERJECTION },
    { "aye",     POS_INTERJECTION },
    { "eeh",     POS_INTERJECTION },
    { "hee",     POS_INTERJECTION },
    { "ooh",     POS_INTERJECTION },
    { "wa",      POS_INTERJECTION },
    { "wee",     POS_INTERJECTION },
    { "pyo",     POS_INTERJECTION },
    { "asyi",    POS_INTERJECTION },
    { "dore",    POS_INTERJECTION }, /* look! / behold!                   */
    { "ni",      POS_CONJUNCTION  }, /* copula / is (equative verb)        */
    { "si",      POS_CONJUNCTION  }, /* negative copula / is not           */
    /* ── Copula forms of kuba (to be) ──────────────────────────────────── */
    { "ari",     POS_VERB_CONJ   }, /* 3sg class1 copula: (s)he/it is     */
    { "ndi",     POS_VERB_CONJ   }, /* 1sg copula: I am                   */
    { "uri",     POS_VERB_CONJ   }, /* 2sg copula: you are                */
    { "turi",    POS_VERB_CONJ   }, /* 1pl copula: we are                 */
    { "muri",    POS_VERB_CONJ   }, /* 2pl copula: you all are            */
    { "bari",    POS_VERB_CONJ   }, /* 3pl copula: they are               */
    { "kari",    POS_VERB_CONJ   }, /* Nt.12 copula: it (ka-class) is     */
    { "biri",    POS_VERB_CONJ   }, /* Nt.8 copula: they are              */
    { "riri",    POS_VERB_CONJ   }, /* Nt.5 copula: it is                 */
    { "ziri",    POS_VERB_CONJ   }, /* Nt.10 copula: they are             */
    /* ── Quotative & other particles ──────────────────────────────────── */
    { "iti",     POS_VERB_PARTICLE}, /* quotative particle: saying "..."   */
    { "n",       POS_CONJUNCTION  }, /* elided 'na' before apostrophe      */
    { "y",       POS_CONJUNCTION  }, /* elided 'ya' possessive connector   */
    { "k",       POS_PREPOSITION  }, /* elided 'ku/ka' before apostrophe   */
    { "b",       POS_CONJUNCTION  }, /* elided 'ba' before apostrophe      */
    { "w",       POS_CONJUNCTION  }, /* elided 'wa' before apostrophe      */
    { "se",      POS_ADVERB       }, /* father (sometimes invariable use)  */
    { "ko",      POS_VERB_PARTICLE}, /* complementizer 'that'              */
    { "yego",    POS_ADVERB       }, /* yes                               */
    { "oya",     POS_ADVERB       }, /* no                                */
    { "yee",     POS_INTERJECTION },
    /* ── Akamamo (adverbs) ──────────────────────────────────────────────   */
    { "vuba",       POS_ADVERB }, /* quickly / soon                        */
    { "cyane",      POS_ADVERB }, /* very / a lot                          */
    { "gato",       POS_ADVERB }, /* a little                              */
    { "hanze",      POS_ADVERB }, /* outside                               */
    { "imbere",     POS_ADVERB }, /* in front / before                     */
    { "mbere",      POS_ADVERB }, /* firstly / before (short form of imbere)*/
    { "inyuma",     POS_ADVERB }, /* behind / after                        */
    { "hejuru",     POS_ADVERB }, /* above / up                            */
    { "munsi",      POS_ADVERB }, /* below / down                          */
    { "neza",       POS_ADVERB }, /* well / nicely                         */
    { "bidasanzwe", POS_ADVERB }, /* unusually                             */
    { "buri",       POS_ADVERB }, /* every                                 */
    { "ubwira",     POS_ADVERB }, /* alone                                 */
    { "hamwe",      POS_ADVERB }, /* together                              */
    { "gusa",       POS_ADVERB }, /* only / just                           */
    { "kera",       POS_ADVERB }, /* long ago / previously                 */
    { "ubu",        POS_ADVERB }, /* now                                   */
    { "ejo",        POS_ADVERB }, /* yesterday / tomorrow (context)        */
    /* "uyu" is a demonstrative pronoun; NOT listed here                  */
    { "rimwe",      POS_ADVERB }, /* sometimes / once                      */
    { "ntaho",      POS_ADVERB }, /* nowhere                               */
    /* ── Indangahantu (locatives / place adverbs) ───────────────────────   */
    { "hano",    POS_LOCATIVE }, /* here                                   */
    { "aho",     POS_LOCATIVE }, /* there (relative)                       */
    { "hariya",  POS_LOCATIVE }, /* over there                             */
    { "hejuru",  POS_LOCATIVE }, /* above                                  */
    { "munsi",   POS_LOCATIVE }, /* below                                  */
    { "hanze",   POS_LOCATIVE }, /* outside                                */
    { "imbere",  POS_LOCATIVE }, /* in front                               */
    { "inyuma",  POS_LOCATIVE }, /* behind                                 */
    { "hagati",  POS_LOCATIVE }, /* in the middle                          */
    { "hafi",    POS_LOCATIVE }, /* near                                   */
    { "hose",    POS_LOCATIVE }, /* everywhere                             */
    { "hahandi", POS_LOCATIVE }, /* elsewhere                              */
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
 * 5. KNOWN VERB STEMS
 *    Sources:
 *      – "Ikinyarwanda Amashuri Nderabarezi TTC" (REB 2020)
 *      – Bibiliya Yera 2001 corpus (top-frequency infinitives/conjugations)
 *    Format: bare stem (what follows ku/gu/kw/gw prefix before final -a)
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
    "koresh",   /* gukoresha   – to use                                   */
    "kor",      /* gukora      – to work / do                             */
    "ram",      /* gurama      – to grow crops                            */
    "bik",      /* kubika      – to store                                 */
    "hind",     /* guhinda     – to plant                                 */
    "hindur",   /* guhindura   – to change / transform                    */
    "himbir",   /* guhimbira   – to compose for                           */
    "ambuk",    /* kwambuka    – to cross                                 */
    "himb",     /* guhimba     – to compose / invent                      */
    "er",       /* gutera      – to plant / strike                        */
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
    "jyan",     /* kujyana     – to take / go together                    */
    "iruk",     /* kwiruka     – to run                                   */
    "irukan",   /* kwirukana   – to chase away                            */
    "inginga",  /* kwinginga   – to beg / plead                           */
    "tonder",   /* kwitondera  – to be careful / pay attention            */
    "rushy",    /* kurusha     – to surpass / be more than                */
    "tabur",    /* gutabara    – to rescue / save                         */
    "ic",       /* kwica       – to kill                                  */
    "jyen",     /* kujyena     – to walk along                            */
    "bw",       /* kubwa       – to fall (dup, harmless)                  */
    NULL
};

bool kin_is_known_verb_stem(const char *stem) {
    for (int i = 0; VERB_STEMS[i]; i++)
        if (strcmp(stem, VERB_STEMS[i]) == 0) return true;
    return false;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 6. KNOWN NOUN STEMS – with their primary noun class
 *    These are used as a fallback when morphological analysis is ambiguous
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
    { "wenge",   14 }, /* ubwenge – wisdom (nt.14)          */
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
    { "watsi",   14 }, /* ubwatsi – grass / vegetation (Nt.14) */
    { "miro",     4 }, /* imirimo – works / tasks (Nt.4)   */
    { "ko",      12 }, /* amoko – kinds / types (Nt.6)     */
    { "gibo",    14 }, /* ubugibo – blessing                */
    { "bi",       9 }, /* ibikorwa – deeds                  */
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
    /* Common Nt.6 mass nouns */
    { "amazi",    6,  "zi"     },  /* water                               */
    { "amahoro",  6,  "horo"   },  /* peace / greetings                   */
    { "amafaranga",6, "faranga"},  /* money                               */
    { "amata",    6,  "ta"     },  /* milk                                */
    /* Other common irregulars */
    { "ejo",      0,  ""       },  /* yesterday / tomorrow (context-dep.) */
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
 * 7. String helpers for display
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
        case PRON_POSSESSIVE:    return "Ikinyazina ngenera (Possessive)";
        case PRON_REFLEXIVE:     return "Ikinyazina ngenera ngenga (Reflexive)";
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
        case TENSE_PAST_PERF:    return "Impitakere (Impitagihe – recent past)";
        case TENSE_PAST_IMPF:    return "Impitakera (Impitagihe – remote/habitual past)";
        case TENSE_FUTURE:       return "Inzagihe (Future)";
        case TENSE_SUBJUNCTIVE:  return "Isabira (Subjunctive/Conditional)";
        case TENSE_NARRATIVE:    return "Inshinga y'imigani (Narrative)";
        default:                 return "";
    }
}
