/*
 * morphology.c
 * Morphological analysis for Kinyarwanda words.
 *
 * Implements the grammar rules from the textbook:
 *   Noun structure:  D + RT + C  (Indomo + Indanganteko + Igicumbi)  p.61-63
 *   Adj  structure:  RS + C      (Indangasano + Igicumbi)            p.65-67
 *   Verb infinitive: ku/gu/kw/gw + stem + a                         p.88+
 *   Phonological rules (igenamajwi):
 *     u → w before vowel               (Umwana: u+mu+ana, u→w)      p.62
 *     i → y before vowel               (Icyatsi: i+ki+atsi, i→y)    p.62
 *     a → Ø before vowel (elision)     (Abana:  a+ba+ana, a→Ø/-J)   p.62
 *     n → m before bilabials (mb, mp, mf, mv)                        p.7-8
 *     k → g before voiced consonants                                 p.7-8
 */

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "../include/kinyarwanda.h"

/* ── Utility: string helpers ─────────────────────────────────────────────── */

void kin_strlower(const char *src, char *dst, size_t dstlen) {
    size_t i = 0;
    for (; src[i] && i + 1 < dstlen; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

bool kin_starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

bool kin_ends_with(const char *s, const char *suffix) {
    size_t sl = strlen(s), pl = strlen(suffix);
    if (pl > sl) return false;
    return strcmp(s + sl - pl, suffix) == 0;
}

void kin_str_trim(char *s) {
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t l = strlen(s);
    while (l > 0 && (s[l-1] == ' ' || s[l-1] == '\t' || s[l-1] == '\n'))
        s[--l] = '\0';
}

/* ── Vowel check ─────────────────────────────────────────────────────────── */
static bool is_vowel(char c) {
    return c=='a'||c=='e'||c=='i'||c=='o'||c=='u';
}

/*
 * kin_detect_noun_class()
 *
 * Given a lowercased word, return the most likely noun class (1-16)
 * by matching the D+RT prefix sequence from the book's class table.
 * Returns 0 if unrecognised.
 *
 * Priority rules handle ambiguous prefixes:
 *   "umu" → could be Nt.1 (human) or Nt.3 (non-human); return 1
 *   "aba" → Nt.2 (human plural); BUT also some demonstratives — check length
 *   "in"  → Nt.9/10; distinguish by context later
 *   Verb infinitives start with "gu"/"ku"/"kw"/"gw" — handled separately
 */
int kin_detect_noun_class(const char *w) {
    /* Length guard */
    size_t wlen = strlen(w);
    if (wlen < 3) return 0;

    /* Ordered by prefix length (longest first) to avoid partial matches */
    if (kin_starts_with(w, "umu") && wlen > 4) return 1;  /* Nt.1/3      */
    if (kin_starts_with(w, "aba") && wlen > 4) return 2;  /* Nt.2        */
    if (kin_starts_with(w, "imi") && wlen > 4) return 4;  /* Nt.4        */
    if (kin_starts_with(w, "ama") && wlen > 4) return 6;  /* Nt.6        */
    if (kin_starts_with(w, "iki") && wlen > 4) return 7;  /* Nt.7        */
    if (kin_starts_with(w, "igi") && wlen > 4) return 7;  /* Nt.7 k→g   */
    if (kin_starts_with(w, "ibi") && wlen > 4) return 8;  /* Nt.8        */
    if (kin_starts_with(w, "iby") && wlen > 4) return 8;  /* Nt.8 i→y   */
    if (kin_starts_with(w, "uru") && wlen > 4) return 11; /* Nt.11       */
    if (kin_starts_with(w, "aka") && wlen > 4) return 12; /* Nt.12       */
    if (kin_starts_with(w, "utu") && wlen > 4) return 13; /* Nt.13       */
    if (kin_starts_with(w, "ubu") && wlen > 4) return 14; /* Nt.14       */
    if (kin_starts_with(w, "uku") && wlen > 4) return 15; /* Nt.15       */
    if (kin_starts_with(w, "ukw") && wlen > 4) return 15; /* Nt.15 u→w  */
    if (kin_starts_with(w, "aha") && wlen > 4) return 16; /* Nt.16       */
    /* Phonological variants for Nt.14 and Nt.11 before vowel-initial stems */
    if (kin_starts_with(w, "ubw") && wlen > 4) return 14; /* Nt.14 u→w  */
    if (kin_starts_with(w, "urw") && wlen > 4) return 11; /* Nt.11 u→w  */

    /* Nt.5: i + ri (sometimes surface as "iri", sometimes elided to "i")  */
    if (kin_starts_with(w, "iri") && wlen > 4) return 5;

    /* Nt.9/10: i + n- (n changes based on following consonant)            */
    /* Covers: in-, im- (n+bilabial), iny-, inz-, ind-, ing-, inj-, ink... */
    if (w[0]=='i' && w[1]=='n' && wlen > 3) return 9;
    if (w[0]=='i' && w[1]=='m' && wlen > 3) return 9;  /* n→m before bilabial */

    /* Fallback for Nt.1/Nt.3 words that surfaced as "umw-" (u→w) */
    if (kin_starts_with(w, "umw") && wlen > 4) return 1;
    /* Short form without D vowel: mwana (= umwana), mwami, etc.        */
    if (kin_starts_with(w, "mw")  && wlen > 3) return 1;
    /* Nt.2 words surfaced as "ab-" + vowel (a→Ø) */
    if (kin_starts_with(w, "ab")  && is_vowel(w[2]) && wlen > 4) return 2;
    /* Nt.4 words surfaced as "imy-" (i→y before vowel) */
    if (kin_starts_with(w, "imy") && wlen > 4) return 4;
    /* Nt.8 words surfaced as "iby-" */
    if (kin_starts_with(w, "iby") && wlen > 4) return 8;
    /* Nt.7 words surfaced as "icy-" */
    if (kin_starts_with(w, "icy") && wlen > 4) return 7;

    return 0;
}

/*
 * kin_strip_noun_prefix()
 *
 * Decompose a noun into its prefix (D+RT) and stem (Igicumbi).
 * Applies phonological reversal rules from book p.62-63.
 * Writes the stem into stem_out and detected class into *class_out.
 * Returns true if a class was detected.
 */
bool kin_strip_noun_prefix(const char *word, char *stem_out, int *class_out) {
    int cls = kin_detect_noun_class(word);
    if (cls == 0) { stem_out[0] = '\0'; return false; }
    if (class_out) *class_out = cls;

    const char *stem_start = word;

    /* Strip the surface prefix (D+RT combined) */
    switch (cls) {
        case 1: case 3:
            if (kin_starts_with(word, "umw"))      stem_start = word + 2; /* u+mu+V → umw+V */
            else if (kin_starts_with(word, "umu")) stem_start = word + 3;
            else if (kin_starts_with(word, "mw"))  stem_start = word + 1; /* short form mw+V*/
            break;
        case 2:
            if (kin_starts_with(word,"ab") && is_vowel(word[2]))
                                                   stem_start = word + 1; /* a+ba+V → ab+V */
            else if (kin_starts_with(word,"aba"))  stem_start = word + 3;
            break;
        case 4:
            if (kin_starts_with(word,"imy"))       stem_start = word + 2; /* i+mi+V → imy+V*/
            else if (kin_starts_with(word,"imi"))  stem_start = word + 3;
            break;
        case 5:
            if (kin_starts_with(word,"iri"))       stem_start = word + 3;
            else                                   stem_start = word + 1; /* bare i- */
            break;
        case 6:
            stem_start = word + 3; /* ama */
            break;
        case 7:
            if (kin_starts_with(word,"icy"))       stem_start = word + 2; /* ky→cy   */
            else if (kin_starts_with(word,"igi"))  stem_start = word + 3; /* k→g rule*/
            else if (kin_starts_with(word,"iki"))  stem_start = word + 3;
            break;
        case 8:
            if (kin_starts_with(word,"iby"))       stem_start = word + 2;
            else if (kin_starts_with(word,"ibi"))  stem_start = word + 3;
            break;
        case 9: case 10:
            stem_start = word + 1; /* skip 'i', keep n/m as part of stem  */
            break;
        case 11:
            if (kin_starts_with(word, "urw"))  stem_start = word + 2; /* u→w */
            else                               stem_start = word + 3; /* uru */
            break;
        case 12:
            stem_start = word + 3; /* aka */
            break;
        case 13:
            stem_start = word + 3; /* utu */
            break;
        case 14:
            if (kin_starts_with(word, "ubw"))  stem_start = word + 2; /* u→w */
            else                               stem_start = word + 3; /* ubu */
            break;
        case 15:
            if (kin_starts_with(word, "ukw"))  stem_start = word + 2; /* u→w */
            else                               stem_start = word + 3; /* uku */
            break;
        case 16:
            stem_start = word + 3; /* aha */
            break;
        default:
            stem_start = word;
    }

    strncpy(stem_out, stem_start, KIN_MAX_STEM - 1);
    stem_out[KIN_MAX_STEM - 1] = '\0';
    return true;
}

/*
 * kin_is_verb_infinitive()
 *
 * Kinyarwanda verb infinitives (class Nt.15) have the surface form:
 *   gu + C(voiced)... + stem + a
 *   ku + C(voiceless)... + stem + a
 *   kw / gw + V + stem + a   (before vowel-initial stems)
 *
 * The final vowel is always -a.
 * Returns true and writes the bare stem (without prefix and final -a).
 */
bool kin_is_verb_infinitive(const char *word, char *stem_out) {
    size_t len = strlen(word);
    if (len < 4) return false;
    /* Must end in 'a' (the final vowel of the infinitive)                 */
    if (word[len - 1] != 'a') return false;

    const char *inner = NULL;  /* pointer past the prefix                  */

    if (kin_starts_with(word, "kw") && is_vowel(word[2]))   inner = word + 2;
    else if (kin_starts_with(word, "gw") && is_vowel(word[2])) inner = word + 2;
    else if (kin_starts_with(word, "gu") && !is_vowel(word[2])) inner = word + 2;
    else if (kin_starts_with(word, "ku") && !is_vowel(word[2])) inner = word + 2;
    else return false;

    /* Inner must be at least 2 chars (1 consonant + final a) */
    size_t inner_len = strlen(inner);
    if (inner_len < 2) return false;

    /* Strip final 'a' to get the stem */
    if (stem_out) {
        strncpy(stem_out, inner, inner_len - 1);
        stem_out[inner_len - 1] = '\0';
    }
    return true;
}

/*
 * kin_is_verb_conjugated()
 *
 * Detects a conjugated verb and identifies its TENSE (igihe):
 *
 *   Present (igihe cy'ubu):
 *     SP + ra + stem + a   → aragenda, iravuga, biravuga
 *     SP + stem + a        → ibona (ra dropped in some contexts)
 *
 *   Past perfect (igihe gishize cyane):
 *     SP_past + stem + ye  → yaremye, bagiye, yagize
 *
 *   Past imperfect / habitual (igihe gishize kera):
 *     SP + stem + aga      → yagendaga, yagiraga
 *
 *   Future (igihe kizaza):
 *     SP + za + stem + a   → azagenda, tuzakora
 *
 *   Subjunctive / optative (isabira):
 *     SP + stem + e        → agende, akore
 *
 *   Narrative consecutive (imigani):
 *     SP + ka + stem + a   → akagenda (then he went)
 *
 * Subject prefixes are matched longest-first to prevent partial collisions.
 * `tense_out` may be NULL if the caller only needs stem/class.
 */
bool kin_is_verb_conjugated(const char *word, char *stem_out, int *subj_class,
                            VerbTense *tense_out) {
    size_t len = strlen(word);
    if (len < 4) return false;

    /* Subject prefix table – longest entries first.
     * cls == 0 means "personal pronoun / ambiguous class"
     * Some entries represent collapsed SP+tense (e.g. "ara" = a+ra). */
    static const struct { const char *pfx; int cls; } SP[] = {
        /* 4-char combined prefixes */
        { "twa",   0  },  /* 1pl past / Nt.13                             */
        { "mwa",   0  },  /* 2pl past                                     */
        { "rwa",  11  },  /* Nt.11 past                                   */
        { "bwa",  14  },  /* Nt.14 past                                   */
        { "kwa",  15  },  /* Nt.15 past                                   */
        /* 3-char prefixes */
        { "ara",   1  },  /* a+ra — Nt.1 3sg present (ra already embedded)*/
        { "nda",   0  },  /* 1sg negative/emphatic present (nd+a)         */
        { "ndi",   0  },  /* 1sg copula                                   */
        /* 2-char prefixes */
        { "ba",    2  },  /* Nt.2                                         */
        { "na",    0  },  /* 1sg past                                     */
        { "wa",    3  },  /* 2sg / Nt.3 past                              */
        { "ya",    6  },  /* Nt.6 present OR Nt.1 past (resolved by suffix)*/
        { "za",   10  },  /* Nt.10 past                                   */
        { "tu",    0  },  /* 1pl / Nt.13                                  */
        { "mu",    0  },  /* 2pl                                          */
        { "ri",    5  },  /* Nt.5                                         */
        { "ki",    7  },  /* Nt.7                                         */
        { "bi",    8  },  /* Nt.8                                         */
        { "zi",   10  },  /* Nt.10 present                                */
        { "ru",   11  },  /* Nt.11 present                                */
        { "ka",   12  },  /* Nt.12 (also narrative marker, see below)     */
        { "bu",   14  },  /* Nt.14                                        */
        { "ku",   15  },  /* Nt.15                                        */
        { "ha",   16  },  /* Nt.16                                        */
        /* 1-char prefixes – lowest priority */
        { "u",     3  },  /* 2sg / Nt.3 present                          */
        { "i",     4  },  /* Nt.4 / Nt.9                                 */
        { "a",     1  },  /* Nt.1 / Nt.6 bare (no-ra form)               */
        { "n",     0  },  /* 1sg bare                                     */
        { NULL, 0 }
    };

    for (int i = 0; SP[i].pfx; i++) {
        size_t plen = strlen(SP[i].pfx);
        if (!kin_starts_with(word, SP[i].pfx)) continue;
        if (len <= plen + 1) continue;  /* need at least 2 chars after prefix */

        const char *inner = word + plen;
        size_t ilen = strlen(inner);

        /* ── For "ara" the ra tense marker is already embedded.
         *    Remaining inner must end in 'a' (present final vowel).       */
        if (strcmp(SP[i].pfx, "ara") == 0) {
            if (inner[ilen - 1] != 'a' || ilen < 2) continue;
            if (stem_out) { strncpy(stem_out, inner, ilen-1); stem_out[ilen-1]='\0'; }
            if (subj_class) *subj_class = 1;
            if (tense_out)  *tense_out  = TENSE_PRESENT;
            return true;
        }

        /* ── Detect tense from inner content ───────────────────────── */

        /* FUTURE: inner starts with "za" and ends in 'a' */
        if (kin_starts_with(inner, "za") && ilen > 3 && inner[ilen-1] == 'a') {
            const char *stem = inner + 2;
            size_t slen = ilen - 3;
            if (slen < 1) continue;
            if (stem_out) { strncpy(stem_out, stem, slen); stem_out[slen] = '\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_FUTURE;
            return true;
        }

        /* NARRATIVE: inner starts with "ka" and ends in 'a' (not "ka" prefix itself) */
        if (kin_starts_with(inner, "ka") && ilen > 3 && inner[ilen-1] == 'a'
            && strcmp(SP[i].pfx, "ka") != 0) {
            const char *stem = inner + 2;
            size_t slen = ilen - 3;
            if (slen < 1) continue;
            if (stem_out) { strncpy(stem_out, stem, slen); stem_out[slen] = '\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_NARRATIVE;
            return true;
        }

        /* PRESENT with explicit "ra" marker: inner starts with "ra",
         * ends in 'a' (normal) or 'o' (locative: -ho/-yo suffix)         */
        if (kin_starts_with(inner, "ra") && ilen > 3 &&
            (inner[ilen-1] == 'a' || inner[ilen-1] == 'o')) {
            const char *stem = inner + 2;
            size_t slen = ilen - 3;
            if (slen < 1) continue;
            if (stem_out) { strncpy(stem_out, stem, slen); stem_out[slen] = '\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_PRESENT;
            return true;
        }

        /* PAST IMPERFECT: inner ends in "aga" */
        if (ilen > 4 && kin_ends_with(inner, "aga")) {
            size_t slen = ilen - 3;
            if (stem_out) { strncpy(stem_out, inner, slen); stem_out[slen] = '\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_PAST_IMPF;
            return true;
        }

        /* PAST PERFECT: inner ends in "ye" */
        if (ilen > 3 && kin_ends_with(inner, "ye")) {
            size_t slen = ilen - 2;
            if (stem_out) { strncpy(stem_out, inner, slen); stem_out[slen] = '\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_PAST_PERF;
            return true;
        }

        /* SUBJUNCTIVE: inner ends in 'e' (not "ye" – already caught above) */
        if (ilen >= 2 && inner[ilen-1] == 'e') {
            size_t slen = ilen - 1;
            if (stem_out) { strncpy(stem_out, inner, slen); stem_out[slen] = '\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_SUBJUNCTIVE;
            return true;
        }

        /* PRESENT (no-ra / contracted): ends in 'a' (normal) or 'o' (locative) */
        if (ilen >= 2 && (inner[ilen-1] == 'a' || inner[ilen-1] == 'o')) {
            size_t slen = ilen - 1;
            if (stem_out) { strncpy(stem_out, inner, slen); stem_out[slen] = '\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_PRESENT_NORA;
            return true;
        }
    }
    return false;
}

/*
 * kin_strip_adj_prefix()
 *
 * Adjective (ntera) structure:  RS + C
 * RS (Indangasano) is the concordance prefix that matches the noun's class.
 * The stem C is one of the ~19 adjective stems in the book.
 *
 * The concordance prefixes are the same as the indanganteko of the noun
 * class (mu, ba, mu, mi, ri, ma, ki, bi, n, n, ru, ka, tu, bu, ku, ha)
 * BUT they appear attached to the adjective stem.
 *
 * Approach: iterate over noun classes, try stripping each concordance
 * prefix, and check if the remainder is a known adjective stem.
 */
bool kin_strip_adj_prefix(const char *word, char *stem_out, int *class_out) {
    /* Concordance prefixes for adjective, per class (from table p.68) */
    static const struct { const char *pfx; int cls; } ADJ_PREFIXES[] = {
        { "mu",  1  }, { "ba",  2  }, { "mu",  3  }, { "mi",  4  },
        { "ri",  5  }, { "ma",  6  }, { "ki",  7  }, { "bi",  8  },
        { "n",   9  }, { "zi",  10 }, { "ru",  11 }, { "ka",  12 },
        { "tu",  13 }, { "bu",  14 }, { "ku",  15 }, { "ha",  16 },
        /* Phonological variants */
        { "mw",  1  }, /* mu + vowel-initial stem (u→w)                   */
        { "by",  8  }, /* bi + vowel-initial stem (i→y)                   */
        { "cy",  7  }, /* ki + vowel-initial stem (i→y, ky→cy)            */
        { "my",  4  }, /* mi + vowel-initial stem (i→y)                   */
        { "ny",  9  }, /* n  + vowel-initial stem (n→ny before some V)    */
        { NULL, 0 }
    };

    for (int i = 0; ADJ_PREFIXES[i].pfx; i++) {
        size_t plen = strlen(ADJ_PREFIXES[i].pfx);
        if (kin_starts_with(word, ADJ_PREFIXES[i].pfx)) {
            const char *stem = word + plen;
            if (kin_is_adj_stem(stem)) {
                if (stem_out)  strncpy(stem_out, stem, KIN_MAX_STEM - 1);
                if (class_out) *class_out = ADJ_PREFIXES[i].cls;
                return true;
            }
        }
    }
    return false;
}
