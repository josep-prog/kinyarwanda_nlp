/*
 * g2p.c
 * Kinyarwanda Grapheme-to-Phoneme (G2P) converter and text normalizer.
 *
 * Kinyarwanda orthography is almost perfectly phonemic.  The main complexity
 * is digraph detection (longest-match-first) and the mapping of digraphs like
 * 'sh', 'ny', 'ts', 'nsh' to single phonemes.
 *
 * Algorithm:
 *   1. Normalize input (lowercase, expand contractions, digits, punctuation)
 *   2. For each word, scan left-to-right with longest-match-first:
 *        a. Try 3-character patterns: nsh, nzw, ngw, shw
 *        b. Try 2-character patterns: sh, ny, ts, bw, cw, dw, fw, gw, hw,
 *           kw, mw, nw, pw, rw, sw, tw, vw, yw, zw, mb, mf, mp, mv, nd,
 *           ng, nj, nk, nz, nt
 *        c. Fall back to single character
 *   3. Build KinPhonemeSeq with ID array + ASCII repr string
 *
 * Sources:
 *   - RALC 2017 Orthographic Standard (Amategeko y'Igenantego)
 *   - REB TTC 2020 "Ikinyarwanda Amashuri Nderabarezi"
 *   - Native-speaker validation
 */

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/g2p.h"
#include "../include/kinyarwanda.h"

/* ══════════════════════════════════════════════════════════════════════════
 * 1. PHONEME TABLE
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    KinPhonemeID  id;
    const char   *token;     /* ASCII output token, e.g. "sh"              */
    const char   *ipa;       /* IPA string, e.g. "ʃ"                       */
    const char   *grapheme;  /* Orthographic input that maps here          */
} PhonemeEntry;

static const PhonemeEntry PHONEME_TABLE[] = {
    { PH_NULL,       "∅",   "∅",    ""    },

    /* Vowels */
    { PH_A,          "a",   "a",    "a"   },
    { PH_E,          "e",   "e",    "e"   },
    { PH_I,          "i",   "i",    "i"   },
    { PH_O,          "o",   "o",    "o"   },
    { PH_U,          "u",   "u",    "u"   },

    /* Simple consonants */
    { PH_B,          "b",   "b",    "b"   },
    { PH_D,          "d",   "d",    "d"   },
    { PH_F,          "f",   "f",    "f"   },
    { PH_G,          "g",   "ɡ",    "g"   },
    { PH_H,          "h",   "h",    "h"   },
    { PH_K,          "k",   "k",    "k"   },
    { PH_L,          "l",   "l",    "l"   },
    { PH_M,          "m",   "m",    "m"   },
    { PH_N,          "n",   "n",    "n"   },
    { PH_P,          "p",   "p",    "p"   },
    { PH_R,          "r",   "ɾ",    "r"   },
    { PH_S,          "s",   "s",    "s"   },
    { PH_T,          "t",   "t",    "t"   },
    { PH_V,          "v",   "v",    "v"   },
    { PH_W,          "w",   "w",    "w"   },
    { PH_Y,          "y",   "j",    "y"   },
    { PH_Z,          "z",   "z",    "z"   },

    /* Affricates / digraph consonants */
    { PH_C,          "ch",  "tʃ",   "c"   },
    { PH_J,          "j",   "dʒ",   "j"   },
    { PH_SH,         "sh",  "ʃ",    "sh"  },
    { PH_NY,         "ny",  "ɲ",    "ny"  },
    { PH_TS,         "ts",  "ts",   "ts"  },

    /* Labialized consonants */
    { PH_BW,         "bw",  "bʷ",   "bw"  },
    { PH_CW,         "chw", "tʃʷ",  "cw"  },
    { PH_DW,         "dw",  "dʷ",   "dw"  },
    { PH_FW,         "fw",  "fʷ",   "fw"  },
    { PH_GW,         "gw",  "ɡʷ",   "gw"  },
    { PH_HW,         "hw",  "hʷ",   "hw"  },
    { PH_KW,         "kw",  "kʷ",   "kw"  },
    { PH_MW,         "mw",  "mʷ",   "mw"  },
    { PH_NW,         "nw",  "nʷ",   "nw"  },
    { PH_PW,         "pw",  "pʷ",   "pw"  },
    { PH_RW,         "rw",  "ɾʷ",   "rw"  },
    { PH_SW,         "sw",  "sʷ",   "sw"  },
    { PH_TW,         "tw",  "tʷ",   "tw"  },
    { PH_VW,         "vw",  "vʷ",   "vw"  },
    { PH_YW,         "yw",  "jʷ",   "yw"  },
    { PH_ZW,         "zw",  "zʷ",   "zw"  },
    { PH_SHW,        "shw", "ʃʷ",   "shw" },

    /* Prenasalized clusters */
    { PH_MB,         "mb",  "mb",   "mb"  },
    { PH_MF,         "mf",  "mf",   "mf"  },
    { PH_MP,         "mp",  "mp",   "mp"  },
    { PH_MV,         "mv",  "mv",   "mv"  },
    { PH_ND,         "nd",  "nd",   "nd"  },
    { PH_NG,         "ng",  "ŋɡ",   "ng"  },
    { PH_NGW,        "ngw", "ŋɡʷ",  "ngw" },
    { PH_NJ,         "nj",  "ndʒ",  "nj"  },
    { PH_NK,         "nk",  "ŋk",   "nk"  },
    { PH_NSH,        "nsh", "nʃ",   "nsh" },
    { PH_NT,         "nt",  "nt",   "nt"  },
    { PH_NZ,         "nz",  "nz",   "nz"  },
    { PH_NZW,        "nzw", "nzʷ",  "nzw" },

    /* Prosodic markers */
    { PH_WORD_BOUND,  "|",  "|",    " "   },
    { PH_PAUSE_SHORT, ",",  ",",    ","   },
    { PH_PAUSE_LONG,  ".",  ".",    "."   },
};

