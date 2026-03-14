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
    if (kin_starts_with(w, "aga") && wlen > 4) return 12; /* Nt.12 k→g  */
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

    /* Nt.5: bare i- prefix when RT 'ri' is elided before the stem.       *
     * Common pattern in Kinyarwanda (p.62): i + ri + C-stem → i + C-stem *
     * e.g. ibuye, izina, ishuri, isoko, itara, ifarasi, itonde.           *
     * Checked AFTER Nt.9 (in/im) and all specific iki/ibi/igi/iby/icy.   */
    if (w[0]=='i' && wlen > 3 && !is_vowel(w[1]) && w[1]!='n' && w[1]!='m')
        return 5;

    /* Dropped D-vowel variants – the D vowel (u/i/a) is elided in fast speech *
     * or before another vowel-ending word.  These come AFTER the full prefix   *
     * checks so they never override the canonical forms.                        *
     *   Nt.7:  igi/iki+stem → gi+stem  (igihugu → gihugu, igihe → gihe)       *
     *   Nt.4:  imi+stem → mi+stem      (imirongo → mirongo)                    *
     *   Nt.6:  ama+stem → ma+stem      (amaso → maso, amafi → mafi)            *
     *   Nt.9:  in/im+stem → n/m drop   (inzu → nzu, handled by KNOWN_WORDS)   */
    if (kin_starts_with(w, "gi") && wlen > 3 && !is_vowel(w[2])) return 7;
    if (kin_starts_with(w, "mi") && wlen > 4 && !is_vowel(w[2])) return 4;
    if (kin_starts_with(w, "ma") && wlen > 3 && !is_vowel(w[2])) return 6;
    /* Nt.12: dropped D vowel 'a' from "aka" → bare "ka", k→g voiced → "ga"  *
     * e.g. gasozi=agasozi (small hill), gatabo=agatabo (small book)          *
     * MUST come AFTER "aga" check.  Gated to avoid "ga" akamamo particle.   */
    if (kin_starts_with(w, "ga") && wlen > 3 && !is_vowel(w[2])) return 12;
    /* Nt.4: dropped D vowel 'i' from "imi" → bare "mi", i→y before vowel    *
     * → "my". e.g. myaka=imyaka (years), myambaro=imyambaro (clothes)       *
     * MUST come AFTER "imy" / "imi" / "mi" checks.                           */
    if (kin_starts_with(w, "my") && wlen > 4) return 4;
    /* Nt.1/3: dropped D vowel 'u' from "umu" → bare "mu"                *
     * e.g. musozi=umusozi, mudugudu=umudugudu, mugenzi=umugenzi          *
     * MUST come AFTER "umu" check to avoid override of full forms.        *
     * Excluded: words starting with mu+vowel (those are usually verbs)   */
    if (kin_starts_with(w, "mu") && wlen > 4 && !is_vowel(w[2])) return 1;
    /* Nt.14: dropped D vowel 'u' from "ubu" → bare "bu"                 *
     * e.g. butayu=ubutayu, burenganzira=uburenganzira                    *
     * MUST come AFTER "ubu" check.                                        */
    if (kin_starts_with(w, "bu") && wlen > 4 && !is_vowel(w[2])
        && w[2] != 'r' /* avoid "buri"=adverb */ ) return 14;
    /* Nt.2: dropped D vowel 'a' from "aba" → bare "ba".                 *
     * Pattern: agent nouns (abahinzi, abakunzi, abakozi) appear as       *
     * "bahinzi", "bakunzi", "bakozi" with 'a' dropped.                   *
     * Guard: ends in 'i' (agent nouns) AND length > 5 to avoid           *
     * colliding with verbs "bakora" (they work, ends in 'a').             */
    if (kin_starts_with(w, "ba") && wlen > 5 && !is_vowel(w[2])
        && w[wlen-1] == 'i') return 2;

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
            else if (kin_starts_with(word, "mu"))  stem_start = word + 2; /* dropped D 'u'  */
            break;
        case 2:
            if (kin_starts_with(word,"ab") && is_vowel(word[2]))
                                                   stem_start = word + 1; /* a+ba+V → ab+V */
            else if (kin_starts_with(word,"aba"))  stem_start = word + 3;
            else if (kin_starts_with(word,"ba"))   stem_start = word + 2; /* dropped D 'a' */
            break;
        case 4:
            if (kin_starts_with(word,"imy"))       stem_start = word + 2; /* i+mi+V → imy+V*/
            else if (kin_starts_with(word,"imi"))  stem_start = word + 3;
            else if (kin_starts_with(word,"my"))   stem_start = word + 1; /* dropped D + i→y*/
            else if (kin_starts_with(word,"mi"))   stem_start = word + 2; /* dropped D */
            break;
        case 5:
            if (kin_starts_with(word,"iri"))       stem_start = word + 3;
            else                                   stem_start = word + 1; /* bare i- */
            break;
        case 6:
            if (kin_starts_with(word,"ama"))       stem_start = word + 3; /* ama */
            else if (kin_starts_with(word,"ma"))   stem_start = word + 2; /* dropped D */
            break;
        case 7:
            if (kin_starts_with(word,"icy"))       stem_start = word + 2; /* ky→cy   */
            else if (kin_starts_with(word,"igi"))  stem_start = word + 3; /* k→g rule*/
            else if (kin_starts_with(word,"iki"))  stem_start = word + 3;
            else if (kin_starts_with(word,"gi"))   stem_start = word + 2; /* dropped D */
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
            if (kin_starts_with(word, "aga"))  stem_start = word + 3; /* k→g variant */
            else if (kin_starts_with(word,"ga")) stem_start = word + 2; /* dropped D */
            else                               stem_start = word + 3; /* aka */
            break;
        case 13:
            stem_start = word + 3; /* utu */
            break;
        case 14:
            if (kin_starts_with(word, "ubw"))  stem_start = word + 2; /* u→w */
            else if (kin_starts_with(word, "ubu")) stem_start = word + 3; /* ubu */
            else if (kin_starts_with(word, "bu"))  stem_start = word + 2; /* dropped D 'u' */
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

    /* Handle locative suffixes (-ho, -mo, -yo) appended after final -a.   *
     * e.g. guturaho = gutura+ho, gucamo = guca+mo, guturayo = gutura+yo.  *
     * Strip the 2-char locative suffix if the preceding char is 'a', then  *
     * recursively check if the stripped form is a valid infinitive.         */
    if (len > 5 && word[len-3] == 'a' &&
        (kin_ends_with(word, "ho") || kin_ends_with(word, "mo") ||
         kin_ends_with(word, "yo"))) {
        char locbuf[KIN_MAX_WORD];
        strncpy(locbuf, word, len - 2);
        locbuf[len - 2] = '\0';
        return kin_is_verb_infinitive(locbuf, stem_out);
    }

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