const char *kin_phoneme_token(KinPhonemeID id) {
    if (id < 0 || id >= PH_COUNT) return "?";
    for (int i = 0; i < (int)(sizeof(PHONEME_TABLE)/sizeof(PHONEME_TABLE[0])); i++)
        if (PHONEME_TABLE[i].id == id) return PHONEME_TABLE[i].token;
    return "?";
}

const char *kin_phoneme_ipa(KinPhonemeID id) {
    if (id < 0 || id >= PH_COUNT) return "?";
    for (int i = 0; i < (int)(sizeof(PHONEME_TABLE)/sizeof(PHONEME_TABLE[0])); i++)
        if (PHONEME_TABLE[i].id == id) return PHONEME_TABLE[i].ipa;
    return "?";
}

int kin_phoneme_count(void) { return (int)PH_COUNT; }

/* ══════════════════════════════════════════════════════════════════════════
 * 2. GRAPHEME → PHONEME MAPPING TABLE
 *
 * Ordered longest-match-first.  The scanner tries each entry in order
 * and takes the first match at the current position.
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct { const char *graph; KinPhonemeID phone; } G2PRule;

/* 3-character rules first */
static const G2PRule G2P_3[] = {
    { "nsh", PH_NSH },
    { "nzw", PH_NZW },
    { "ngw", PH_NGW },
    { "shw", PH_SHW },
    { NULL,  PH_NULL }
};

/* 2-character rules second */
static const G2PRule G2P_2[] = {
    /* Digraph consonants */
    { "sh", PH_SH  },
    { "ny", PH_NY  },
    { "ts", PH_TS  },
    /* Prenasalized clusters — checked before single 'n' */
    { "mb", PH_MB  },
    { "mf", PH_MF  },
    { "mp", PH_MP  },
    { "mv", PH_MV  },
    { "nd", PH_ND  },
    { "ng", PH_NG  },
    { "nj", PH_NJ  },
    { "nk", PH_NK  },
    { "nt", PH_NT  },
    { "nz", PH_NZ  },
    /* Labialized consonants */
    { "bw", PH_BW  },
    { "cw", PH_CW  },
    { "dw", PH_DW  },
    { "fw", PH_FW  },
    { "gw", PH_GW  },
    { "hw", PH_HW  },
    { "kw", PH_KW  },
    { "mw", PH_MW  },
    { "nw", PH_NW  },
    { "pw", PH_PW  },
    { "rw", PH_RW  },
    { "sw", PH_SW  },
    { "tw", PH_TW  },
    { "vw", PH_VW  },
    { "yw", PH_YW  },
    { "zw", PH_ZW  },
    { NULL, PH_NULL }
};