/* ══════════════════════════════════════════════════════════════════════════
 * Object marker (OM / indangakinyazina y'inshinga) table
 *
 * Structure per REB Year-2 book p.60 and phonological rules p.62:
 *   u → w before vowel-initial stem  (mu → mw, bu → bw, …)
 *   i → y before vowel-initial stem  (ki → cy, bi → by, …)
 *
 * OM appears between SP (and tense marker) and the verb stem:
 *   SP + [ra/za/ka] + OM + STEM + FV
 *   e.g.  a-ra-mu-bon-a  →  aramubona  (he sees him, OM cls1)
 *         ba-mw-irukan-ye → bamwirukanye (they chased him, OM cls1 before V)
 *
 * PLEASE VERIFY with native speaker: classes marked (* unverified *)
 * ══════════════════════════════════════════════════════════════════════════ */
typedef struct { const char *om; const char *om_v; int cls; } OmEntry;

static const OmEntry OM_TABLE[] = {
    /* Confirmed (book p.60 + common usage): */
    { "mu",  "mw",  1  },  /* cls 1 human sg:  aramubona, aramwigisha    */
    { "ki",  "cy",  7  },  /* cls 7 thing sg:  arakibona, aracyigisha    */
    { "bi",  "by",  8  },  /* cls 8 thing pl:  arabibona, arabigisha     */
    { "bu",  "bw",  14 },  /* cls 14 abstract: arabubwira, arabwigisha   */
    /* Following the u→w / i→y phonological rules (* unverified *): */
    { "ba",  "ba",  2  },  /* cls 2 human pl:  arababona                 */
    { "wu",  "wu",  3  },  /* cls 3 tree sg:   (* unverified *)           */
    { "yi",  "yi",  4  },  /* cls 4 tree pl:   arayibona (* unverified *)*/
    { "ri",  "ry",  5  },  /* cls 5 sg:        (* unverified *)           */
    { "ya",  "ya",  6  },  /* cls 6 pl/mass:   arayabona (* unverified *)*/
    { "zi",  "zi",  10 },  /* cls 10 pl:       arazibona (* unverified *)*/
    { "ru",  "rw",  11 },  /* cls 11 long:     ararubona (* unverified *)*/
    { "ka",  "ka",  12 },  /* cls 12 dim sg:   arakabona (* unverified *)*/
    { "tu",  "tw",  13 },  /* cls 13 dim pl:   aratubona (* unverified *)*/
    { "ku",  "kw",  15 },  /* cls 15 infinit:  arakubona (* unverified *)*/
    { "ha",  "ha",  16 },  /* cls 16 locative: (* unverified *)           */
    { NULL, NULL, 0 }
};
/* NOTE: Single-char vowel-form variants (k, h, b, y, w, z) are intentionally
 * omitted — they are too ambiguous against real consonant-initial stems.
 * Only 2-char OM forms are used. The mw/cy/by/bw/rw/tw/kw variants are kept
 * because they are truly distinct from any 2-char stem start. */