/* Single-character fallback */
static const G2PRule G2P_1[] = {
    { "a",  PH_A   },
    { "b",  PH_B   },
    { "c",  PH_C   },   /* 'c' = /tʃ/ in Kinyarwanda */
    { "d",  PH_D   },
    { "e",  PH_E   },
    { "f",  PH_F   },
    { "g",  PH_G   },
    { "h",  PH_H   },
    { "i",  PH_I   },
    { "j",  PH_J   },
    { "k",  PH_K   },
    { "l",  PH_L   },
    { "m",  PH_M   },
    { "n",  PH_N   },
    { "o",  PH_O   },
    { "p",  PH_P   },
    { "r",  PH_R   },
    { "s",  PH_S   },
    { "t",  PH_T   },
    { "u",  PH_U   },
    { "v",  PH_V   },
    { "w",  PH_W   },
    { "y",  PH_Y   },
    { "z",  PH_Z   },
    { NULL, PH_NULL }
};

/* ══════════════════════════════════════════════════════════════════════════
 * 3. TEXT NORMALIZATION
 * ══════════════════════════════════════════════════════════════════════════ */

/* Kinyarwanda number words (cardinal, used in text normalization).
 * Kinyarwanda number system:
 *   Units 1-9 have two registers (formal/informal); we use the common form.
 *   Tens: makumyabiri(20), mirongo(30+), etc.
 *   ijana = 100, igihumbi = 1000
 */
static const char *DIGITS_UNITS[] = {
    "zeru",        /* 0  — loanword, no native form */
    "rimwe",       /* 1  — or "umwe" (depends on noun class) */
    "kabiri",      /* 2  */
    "gatatu",      /* 3  */
    "kane",        /* 4  */
    "gatanu",      /* 5  */
    "gatandatu",   /* 6  */
    "karindwi",    /* 7  */
    "umunani",     /* 8  */
    "icyenda",     /* 9  */
};

static const char *DIGITS_TENS[] = {
    "",             /* 0 */
    "icumi",        /* 10 */
    "makumyabiri",  /* 20 */
    "mirongo itatu",/* 30 */
    "mirongo ine",  /* 40 */
    "mirongo itanu",/* 50 */
    "mirongo itandatu", /* 60 */
    "mirongo irindwi",  /* 70 */
    "mirongo inani",    /* 80 */
    "mirongo icyenda",  /* 90 */
};

/* Expand a number 0-9999 into Kinyarwanda words.
 * Returns number of chars written (not including NUL). */
static int expand_number(long n, char *buf, size_t bufsz) {
    if (bufsz == 0) return 0;
    buf[0] = '\0';
    int written = 0;

    if (n < 0) {
        int w = snprintf(buf, bufsz, "ubuzima bw'uburemere buke ");
        written += w; buf += w; bufsz -= (size_t)w;
        n = -n;
    }

    if (n == 0) {
        return snprintf(buf, bufsz, "zeru");
    }

    /* Thousands */
    if (n >= 1000) {
        long thou = n / 1000;
        int w;
        if (thou == 1) {
            w = snprintf(buf, bufsz, "igihumbi ");
        } else {
            w = snprintf(buf, bufsz, "ibihumbi %s ", DIGITS_UNITS[thou < 10 ? thou : 0]);
        }
        if (w > 0) { written += w; buf += w; bufsz -= (size_t)w; }
        n %= 1000;
        if (n > 0 && bufsz > 4) {
            int w2 = snprintf(buf, bufsz, "na ");
            written += w2; buf += w2; bufsz -= (size_t)w2;
        }
    }

    /* Hundreds */
    if (n >= 100) {
        long hund = n / 100;
        int w;
        if (hund == 1) {
            w = snprintf(buf, bufsz, "ijana ");
        } else {
            w = snprintf(buf, bufsz, "amagana %s ", hund < 10 ? DIGITS_UNITS[hund] : "?");
        }
        if (w > 0) { written += w; buf += w; bufsz -= (size_t)w; }
        n %= 100;
        if (n > 0 && bufsz > 4) {
            int w2 = snprintf(buf, bufsz, "na ");
            written += w2; buf += w2; bufsz -= (size_t)w2;
        }
    }

    /* Tens */
    if (n >= 10) {
        long tens = n / 10;
        int w = snprintf(buf, bufsz, "%s ", DIGITS_TENS[tens]);
        if (w > 0) { written += w; buf += w; bufsz -= (size_t)w; }
        n %= 10;
        if (n > 0 && bufsz > 4) {
            int w2 = snprintf(buf, bufsz, "na ");
            written += w2; buf += w2; bufsz -= (size_t)w2;
        }
    }

    /* Units */
    if (n > 0) {
        int w = snprintf(buf, bufsz, "%s", DIGITS_UNITS[n]);
        if (w > 0) { written += w; buf += w; bufsz -= (size_t)w; }
    }

    return written;
}

/* Common abbreviation expansions.
 * Sorted by length (longest first) so that "km" doesn't eat into "km²" etc.
 */
typedef struct { const char *abbr; const char *expansion; } AbbrevEntry;
static const AbbrevEntry ABBREVS[] = {
    { "km",     "kilometero"  },
    { "kg",     "kilogiramu"  },
    { "cm",     "santimetero" },
    { "mm",     "milimetero"  },
    { "mg",     "miligiramu"  },
    { "dl",     "desilite"    },
    { "ml",     "mililitre"   },
    { "nr",     "nomero"      },
    { "no",     "nomero"      },
    { "frw",    "amafaranga"  },
    { "rwf",    "amafaranga"  },
    { "usd",    "amadolari"   },
    { "dr",     "dogiteri"    },
    { "prof",   "profeseri"   },
    { NULL,     NULL          }
};