/*
 * om_strip() — try to peel an object marker off the front of `stem`.
 * Returns true if an OM was found.
 * bare_out receives the stem text after the OM (must be ≥ 2 chars to be valid).
 */
static bool om_strip(const char *stem, int *om_cls_out,
                     char *bare_out, size_t bare_sz) {
    size_t slen = strlen(stem);
    for (int i = 0; OM_TABLE[i].om; i++) {
        /* Try normal form */
        size_t olen = strlen(OM_TABLE[i].om);
        if (slen > olen + 1 && kin_starts_with(stem, OM_TABLE[i].om)) {
            if (om_cls_out) *om_cls_out = OM_TABLE[i].cls;
            if (bare_out) { strncpy(bare_out, stem + olen, bare_sz - 1);
                            bare_out[bare_sz - 1] = '\0'; }
            return true;
        }
        /* Try vowel-initial variant */
        size_t ovlen = strlen(OM_TABLE[i].om_v);
        if (slen > ovlen + 1 && kin_starts_with(stem, OM_TABLE[i].om_v)) {
            /* Disambiguate single-char variants: 'y' could be cls4/6/9,
             * 'w' cls3/13/14/15, 'b' cls2 — only accept if next char is vowel */
            if (ovlen == 1 && !is_vowel(stem[1])) continue;
            if (om_cls_out) *om_cls_out = OM_TABLE[i].cls;
            if (bare_out) { strncpy(bare_out, stem + ovlen, bare_sz - 1);
                            bare_out[bare_sz - 1] = '\0'; }
            return true;
        }
    }
    return false;
}

/*
 * ext_strip() — detect and strip a derivational extension (itondaguranshinga)
 * from the END of the extracted stem (FV already removed).
 *
 * Minimum bare stem after stripping = 2 chars (prevents false positives
 * on short stems like "er" for gutera).
 *
 * Extensions per REB Year-2 book Chapter 26:
 *   Passive (imbundo):    stem ends in -w    gukorwa  → stem "korw"
 *   Causative (integeko): stem ends in -ish/-esh      gukorisha → "korish"
 *   Applicative (ikirango): stem ends in -ir/-er      gukorera → "korer"
 *   Reciprocal:           stem ends in -an            gukorana → "koran"
 */
static bool ext_strip(const char *stem, VerbExtension *ext_out,
                      char *bare_out, size_t bare_sz) {
    size_t slen = strlen(stem);
    char tmp[KIN_MAX_STEM];

    /* Causative: -ish or -esh (check first – longer match) */
    if (slen > 4 && (kin_ends_with(stem, "ish") || kin_ends_with(stem, "esh"))) {
        size_t blen = slen - 3;
        strncpy(tmp, stem, blen); tmp[blen] = '\0';
        if (blen >= 2) {
            if (ext_out)  *ext_out = VEXT_CAUSATIVE;
            if (bare_out) { strncpy(bare_out, tmp, bare_sz-1); bare_out[bare_sz-1]='\0'; }
            return true;
        }
    }
    /* Applicative: -ir or -er */
    if (slen > 4 && (kin_ends_with(stem, "ir") || kin_ends_with(stem, "er"))) {
        size_t blen = slen - 2;
        strncpy(tmp, stem, blen); tmp[blen] = '\0';
        if (blen >= 2) {
            if (ext_out)  *ext_out = VEXT_APPLICATIVE;
            if (bare_out) { strncpy(bare_out, tmp, bare_sz-1); bare_out[bare_sz-1]='\0'; }
            return true;
        }
    }
    /* Reciprocal: -an */
    if (slen > 4 && kin_ends_with(stem, "an")) {
        size_t blen = slen - 2;
        strncpy(tmp, stem, blen); tmp[blen] = '\0';
        if (blen >= 2) {
            if (ext_out)  *ext_out = VEXT_RECIPROCAL;
            if (bare_out) { strncpy(bare_out, tmp, bare_sz-1); bare_out[bare_sz-1]='\0'; }
            return true;
        }
    }
    /* Passive: -w (single char, check last) */
    if (slen > 3 && stem[slen-1] == 'w') {
        size_t blen = slen - 1;
        strncpy(tmp, stem, blen); tmp[blen] = '\0';
        if (blen >= 2) {
            if (ext_out)  *ext_out = VEXT_PASSIVE;
            if (bare_out) { strncpy(bare_out, tmp, bare_sz-1); bare_out[bare_sz-1]='\0'; }
            return true;
        }
    }
    return false;
}

/*
 * verb_match_inner() — core SP + tense matching.
 * Extracted stem is placed in stem_buf (raw, may include OM prefix).
 * Returns true on any structural match.
 */