void kin_normalize_text(const char *input, char *output, size_t outsize) {
    if (!input || !output || outsize < 2) return;
    char *out = output;
    char *end = output + outsize - 1;
    const char *p = input;

    while (*p && out < end) {

        /* ── Apostrophe contraction: n'amazi → na amazi ──────────────── *
         * In Kinyarwanda, 'n'' is a contraction of 'na' (and/with) before     *
         * a vowel-initial word.  Restore the elided 'a' before separating.    *
         * Other single-letter contractions (e.g. y' from ya, k' from ku):     *
         * also restore the missing vowel:                                       *
         *   n' → na   (na = and/with)                                         *
         *   y' → ya   (ya = past SP / Nt.6 pres. SP / possessive particle)   *
         *   k' → ku   (ku = locative / infinitive prefix)                     *
         *   m' → mu   (mu = inside / Nt.1 RT)                                 *
         *   All others: just replace apostrophe with a space.                  */
        if (*p == '\'' && p > input) {
            if (isalpha((unsigned char)*(p-1))) {
                char prev = (char)tolower((unsigned char)*(p-1));
                /* Restore elided vowel for known single-letter contractions */
                if (prev == 'n' && out < end) *out++ = 'a';
                else if (prev == 'y' && out < end) *out++ = 'a';
                else if (prev == 'k' && out < end) *out++ = 'u';
                else if (prev == 'm' && out < end) *out++ = 'u';
                /* Separate with a space */
                if (out < end) *out++ = ' ';
                p++;
                continue;
            }
        }

        /* ── Digit run → Kinyarwanda number word ─────────────────────── */
        if (isdigit((unsigned char)*p)) {
            long n = strtol(p, (char **)&p, 10);
            char numbuf[128];
            int nw = expand_number(n, numbuf, sizeof(numbuf));
            /* Trim trailing space from numbuf */
            while (nw > 0 && numbuf[nw-1] == ' ') { numbuf[--nw] = '\0'; }
            for (int i = 0; i < nw && out < end; i++) *out++ = numbuf[i];
            continue;
        }

        /* ── Abbreviation at word boundary ────────────────────────────── */
        if (isalpha((unsigned char)*p) &&
            (p == input || !isalpha((unsigned char)*(p-1)))) {
            bool matched = false;
            for (int i = 0; ABBREVS[i].abbr; i++) {
                size_t alen = strlen(ABBREVS[i].abbr);
                /* Case-insensitive compare */
                bool eq = true;
                for (size_t j = 0; j < alen; j++) {
                    if (tolower((unsigned char)p[j]) != ABBREVS[i].abbr[j]) {
                        eq = false; break;
                    }
                }
                /* Must be followed by non-alpha (word boundary) */
                if (eq && !isalpha((unsigned char)p[alen])) {
                    const char *exp = ABBREVS[i].expansion;
                    size_t elen = strlen(exp);
                    for (size_t j = 0; j < elen && out < end; j++) *out++ = exp[j];
                    p += alen;
                    matched = true;
                    break;
                }
            }
            if (matched) continue;
        }

        /* ── Punctuation → prosodic marker ────────────────────────────── */
        if (*p == '.' || *p == '!' || *p == '?') {
            /* Sentence-final pause: write space then dot */
            if (out < end && out > output && *(out-1) != ' ') *out++ = ' ';
            if (out < end) *out++ = '.';
            p++;
            continue;
        }
        if (*p == ',') {
            if (out < end && out > output && *(out-1) != ' ') *out++ = ' ';
            if (out < end) *out++ = ',';
            p++;
            continue;
        }
        if (*p == '-' || *p == ';' || *p == ':') {
            /* Treat as short pause */
            if (out < end) *out++ = ' ';
            p++;
            continue;
        }

        /* ── Default: pass through (collapse multiple spaces) ────────── */
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            if (out > output && *(out-1) != ' ') *out++ = ' ';
            p++;
            continue;
        }

        if (out < end) *out++ = *p;
        p++;
    }

    *out = '\0';

    /* Trim trailing space */
    size_t len = strlen(output);
    while (len > 0 && output[len-1] == ' ') output[--len] = '\0';
}

/* ══════════════════════════════════════════════════════════════════════════
 * 4. G2P CORE — SINGLE WORD CONVERSION
 * ══════════════════════════════════════════════════════════════════════════ */

/*
 * Append a phoneme to the sequence.
 * Also appends the ASCII token to seq->repr with a space separator.
 */
static bool seq_append(KinPhonemeSeq *seq, KinPhonemeID ph) {
    if (seq->count >= G2P_MAX_PHONEMES - 1) return false;
    seq->phones[seq->count++] = ph;

    const char *tok = kin_phoneme_token(ph);
    size_t repr_len = strlen(seq->repr);
    size_t tok_len  = strlen(tok);

    /* Space separator (not before the very first token) */
    if (repr_len > 0 && seq->repr[repr_len-1] != '|' &&
        repr_len + 1 + tok_len + 1 < G2P_MAX_REPR) {
        seq->repr[repr_len++] = ' ';
        seq->repr[repr_len]   = '\0';
    }

    if (repr_len + tok_len + 1 < G2P_MAX_REPR) {
        memcpy(seq->repr + repr_len, tok, tok_len + 1);
    }
    return true;
}