static bool verb_match_inner(const char *word, char *stem_buf, int *subj_class,
                             VerbTense *tense_out) {
    size_t len = strlen(word);
    if (len < 3) return false;

    static const struct { const char *pfx; int cls; } SP[] = {
        /* 4-char combined prefixes */
        { "twa",   0  },  /* 1pl past / Nt.13                             */
        { "mwa",   0  },  /* 2pl past                                     */
        { "rwa",  11  },  /* Nt.11 past                                   */
        { "bwa",  14  },  /* Nt.14 past                                   */
        { "kwa",  15  },  /* Nt.15 past                                   */
        /* 3-char prefixes */
        { "ara",   1  },  /* a+ra — Nt.1 3sg present (ra already embedded)*/
        { "nda",   0  },  /* 1sg present (nd+a)                           */
        { "ndi",   0  },  /* 1sg copula                                   */
        /* 2-char prefixes */
        { "ba",    2  },
        /* Vowel-initial stem variants: u→w before vowel (bu→bw, ru→rw, tu→tw) */
        { "bw",   14  },  /* Nt.14 bu+vowel → bw (bwera, bwigisha)        */
        { "rw",   11  },  /* Nt.11 ru+vowel → rw (rwera, rwemera)         */
        { "tw",    0  },  /* 1pl   tu+vowel → tw (twemera, twiga)         */
        /* i→y before vowel-initial stems (phonological rule p.7-8)       */
        { "cy",    7  },  /* Nt.7  ki+vowel → cy  (cyatumye, cyemera, cyari)  */
        { "by",    8  },  /* Nt.8  bi+vowel → by  (byari, byemera, byibutse)  */
        { "ry",    5  },  /* Nt.5  ri+vowel → ry  (ryari, ryibutse)           */
        { "zy",   10  },  /* Nt.10 zi+vowel → zy  (zyari etc.)                */
        /* n→m before bilabials/labiodentals (p.7-8): 1sg "n" → "m"        */
        { "mb",    0  },  /* 1sg n→m before b: mbona, mbara, mbiruka          */
        { "mp",    0  },  /* 1sg n→m before p: mpaye, mpuye                   */
        { "mf",    0  },  /* 1sg n→m before f: mfite, mfasha                  */
        { "mv",    0  },  /* 1sg n→m before v: mvuga, mvanze                  */
        /* 1pl SP variant: tu → du before voiced consonants                  */
        { "du",    0  },  /* 1pl tu+voiced→du: dufite, dukora, dushaka        */
        { "na",    0  },  /* 1sg past                                     */
        { "wa",    3  },  /* 2sg / Nt.3 past                              */
        { "ya",    6  },  /* Nt.6 present OR Nt.1 past                   */
        { "za",   10  },  /* Nt.10 past                                   */
        { "tu",    0  },  /* 1pl / Nt.13                                  */
        { "mu",    0  },  /* 2pl                                          */
        { "ri",    5  },
        { "ki",    7  },
        { "bi",    8  },
        { "zi",   10  },
        { "ru",   11  },
        { "ka",   12  },
        { "bu",   14  },
        { "ku",   15  },
        { "ha",   16  },
        /* 1-char prefixes – lowest priority */
        { "u",     3  },
        { "i",     4  },
        { "a",     1  },
        { "n",     0  },
        { "w",     3  },  /* Nt.3/2sg u+vowel → w (wera=u+er+a, wemera)  */
        /* Elided SP: ya+V-stem → y+V-stem (a→Ø before vowel, p.7-8)     *
         * e.g. ya+ig+a = "yiga" (SP ya→y before i-initial stem)         */
        { "y",     6  },  /* elided "ya" SP (cls6 present or cls1 past)   */
        /* Elided Nt.2 SP: ba+vowel → b+vowel (biga, begereye, bemeye)    */
        { "b",     2  },  /* elided "ba" SP before vowel-initial stems     */
        { NULL, 0 }
    };

    for (int i = 0; SP[i].pfx; i++) {
        size_t plen = strlen(SP[i].pfx);
        if (!kin_starts_with(word, SP[i].pfx)) continue;
        if (len <= plen + 1) continue;

        const char *inner = word + plen;
        size_t ilen = strlen(inner);

        /* "ara" has ra already embedded; inner ends in 'a' */
        if (strcmp(SP[i].pfx, "ara") == 0) {
            if (inner[ilen-1] != 'a' || ilen < 2) continue;
            if (stem_buf) { strncpy(stem_buf, inner, ilen-1); stem_buf[ilen-1]='\0'; }
            if (subj_class) *subj_class = 1;
            if (tense_out)  *tense_out  = TENSE_PRESENT;
            return true;
        }

        /* FUTURE: za + stem + a */
        if (kin_starts_with(inner, "za") && ilen > 3 && inner[ilen-1]=='a') {
            const char *s = inner + 2; size_t sl = ilen - 3;
            if (sl < 1) continue;
            if (stem_buf) { strncpy(stem_buf, s, sl); stem_buf[sl]='\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_FUTURE;
            return true;
        }
        /* NARRATIVE: ka + stem + a (when SP is not "ka" itself) */
        if (kin_starts_with(inner, "ka") && ilen > 3 && inner[ilen-1]=='a'
            && strcmp(SP[i].pfx, "ka") != 0) {
            const char *s = inner + 2; size_t sl = ilen - 3;
            if (sl < 1) continue;
            if (stem_buf) { strncpy(stem_buf, s, sl); stem_buf[sl]='\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_NARRATIVE;
            return true;
        }
        /* OPTATIVE: ra + ka + stem + a  (Inyifurizo: SP+ra+ka+stem+a)    *
         * e.g.  urakabyara = u + ra + ka + byar + a                      *
         * Must be checked BEFORE plain "ra" present to avoid wrong match. */
        if (kin_starts_with(inner, "raka") && ilen > 6 && inner[ilen-1]=='a') {
            const char *s = inner + 4; size_t sl = ilen - 5;
            if (sl < 1) continue;
            if (stem_buf) { strncpy(stem_buf, s, sl); stem_buf[sl]='\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_OPTATIVE;
            return true;
        }
        /* PRESENT with ra marker */
        if (kin_starts_with(inner, "ra") && ilen > 3 &&
            (inner[ilen-1]=='a' || inner[ilen-1]=='o')) {
            const char *s = inner + 2; size_t sl = ilen - 3;
            if (sl < 1) continue;
            if (stem_buf) { strncpy(stem_buf, s, sl); stem_buf[sl]='\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_PRESENT;
            return true;
        }
        /* PAST IMPERFECT: ends in aga */
        if (ilen > 4 && kin_ends_with(inner, "aga")) {
            size_t sl = ilen - 3;
            if (stem_buf) { strncpy(stem_buf, inner, sl); stem_buf[sl]='\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_PAST_IMPF;
            return true;
        }
        /* PAST PERFECT: ends in ye */
        if (ilen > 3 && kin_ends_with(inner, "ye")) {
            size_t sl = ilen - 2;
            if (stem_buf) { strncpy(stem_buf, inner, sl); stem_buf[sl]='\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_PAST_PERF;
            return true;
        }
        /* SUBJUNCTIVE: ends in e */
        if (ilen >= 2 && inner[ilen-1]=='e') {
            size_t sl = ilen - 1;
            if (stem_buf) { strncpy(stem_buf, inner, sl); stem_buf[sl]='\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_SUBJUNCTIVE;
            return true;
        }
        /* PRESENT no-ra: ends in a or o */
        if (ilen >= 2 && (inner[ilen-1]=='a' || inner[ilen-1]=='o')) {
            size_t sl = ilen - 1;
            if (stem_buf) { strncpy(stem_buf, inner, sl); stem_buf[sl]='\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_PRESENT_NORA;
            return true;
        }
    }
    return false;
}

/*
 * kin_is_verb_conjugated()
 *
 * Full conjugated-verb detector with three new layers on top of the SP/tense
 * matching:
 *
 * LAYER 1 – Negation (inshinga y'ubunyagatifu):
 *   Negative = nt- prepended before the subject prefix.
 *   ntaragenda = nt + a(SP cls1) + ra + gend + a   ← class 1, NOT "nti-"
 *   ntiragenda = nt + i(SP cls4/9) + ra + gend + a ← class 4/9, non-human
 *   The "ti" the user corrected: t(from nt) + i(subject prefix of cls4/9).
 *
 * LAYER 2 – Object marker (OM / indangakinyazina y'inshinga):
 *   Detected from the front of the extracted raw stem (after SP/tense).
 *   SP + [tense] + OM + STEM + FV
 *   aramubona: SP=ara, raw_stem="mubon" → OM=mu(cls1), bare_stem="bon"
 *   bamwirukanye: SP=ba, raw_stem="mwirukan" → OM=mw(cls1), bare_stem="irukan"
 *
 * LAYER 3 – Verb extension (itondaguranshinga, REB Year-2 ch.26):
 *   Detected from the END of the bare stem (after OM stripped).
 *   Passive -w, Causative -ish/-esh, Applicative -ir/-er, Reciprocal -an.
 *
 * All output pointers are NULL-safe.
 */
bool kin_is_verb_conjugated(const char *word, char *stem_out, int *subj_class,
                            VerbTense *tense_out, int *obj_class_out,
                            VerbExtension *ext_out, bool *neg_out) {
    size_t len = strlen(word);
    /* Minimum 3: allows short imperatives like "jya" (go!), "iga" (learn!) */
    if (len < 3) return false;

    /* ── LAYER 1: Detect and strip nt- negation prefix ─────────────────── */
    bool is_neg = false;
    const char *parse_word = word;
    if (len > 3 && word[0] == 'n' && word[1] == 't') {
        is_neg   = true;
        parse_word = word + 2;
    }

    /* ── LAYER 2: Core SP + tense matching ─────────────────────────────── */
    char raw_stem[KIN_MAX_STEM] = "";
    int  cls   = 0;
    VerbTense tense = TENSE_NONE;

    if (!verb_match_inner(parse_word, raw_stem, &cls, &tense)) {
        /* ── LAYER 2b: Imperative fallback (Integeko) ──────────────────── *
         * If no SP was recognized AND the word ends in 'a' AND the word    *
         * (minus final 'a') is a known verb stem → mark as imperative.     *
         * e.g. "soma!" "genda!" "reba!" "andika!"                         *
         * Negatives cannot be bare imperatives so skip if is_neg=true.     */
        size_t plen = strlen(parse_word);
        if (!is_neg && plen >= 3 && parse_word[plen-1] == 'a') {
            char imp[KIN_MAX_STEM];
            strncpy(imp, parse_word, plen - 1);
            imp[plen - 1] = '\0';
            if (kin_is_known_verb_stem(imp)) {
                if (stem_out)      { strncpy(stem_out, imp, KIN_MAX_STEM-1);
                                     stem_out[KIN_MAX_STEM-1] = '\0'; }
                if (subj_class)    *subj_class    = 0;
                if (tense_out)     *tense_out     = TENSE_IMPERATIVE;
                if (obj_class_out) *obj_class_out = 0;
                if (ext_out)       *ext_out       = VEXT_NONE;
                if (neg_out)       *neg_out       = false;
                return true;
            }
            /* Try stripping one verb extension from imp then rechecking stem.  *
             * Handles extended imperatives: hungira (hung+ir+a), erekana        *
             * (erek+an+a), hitirwa (hit+ir+w+a, passive), curira (cur+ir+a).   */
            char bare_imp[KIN_MAX_STEM];
            VerbExtension imp_ext = VEXT_NONE;
            if (ext_strip(imp, &imp_ext, bare_imp, sizeof(bare_imp))) {
                if (kin_is_known_verb_stem(bare_imp)) {
                    if (stem_out)      { strncpy(stem_out, bare_imp, KIN_MAX_STEM-1);
                                         stem_out[KIN_MAX_STEM-1] = '\0'; }
                    if (subj_class)    *subj_class    = 0;
                    if (tense_out)     *tense_out     = TENSE_IMPERATIVE;
                    if (obj_class_out) *obj_class_out = 0;
                    if (ext_out)       *ext_out       = imp_ext;
                    if (neg_out)       *neg_out       = false;
                    return true;
                }
            }
        }
        return false;
    }

    /* ── LAYER 3a: Strip object marker from front of raw_stem ──────────── */
    int  obj_cls = 0;
    char after_om[KIN_MAX_STEM];
    strncpy(after_om, raw_stem, KIN_MAX_STEM - 1);
    after_om[KIN_MAX_STEM - 1] = '\0';
    om_strip(raw_stem, &obj_cls, after_om, sizeof(after_om));

    /* ── LAYER 3b: Strip verb extension from end of after_om ───────────── */
    VerbExtension vext = VEXT_NONE;
    char bare[KIN_MAX_STEM];
    strncpy(bare, after_om, KIN_MAX_STEM - 1);
    bare[KIN_MAX_STEM - 1] = '\0';
    ext_strip(after_om, &vext, bare, sizeof(bare));

    /* ── Write outputs ──────────────────────────────────────────────────── */
    /* stem_out gets the bare stem (most useful for lexicon lookups)        */
    if (stem_out)      { strncpy(stem_out, bare, KIN_MAX_STEM-1);
                         stem_out[KIN_MAX_STEM-1] = '\0'; }
    if (subj_class)    *subj_class    = cls;
    if (tense_out)     *tense_out     = tense;
    if (obj_class_out) *obj_class_out = obj_cls;
    if (ext_out)       *ext_out       = vext;
    if (neg_out)       *neg_out       = is_neg;
    return true;
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
        /* Phonological variants (u→w / i→y before vowel-initial stems) */
        { "mw",  1  }, /* mu + vowel-initial stem (u→w): mwiza, mwinshi   */
        { "rw",  11 }, /* ru + vowel-initial stem (u→w): rwiza, rwinshi   */
        { "bw",  14 }, /* bu + vowel-initial stem (u→w): bwiza, bwinshi   */
        { "kw",  15 }, /* ku + vowel-initial stem (u→w): kwiza, kwinshi   */
        { "tw",  13 }, /* tu + vowel-initial stem (u→w): twiza, twinshi   */
        { "by",  8  }, /* bi + vowel-initial stem (i→y)                   */
        { "cy",  7  }, /* ki + vowel-initial stem (i→y, ky→cy)            */
        { "my",  4  }, /* mi + vowel-initial stem (i→y)                   */
        { "ny",  9  }, /* n  + vowel-initial stem (n→ny before some V)    */
        { "ry",  5  }, /* ri + vowel-initial stem (i→y): ryiza, ryinshi   */
        /* a+i→e fusion: prefix ending in 'a' + i-initial stem → 'e'+stem */
        /* (Amategeko y'igenamajwi p.7-8: a+i contraction)                */
        { "be",  2  }, /* ba + inshi/iza → benshi/beza                    */
        { "me",  6  }, /* ma + inshi/iza → menshi/meza                    */
        { "ye",  4  }, /* ya + i-stem (Nt.4)                              */
        { "ze",  10 }, /* za + i-stem (Nt.10)                             */
        { NULL, 0 }
    };

    /* For the a+i→e fused prefixes, the stem stored in ADJ_STEMS starts
     * with 'i'.  We need to prepend 'i' to whatever follows the fused pfx.
     * We handle this by checking both the remainder AND 'i'+remainder.     */
    static const char *FUSED_PREFIXES[] = { "be", "me", "ye", "ze", NULL };

    for (int i = 0; ADJ_PREFIXES[i].pfx; i++) {
        size_t plen = strlen(ADJ_PREFIXES[i].pfx);
        if (!kin_starts_with(word, ADJ_PREFIXES[i].pfx)) continue;

        const char *sfx = word + plen;  /* everything after the prefix  */

        /* Standard check: is sfx directly a known adj stem? */
        if (kin_is_adj_stem(sfx)) {
            if (stem_out)  strncpy(stem_out, sfx, KIN_MAX_STEM - 1);
            if (class_out) *class_out = ADJ_PREFIXES[i].cls;
            return true;
        }

        /* a+i→e fusion check: for fused prefixes ("be","me","ye","ze"),
         * the original stem starts with 'i'.  Try "i" + sfx as stem.   */
        bool is_fused = false;
        for (int j = 0; FUSED_PREFIXES[j]; j++) {
            if (strcmp(ADJ_PREFIXES[i].pfx, FUSED_PREFIXES[j]) == 0) {
                is_fused = true; break;
            }
        }
        if (is_fused) {
            char restored[KIN_MAX_STEM];
            restored[0] = 'i';
            strncpy(restored + 1, sfx, KIN_MAX_STEM - 2);
            restored[KIN_MAX_STEM - 1] = '\0';
            if (kin_is_adj_stem(restored)) {
                /* Return the reconstructed i-initial stem for display */
                if (stem_out)  strncpy(stem_out, restored, KIN_MAX_STEM - 1);
                if (class_out) *class_out = ADJ_PREFIXES[i].cls;
                return true;
            }
        }
    }
    return false;
}