bool kin_g2p_word(const char *word, KinPhonemeSeq *out) {
    if (!word || !out) return false;

    /* Lowercase copy */
    char lower[G2P_MAX_REPR];
    size_t wlen = strlen(word);
    if (wlen == 0) return false;
    if (wlen >= sizeof(lower)) wlen = sizeof(lower) - 1;
    for (size_t i = 0; i < wlen; i++) lower[i] = (char)tolower((unsigned char)word[i]);
    lower[wlen] = '\0';

    /* Clear output */
    out->count = 0;
    out->repr[0] = '\0';

    const char *p = lower;
    bool any = false;

    while (*p) {
        bool matched = false;

        /* Prosodic markers (passed through from normalizer) */
        if (*p == '.') {
            seq_append(out, PH_PAUSE_LONG);
            p++; any = true; continue;
        }
        if (*p == ',') {
            seq_append(out, PH_PAUSE_SHORT);
            p++; any = true; continue;
        }
        if (*p == ' ' || *p == '\t') {
            p++; continue;
        }

        /* ── 3-char rules ──────────────────────────────────────────────── */
        for (int i = 0; G2P_3[i].graph && !matched; i++) {
            if (strncmp(p, G2P_3[i].graph, 3) == 0) {
                seq_append(out, G2P_3[i].phone);
                p += 3; matched = true; any = true;
            }
        }
        if (matched) continue;

        /* ── 2-char rules ──────────────────────────────────────────────── */
        if (*(p+1)) {
            for (int i = 0; G2P_2[i].graph && !matched; i++) {
                if (strncmp(p, G2P_2[i].graph, 2) == 0) {
                    seq_append(out, G2P_2[i].phone);
                    p += 2; matched = true; any = true;
                }
            }
        }
        if (matched) continue;

        /* ── Single-char fallback ──────────────────────────────────────── */
        for (int i = 0; G2P_1[i].graph && !matched; i++) {
            if (*p == G2P_1[i].graph[0]) {
                seq_append(out, G2P_1[i].phone);
                p++; matched = true; any = true;
            }
        }

        /* Unknown character (non-Kinyarwanda, digit already expanded) */
        if (!matched) p++;
    }

    return any;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 5. G2P SENTENCE CONVERSION
 * ══════════════════════════════════════════════════════════════════════════ */

bool kin_g2p_sentence(const char *text, KinPhonemeSeq *out) {
    if (!text || !out) return false;

    /* Step 1: normalize */
    char norm[G2P_MAX_NORM];
    kin_normalize_text(text, norm, sizeof(norm));

    /* Clear output */
    out->count = 0;
    out->repr[0] = '\0';

    /* Step 2: split into words and convert each */
    char buf[G2P_MAX_NORM];
    strncpy(buf, norm, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Lowercase entire buffer */
    for (char *q = buf; *q; q++) *q = (char)tolower((unsigned char)*q);

    const char *p = buf;
    bool first_word = true;

    while (*p) {
        /* Skip spaces */
        while (*p == ' ') p++;
        if (!*p) break;

        /* Collect next token (word or punctuation marker) */
        if (*p == '.' || *p == ',') {
            /* Prosodic marker */
            seq_append(out, (*p == '.') ? PH_PAUSE_LONG : PH_PAUSE_SHORT);
            p++;
            first_word = true; /* Reset: next word doesn't need a boundary marker */
            continue;
        }

        /* Word boundary marker between words */
        if (!first_word) {
            seq_append(out, PH_WORD_BOUND);
        }
        first_word = false;

        /* Find end of word */
        const char *word_start = p;
        while (*p && *p != ' ' && *p != '.' && *p != ',') p++;
        size_t word_len = (size_t)(p - word_start);

        /* Copy and convert word */
        char word[G2P_MAX_REPR];
        if (word_len >= sizeof(word)) word_len = sizeof(word) - 1;
        strncpy(word, word_start, word_len);
        word[word_len] = '\0';

        /* Convert this word's phonemes directly into out */
        /* We do this inline to share the same out buffer */
        KinPhonemeSeq word_seq;
        word_seq.count = 0;
        word_seq.repr[0] = '\0';
        kin_g2p_word(word, &word_seq);

        for (int i = 0; i < word_seq.count; i++) {
            seq_append(out, word_seq.phones[i]);
        }
    }

    return out->count > 0;
}
