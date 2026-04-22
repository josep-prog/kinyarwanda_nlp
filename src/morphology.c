/*
 * morphology.c  —  Morphological analysis for Kinyarwanda.
 *
 * This file implements the DETECTION functions for Trees 1, 2 and 3.
 * The dispatch and morpheme FILLING is in morph_dispatch.c.
 *
 * File layout (sequential by tree):
 *
 *   § 0  UTILITIES        kin_strlower, kin_starts_with, kin_ends_with, kin_str_trim
 *
 *   § 1  PHONOLOGICAL RULES (Amategeko y'igenamajwi)
 *        kin_has_vowel_hiatus()    — Iranya ry'impanvu (VV contact check)
 *        kin_vv_join()             — Iranya ry'impanvu (VV contact resolver)
 *        kin_has_invalid_cluster() — Iteganyo ry'inzarara z'inkongi
 *
 *   § 2  TREE 1 — IZINA MBONERA (Noun)     Formula: D + RT + C
 *        kin_detect_noun_class()    — identify inteko (class 1-16) from prefix
 *        kin_strip_noun_prefix()    — split word into prefix (D+RT) and igicumbi
 *
 *   § 3  TREE 2 — NTERA (Adjective)        Formula: RS + C
 *        kin_strip_adj_prefix()    — split word into RS (concordance) and stem
 *        [adj stems listed in lexicon.c ADJ_STEMS[]]
 *
 *   § 4  TREE 3 — INSHINGA (Verb)
 *        § 4a  IMBUNDO (Infinitive)        Formula: PREF + C + FV
 *              kin_is_valid_verb_stem_shape()
 *              kin_is_verb_infinitive()
 *        § 4b  ITONDAGUYE (Conjugated)     Formula: SP + (TM) + (OM) + C + (EXT) + FV
 *              SP_TABLE[]           — subject prefix table (all classes + persons)
 *              OM_TABLE[]           — object marker table (indangakinyazina)
 *              ext_strip()          — detect and strip utumamo (verb extensions)
 *              kin_is_verb_conjugated()
 *
 * Phonological rules cited throughout (RALC 2017 / REB S3-S4):
 *   §1.1  u → w / _V    (mu+ana → mwana; ku+iga → kwiga)
 *   §1.1  i → y / _V    (ki+atsi → cyatsi; mi+uko → myuko)
 *   §1.1  a → ∅ / _V    (ba+ana → bana)
 *   §3.3  n → m / _bilabial  (n+baga → mbaga; n+vura → mvura)
 *   §3.5  r → d / n_    (n+ruru → nduru)
 *   §3.7  k → g / _voiced   (ki+haza → gihaza)
 *   §2.4  n + y → nz    (n+yoga → nzoga; Nt.9/10 only)
 */

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "../include/kinyarwanda.h"

/* ══════════════════════════════════════════════════════════════════════════
 * § 0  UTILITIES
 * ══════════════════════════════════════════════════════════════════════════ */

/* kin_strlower: lower-case src into dst, also normalising UTF-8 macron vowels
 * (ā/Ā→a  ē/Ē→e  ī/Ī→i  ō/Ō→o  ū/Ū→u) that appear in some Bible PDFs.
 * All are 2-byte sequences:
 *   C4 80/81=Āā  C4 92/93=Ēē  C4 AA/AB=Īī  C5 8C/8D=Ōō  C5 AA/AB=Ūū  */
void kin_strlower(const char *src, char *dst, size_t dstlen) {
    size_t i = 0, j = 0;
    while (src[i] && j + 1 < dstlen) {
        unsigned char c0 = (unsigned char)src[i];
        unsigned char c1 = src[i + 1] ? (unsigned char)src[i + 1] : 0;
        if (c0 == 0xC4 && (c1 == 0x80 || c1 == 0x81)) { dst[j++]='a'; i+=2; continue; }
        if (c0 == 0xC4 && (c1 == 0x92 || c1 == 0x93)) { dst[j++]='e'; i+=2; continue; }
        if (c0 == 0xC4 && (c1 == 0xAA || c1 == 0xAB)) { dst[j++]='i'; i+=2; continue; }
        if (c0 == 0xC5 && (c1 == 0x8C || c1 == 0x8D)) { dst[j++]='o'; i+=2; continue; }
        if (c0 == 0xC5 && (c1 == 0xAA || c1 == 0xAB)) { dst[j++]='u'; i+=2; continue; }
        dst[j++] = (char)tolower(c0);
        i++;
    }
    dst[j] = '\0';
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

/* ══════════════════════════════════════════════════════════════════════════
 * § 1  PHONOLOGICAL RULES  (Amategeko y'igenamajwi)
 *      These rules apply across ALL trees.  Every morpheme boundary is
 *      subject to these rules before any surface form is produced.
 *      Rule set source: RALC 2017 + REB S3/S4 textbooks.
 * ══════════════════════════════════════════════════════════════════════════ */

static bool is_vowel(char c) {
    return c=='a'||c=='e'||c=='i'||c=='o'||c=='u';
}

/*
 * kin_has_vowel_hiatus()
 *
 * Amategeko y'igenamajwi – IRANYA RY'IMPANVU (vowel contact rule):
 *   In Kinyarwanda, no two vowels may appear ADJACENT within a word.
 *   Valid words always resolve VV contact via:
 *     u→w, i→y (glide formation), a→Ø (elision), or a+i→e (fusion).
 *
 *   This function detects illegal VV sequences in written Kinyarwanda.
 *   Returns true (= error) when any two adjacent characters are both vowels.
 *
 *   Exceptions – these multi-vowel written forms are legitimate:
 *     • Loanwords (they keep their foreign phonology, tagged POS_FOREIGN)
 *     • Interjections: "ooh", "yee", "eeh" (already in INVARIABLES)
 *     • Proper nouns (flagged is_proper_noun = true by the tokenizer)
 *   The caller is responsible for skipping those categories.
 *
 *   Examples of WRONG forms that trigger this:
 *     ✗ "baeza"  → should be "beza"   (ba+iza: a+i→e, not ba+eza)
 *     ✗ "mueza"  → should be "mweza"  (mu+iza: u→w)
 *     ✗ "kuamara"→ should be "kwamara"(ku+amara: u→w)
 *     ✗ "baamara"→ should be "bamara" (ba+amara: a→Ø)
 */
bool kin_has_vowel_hiatus(const char *word) {
    if (!word || !word[0]) return false;
    for (int i = 0; word[i] && word[i+1]; i++) {
        if (is_vowel(word[i]) && is_vowel(word[i+1]))
            return true;
    }
    return false;
}

/*
 * kin_vv_join()
 *
 * Amategeko y'igenamajwi – GUHUZA INZIRA Z'INTERA (VV contact resolver):
 *   Join a prefix and a stem, resolving any vowel-vowel contact that would
 *   arise at the boundary.  This is the companion to kin_has_vowel_hiatus():
 *   where that function detects a violation, this one prevents it.
 *
 *   Rules applied (RALC 2017 / REB p.7-8), in priority order:
 *
 *   1. prefix ends in 'u' → u becomes 'w' before the stem vowel
 *        mu + iga  → mwiga      ku  + amara → kwamara
 *
 *   2. prefix ends in 'u' → u becomes 'w' before the stem vowel.
 *      Sub-rule (Official Orthography Rules, Table 0): the clusters kw, gw, hw
 *      are NOT written before back round vowels 'o' and 'u' — the 'w' is
 *      dropped because it is redundant before a round vowel:
 *        ku + iga  → kwiga      ku  + amara → kwamara   (w kept before a/e/i)
 *        ku + oma  → koma       ku  + ura   → kura       (w dropped before o/u)
 *        gu + oma  → goma       hu  + ora   → hora       (same for g/h)
 *        mu + oma  → mwoma      tu  + ura   → twura      (other consonants keep w)
 *
 *   3. prefix ends in 'i' → i becomes 'y' before the stem vowel; if the
 *      consonant before 'i' is 'k', it palatalises: k+y → cy.
 *      Sub-rule (Official Orthography Rules, Table 1): cy and jy clusters are
 *      written only before 'a', 'o', 'u'. Before the front vowels 'i' or 'e'
 *      the 'y' is dropped and the palatalisation reverses:
 *        cy + i → ki   cy + e → ke   jy + e → ge
 *      Examples:
 *        ki + ama  → cyama      ki  + iga  → kiga   ki + eza  → keza
 *        bi + iza  → byiza      bi  + eza  → byeza  (b is not k, no reversal)
 *
 *   4a. prefix ends in 'a', preceded by a glide (y/w) → 'a' simply elides
 *        ya + iga  → yiga       ya  + eza   → yeza
 *
 *   4b. prefix ends in 'a' + stem starts with 'i' → a+i fuse to 'e'
 *        ka + iza  → keza       ba  + inja  → benja  (a + i → e)
 *        NOTE: rule 4b only fires when the character before 'a' is a true
 *        consonant (not a glide), because glides block the fusion (rule 4a).
 *
 *   4c. prefix ends in 'a' + stem starts with any other vowel → 'a' elides
 *        ba + enda → benda      ya  + oya   → yoya   ba + amara → bamara
 *
 *   If there is no VV contact (prefix ends in consonant OR stem starts with
 *   consonant), the strings are simply concatenated unchanged.
 *
 *   Parameters:
 *     prefix  – morpheme that precedes the stem (e.g. "ba", "mu", "ya", "ku")
 *     stem    – igicumbi or following morpheme (e.g. "iga", "eza", "amara")
 *     out     – destination buffer for the joined surface form
 *     out_sz  – size of out (should be at least strlen(prefix)+strlen(stem)+1)
 */
void kin_vv_join(const char *prefix, const char *stem,
                 char *out, size_t out_sz)
{
    if (!prefix || !stem || !out || out_sz == 0) return;

    size_t plen = strlen(prefix);
    size_t slen = strlen(stem);

    /* No prefix or no stem: trivial copy */
    if (plen == 0) { snprintf(out, out_sz, "%s", stem);   return; }
    if (slen == 0) { snprintf(out, out_sz, "%s", prefix); return; }

    char p_end   = prefix[plen - 1];
    char p_prev  = plen > 1 ? prefix[plen - 2] : '\0';
    char s_start = stem[0];

    /* No VV contact: just concatenate */
    if (!is_vowel(p_end) || !is_vowel(s_start)) {
        snprintf(out, out_sz, "%s%s", prefix, stem);
        return;
    }

    /* prefix body = everything except its final vowel */
    size_t base = plen - 1;

    if (p_end == 'u') {
        /* Rule 2: u → w
         * Sub-rule: kw/gw/hw before o/u → drop w (round vowel makes w redundant)
         * Only k, g, h trigger this; other consonants (m, t, r, b…) keep w. */
        bool round_V = (s_start == 'o' || s_start == 'u');
        bool kgh     = (p_prev == 'k' || p_prev == 'g' || p_prev == 'h');
        if (round_V && kgh) {
            /* kw+o→ko, kw+u→ku, gw+o→go, gw+u→gu, hw+o→ho, hw+u→hu */
            snprintf(out, out_sz, "%.*s%s", (int)base, prefix, stem);
        } else {
            snprintf(out, out_sz, "%.*sw%s", (int)base, prefix, stem);
        }

    } else if (p_end == 'i') {
        /* Rule 3: i → y; k before y palatalises to c (ki → cy)
         * Sub-rule: cy/jy before front vowels i/e — drop y and reverse:
         *   cy + i → ki,  cy + e → ke,  jy + e → ge  */
        bool front_V = (s_start == 'i' || s_start == 'e');
        if (p_prev == 'k') {
            if (front_V) {
                /* cy + i/e → ki/ke: k restored, y omitted */
                snprintf(out, out_sz, "%.*sk%s", (int)(base - 1), prefix, stem);
            } else {
                /* cy + a/o/u: valid — k→c, append y+stem */
                snprintf(out, out_sz, "%.*scy%s", (int)(base - 1), prefix, stem);
            }
        } else if (p_prev == 'j' && s_start == 'e') {
            /* jy + e → ge */
            snprintf(out, out_sz, "%.*sg%s", (int)(base - 1), prefix, stem);
        } else {
            snprintf(out, out_sz, "%.*sy%s", (int)base, prefix, stem);
        }

    } else {
        /* p_end == 'a' */
        bool prev_is_glide = (p_prev == 'y' || p_prev == 'w');
        if (!prev_is_glide && s_start == 'i') {
            /* Rule 4b: a + i → e (fusion); consume the stem's leading 'i' */
            snprintf(out, out_sz, "%.*se%s", (int)base, prefix, stem + 1);
        } else {
            /* Rule 4a / 4c: a elides — keep full stem */
            snprintf(out, out_sz, "%.*s%s", (int)base, prefix, stem);
        }
    }
}

/*
 * kin_has_invalid_cluster()
 *
 * Iteganyo ry'inzarara z'inkongi – CONSONANT CLUSTER RULE:
 *   Based on the Official/Approved Kinyarwanda Orthography Rules (RALC).
 *   Only the recognised consonant clusters (ibihekane) may be used.
 *
 *   Valid 2-letter clusters (ibihekane by'inyuguti ebyiri):
 *     by bw cw cy dw fw gw hw jw kw
 *     mf mp mv mw nw ns nt nz
 *     pf pw py rw ry sh sw sy ts tw ty vw vy zw
 *
 *   Any consonant may be preceded by a nasal (m/n) — these form larger
 *   clusters (mbw, mpw, ndw, ngw, nsh, etc.) whose 2-char tails are in
 *   the list above.
 *
 *   Note: 'sh' is a digraph for a single phoneme /ʃ/ and is always valid.
 *         'l' is valid only in proper names (Kigali, Repubulika, Leta …).
 *
 *   Returns true (= error) when an illegal CC pair is found.
 */
bool kin_has_invalid_cluster(const char *word) {
    if (!word) return false;
    for (int i = 0; word[i] && word[i+1]; i++) {
        char c1 = word[i], c2 = word[i+1];
        /* Skip: not a CC pair */
        if (is_vowel(c1) || is_vowel(c2)) continue;
        /* Nasals can begin any cluster (m/n start all nasal-initial clusters) */
        if (c1 == 'm' || c1 == 'n') continue;
        /* Glides y/w are valid as second member after any consonant */
        if (c2 == 'y' || c2 == 'w') continue;
        /* sh: digraph for /ʃ/ — always valid */
        if (c1 == 's' && c2 == 'h') continue;
        /* ts: valid cluster (e.g. kotswa, kwatswam) */
        if (c1 == 't' && c2 == 's') continue;
        /* pf: valid cluster (e.g. ipfundo, gukapfa) */
        if (c1 == 'p' && c2 == 'f') continue;
        /* All other non-nasal, non-glide CC pairs are invalid */
        return true;
    }
    return false;
}

/* ══════════════════════════════════════════════════════════════════════════
 * § 2  TREE 1 — IZINA MBONERA (Common Noun)
 *
 *  Formula:   D  +  RT  +  C
 *             │     │      └─ Igicumbi (stem; invariant; carries meaning)
 *             │     └──────── Indanganteko (class marker; drives all agreement)
 *             └────────────── Indomo (initial vowel: i / u / a; may be Ø)
 *
 *  The 16 inteko (noun classes) and their D+RT forms:
 *   Nt.1  umu-  (human sg)       Nt.2  aba-  (human pl)
 *   Nt.3  umu-  (thing sg)       Nt.4  imi-  (thing pl)
 *   Nt.5  i/iri- (sing)          Nt.6  ama-  (pl/mass)
 *   Nt.7  iki-  (thing sg)       Nt.8  ibi-  (thing pl)
 *   Nt.9  in/im- (animal/thing)  Nt.10 in/im- (animal/thing pl)
 *   Nt.11 uru-  (long/thin sg)   Nt.12 aka-  (diminutive sg)
 *   Nt.13 utu-  (diminutive pl)  Nt.14 ubu-  (abstract)
 *   Nt.15 uku-  (infinitive)     Nt.16 aha-  (locative)
 *
 *  Tree transitions from izina mbonera:
 *   → izina ntera (POS_RELATIVE_NOUN): noun qualifying another, via ngenera connector
 *   → proper name: indomo dropped (umugabo → Mugabo); detected by capitalisation
 *   → deverbative noun: igicumbi derived from verb root; check_deverbative() in pos_tagger.c
 *   → class shift: umugabo(Nt.1) → akagabo(Nt.12) → utugabo(Nt.13) → ubugabo(Nt.14)
 *     [class shifts not yet explicitly tracked — planned]
 *
 *  Disambiguation from verbs:
 *   Noun-class prefixes (umu/aba/iki/ibi/uru/aka/utu/ubu) overlap with verb SP.
 *   Guard: prefer verb when tense marker is unambiguous AND stem is valid.
 *   Handled in pos_tagger.c step 6 (noun) vs step 8 (verb) priority.
 * ══════════════════════════════════════════════════════════════════════════ */

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
    if (kin_starts_with(w, "am") && wlen > 4 && is_vowel(w[2])) return 6; /* Nt.6 a→∅/V */
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

    /* Nt.7: words surfaced as "icy-" (ki + vowel-initial stem → cy)        *
     * e.g. icyiza = i + cy(ki+iza) + iza; icyatsi = i + cy(ki+atsi) + atsi.
     * Checked BEFORE the bare i- fallback so "icyiza" is not misread as Nt.5. */
    if (kin_starts_with(w, "icy") && wlen > 4) return 7;

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
     *   Nt.8:  ibi+stem → bi+stem      (ibirori → birori, ibicumbi → bicumbi) *
     *   Nt.7:  iki+stem → ki+stem      (ikigiti → kigiti, ikibindi → kibindi) *
     *   Nt.5:  iri+stem → ri+stem      (irisoko? rihari, ribuyu)              *
     *   Nt.4:  imi+stem → mi+stem      (imirongo → mirongo)                    *
     *   Nt.6:  ama+stem → ma+stem      (amaso → maso, amafi → mafi)            *
     *   Nt.9:  in/im+stem → n/m drop   (inzu → nzu, handled by KNOWN_WORDS)   */
    if (kin_starts_with(w, "gi") && wlen > 3 && !is_vowel(w[2])) return 7;
    /* Nt.8: bi+C+stem. Guard: NOT ending in 'a' or 'e' to avoid catching   *
     * conjugated verbs like "bikora"(they work) or "bigenda"(they go).      *
     * Words ending in 'i','u','o' are nouns: birori, bicumbi, binshuti.     */
    if (kin_starts_with(w, "bi") && wlen > 4 && !is_vowel(w[2])
        && w[wlen-1] != 'a' && w[wlen-1] != 'e') return 8;
    /* Nt.7: ki+C+stem (standard k, complement to gi for k→g voiced).       *
     * Same guard as bi: exclude 'a'/'e' endings to avoid verb SPs.         *
     * e.g. kigiti=ikigiti, kibindi=ikibindi, kinshi=ikinshi.                */
    if (kin_starts_with(w, "ki") && wlen > 4 && !is_vowel(w[2])
        && w[wlen-1] != 'a' && w[wlen-1] != 'e') return 7;
    /* Nt.5: ri+C+stem (ri = indanganteko of Nt.5, often elided → bare i-). *
     * e.g. rihari (irihari?), ribuyu (iribuyu?).  Same FV guard.            */
    if (kin_starts_with(w, "ri") && wlen > 4 && !is_vowel(w[2])
        && w[wlen-1] != 'a' && w[wlen-1] != 'e') return 5;
    if (kin_starts_with(w, "mi") && wlen > 4 && !is_vowel(w[2])) return 4;
    if (kin_starts_with(w, "ma") && wlen > 3 && !is_vowel(w[2])) return 6;
    /* Nt.12: dropped D vowel 'a' from "aka" → bare "ka" (voiceless variant)  *
     * and k→g voiced → bare "ga".                                            *
     * e.g. gasozi=agasozi, kabati=akabati, kanzu=akanzu, kabiri=akabiri      *
     * Guard: NOT ending in 'a'/'e' to avoid catching Nt.12 verb SP "ka".    *
     * MUST come AFTER "aka"/"aga" full-prefix checks.                        *
     * Length > 4 to avoid very short ambiguous forms.                        */
    if (kin_starts_with(w, "ka") && wlen > 4 && !is_vowel(w[2])
        && w[wlen-1] != 'a' && w[wlen-1] != 'e') return 12;
    if (kin_starts_with(w, "ga") && wlen > 3 && !is_vowel(w[2])
        && w[wlen-1] != 'a' && w[wlen-1] != 'e') return 12;
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
     * e.g. burezi=uburezi, burayi=uburayi, burakari=uburakari           *
     * MUST come AFTER "ubu" check.                                        *
     * NOTE: "buri" (every/each) is already in INVARIABLES, checked first */
    if (kin_starts_with(w, "bu") && wlen > 4 && !is_vowel(w[2])) return 14;
    /* Nt.14: ubw+vowel → bw (u elided). e.g. bwangavu=ubwangavu.        *
     * MUST come AFTER "ubw" check.  No vowel guard needed: "ubw+V" → bw  *
     * is a standard phonological contraction (u+bw+angavu=ubwangavu).    */
    if (kin_starts_with(w, "bw") && wlen > 4) return 14;
    /* Nt.11: dropped D vowel 'u' from "uru" → bare "ru".               *
     * e.g. rurimi=ururimi (language), rushyi=urushyi, rutoki=urutoki,   *
     * rukali=urukali, rumanzi=urumanzi, rugezi=urugezi.                  *
     * MUST come AFTER "uru"/"urw" checks.                                *
     * Guard: NOT ending in 'a'/'e' to avoid conjugated verb SPs like    *
     * "rugenda" (Nt.11 SP + gend + a) being misread as nouns.           */
    if (kin_starts_with(w, "ru") && wlen > 4 && !is_vowel(w[2])
        && w[wlen-1] != 'a' && w[wlen-1] != 'e') return 11;
    /* Nt.2: dropped D vowel 'a' from "aba" → bare "ba".                 *
     * Pattern: agent/plural nouns (abahinzi, abarimu, abapfumu) appear  *
     * as "bahinzi", "barimu", "bapfumu" with 'a' dropped.               *
     * Guard: NOT ending in 'a' (would conflict with verbs bakora/bagenda)*
     * Length > 5 to avoid very short ambiguous forms.                    */
    if (kin_starts_with(w, "ba") && wlen > 5 && !is_vowel(w[2])
        && w[wlen-1] != 'a' && w[wlen-1] != 'e') return 2;

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
    /* Dropped 'i'/'u' from relative connectors before vowel-initial nouns:   *
     * "iby'abantu" → "byabantu", "icy'umuntu" → "cyumuntu",                *
     * "rw'igihugu" → "rwigihugu", "ry'igihugu" → "ryigihugu",              *
     * "tw'ubwenge" → "twubwenge".                                            *
     * These compound possessives appear merged in PDF/textbook sources.      */
    if (kin_starts_with(w, "by") && wlen > 4 && is_vowel(w[2])) return 8;
    if (kin_starts_with(w, "cy") && wlen > 4 && is_vowel(w[2])) return 7;
    if (kin_starts_with(w, "rw") && wlen > 4 && is_vowel(w[2])) return 11;
    if (kin_starts_with(w, "ry") && wlen > 4 && is_vowel(w[2])) return 5;
    if (kin_starts_with(w, "tw") && wlen > 4 && is_vowel(w[2])) return 13;
    /* "wa"/"w'" + vowel-initial noun → "wu/wi/we" merged forms (Nt.1/3 poss.)*
     * "w'umuntu" → wumuntu; "w'ubucuruzi" → wubucuruzi.                    *
     * Gated: wlen > 5, and NOT ending in 'a'/'e' to avoid verb SPs.        */
    if (kin_starts_with(w, "wu") && wlen > 5 && !is_vowel(w[2])
        && w[wlen-1] != 'a' && w[wlen-1] != 'e') return 3;
    /* "ya"/"y'" + vowel-initial noun → "yi/yu" merged forms (Nt.4/6 poss.) *
     * "y'ibiti" → yibiti; "y'igihugu" → yigihugu; "y'imigani" → yimigani. *
     * Gated: wlen > 5, not ending in verb FVs 'a'/'e'.                     */
    if (kin_starts_with(w, "yi") && wlen > 5 && !is_vowel(w[2])
        && w[wlen-1] != 'a' && w[wlen-1] != 'e') return 4;

    /* "nka" + noun = comparison particle + noun (merged form in Bible text). *
     * "nka" (= like/as/such as) is a preposition that appears fused with the *
     * following noun: nkabantu (like people), nkamazi (like water).          *
     * When present, recursively detect the class of the underlying noun.     *
     * Length thresholds are lower here because the outer "nka" context makes *
     * it safe to relax the dropped-D ambiguity guards.                       */
    if (kin_starts_with(w, "nka") && wlen > 6) {
        const char *sub = w + 3;   /* sub-word after "nka"               */
        size_t slen = wlen - 3;
        /* Try full-prefix forms first (these have no threshold issue)    */
        int sub_cls = kin_detect_noun_class(sub);
        if (sub_cls > 0) return sub_cls;
        /* Relaxed dropped-D checks for short sub-words (>= 4 chars total) */
        if (slen >= 4) {
            if (kin_starts_with(sub,"ba") && !is_vowel(sub[2])) return 2;  /* abantu */
            if (kin_starts_with(sub,"ma") && !is_vowel(sub[2])) return 6;  /* amazi */
            if (kin_starts_with(sub,"bi") && !is_vowel(sub[2])) return 8;  /* ibintu */
            if (kin_starts_with(sub,"ki") && !is_vowel(sub[2])) return 7;  /* ikintu */
            if (kin_starts_with(sub,"bu") && !is_vowel(sub[2])) return 14; /* uburyo */
            if (kin_starts_with(sub,"ru") && !is_vowel(sub[2])) return 11; /* urugo */
            if (kin_starts_with(sub,"ha") && !is_vowel(sub[2])) return 16; /* ahantu */
            if (kin_starts_with(sub,"mu") && !is_vowel(sub[2])) return 1;  /* umuntu */
            if (kin_starts_with(sub,"mi") && !is_vowel(sub[2])) return 4;  /* imirimo */
            if (kin_starts_with(sub,"ri") && !is_vowel(sub[2])) return 5;  /* iryo */
            if (kin_starts_with(sub,"zi") && !is_vowel(sub[2])) return 10; /* izindi */
            if (kin_starts_with(sub,"gi") && !is_vowel(sub[2])) return 7;  /* igiti */
        }
    }

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
    /* "nka" comparison prefix: strip "nka" and process the embedded noun.   *
     * We call ourselves recursively on the sub-word. For short sub-words    *
     * that fail the length guard in kin_detect_noun_class, we explicitly    *
     * handle the common dropped-D prefixes (ba, ma, ha, bi, ki, etc.)       */
    size_t wlencheck = strlen(word);
    if (kin_starts_with(word, "nka") && wlencheck > 6) {
        const char *sub = word + 3;
        size_t slen = wlencheck - 3;
        /* First try normal processing of the sub-word */
        char sub_stem[KIN_MAX_STEM];
        int  sub_cls = 0;
        if (kin_strip_noun_prefix(sub, sub_stem, &sub_cls) && sub_cls > 0) {
            if (class_out) *class_out = sub_cls;
            strncpy(stem_out, sub_stem, KIN_MAX_STEM - 1);
            stem_out[KIN_MAX_STEM - 1] = '\0';
            return true;
        }
        /* Relaxed dropped-D handling for short sub-words */
        if (slen >= 4) {
            const char *st = NULL; int rc = 0;
            if (kin_starts_with(sub,"ba") && !is_vowel(sub[2])) { rc=2; st=sub+2; }
            else if (kin_starts_with(sub,"ma") && !is_vowel(sub[2])) { rc=6; st=sub+2; }
            else if (kin_starts_with(sub,"bi") && !is_vowel(sub[2])) { rc=8; st=sub+2; }
            else if (kin_starts_with(sub,"ki") && !is_vowel(sub[2])) { rc=7; st=sub+2; }
            else if (kin_starts_with(sub,"gi") && !is_vowel(sub[2])) { rc=7; st=sub+2; }
            else if (kin_starts_with(sub,"bu") && !is_vowel(sub[2])) { rc=14;st=sub+2; }
            else if (kin_starts_with(sub,"ru") && !is_vowel(sub[2])) { rc=11;st=sub+2; }
            else if (kin_starts_with(sub,"ha") && !is_vowel(sub[2])) { rc=16;st=sub+2; }
            else if (kin_starts_with(sub,"mu") && !is_vowel(sub[2])) { rc=1; st=sub+2; }
            else if (kin_starts_with(sub,"mi") && !is_vowel(sub[2])) { rc=4; st=sub+2; }
            if (rc > 0 && st) {
                if (class_out) *class_out = rc;
                strncpy(stem_out, st, KIN_MAX_STEM - 1);
                stem_out[KIN_MAX_STEM - 1] = '\0';
                return true;
            }
        }
    }

    int cls = kin_detect_noun_class(word);
    if (cls == 0) { stem_out[0] = '\0'; return false; }
    if (class_out) *class_out = cls;

    const char *stem_start = word;

    /* Strip the surface prefix (D+RT combined) */
    switch (cls) {
        case 1: case 3:
            if (kin_starts_with(word, "umw"))      stem_start = word + 3; /* u + mu→mw/V: skip umw */
            else if (kin_starts_with(word, "umu")) stem_start = word + 3;
            else if (kin_starts_with(word, "mw"))  stem_start = word + 2; /* dropped D: mu→mw/V, skip mw */
            else if (kin_starts_with(word, "wu"))  stem_start = word + 1; /* w'+u-noun: wumuntu */
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
            else if (kin_starts_with(word,"yi"))   stem_start = word + 1; /* y'+i-noun: yibiti */
            else if (kin_starts_with(word,"mi"))   stem_start = word + 2; /* dropped D */
            break;
        case 5:
            if (kin_starts_with(word,"iri"))       stem_start = word + 3;
            else if (kin_starts_with(word,"ry"))   stem_start = word + 2; /* ry+V compound */
            else if (kin_starts_with(word,"ri"))   stem_start = word + 2; /* dropped D */
            else                                   stem_start = word + 1; /* bare i-   */
            break;
        case 6:
            if (kin_starts_with(word,"ama"))       stem_start = word + 3; /* ama */
            else if (kin_starts_with(word,"am") && is_vowel((unsigned char)word[2]))
                                                   stem_start = word + 2; /* a+m+V: a→∅ §1.1 */
            else if (kin_starts_with(word,"ma"))   stem_start = word + 2; /* dropped D */
            break;
        case 7:
            if (kin_starts_with(word,"icy"))       stem_start = word + 3; /* i+cy+stem: ki→cy before vowel */
            else if (kin_starts_with(word,"igi"))  stem_start = word + 3; /* k→g rule*/
            else if (kin_starts_with(word,"iki"))  stem_start = word + 3;
            else if (kin_starts_with(word,"gi"))   stem_start = word + 2; /* dropped D k→g */
            else if (kin_starts_with(word,"ki"))   stem_start = word + 2; /* dropped D k   */
            else if (kin_starts_with(word,"cy"))   stem_start = word + 2; /* dropped i: icy→cy */
            break;
        case 8:
            if (kin_starts_with(word,"iby"))       stem_start = word + 3; /* i+by+stem: bi→by before vowel */
            else if (kin_starts_with(word,"ibi"))  stem_start = word + 3;
            else if (kin_starts_with(word,"bi"))   stem_start = word + 2; /* dropped D 'i' */
            else if (kin_starts_with(word,"by"))   stem_start = word + 2; /* dropped i: iby→by */
            break;
        case 9: case 10:
            stem_start = word + 1; /* skip 'i', keep n/m as part of stem  */
            break;
        case 11:
            if (kin_starts_with(word, "urw"))  stem_start = word + 3; /* u + ru→rw/V: skip urw */
            else if (kin_starts_with(word,"rw")) stem_start = word + 2; /* dropped D: ru→rw/V, skip rw */
            else if (kin_starts_with(word,"uru")) stem_start = word + 3; /* full form */
            else if (kin_starts_with(word,"ru"))  stem_start = word + 2; /* dropped D 'u' */
            else                               stem_start = word + 3; /* fallback */
            break;
        case 12:
            if (kin_starts_with(word, "aga"))  stem_start = word + 3; /* k→g variant */
            else if (kin_starts_with(word,"ga")) stem_start = word + 2; /* dropped D k→g */
            else if (kin_starts_with(word,"ka")) stem_start = word + 2; /* dropped D k   */
            else                               stem_start = word + 3; /* aka */
            break;
        case 13:
            if (kin_starts_with(word, "tw"))  stem_start = word + 2; /* dropped D */
            else                             stem_start = word + 3; /* utu */
            break;
        case 14:
            if (kin_starts_with(word, "ubw"))  stem_start = word + 3; /* u + bu→bw/V: skip ubw */
            else if (kin_starts_with(word, "ubu")) stem_start = word + 3; /* ubu */
            else if (kin_starts_with(word, "bw"))  stem_start = word + 2; /* dropped D: bu→bw/V, skip bw */
            else if (kin_starts_with(word, "bu"))  stem_start = word + 2; /* dropped D 'u' */
            break;
        case 15:
            if (kin_starts_with(word, "ukw"))  stem_start = word + 3; /* u + ku→kw/V: skip ukw */
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

/* ══════════════════════════════════════════════════════════════════════════
 * § 4  TREE 3 — INSHINGA (Verb)
 *
 *  The verb tree has two main branches detected here:
 *    § 4a  IMBUNDO (Infinitive / citation form)   → kin_is_verb_infinitive()
 *    § 4b  ITONDAGUYE (Conjugated verb)           → kin_is_verb_conjugated()
 *
 *  Verb extensions (utumamo) are a sub-tree of the conjugated branch:
 *    -w-      Imbundo    (passive)          gukorwa, gukozwa
 *    -ish-    Integeko   (causative)        gukoresha, kwigisha
 *    -ir-     Ikirango   (applicative)      gukorera, guhingira
 *    -an-     Igisubizo  (reciprocal)       gukorana, kwigana
 *    -ik-     Ngirika    (stative/potent.)  gufatika, guhingika
 *    -uk-/-ur-Ngiruka/ra (reversive)        gufunguka, gufungura
 *
 *  Verb modes (uburyo) are distinguished by their TM+FV combination:
 *    Ikirango   (indicative)  — all tenses, TM varies
 *    Inyifurizo (optative)    — SP+ra+ka+C+a
 *    Integeko   (imperative)  — bare C+a (no SP)
 *    Inkurikizo (sequential)  — SP+ka+C+a
 *    Ikigombero (subjunctive) — SP+C+e
 *    Inziganyo  (conditional) — SP+a+[ku]+C+a
 *
 *  Tree transitions from inshinga:
 *   → izina rivuye mu nshinga (deverbative noun): verb root → POS_NOUN
 *     check_deverbative() in pos_tagger.c detects this after noun tagging
 *   → Inshinga nkene (copula kuba): TENSE_COPULA_PAST / TENSE_COPULA_PRES
 *   → Ingirwanshinga (-ti quotative): frozen suppletive forms in INVARIABLES
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── § 4a  IMBUNDO (Verb Infinitive) ─────────────────────────────────────── */

/*
 * kin_is_verb_infinitive()
 *
 * Kinyarwanda verb infinitives (class Nt.15) have the surface form:
 *   gu + C(voiced)... + stem + a
 *   ku + C(voiceless)... + stem + a
 *   kw / gw + V + stem + a   (before vowel-initial stems)
 *
 * The canonical final vowel is -a, but in relative and complement clauses
 * the infinitive class verb can appear with derived endings:
 *   -e      subjunctive/relative:    gukore (that it be done)
 *   -ye     past perfect:            gupfuye (having died), guteye (having caused)
 *   -tse    past perf (C-final st.): guhindutse, gutebutse
 *   -we     passive:                 gufitwe, gutangirwe, gushyirwe
 *   -rwe    passive applicative:     guhorerwe
 *   -jewe   passive causative:       gusohozwe
 *
 * Returns true and writes the bare stem (without prefix and without FV/suffix).
 */

/*
 * kin_is_valid_verb_stem_shape() — phonological plausibility check.
 *
 * Replaces the word-list gate (kin_is_known_verb_stem) for tenses where
 * the morphological context is distinctive enough that any phonologically
 * legal stem can be accepted.
 *
 * Rules derived from Kinyarwanda amategeko y'igenamajwi (phonological rules):
 *   1. Minimum 2 characters — monosyllabic stems do exist (e.g. "b" of kuba)
 *      but we require ≥2 to avoid matching noise.
 *   2. Must contain at least one consonant — pure vowel sequences are not
 *      valid stems (no Kinyarwanda verb root is all vowels).
 *   3. No run of 3+ consecutive consonants — Kinyarwanda phonotactics allow
 *      two consonants in onset/coda but not three.
 * These three rules accept all real stems (bon, gend, gir, tabar, fash…)
 * while rejecting noise (aa, oo, ttt, …).
 */
bool kin_is_valid_verb_stem_shape(const char *stem) {
    if (!stem) return false;
    size_t len = strlen(stem);
    if (len < 2) return false;

    bool has_consonant = false;
    int  cons_run      = 0;
    for (size_t i = 0; i < len; i++) {
        char c = (char)(stem[i] | 0x20);   /* to lower — stems are already lc */
        bool vowel = (c=='a'||c=='e'||c=='i'||c=='o'||c=='u');
        if (!vowel) {
            has_consonant = true;
            if (++cons_run > 2) return false;   /* triple consonant cluster */
        } else {
            cons_run = 0;
        }
    }
    return has_consonant;
}

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

    const char *inner = NULL;  /* pointer past the prefix                  */

    if (kin_starts_with(word, "kw") && is_vowel(word[2]))   inner = word + 2;
    else if (kin_starts_with(word, "gw") && is_vowel(word[2])) inner = word + 2;
    else if (kin_starts_with(word, "gu") && !is_vowel(word[2])) inner = word + 2;
    else if (kin_starts_with(word, "ku") && !is_vowel(word[2])) inner = word + 2;
    /* §1.2 u→∅ before back vowels o and u (not glide formation):
     * ku + o-initial stem → k + o... (kororoka = ku+ororok+a, not *kwororoka)
     * ku + u-initial stem → k + u... (kuzura   = ku+uzur+a,   not *kwuzura)
     * Minimum length guard > 4: k + vowel + stem(≥1) + a = at least 4 chars. */
    else if (word[0] == 'k' && (word[1] == 'o' || word[1] == 'u') && len > 4)
        inner = word + 1;
    else return false;

    /* Inner must be at least 2 chars */
    size_t inner_len = strlen(inner);
    if (inner_len < 2) return false;

    /* ── CANONICAL: ends in 'a' ─────────────────────────────────────────── */
    if (inner[inner_len - 1] == 'a') {
        if (stem_out) {
            strncpy(stem_out, inner, inner_len - 1);
            stem_out[inner_len - 1] = '\0';
        }
        return true;
    }

    /* ── EXTENDED: non-standard final vowels (relative/complement/passive) ─ *
     * Only accept if the word is long enough (len ≥ 6) to avoid accidental  *
     * matches on very short words.                                            */
    if (len < 6) return false;

    /* Past perfect -ye: gupfuye, guteye, gushavuye */
    if (inner_len >= 3 && kin_ends_with(inner, "ye")) {
        size_t sl = inner_len - 2;
        if (stem_out) { strncpy(stem_out, inner, sl); stem_out[sl] = '\0'; }
        return true;
    }
    /* Past perfect -tse (consonant-final stems): guhindutse, gutamurutse */
    if (inner_len >= 4 && kin_ends_with(inner, "tse")) {
        size_t sl = inner_len - 3;
        if (stem_out) { strncpy(stem_out, inner, sl); stem_out[sl] = '\0'; }
        return true;
    }
    /* Passive -we / -rwe / -jwe / -zwe: gufitwe, guhorerwe, gushyirwe */
    if (inner_len >= 3 && kin_ends_with(inner, "we")) {
        size_t sl = inner_len - 2;
        if (stem_out) { strncpy(stem_out, inner, sl); stem_out[sl] = '\0'; }
        return true;
    }
    /* Subjunctive / relative -e: gukore, gufashe, gushire */
    if (inner_len >= 2 && inner[inner_len - 1] == 'e') {
        size_t sl = inner_len - 1;
        if (stem_out) { strncpy(stem_out, inner, sl); stem_out[sl] = '\0'; }
        return true;
    }

    return false;
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
 * bare_out receives the stem text after the OM (≥ 1 char; LAYER 3c in
 * kin_is_verb_conjugated validates the bare stem is a known root).
 * Single-char roots like "h" (guha), "b" (kuba), "z" (kuza) are valid
 * and must be reachable here — e.g. i+bi+h+a = ibiha (God gives).
 */
static bool om_strip(const char *stem, int *om_cls_out,
                     char *bare_out, size_t bare_sz) {
    size_t slen = strlen(stem);
    for (int i = 0; OM_TABLE[i].om; i++) {
        /* Try normal form: require at least 1 char of bare stem remaining */
        size_t olen = strlen(OM_TABLE[i].om);
        if (slen > olen && kin_starts_with(stem, OM_TABLE[i].om)) {
            if (om_cls_out) *om_cls_out = OM_TABLE[i].cls;
            if (bare_out) { strncpy(bare_out, stem + olen, bare_sz - 1);
                            bare_out[bare_sz - 1] = '\0'; }
            return true;
        }
        /* Try vowel-initial variant */
        size_t ovlen = strlen(OM_TABLE[i].om_v);
        if (slen > ovlen && kin_starts_with(stem, OM_TABLE[i].om_v)) {
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
    /* Stative: -ik- / -ek- (Ngirika)
     * Vowel harmony: stems with mid vowel (e/o) take -ek-, others take -ik-.
     *   guhingika → hing+ik+a  (stem vowel /i/ → -ik-)
     *   gutekeka  → tek+ek+a   (stem vowel /e/ → -ek-)
     *   gusomeka  → som+ek+a   (stem vowel /o/ → -ek-)
     * Checked before reversive -uk/-ok because both end in consonant+k. */
    if (slen > 4 && (kin_ends_with(stem, "ik") || kin_ends_with(stem, "ek"))) {
        size_t blen = slen - 2;
        strncpy(tmp, stem, blen); tmp[blen] = '\0';
        if (blen >= 2) {
            if (ext_out)  *ext_out = VEXT_STATIVE;
            if (bare_out) { strncpy(bare_out, tmp, bare_sz-1); bare_out[bare_sz-1]='\0'; }
            return true;
        }
    }
    /* Reversive -ur (Ngirura: gufungura → fung+ur+a, guhindura → hind+ur+a) */
    if (slen > 4 && kin_ends_with(stem, "ur")) {
        size_t blen = slen - 2;
        strncpy(tmp, stem, blen); tmp[blen] = '\0';
        if (blen >= 2) {
            if (ext_out)  *ext_out = VEXT_REVERSIVE;
            if (bare_out) { strncpy(bare_out, tmp, bare_sz-1); bare_out[bare_sz-1]='\0'; }
            return true;
        }
    }
    /* Reversive -uk (Ngiruka: gufunguka → fung+uk+a, guhinduka → hind+uk+a) */
    if (slen > 4 && kin_ends_with(stem, "uk")) {
        size_t blen = slen - 2;
        strncpy(tmp, stem, blen); tmp[blen] = '\0';
        if (blen >= 2) {
            if (ext_out)  *ext_out = VEXT_REVERSIVE;
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
    /* Passive-perfect epenthetic suffix: -ejw or -ijw                       *
     * Phonological variant of passive -w- on stems ending in -sh/-esh:     *
     * epenthetic vowel (e or i by vowel harmony) is inserted before -jw-.  *
     *   gupesha  (root pesh): pesh + ejw + e = peshejwe                    *
     *   kwandika (root andik): andik + ijw + e = andikijwe                 *
     * Must be checked BEFORE the single-char -w passive to prevent -w from *
     * consuming the 'w' of "ejw", leaving "peshej" as a false root.        *
     * slen > 4: minimum bare(2) + suffix(3) = 5.                           */
    if (slen > 4 &&
        (kin_ends_with(stem, "ejw") || kin_ends_with(stem, "ijw"))) {
        size_t blen = slen - 3;
        strncpy(tmp, stem, blen); tmp[blen] = '\0';
        if (blen >= 2) {
            if (ext_out)  *ext_out = VEXT_PASSIVE;
            if (bare_out) { strncpy(bare_out, tmp, bare_sz-1); bare_out[bare_sz-1]='\0'; }
            return true;
        }
    }
    /* Passive: -w (single char, check last).
     * Guard slen >= 3 (not >3): short passive roots like "it+w" (kwitwa)
     * have slen=3; the inner blen>=2 check already prevents bare stems <2. */
    if (slen >= 3 && stem[slen-1] == 'w') {
        size_t blen = slen - 1;
        strncpy(tmp, stem, blen); tmp[blen] = '\0';
        if (blen >= 2) {
            if (ext_out)  *ext_out = VEXT_PASSIVE;
            if (bare_out) { strncpy(bare_out, tmp, bare_sz-1); bare_out[bare_sz-1]='\0'; }
            return true;
        }
    }
    /* Causative -iz-/-ez- (allomorph for applicative-base verbs):
     * When a verb whose root ends in -ir (applicative) takes the causative-y
     * extension, the sequence -ir + y fuses: r+y→z, giving -iz-.
     * e.g. gusinziriza: root sinzir + iz(causative-y on applicative) + a
     *      sinziriz → strip iz → sinzir = known root (gusinzira) ✓
     * Also handles -ez- (vowel-harmony variant for mid-vowel roots).
     * Guard: stripped result must be a known verb stem (prevents false splits). */
    if (slen > 4 && (kin_ends_with(stem, "iz") || kin_ends_with(stem, "ez"))) {
        size_t blen = slen - 2;
        strncpy(tmp, stem, blen); tmp[blen] = '\0';
        if (blen >= 2 && kin_is_known_verb_stem(tmp)) {
            if (ext_out)  *ext_out = VEXT_CAUSATIVE_IZ;
            if (bare_out) { strncpy(bare_out, tmp, bare_sz-1); bare_out[bare_sz-1]='\0'; }
            return true;
        }
    }
    /* Causative-y (Ngiza): stem ends in 'z' where z ← r+y (r+y→z rule, §1.3).
     * Replace final 'z' with 'r'; if the result is a known verb stem, this is
     * the -y- causative form of that r-final base verb.
     * e.g. "mez" → "mer" (kumera → kumeza); bare_out = "mer" (citation root).
     * e.g. "ez"  → "er"  (kwera→kweza, 2-char root): guard is >= 2.
     * Checked last because z is a valid base-stem consonant; we only strip when
     * the r-form is a confirmed known stem. */
    if (slen >= 2 && stem[slen-1] == 'z') {
        strncpy(tmp, stem, slen - 1); tmp[slen-1] = 'r'; tmp[slen] = '\0';
        if (kin_is_known_verb_stem(tmp)) {
            if (ext_out)  *ext_out = VEXT_CAUSATIVE_Y;
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
        /* Vowel-initial stem variants: u→w before vowel (bu→bw, ru→rw, tu→tw, mu→mw) */
        { "bw",   14  },  /* Nt.14 bu+vowel → bw (bwera, bwigisha)        */
        { "rw",   11  },  /* Nt.11 ru+vowel → rw (rwera, rwemera)         */
        { "tw",    0  },  /* 1pl   tu+vowel → tw (twemera, twiga)         */
        { "mw",    0  },  /* 2pl   mu+vowel → mw (mwuzure, mwororoke)     */
        /* Past augment variants: consonant SP + a(past augment) → 3-char SP  *
         * In past tense, the vowel SP (ki/bi/ri/zi) fuses with the past      *
         * augment 'a': ki+a = kya → surface cy before vowel = cya.           *
         * e.g. byagiraga = bya(SP past Nt.8) + gir + aga                    *
         *      cyagiraga = cya(SP past Nt.7) + gir + aga                    *
         * These MUST appear before the 2-char by/cy/ry/zy entries so that    *
         * bya/cya match before by/cy in the greedy prefix scan.              */
        { "cya",   7  },  /* Nt.7  past (ki+a → kya → cya before vowel)      */
        { "bya",   8  },  /* Nt.8  past (bi+a → bya)                         */
        { "rya",   5  },  /* Nt.5  past (ri+a → rya)                         */
        { "zya",  10  },  /* Nt.10 past (zi+a → zya)                         */
        /* i→y before vowel-initial stems (phonological rule p.7-8)       */
        { "cy",    7  },  /* Nt.7  ki+vowel → cy  (cyatumye, cyemera, cyari)  */
        { "by",    8  },  /* Nt.8  bi+vowel → by  (byari, byemera, byibutse)  */
        { "ry",    5  },  /* Nt.5  ri+vowel → ry  (ryari, ryibutse)           */
        { "zy",   10  },  /* Nt.10 zi+vowel → zy  (zyari etc.)                */
        { "zu",   10  },  /* Nt.10 zi+u-initial verb → zu  (zumva, zubaka)    */
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
        { "gi",    7  },  /* Nt.7  ki→gi before consonant §3.7.1 (gikora, gikururuka) */
        { "bi",    8  },
        { "zi",   10  },
        { "ru",   11  },
        { "ka",   12  },
        { "ga",   12  },  /* Nt.12 ka→ga before consonant §3.7.1 (gahinda, gakora)   */
        { "bu",   14  },
        { "ku",   15  },
        { "ha",   16  },
        /* Augmented locative SP: ha + u/i/o augment → hu/hi/ho            *
         * In complement/passive constructions, ha+u-augment → "hu-"       *
         * e.g. "hubakwa" = ha(SP16) + u(augment) + bak + w + a           *
         *      "hifashishijwe" = ha(SP16) + i(augment) + fashi + sh + ijwe */
        { "hu",   16  }, /* ha + u-augment (hubakwa, hubatswe, hubahirizwa) */
        { "hi",   16  }, /* ha + i-augment (hifashishijwe, hitabwa, hitaka)*/
        /* 1-char prefixes – lowest priority */
        { "u",     3  },
        { "i",     4  },
        { "a",     1  },
        { "n",     0  },
        { "w",     3  },  /* Nt.3/2sg u+vowel → w (wera=u+er+a, wemera)  */
        /* Elided SP: ya+V-stem → y+V-stem (a→Ø before vowel, p.7-8)     *
         * e.g. ya+ig+a = "yiga" (SP ya→y before i-initial stem)         */
        { "y",     0  },  /* elided "ya" SP (ambiguous: cls1 past, cls6 pres, cls9 before vowel) */
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

        /* "ara" has ra already embedded; inner ends in 'a' (or locative) */
        if (strcmp(SP[i].pfx, "ara") == 0) {
            /* ara + stem + aho/amo/ayo: PRESENT + LOCATIVE suffix */
            if (ilen >= 5 &&
                (kin_ends_with(inner, "aho") || kin_ends_with(inner, "amo") ||
                 kin_ends_with(inner, "ayo"))) {
                size_t sl = ilen - 3;   /* strip a + 2-char locative */
                if (sl >= 2) {
                    if (stem_buf) { strncpy(stem_buf, inner, sl); stem_buf[sl]='\0'; }
                    if (subj_class) *subj_class = 1;
                    if (tense_out)  *tense_out  = TENSE_PRESENT;
                    return true;
                }
            }
            if (inner[ilen-1] != 'a' || ilen < 2) continue;
            if (stem_buf) { strncpy(stem_buf, inner, ilen-1); stem_buf[ilen-1]='\0'; }
            if (subj_class) *subj_class = 1;
            if (tense_out)  *tense_out  = TENSE_PRESENT;
            return true;
        }

        /* FUTURE: za + stem + a */
        if (kin_starts_with(inner, "za") && ilen > 3 && inner[ilen-1]=='a') {
            const char *s = inner + 2; size_t sl = ilen - 3;
            /* Guard: a 1-char root must be a known stem (z=kuza, b=kuba, h=guha,
             * v=kuva). Rejects "n" from "azana" (which is kuzana, present) so
             * that the PRESENT_NORA branch below can give the correct reading.
             * NOTE: do NOT use continue — that would skip to the next SP prefix,
             * preventing the PRESENT_NORA branches from running for this SP. */
            bool fut_ok = (sl >= 2);
            if (!fut_ok && sl == 1) {
                char tmp[2] = {s[0], '\0'};
                fut_ok = kin_is_known_verb_stem(tmp);
            }
            if (fut_ok) {
                if (stem_buf) { strncpy(stem_buf, s, sl); stem_buf[sl]='\0'; }
                if (subj_class) *subj_class = SP[i].cls;
                if (tense_out)  *tense_out  = TENSE_FUTURE;
                return true;
            }
            /* sl==0 or unknown 1-char root: fall through to other tense checks */
        }
        /* FUTURE + LOCATIVE: za + stem + a + ho/mo/yo                        *
         * e.g. nzabaho = n(1sg) + za + b(kuba) + a + ho                      *
         *      azabaho = a(Nt.1) + za + b + a + ho                           *
         *      bizabaho = bi(Nt.8) + za + b + a + ho                         *
         * Structure: inner = "za" + stem + "a" + "ho/mo/yo" (len ≥ 6)        *
         * The char at [ilen-3] must be 'a' (final vowel before locative).    */
        if (kin_starts_with(inner, "za") && ilen >= 6 &&
            (kin_ends_with(inner,"ho") || kin_ends_with(inner,"mo") ||
             kin_ends_with(inner,"yo")) &&
            inner[ilen-3] == 'a') {
            /* stem = between "za" and the "a+loc" at the end */
            const char *s = inner + 2; size_t sl = ilen - 5;
            if (sl < 1) continue;
            if (stem_buf) { strncpy(stem_buf, s, sl); stem_buf[sl]='\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_FUTURE;
            return true;
        }
        /* FUTURE + FV=e (neg-future subjunctive/prohibitive): za + stem + e   *
         * e.g. ntuzakiryeho → inner="zakirye" stripped to stem="kiry"        *
         * Must be checked BEFORE NARRATIVE to avoid 'za' matching 'ka'.      */
        if (kin_starts_with(inner, "za") && ilen > 3 && inner[ilen-1]=='e') {
            const char *s = inner + 2; size_t sl = ilen - 3;
            if (sl < 1) continue;
            if (stem_buf) { strncpy(stem_buf, s, sl); stem_buf[sl]='\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_FUTURE_SUBJ;
            return true;
        }
        /* FUTURE + FV=e + LOCATIVE: za + stem + e + ho/mo/yo                 */
        if (kin_starts_with(inner, "za") && ilen >= 6 &&
            (kin_ends_with(inner,"ho") || kin_ends_with(inner,"mo") ||
             kin_ends_with(inner,"yo")) &&
            inner[ilen-3] == 'e') {
            const char *s = inner + 2; size_t sl = ilen - 5;
            if (sl < 1) continue;
            if (stem_buf) { strncpy(stem_buf, s, sl); stem_buf[sl]='\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_FUTURE_SUBJ_LOC;
            return true;
        }
        /* NARRATIVE + FV=e (neg-narrative subjunctive): ka + stem + e        *
         * e.g. ntukazirye → inner="kazirye" → stem="ziry"                    */
        if (kin_starts_with(inner, "ka") && ilen > 3 && inner[ilen-1]=='e'
            && strcmp(SP[i].pfx, "ka") != 0) {
            const char *s = inner + 2; size_t sl = ilen - 3;
            if (sl < 1) continue;
            if (stem_buf) { strncpy(stem_buf, s, sl); stem_buf[sl]='\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_NARRATIVE_SUBJ;
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
        /* NARRATIVE voiced: ka→ga (k→g §3.7.1 after vowel-final SP).
         * In the inkurikizo (narrative/sequential) tense the TM 'ka' voices
         * to 'ga' after certain SPs, e.g. ki+ga+tos+a = kigatosa.
         * Gate on stem validation (≥ 2 chars, known root) to avoid false
         * positives against roots that happen to start with the letters
         * left after stripping 'ga' (e.g. kugaba root='gab' not 'b').   */
        if (kin_starts_with(inner, "ga") && ilen > 3 && inner[ilen-1]=='a'
            && strcmp(SP[i].pfx, "ka") != 0
            && strcmp(SP[i].pfx, "ga") != 0) {
            size_t sl = ilen - 3;
            if (sl >= 2) {
                char cand[KIN_MAX_STEM];
                strncpy(cand, inner + 2, sl); cand[sl] = '\0';
                bool found = kin_is_known_verb_stem(cand);
                /* h→s: root-final h surfaces as s before FV 'a' in conjugated
                 * forms (e.g. toh→tos in kigatosa from gutoha).  Try restoring
                 * the citation h-form if the s-form is not in the lexicon.      */
                if (!found && sl >= 2 && cand[sl - 1] == 's') {
                    char cand_h[KIN_MAX_STEM];
                    strncpy(cand_h, cand, sl - 1);
                    cand_h[sl - 1] = 'h'; cand_h[sl] = '\0';
                    if (kin_is_known_verb_stem(cand_h)) {
                        strncpy(cand, cand_h, KIN_MAX_STEM - 1);
                        cand[KIN_MAX_STEM - 1] = '\0';
                        found = true;
                    }
                }
                if (found) {
                    if (stem_buf) { strncpy(stem_buf, cand, KIN_MAX_STEM-1);
                                    stem_buf[KIN_MAX_STEM-1] = '\0'; }
                    if (subj_class) *subj_class = SP[i].cls;
                    if (tense_out)  *tense_out  = TENSE_NARRATIVE;
                    return true;
                }
            }
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
        /* PRESENT with ra marker (consonant-initial root) */
        if (kin_starts_with(inner, "ra") && ilen > 3 &&
            (inner[ilen-1]=='a' || inner[ilen-1]=='o')) {
            const char *s = inner + 2; size_t sl = ilen - 3;
            if (sl < 1) continue;
            if (stem_buf) { strncpy(stem_buf, s, sl); stem_buf[sl]='\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_PRESENT;
            return true;
        }
        /* PRESENT with ra, vowel-initial root — §1.1 rule 4c: the 'a' of TM 'ra'
         * elides before a vowel-initial root, leaving only 'r' on the surface.
         * Pattern: inner = 'r' + vowel + rest + 'a'
         *   e.g.  ireza  = i(SP·Nt.4) + r[a→∅] + ez(root) + a(FV)
         *         areza  = a(SP·Nt.1) + r[a→∅] + ez(root) + a(FV)
         *         iremera= i(SP·Nt.4) + r[a→∅] + emer(root) + a(FV)
         * Validated: root must be a known stem or a causative-y surface (§1.3)
         * to prevent accidental matches on consonant-r-initial habitual forms.  */
        if (ilen >= 4 && inner[0] == 'r' && is_vowel(inner[1]) && inner[ilen-1] == 'a') {
            size_t rvlen = ilen - 2;   /* strip leading 'r' and trailing 'a' */
            char   rvbuf[KIN_MAX_STEM];
            if (rvlen >= 2 && rvlen < KIN_MAX_STEM - 1) {
                strncpy(rvbuf, inner + 1, rvlen); rvbuf[rvlen] = '\0';
                if (kin_is_known_verb_stem(rvbuf) || kin_is_causative_y_surface(rvbuf)) {
                    if (stem_buf) { strncpy(stem_buf, rvbuf, KIN_MAX_STEM-1);
                                    stem_buf[KIN_MAX_STEM-1] = '\0'; }
                    if (subj_class) *subj_class = SP[i].cls;
                    if (tense_out)  *tense_out  = TENSE_PRESENT;
                    return true;
                }
            }
        }
        /* PAST IMPERFECT: ends in aga
         * ilen >= 4 (not > 4): single-consonant roots must be reachable.
         * e.g. kuva (root=v): cya+v+aga=cyavaga → inner="vaga" (len=4)
         *      kuba (root=b): cya+b+aga=cyabaga → inner="baga" (len=4)
         * These are unrelated verbs; both happen to have 1-char roots. */
        if (ilen >= 4 && kin_ends_with(inner, "aga")) {
            size_t sl = ilen - 3;
            if (stem_buf) { strncpy(stem_buf, inner, sl); stem_buf[sl]='\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_PAST_IMPF;
            return true;
        }
        /* PAST PERFECT with epenthetic 'i' (-iye FV):                         *
         * Rule: consonant-final root + iye → e.g. zi+kwir+iye = zikwiriye    *
         * Epenthesis of 'i' occurs before 'ye' when root ends in consonant.  *
         * Must gate on kin_is_known_verb_stem to avoid false positives        *
         * (otherwise "biye", "tiye" etc. would all fire here).               *
         * Only applies when ortho rule 3.9.3 would give wrong result (r+ye). */
        if (ilen > 4 && kin_ends_with(inner, "iye")) {
            size_t sl = ilen - 3;
            char cand[KIN_MAX_STEM];
            strncpy(cand, inner, sl); cand[sl] = '\0';
            if (sl >= 2 && !is_vowel((unsigned char)cand[sl-1])
                        && kin_is_known_verb_stem(cand)) {
                if (stem_buf) { strncpy(stem_buf, cand, KIN_MAX_STEM-1);
                                stem_buf[KIN_MAX_STEM-1] = '\0'; }
                if (subj_class) *subj_class = SP[i].cls;
                if (tense_out)  *tense_out  = TENSE_PAST_PERF;
                return true;
            }
        }
        /* PAST PERFECT: y-final root + bare 'e' FV                              *
         * Rule: some roots end in 'y' and take bare 'e' FV (not 'ye').         *
         *   giy+e = giye  (yagiye ← kugenda, suppletive past stem)             *
         *   jy+e  = jye   (bajye  ← kujya, directional go)                    *
         * Only fires for past-form SPs (same list as PAST_SP_LIST).            */
        {
            static const char *PAST_SP_Y[] = {
                "twa","ya","wa","na","bya","cya","rya","zya",
                "bwa","kwa","rwa","mwa","za","ba","mu","tu","a",
                "ni","ri","zi","bi","ki","ru","ka","bu","ku","ha", NULL
            };
            bool sp_is_past_y = false;
            for (int pi = 0; PAST_SP_Y[pi]; pi++) {
                if (strcmp(SP[i].pfx, PAST_SP_Y[pi]) == 0)
                    { sp_is_past_y = true; break; }
            }
            if (sp_is_past_y && ilen >= 3 && inner[ilen-1] == 'e' && inner[ilen-2] == 'y') {
                size_t sl = ilen - 1;   /* strip just 'e', keep 'y' */
                char cand_y[KIN_MAX_STEM];
                strncpy(cand_y, inner, sl); cand_y[sl] = '\0';
                if (kin_is_known_verb_stem(cand_y)) {
                    if (stem_buf) { strncpy(stem_buf, cand_y, KIN_MAX_STEM-1);
                                    stem_buf[KIN_MAX_STEM-1] = '\0'; }
                    if (subj_class) *subj_class = SP[i].cls;
                    if (tense_out)  *tense_out  = TENSE_PAST_PERF;
                    return true;
                }
            }
        }
        /* PAST PERFECT: gucya surface form — cy→k, iye→eye (§11.3)
         * §11.3: cy cannot precede i or e; before 'e' it becomes 'k'.
         * Past perf FV "-iye" triggers palatal harmony: i→e after palatal cy.
         * Derivation: SP + cy + iye → SP + k + eye.
         * inner for any SP = "keye" (exactly 4 chars).
         * Must be checked before the general "ye" branch to prevent
         * misparse as stem="ke" (unknown root). */
        if (ilen == 4 && strcmp(inner, "keye") == 0) {
            if (stem_buf) { strncpy(stem_buf, "cy", KIN_MAX_STEM-1); stem_buf[2] = '\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_PAST_PERF;
            return true;
        }
        /* PAST PERFECT: ends in ye
         * Guard: if inner ends in ...y+e AND stripping only 'e' gives a
         * known y-final root (e.g. "meny" from "menye"), the 'y' belongs
         * to that root (kumenya), NOT to the FV "ye" (kumena past perf).
         * In that case skip — the SUBJUNCTIVE check below will handle it
         * with the correct root and FV='e'.
         * Example: i+meny+e (kumenya subjunctive "ngo imenye") must not
         * be parsed as i+men+ye (kumena past perfect).                    */
        if (ilen > 3 && kin_ends_with(inner, "ye")) {
            bool y_root_exists = false;
            if (inner[ilen-2] == 'y') {
                char cand_y[KIN_MAX_STEM];
                size_t csl = ilen - 1;
                strncpy(cand_y, inner, csl); cand_y[csl] = '\0';
                y_root_exists = kin_is_known_verb_stem(cand_y);
            }
            if (!y_root_exists) {
                size_t sl = ilen - 2;           /* strip "ye" */
                const char *stem_src = inner;
                size_t stem_len = sl;
                /* ra-TM inside "ye" path: if stripping "ye" gives a
                 * candidate that starts with "ra" but is not itself a
                 * known root, and dropping "ra" gives a known root,
                 * this is SP+ra+root+ye — strip ra-TM here so
                 * morph_dispatch sees the bare root.
                 * Guard: only when full candidate is NOT known (prevents
                 * stripping "ra" from genuine r-initial roots like "raba"
                 * (kuraba) which ARE in VERB_STEMS). */
                if (sl >= 3 && inner[0]=='r' && inner[1]=='a') {
                    char cand[KIN_MAX_STEM];
                    strncpy(cand, inner, sl); cand[sl] = '\0';
                    if (!kin_is_known_verb_stem(cand) &&
                            kin_is_known_verb_stem(cand + 2)) {
                        stem_src = inner + 2;
                        stem_len = sl - 2;
                    }
                }
                if (stem_buf && stem_len < KIN_MAX_STEM) {
                    strncpy(stem_buf, stem_src, stem_len);
                    stem_buf[stem_len] = '\0';
                }
                if (subj_class) *subj_class = SP[i].cls;
                if (tense_out)  *tense_out  = TENSE_PAST_PERF;
                return true;
            }
            /* y belongs to a longer root — fall through to SUBJUNCTIVE */
        }
        /* PAST PERFECT with ra tense marker: SP + ra + stem + e               *
         * e.g. murakoze = mu(SP) + ra + koz + e (2pl past perfect)            *
         *      mwaramutse = mwa(SP) + ra + muts + e (2pl, good morning)        *
         * Note: the "ara" special case above already handles Nt.1 3sg present. *
         * This catches all other SPs where ra is a separate tense marker.      */
        if (kin_starts_with(inner, "ra") && ilen >= 4 && inner[ilen-1] == 'e') {
            const char *s = inner + 2; size_t sl = ilen - 3;
            if (sl >= 1) {
                if (stem_buf) { strncpy(stem_buf, s, sl); stem_buf[sl]='\0'; }
                if (subj_class) *subj_class = SP[i].cls;
                if (tense_out)  *tense_out  = TENSE_PAST_PERF;
                return true;
            }
        }
        /* SUBJUNCTIVE + LOCATIVE: SP + stem + e(SUBJ FV) + ho/mo/yo           *
         * e.g. habeho = ha(SP16) + b + e(SUBJ FV) + ho  → "let there be"  *
         *      abeho  = a(SP1)   + b + e           + ho  → "that he be"    *
         * Checked before plain SUBJUNCTIVE so the locative is not absorbed  *
         * into the stem.  Requires inner length ≥ 4: stem(≥1) + e + ho.   */
        if (ilen >= 4 &&
            (kin_ends_with(inner, "eho") || kin_ends_with(inner, "emo") ||
             kin_ends_with(inner, "eyo"))) {
            size_t sl = ilen - 3;   /* strip e + ho/mo/yo  */
            if (sl >= 1) {
                if (stem_buf) { strncpy(stem_buf, inner, sl); stem_buf[sl]='\0'; }
                if (subj_class) *subj_class = SP[i].cls;
                if (tense_out)  *tense_out  = TENSE_SUBJUNCTIVE_LOC;
                return true;
            }
        }
        /* PAST PERFECT with 'tse' FV: k+y→ts rule §1.3 (andik+ye→anditse).       *
         * Only fires for past-form SPs.  Strip "tse", recover 'k', check stem.  *
         * Also handles vowel contact: ya+a-initial-root → root's initial 'a'    *
         * is absorbed by SP's 'a'; prepend 'a' to recover full root (andik).    */
        {
            static const char *PAST_SP_TSE[] = {
                "twa","ya","wa","na","bya","cya","rya","zya",
                "bwa","kwa","rwa","mwa","za", NULL
            };
            bool sp_is_past_k = false;
            for (int pi = 0; PAST_SP_TSE[pi]; pi++) {
                if (strcmp(SP[i].pfx, PAST_SP_TSE[pi]) == 0)
                    { sp_is_past_k = true; break; }
            }
            if (sp_is_past_k && ilen >= 4 && kin_ends_with(inner, "tse")) {
                size_t sl = ilen - 3;   /* strip "tse" */
                /* Try stem+k directly */
                char cand_k[KIN_MAX_STEM];
                if (sl + 2 < KIN_MAX_STEM) {
                    strncpy(cand_k, inner, sl); cand_k[sl] = 'k'; cand_k[sl+1] = '\0';
                    if (kin_is_known_verb_stem(cand_k)) {
                        if (stem_buf) { strncpy(stem_buf, cand_k, KIN_MAX_STEM-1); stem_buf[KIN_MAX_STEM-1] = '\0'; }
                        if (subj_class) *subj_class = SP[i].cls;
                        if (tense_out)  *tense_out  = TENSE_PAST_PERF;
                        return true;
                    }
                    /* Recover 'a' prefix for vowel-initial roots (ya+andik: a+a→a elides root's 'a') */
                    if (sl + 3 < KIN_MAX_STEM) {
                        char cand_ak[KIN_MAX_STEM];
                        cand_ak[0] = 'a';
                        strncpy(cand_ak + 1, inner, sl);
                        cand_ak[sl+1] = 'k'; cand_ak[sl+2] = '\0';
                        if (kin_is_known_verb_stem(cand_ak)) {
                            if (stem_buf) { strncpy(stem_buf, cand_ak, KIN_MAX_STEM-1); stem_buf[KIN_MAX_STEM-1] = '\0'; }
                            if (subj_class) *subj_class = SP[i].cls;
                            if (tense_out)  *tense_out  = TENSE_PAST_PERF;
                            return true;
                        }
                    }
                }
            }
        }
        /* PAST PERFECT: past-form SP + stem + bare 'e' FV                       *
         * Handles three root-final fusions with past FV 'ye':                   *
         *   r+ye→ze (§1.3): kor+ye=koze (yakoze ← gukora)                      *
         *   nd+ye→nze:      tsind+ye=tsinze (yatsinze ← gutsinda)              *
         *   ng+ye→nze:      tang+ye=tanze (yatanze ← gutanga)                  *
         * Condition: SP is a past-form prefix AND inner ends in 'e' AND the     *
         * stem (inner minus 'e') is known directly OR after z/nz reversal.      *
         * This takes priority over SUBJUNCTIVE for past-form SPs.               */
        {
            static const char *PAST_SP_LIST[] = {
                "twa","ya","wa","na","bya","cya","rya","zya",
                "bwa","kwa","rwa","mwa","za", NULL
            };
            bool sp_is_past = false;
            for (int pi = 0; PAST_SP_LIST[pi]; pi++) {
                if (strcmp(SP[i].pfx, PAST_SP_LIST[pi]) == 0)
                    { sp_is_past = true; break; }
            }
            if (sp_is_past && ilen >= 3 && inner[ilen-1] == 'e') {
                size_t sl = ilen - 1;
                char cand[KIN_MAX_STEM];
                strncpy(cand, inner, sl); cand[sl] = '\0';
                bool known = kin_is_known_verb_stem(cand);
                if (!known && sl >= 1 && cand[sl-1] == 'z') {
                    /* r+y→z reversal: recover r-final root (r+ye→ze) */
                    char cand_r[KIN_MAX_STEM];
                    strncpy(cand_r, cand, sl - 1);
                    cand_r[sl-1] = 'r'; cand_r[sl] = '\0';
                    known = kin_is_known_verb_stem(cand_r);
                    /* nd+y→nz and ng+y→nz reversals: prenasalized consonant + ye */
                    if (!known && sl >= 2 && cand[sl-2] == 'n') {
                        /* Try nd: tsind+ye→tsinze */
                        char cand_nd[KIN_MAX_STEM];
                        strncpy(cand_nd, cand, sl - 1);
                        cand_nd[sl-1] = 'd'; cand_nd[sl] = '\0';
                        known = kin_is_known_verb_stem(cand_nd);
                        if (!known) {
                            /* Try ng: tang+ye→tanze, fung+ye→funze */
                            char cand_ng[KIN_MAX_STEM];
                            strncpy(cand_ng, cand, sl - 1);
                            cand_ng[sl-1] = 'g'; cand_ng[sl] = '\0';
                            known = kin_is_known_verb_stem(cand_ng);
                        }
                    }
                    /* g+y→z: bare root-final 'g' (not prenasalised, penult≠'n')  *
                     * e.g. kwiga (root "ig"): ig+ye→ize; r+y→z already tried above */
                    if (!known && (sl < 2 || cand[sl-2] != 'n')) {
                        char cand_g[KIN_MAX_STEM];
                        strncpy(cand_g, cand, sl - 1);
                        cand_g[sl-1] = 'g'; cand_g[sl] = '\0';
                        known = kin_is_known_verb_stem(cand_g);
                    }
                }
                /* t+ye→se rule: root-final 't' palatalizes before FV 'ye', fusing to 's'
                 * e.g. kwita (root "it") → ya+it+ye → y+ise  (t+y→s, y absorbed)
                 * Reverse: strip 'e', replace final 's' with 't', verify known stem. */
                if (!known && sl >= 1 && cand[sl-1] == 's') {
                    char cand_t[KIN_MAX_STEM];
                    strncpy(cand_t, cand, sl - 1);
                    cand_t[sl-1] = 't'; cand_t[sl] = '\0';
                    known = kin_is_known_verb_stem(cand_t);
                }
                if (known) {
                    if (stem_buf) { strncpy(stem_buf, inner, sl); stem_buf[sl] = '\0'; }
                    if (subj_class) *subj_class = SP[i].cls;
                    if (tense_out)  *tense_out  = TENSE_PAST_PERF;
                    return true;
                }
            }
        }
        /* Past-perfect fused FV with a-eliding / vowel-initial SPs.            *
         * These SPs elide their final vowel before a vowel-initial root, so   *
         * the resulting inner is only 3 chars (e.g. "ise", "ize") — too short *
         * for the ilen>3 guard on the standard PAST_PERF branch above.        *
         * Covers all SPs that arise from vowel-contact before vowel roots:    *
         *   ya→y, ba→b, wa→w, na→n   (a-elision)                             *
         *   tu→tw, mu→mw             (u→w §1.1)                               *
         *   ki→cy, bi→by, ri→ry, zi→zy, bu→bw, ru→rw  (high-V→glide §1.1)  *
         * Two root-final fusions handled:                                      *
         *   t+ye→se §3.8: kwita (root "it") → y+ise  e.g. yise               *
         *   g+ye→ze §1.3: kwiga (root "ig") → y+ize  e.g. yize               */
        if ((strcmp(SP[i].pfx,"y")==0  || strcmp(SP[i].pfx,"w")==0
             || strcmp(SP[i].pfx,"b")==0  || strcmp(SP[i].pfx,"n")==0
             || strcmp(SP[i].pfx,"tw")==0 || strcmp(SP[i].pfx,"mw")==0
             || strcmp(SP[i].pfx,"cy")==0 || strcmp(SP[i].pfx,"by")==0
             || strcmp(SP[i].pfx,"ry")==0 || strcmp(SP[i].pfx,"zy")==0
             || strcmp(SP[i].pfx,"bw")==0 || strcmp(SP[i].pfx,"rw")==0)
            && ilen >= 3 && inner[ilen-1] == 'e') {
            size_t sl = ilen - 1;
            char cand_y[KIN_MAX_STEM];
            strncpy(cand_y, inner, sl); cand_y[sl] = '\0';
            /* t+ye→se: replace final 's' with 't' */
            if (sl >= 1 && cand_y[sl-1] == 's') {
                char cand_yt[KIN_MAX_STEM];
                strncpy(cand_yt, cand_y, sl - 1);
                cand_yt[sl-1] = 't'; cand_yt[sl] = '\0';
                if (kin_is_known_verb_stem(cand_yt)) {
                    if (stem_buf) { strncpy(stem_buf, inner, sl); stem_buf[sl] = '\0'; }
                    if (subj_class) *subj_class = SP[i].cls;
                    if (tense_out)  *tense_out  = TENSE_PAST_PERF;
                    return true;
                }
            }
            /* g+ye→ze §1.3: replace final 'z' with 'g'                        *
             * e.g. kwiga (root "ig") → ya+ig+ye → y+ize (ya→y, g+y→z)        *
             * Guard: penult ≠ 'n' to avoid colliding with nz→ng (prenasalised) */
            if (sl >= 1 && cand_y[sl-1] == 'z' &&
                (sl < 2 || cand_y[sl-2] != 'n')) {
                char cand_yg[KIN_MAX_STEM];
                strncpy(cand_yg, cand_y, sl - 1);
                cand_yg[sl-1] = 'g'; cand_yg[sl] = '\0';
                if (kin_is_known_verb_stem(cand_yg)) {
                    if (stem_buf) { strncpy(stem_buf, inner, sl); stem_buf[sl] = '\0'; }
                    if (subj_class) *subj_class = SP[i].cls;
                    if (tense_out)  *tense_out  = TENSE_PAST_PERF;
                    return true;
                }
            }
        }
        /* SUBJUNCTIVE: ends in e */
        if (ilen >= 2 && inner[ilen-1]=='e') {
            size_t sl = ilen - 1;
            if (stem_buf) { strncpy(stem_buf, inner, sl); stem_buf[sl]='\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_SUBJUNCTIVE;
            return true;
        }
        /* COPULA + LOCATIVE (inshinga nkene + umugereka w'ahantu):            *
         * Pattern A — vowel-final SP + "ri" + loc (yariho, ariho, bariho):   *
         *   SP + "riho" / "rimo" / "riyo"  (inner length exactly 4)          *
         *   e.g. ya+riho, a+riho, ba+riho, tu+riho, na+riho, mu+riho         *
         * Pattern B — consonant-final SP + "ari" + loc (byariho, cyariho):   *
         *   SP + "ariho" / "arimo" / "ariyo"  (inner length exactly 5)       *
         *   The 'a' between the consonant SP and 'ri' is the past-tense augment.
         *   e.g. by+ariho (Nt.8 past), cy+ariho (Nt.7 past)                 *
         * Pattern C — 1sg copula "ndi" + loc (ndiho, ndimo, ndiyo):          *
         *   SP="ndi" already consumed; inner = "ho" / "mo" / "yo" (len 2)    *
         *   e.g. ndiho = ndi+ho (I am there)                                 *
         *                                                                      *
         * Root: "b" (from kubaho = ku+b+a+ho, where -ho is post-final loc).  *
         * The final vowel 'a' of kuba is retained before the consonant 'h':  *
         *   kuba+ho → kubaho  (no vowel contact since h is a consonant).     *
         *                                                                      *
         * Tense determination:                                                 *
         *   Past SPs (ya/wa/na/twa/mwa/rwa/bwa/kwa/za) → TENSE_COPULA_PAST  *
         *   Pattern B (consonant SP + 'a'-augment before ri) → COPULA_PAST   *
         *   All other SPs and Pattern C → TENSE_COPULA_PRES                  */
        {
            bool cop_loc   = false;
            bool force_past = false;

            /* Pattern C: SP is "ndi" (1sg copula) and inner is bare locative */
            if (strcmp(SP[i].pfx, "ndi") == 0 && ilen == 2 &&
                (strcmp(inner,"ho")==0 || strcmp(inner,"mo")==0 ||
                 strcmp(inner,"yo")==0)) {
                cop_loc = true;                         /* present: I am there */
            }

            /* Pattern D — PLAIN copula (no locative): SP + "ri"              *
             * e.g. yari = ya(SP past) + ri → "was"                           *
             *      ari  = a(SP Nt.1)   + ri → "is"                           *
             *      bari = ba(SP Nt.2)  + ri → "are"                          *
             *      wari = wa(SP 2sg)   + ri → "were / you are"               *
             * Same past-SP logic as Pattern A applies.                        */
            if (!cop_loc && ilen == 2 && inner[0]=='r' && inner[1]=='i') {
                cop_loc = true;
                const char *sp = SP[i].pfx;
                if (strcmp(sp,"ya")==0 || strcmp(sp,"wa")==0 ||
                    strcmp(sp,"na")==0 || strcmp(sp,"twa")==0 ||
                    strcmp(sp,"mwa")==0 || strcmp(sp,"rwa")==0 ||
                    strcmp(sp,"bwa")==0 || strcmp(sp,"kwa")==0 ||
                    strcmp(sp,"za")==0  || strcmp(sp,"by")==0  ||
                    strcmp(sp,"cy")==0  || strcmp(sp,"ry")==0  ||
                    strcmp(sp,"zy")==0  || strcmp(sp,"bya")==0 ||
                    strcmp(sp,"cya")==0 || strcmp(sp,"rya")==0 ||
                    strcmp(sp,"zya")==0)
                    force_past = true;
            }

            /* Pattern E — PLAIN copula, consonant-final SP + "ari":          *
             * Consonant SPs (by, cy, ry, zy) + past augment 'a' + ri.       *
             * e.g. byari = by(Nt.8 SP) + a(past augment) + ri → "they were" *
             *      cyari = cy(Nt.7 SP) + a + ri → "it was"                  *
             * Inner = "ari" exactly (ilen == 3); always COPULA_PAST.         */
            if (!cop_loc && ilen == 3 &&
                inner[0]=='a' && inner[1]=='r' && inner[2]=='i') {
                cop_loc    = true;
                force_past = true;   /* 'a'-augment always signals past tense */
            }

            /* Pattern A: vowel-final SP → inner = "riho" / "rimo" / "riyo" */
            if (!cop_loc && ilen == 4 &&
                inner[0]=='r' && inner[1]=='i' &&
                (kin_ends_with(inner,"ho") || kin_ends_with(inner,"mo") ||
                 kin_ends_with(inner,"yo"))) {
                cop_loc = true;
                /* Past SPs: ya wa na twa mwa rwa bwa kwa za bya cya rya zya */
                const char *sp = SP[i].pfx;
                if (strcmp(sp,"ya")==0 || strcmp(sp,"wa")==0 ||
                    strcmp(sp,"na")==0 || strcmp(sp,"twa")==0 ||
                    strcmp(sp,"mwa")==0 || strcmp(sp,"rwa")==0 ||
                    strcmp(sp,"bwa")==0 || strcmp(sp,"kwa")==0 ||
                    strcmp(sp,"za")==0  || strcmp(sp,"bya")==0 ||
                    strcmp(sp,"cya")==0 || strcmp(sp,"rya")==0 ||
                    strcmp(sp,"zya")==0)
                    force_past = true;
            }

            /* Pattern B: consonant-final SP → inner = "ariho" / "arimo" / "ariyo" *
             * The 'a' is always the past-tense augment in this structure.          */
            if (!cop_loc && ilen == 5 &&
                inner[0]=='a' && inner[1]=='r' && inner[2]=='i' &&
                (kin_ends_with(inner,"ho") || kin_ends_with(inner,"mo") ||
                 kin_ends_with(inner,"yo"))) {
                cop_loc    = true;
                force_past = true;   /* 'a'-augment signals past tense */
            }

            if (cop_loc) {
                if (stem_buf) { stem_buf[0]='b'; stem_buf[1]='\0'; }
                if (subj_class) *subj_class = SP[i].cls;
                if (tense_out)
                    *tense_out = force_past ? TENSE_COPULA_PAST : TENSE_COPULA_PRES;
                return true;
            }
        }

        /* NEGATIVE PARTICIPIAL (inshinga nkurikije y'impakanyi):              *
         * Pattern: SP + ta + STEM + FV                                       *
         * The -ta- morpheme signals a negative participial / relative clause: *
         *   itagira  = i(SP) + ta + gir + a  → "that which does not have"   *
         *   utagira  = u(SP) + ta + gir + a  → "who does not have"          *
         *   atagira  = a(SP) + ta + gir + a  → "he/she who does not have"   *
         * Checked BEFORE PRESENT_NORA so that "tagir" is not extracted as a  *
         * stem — the correct stem is "gir" (from kugira) after stripping ta. *
         * Requires total inner length ≥ 5: ta(2) + stem(≥2) + FV(1).       */
        if (kin_starts_with(inner, "ta") && ilen >= 5 && inner[ilen-1] == 'a') {
            const char *s = inner + 2; size_t sl = ilen - 3;
            /* Check for additional TM 'ra' after -ta-: SP+ta+ra+stem+a.
             * This is the "not yet" (negative anterior) form.
             * e.g. kataraba = ka+ta+ra+b+a (kuba, "not yet being/existing").
             * Minimum: ta(2)+ra(2)+root(1)+FV(1)=6 → ilen>=6.
             * Allow 1-char roots (e.g. -b- from kuba) for this case only.  */
            if (ilen >= 6 && kin_starts_with(s, "ra")) {
                const char *s2 = s + 2; size_t sl2 = sl - 2;
                if (sl2 >= 1) {
                    if (stem_buf) { strncpy(stem_buf, s2, sl2); stem_buf[sl2]='\0'; }
                    if (subj_class) *subj_class = SP[i].cls;
                    if (tense_out)  *tense_out  = TENSE_NEG_ANTERIOR;
                    return true;
                }
            }
            if (sl >= 2) {
                if (stem_buf) { strncpy(stem_buf, s, sl); stem_buf[sl]='\0'; }
                if (subj_class) *subj_class = SP[i].cls;
                if (tense_out)  *tense_out  = TENSE_NEG_RELATIVE;
                return true;
            }
        }
        /* CONDITIONAL (Inziganyo): past-SP + ku/gu (modal particle) + stem + a
         *
         * Structure: SP(+a fused) + ku/gu + stem + a
         * The conditional TM 'a' is already absorbed into the past-form SP by
         * vowel contact (tu+a→twa, ri+a→rya, etc.).  The distinguishing signal
         * is the ku/gu modal particle that immediately follows the SP+a group.
         *
         * This pattern is detected here, BEFORE the plain PRESENT_NORA catch,
         * so that "twakubaka" is tagged CONDITIONAL rather than PRESENT_NORA.
         *
         * Only fires for past-form SPs (twa/ya/wa/na/bya/cya/rya/zya/bwa/kwa/
         * rwa/mwa/za) because those already incorporate the 'a' TM.  Present-
         * form SPs (a/ba/ki/bi/ri/ru/ka/tu/…) do NOT use this pattern.
         *
         * Textbook examples (S4 SB §6.6):
         *   twatsinda   = tu+a+Ø+tsind+a   (conditional simple — no ku particle)
         *   twakubaka   = tu+a+ku+ubak+a   (conditional with ku particle)
         *   ryakufasha  = ri+a+ku+fash+a
         *   ntitwakubaka→ nti+tu+a+ku+ubak+a (negative conditional)
         *
         * Known limitation: indistinguishable from past-SP + OM(cls15=ku) + stem
         * without broader discourse context. The conditional reading takes priority
         * here; downstream code may refine if sentence context disambiguates.    */
        {
            static const char *PAST_SPS[] = {
                "twa","ya","wa","na","bya","cya","rya","zya",
                "bwa","kwa","rwa","mwa","za", NULL
            };
            bool is_past_sp = false;
            for (int pi = 0; PAST_SPS[pi]; pi++) {
                if (strcmp(SP[i].pfx, PAST_SPS[pi]) == 0) { is_past_sp = true; break; }
            }
            if (is_past_sp &&
                (kin_starts_with(inner, "ku") || kin_starts_with(inner, "gu")) &&
                ilen > 4 && inner[ilen-1] == 'a') {
                /* Strip modal particle (ku/gu) and final vowel */
                const char *s = inner + 2;
                size_t sl = ilen - 3;   /* -2 for ku/gu, -1 for final 'a' */
                if (sl >= 2) {
                    if (stem_buf) { strncpy(stem_buf, s, sl); stem_buf[sl] = '\0'; }
                    if (subj_class) *subj_class = SP[i].cls;
                    if (tense_out)  *tense_out  = TENSE_CONDITIONAL;
                    return true;
                }
            }
        }
        /* PRESENT no-ra + LOCATIVE: inner ends in 'aho', 'amo', or 'ayo'.     *
         * Pattern: SP + stem + a(FV) + ho/mo/yo(locative suffix).             *
         * e.g. imukuramo = i(SP·Nt.9) + mu(OM) + kur(root) + a(FV) + mo(LOC)*
         *      arakoreramo = ara(SP) + korer + a + mo                         *
         * Strip the 3-char complex 'a+locative' to recover the bare stem.    *
         * Must be checked BEFORE the plain PRESENT_NORA (which strips only   *
         * 1 char) to prevent the 'o' of 'mo' being treated as FV alone.      *
         * Guard: sl >= 2 requires at least a 2-char stem before the locative. */
        if (ilen >= 5 &&
            (kin_ends_with(inner, "aho") || kin_ends_with(inner, "amo") ||
             kin_ends_with(inner, "ayo"))) {
            size_t sl = ilen - 3;   /* strip a + 2-char locative */
            if (sl >= 2) {
                if (stem_buf) { strncpy(stem_buf, inner, sl); stem_buf[sl]='\0'; }
                if (subj_class) *subj_class = SP[i].cls;
                if (tense_out)  *tense_out  = TENSE_PRESENT_NORA;
                return true;
            }
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

/* ── § 4b  ITONDAGUYE (Conjugated Verb) ──────────────────────────────────── */

/*
 * kin_is_verb_conjugated()
 *
 * Full conjugated-verb detector with three new layers on top of the SP/tense
 * matching:
 *
 * LAYER 1 – Negation (inshinga y'impakanyi):
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

    /* ── LAYER 1: Detect and strip nt- negation prefix ─────────────────── *
     * The negative particle "nt-" is ALWAYS followed by a vowel-initial SP: *
     *   ntaragenda = nt + a(SP cls1) + ra + gend + a                        *
     *   ntiragenda = nt + i(SP cls4/7/9) + ra + gend + a                   *
     *   ntuzagenda = nt + u(SP 2sg) + za + gend + a                        *
     * When "nt" is followed by 'e', 'o', or a consonant, the "n" is the    *
     * 1sg subject prefix and 't' begins the verb stem:                      *
     *   ntega      = n(1sg) + teg + a   (gutega = to plan/trap)             *
     *   ntege      = n(1sg) + teg + e   (subjunctive)                       *
     *   ntegeko    = n(1sg) + tegek + o (gutegeka = to command)             */
    bool is_neg = false;
    const char *parse_word = word;
    if (len > 3 && word[0] == 'n' && word[1] == 't' &&
        (word[2] == 'a' || word[2] == 'i' || word[2] == 'u')) {
        is_neg   = true;
        parse_word = word + 2;
    }

    /* ── LAYER 1b: "si" negative prefix ────────────────────────────────── *
     * "si" precedes the conjugated verb (SP + tense + stem) to form the    *
     * negative for any person/class, especially 1sg forms:                 *
     *   sinagira  = si + na(1sg past) + gir + a  (I did not do)            *
     *   sindabona = si + nda(1sg pres) + bon + a (I don't see)             *
     *   sinzabona = si + nza(1sg fut) + bon + a  (I won't see)             *
     *   simbeshya = si + mb(1sg n→m/b) + eshya   (I don't lie)            *
     *   simusiga  = si + mu(2pl/OM) + sig + a    (you don't leave)         *
     * Length guard > 4: prevents stripping from short loanwords (sida etc) */
    if (!is_neg && len > 4 && word[0] == 's' && word[1] == 'i') {
        is_neg     = true;
        parse_word = word + 2;   /* continue with SP detection on remainder */
    }

    /* ── LAYER 2: Core SP + tense matching ─────────────────────────────── */
    char raw_stem[KIN_MAX_STEM] = "";
    int  cls   = 0;
    VerbTense tense = TENSE_NONE;

    bool first_ok = verb_match_inner(parse_word, raw_stem, &cls, &tense);

    /* ── LAYER 2a: nti + i-elided SP retry ─────────────────────────────── *
     * "nti" negation leaves parse_word="iXXX". The 'i' can be a vowel that *
     * belongs to the SP that follows, not an Nt.4 SP itself. Run the retry  *
     * unconditionally so a spurious first parse can be overridden.          *
     * e.g. ntimuzarye: first parse → i(SP·Nt.4)+muzar+ye (PAST_PERF, wrong)*
     *      retry "muzarye" → mu(SP·2pl)+za+ry+e (FUTURE_SUBJ, correct).    *
     * Override rule: prefer retry when retry_ok AND (first failed OR retry  *
     * gives a directly-known stem without needing OM processing).           */
    if (is_neg && parse_word[0] == 'i') {
        char retry_stem[KIN_MAX_STEM] = "";
        int  retry_cls  = 0;
        VerbTense retry_tense = TENSE_NONE;
        bool retry_ok = verb_match_inner(parse_word + 1, retry_stem,
                                         &retry_cls, &retry_tense);
        if (retry_ok && (!first_ok || kin_is_known_verb_stem(retry_stem))) {
            strncpy(raw_stem, retry_stem, KIN_MAX_STEM - 1);
            raw_stem[KIN_MAX_STEM - 1] = '\0';
            cls   = retry_cls;
            tense = retry_tense;
            first_ok = true;
        }
    }

    if (!first_ok) {
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
            /* y-glide before final 'a': reciprocal -an + y glide (phonol.)  *
             * "gereranya" = gereran + y + a → strip 'a' → "gererany",        *
             * strip trailing 'y' → "gereran" → check stem, then ext_strip.  *
             * e.g. ganiranya(talk together), beshyanya(lie to each other).   */
            size_t implen = strlen(imp);
            if (implen > 3 && imp[implen-1] == 'y') {
                char imp_noy[KIN_MAX_STEM];
                strncpy(imp_noy, imp, implen - 1);
                imp_noy[implen - 1] = '\0';
                if (kin_is_known_verb_stem(imp_noy)) {
                    if (stem_out)      { strncpy(stem_out, imp_noy, KIN_MAX_STEM-1);
                                         stem_out[KIN_MAX_STEM-1] = '\0'; }
                    if (subj_class)    *subj_class    = 0;
                    if (tense_out)     *tense_out     = TENSE_IMPERATIVE;
                    if (obj_class_out) *obj_class_out = 0;
                    if (ext_out)       *ext_out       = VEXT_NONE;
                    if (neg_out)       *neg_out       = false;
                    return true;
                }
                /* Also try ext_strip on the y-stripped form */
                char bare_noy[KIN_MAX_STEM];
                VerbExtension noy_ext = VEXT_NONE;
                if (ext_strip(imp_noy, &noy_ext, bare_noy, sizeof(bare_noy))) {
                    if (kin_is_known_verb_stem(bare_noy)) {
                        if (stem_out)  { strncpy(stem_out, bare_noy, KIN_MAX_STEM-1);
                                         stem_out[KIN_MAX_STEM-1] = '\0'; }
                        if (subj_class)    *subj_class    = 0;
                        if (tense_out)     *tense_out     = TENSE_IMPERATIVE;
                        if (obj_class_out) *obj_class_out = 0;
                        if (ext_out)       *ext_out       = noy_ext;
                        if (neg_out)       *neg_out       = false;
                        return true;
                    }
                }
            }
        }
        /* ── LAYER 2c: Bare subjunctive fallback (Igihe cy'ubusabe) ────── *
         * If no SP was recognized AND the word ends in 'e' AND the word    *
         * (minus final 'e') is a known verb stem → bare subjunctive.       *
         * e.g. "sanzure" (stem=sanzur+e), used as polite imperative or     *
         * in subordinate clauses after "ngo" where SP is omitted.          *
         * Negatives and very short words are excluded.                     */
        if (!is_neg && plen >= 3 && parse_word[plen-1] == 'e') {
            char subj[KIN_MAX_STEM];
            strncpy(subj, parse_word, plen - 1);
            subj[plen - 1] = '\0';
            if (kin_is_known_verb_stem(subj)) {
                if (stem_out)      { strncpy(stem_out, subj, KIN_MAX_STEM-1);
                                     stem_out[KIN_MAX_STEM-1] = '\0'; }
                if (subj_class)    *subj_class    = 0;
                if (tense_out)     *tense_out     = TENSE_SUBJUNCTIVE;
                if (obj_class_out) *obj_class_out = 0;
                /* Mark reflexive stems: imbundo (i-) was elided.            *
                 * e.g. sanzure ← kwi-sanzur-e: stem "sanzur" is reflexive  */
                if (ext_out)       *ext_out       = kin_is_reflexive_verb_stem(subj)
                                                        ? VEXT_REFLEXIVE : VEXT_NONE;
                if (neg_out)       *neg_out       = false;
                return true;
            }
            /* Also try stripping a verb extension before checking the stem *
             * e.g. "sanzurire" = sanzur + ir + e (applicative subjunctive) */
            char bare_subj[KIN_MAX_STEM];
            VerbExtension subj_ext = VEXT_NONE;
            if (ext_strip(subj, &subj_ext, bare_subj, sizeof(bare_subj))) {
                if (kin_is_known_verb_stem(bare_subj)) {
                    if (stem_out)      { strncpy(stem_out, bare_subj, KIN_MAX_STEM-1);
                                         stem_out[KIN_MAX_STEM-1] = '\0'; }
                    if (subj_class)    *subj_class    = 0;
                    if (tense_out)     *tense_out     = TENSE_SUBJUNCTIVE;
                    if (obj_class_out) *obj_class_out = 0;
                    if (ext_out)       *ext_out       = subj_ext;
                    if (neg_out)       *neg_out       = false;
                    return true;
                }
            }
        }
        return false;
    }

    /* ── LAYER 2.5: Bilabial recovery for 1sg SP (n→m assimilation) ──────── *
     * The SP_TABLE entry "mb/mp/mf/mv" consumes 2 chars (m + bilabial), but    *
     * the bilabial may actually belong to an object marker (OM) that follows.   *
     * e.g. mbihaye = m(1sg) + bi(OM·Nt.8) + h(root) + aye(FV):               *
     *   SP "mb" consumed "mb", leaving raw_stem="ih" (after "aye" strip)       *
     *   or raw_stem="iha" (after "ye" strip) — both start with vowel 'i'.      *
     * Detection: cls==0 (person prefix) AND word starts with "m"+bilabial AND   *
     *   raw_stem[0] is a vowel (the bilabial was not part of the root).         *
     * Recovery: prepend the bilabial char to raw_stem so OM stripping can find  *
     *   the correct OM (e.g. "bi" = Nt.8 object marker).                       */
    if (cls == 0 && strlen(parse_word) >= 3 &&
        parse_word[0] == 'm' && is_vowel((unsigned char)raw_stem[0]) &&
        (parse_word[1]=='b' || parse_word[1]=='p' ||
         parse_word[1]=='f' || parse_word[1]=='v')) {
        char recovered[KIN_MAX_STEM];
        recovered[0] = parse_word[1];
        strncpy(recovered + 1, raw_stem, KIN_MAX_STEM - 2);
        recovered[KIN_MAX_STEM - 1] = '\0';
        strncpy(raw_stem, recovered, KIN_MAX_STEM - 1);
        raw_stem[KIN_MAX_STEM - 1] = '\0';
    }

    /* ── LAYER 3a: Strip object marker from front of raw_stem ──────────── */
    int  obj_cls = 0;
    char after_om[KIN_MAX_STEM];
    strncpy(after_om, raw_stem, KIN_MAX_STEM - 1);
    after_om[KIN_MAX_STEM - 1] = '\0';
    om_strip(raw_stem, &obj_cls, after_om, sizeof(after_om));

    /* ── LAYER 3a.5: Epenthetic-'i' recovery for past-perfect ─────────────── *
     * When tense=PAST_PERF, verb_match_inner uses the 'ye' branch (not 'iye') *
     * whenever an OM precedes the root, because it can't look past the OM to  *
     * test whether stripping 'iye' yields a known root.  The 'ye' branch      *
     * leaves the epenthetic 'i' of the '-iye' complex as the final char of    *
     * after_om.  Strip it to recover the true consonant-final root.           *
     *                                                                          *
     * Condition (all must hold):                                               *
     *   1. tense == TENSE_PAST_PERF                                            *
     *   2. after_om ends in consonant + 'i'  (the epenthetic 'i')             *
     *   3. after_om is NOT itself a known verb stem                            *
     *   4. Stripping the final 'i' yields a known verb stem                   *
     *                                                                          *
     * e.g. umukwiriye: after OM strip → after_om="kwiri"                      *
     *   "kwiri" unknown, strip 'i' → "kwir" = known (gukwira) ✓               */
    if (tense == TENSE_PAST_PERF) {
        size_t alen = strlen(after_om);
        if (alen >= 3 && after_om[alen-1] == 'i'
                      && !is_vowel((unsigned char)after_om[alen-2])
                      && !kin_is_known_verb_stem(after_om)) {
            char try_root[KIN_MAX_STEM];
            strncpy(try_root, after_om, alen - 1);
            try_root[alen - 1] = '\0';
            if (kin_is_known_verb_stem(try_root)) {
                strncpy(after_om, try_root, KIN_MAX_STEM - 1);
                after_om[KIN_MAX_STEM - 1] = '\0';
            }
        }
    }

    /* ── LAYER 3b: Strip verb extension from end of after_om ───────────── */
    VerbExtension vext = VEXT_NONE;
    char bare[KIN_MAX_STEM];
    strncpy(bare, after_om, KIN_MAX_STEM - 1);
    bare[KIN_MAX_STEM - 1] = '\0';
    ext_strip(after_om, &vext, bare, sizeof(bare));

    /* ── LAYER 3b.5: Guard against false-positive extension detection ───── *
     * ext_strip() matches -uk/-ur/-ish/etc. by suffix.  Roots that end in  *
     * those letters get falsely split, e.g. "ruhuk" (root of kuruhuka) has  *
     * -uk stripped → bare="ruh" (unknown) + VEXT_REVERSIVE (wrong).         *
     * Rule: if the stripped bare root is unknown BUT the original stem IS a  *
     * known root, the extension match is a false positive — revert it.       *
     * e.g. ruhuk: bare="ruh" unknown, after_om="ruhuk" known → revert ✓    *
     *      funguk: bare="fung" known → keep (gufunguka, reversive) ✓        */
    /* Prefer the longest known stem: if after_om itself is a known root,
     * the extension match is a false split — revert it.
     * Covers both: bare=unknown (e.g. "ruhuk"→"ruh") and the trickier case
     * where bare is ALSO known (e.g. "tegek"→"teg"/"tegek" both known).  */
    if (vext != VEXT_NONE && kin_is_known_verb_stem(after_om)) {
        vext = VEXT_NONE;
        strncpy(bare, after_om, KIN_MAX_STEM - 1);
        bare[KIN_MAX_STEM - 1] = '\0';
    }

    /* ── LAYER 3b.6: Past-perfect z-final stem — revert false causative-y ── *
     * When tense=PAST_PERF and ext_strip found VEXT_CAUSATIVE_Y (z-final     *
     * raw_stem, r restored into bare), the 'z' came from the past FV rule    *
     * r+ye→ze (§1.3), NOT from the causative -y- extension.                  *
     * ext_strip already restored bare="kor"; keep bare, clear vext.          *
     * e.g. yakoze: raw_stem="koz", ext_strip→bare="kor", CAUSATIVE_Y → NONE */
    if (tense == TENSE_PAST_PERF && vext == VEXT_CAUSATIVE_Y) {
        vext = VEXT_NONE;
        /* bare already holds the r-restored root (ext_strip did z→r) */
    }
    /* ── LAYER 3b.7: Past-perfect nz-final stem — nd/ng+y→nz reversal ────── *
     * When tense=PAST_PERF and bare ends in "nz" (after ext_strip returned   *
     * false or CAUSATIVE_Y was already cleared above), the 'nz' came from    *
     * the past FV fusion: nd+ye→nze or ng+ye→nze.                            *
     * Recover the true root by reversing nz→nd or nz→ng.                    *
     * e.g. yatsinze: bare="tsinz" → "tsind" (gutsinda)                       *
     *      yatanze:  bare="tanz"  → "tang"  (gutanga)                        *
     *      yafunze:  bare="funz"  → "fung"  (gufunga)                        */
    if (tense == TENSE_PAST_PERF && vext == VEXT_NONE) {
        size_t blen = strlen(bare);
        if (blen >= 3 && bare[blen-1] == 'z' && bare[blen-2] == 'n') {
            char try_nd[KIN_MAX_STEM];
            strncpy(try_nd, bare, blen - 1);
            try_nd[blen-1] = 'd'; try_nd[blen] = '\0';
            if (kin_is_known_verb_stem(try_nd)) {
                strncpy(bare, try_nd, KIN_MAX_STEM - 1);
                bare[KIN_MAX_STEM - 1] = '\0';
            } else {
                char try_ng[KIN_MAX_STEM];
                strncpy(try_ng, bare, blen - 1);
                try_ng[blen-1] = 'g'; try_ng[blen] = '\0';
                if (kin_is_known_verb_stem(try_ng)) {
                    strncpy(bare, try_ng, KIN_MAX_STEM - 1);
                    bare[KIN_MAX_STEM - 1] = '\0';
                }
            }
        }
    }

    /* ── LAYER 3b.8: Past-perfect fused-FV stem recovery ───────────────────── *
     * Two root-final fusions with FV 'ye' leave a fused consonant in bare:    *
     *   t+ye→se §3.8: root-final 't' palatalizes with 'y' → 's'              *
     *     e.g. yise: root "it" + ye → bare="is" → restore 't'                 *
     *   g+ye→ze §1.3: root-final 'g' fuses with 'y' → 'z'                   *
     *     e.g. yize: root "ig" + ye → bare="iz" → restore 'g'                */
    if (tense == TENSE_PAST_PERF && vext == VEXT_NONE) {
        size_t blen = strlen(bare);
        /* t+ye→se: replace final 's' with 't' */
        if (blen >= 1 && bare[blen-1] == 's') {
            char try_t[KIN_MAX_STEM];
            strncpy(try_t, bare, blen - 1);
            try_t[blen-1] = 't'; try_t[blen] = '\0';
            if (kin_is_known_verb_stem(try_t)) {
                strncpy(bare, try_t, KIN_MAX_STEM - 1);
                bare[KIN_MAX_STEM - 1] = '\0';
            }
        }
        /* g+ye→ze: replace final 'z' with 'g' (guard: penult≠'n' to avoid   *
         * collision with nz→ng which is handled in LAYER 3b.7)               */
        if (blen >= 1 && bare[blen-1] == 'z' &&
            (blen < 2 || bare[blen-2] != 'n')) {
            char try_g[KIN_MAX_STEM];
            strncpy(try_g, bare, blen - 1);
            try_g[blen-1] = 'g'; try_g[blen] = '\0';
            if (kin_is_known_verb_stem(try_g)) {
                strncpy(bare, try_g, KIN_MAX_STEM - 1);
                bare[KIN_MAX_STEM - 1] = '\0';
            }
        }
    }

    /* ── LAYER 3c: Validate OM; revert if bare stem is unknown ──────────── *
     * If an OM was stripped but the resulting stem is not a known verb stem, *
     * check whether the un-stripped raw_stem (or raw_stem after ext-strip)   *
     * IS known.  If so, the OM was a false positive (e.g. "rw" OM eaten from *
     * stem "rwany" → "any" unknown, but raw_stem "rwany" IS known).          *
     * Example: zi+rwanya → raw_stem="rwany", OM=rw(Nt.11) → bare="any" → BAD*
     *          revert: obj_cls=0, bare="rwany" (no OM, stem known).          */
    if (obj_cls > 0 && !kin_is_known_verb_stem(bare)) {
        char raw_bare[KIN_MAX_STEM];
        VerbExtension raw_vext = VEXT_NONE;
        strncpy(raw_bare, raw_stem, KIN_MAX_STEM - 1);
        raw_bare[KIN_MAX_STEM - 1] = '\0';
        ext_strip(raw_stem, &raw_vext, raw_bare, sizeof(raw_bare));
        if (kin_is_known_verb_stem(raw_stem) || kin_is_known_verb_stem(raw_bare)) {
            obj_cls = 0;
            /* If raw_stem itself is the known root but raw_bare is NOT known,
             * the ext_strip on raw_stem was a false positive (e.g. "ruhuk"
             * → raw_bare="ruh" unknown, raw_vext=VEXT_REVERSIVE wrong).
             * Use VEXT_NONE; the whole raw_stem is the bare root.          */
            if (kin_is_known_verb_stem(raw_stem) && !kin_is_known_verb_stem(raw_bare)) {
                vext = VEXT_NONE;
                strncpy(bare, raw_stem, KIN_MAX_STEM - 1);
            } else {
                vext = raw_vext;
                strncpy(bare, kin_is_known_verb_stem(raw_stem) ? raw_stem : raw_bare,
                        KIN_MAX_STEM - 1);
            }
            bare[KIN_MAX_STEM - 1] = '\0';
        }
    }

    /* ── LAYER 3c.1: Prefer full raw_stem over OM + short bare root ──────── *
     * If an OM was stripped AND the un-stripped raw_stem is ALSO a known     *
     * verb root, the OM match is ambiguous.  Prefer the longer root.         *
     * e.g. raw_stem="ruk" (kuruka) matched OM=ru(Nt.11) + bare="k" (kuba):  *
     *   both k and ruk are known → prefer ruk (no OM).                       *
     * This only fires when bare IS known (the ≥2-char ambiguity case);       *
     * the !known case is already handled by LAYER 3c above.                  */
    if (obj_cls > 0 && kin_is_known_verb_stem(bare) && kin_is_known_verb_stem(raw_stem)) {
        obj_cls = 0;
        vext = VEXT_NONE;
        strncpy(bare, raw_stem, KIN_MAX_STEM - 1);
        bare[KIN_MAX_STEM - 1] = '\0';
    }

    /* ── LAYER 3c.5: Euphonic-z before vowel-initial root ──────────────── *
     * Pattern: SP(vowel-final) + z(euphonic) + root(vowel-initial) + (ext) + FV
     * e.g. azitwa = a(SP·Nt.1) + z(euph.) + it(root) + w(passive) + a(FV)  *
     *      from kwitwa (passive of kwita = to be called/named).              *
     *                                                                         *
     * When the OM-stripper consumed "zi" as Nt.10 OM (obj_cls==10) but the  *
     * raw_stem starts with 'z' + vowel-initial root, the 'z' may actually be *
     * euphonic.  Test by stripping the leading 'z' from raw_stem and then    *
     * attempting ext_strip on the remainder: if the resulting bare root is a  *
     * known verb stem, "zi" was not a real OM.                               *
     * Guard: obj_cls==10 (only "zi" triggers this check; other OMs are safe) */
    if (obj_cls == 10 && raw_stem[0] == 'z' && strlen(raw_stem) >= 3) {
        /* after_z = raw_stem without the leading 'z' */
        char after_z[KIN_MAX_STEM];
        strncpy(after_z, raw_stem + 1, KIN_MAX_STEM - 1);
        after_z[KIN_MAX_STEM - 1] = '\0';
        /* Try ext_strip on after_z to separate root from extension */
        char z_root[KIN_MAX_STEM];
        VerbExtension z_vext = VEXT_NONE;
        strncpy(z_root, after_z, KIN_MAX_STEM - 1);
        z_root[KIN_MAX_STEM - 1] = '\0';
        ext_strip(after_z, &z_vext, z_root, sizeof(z_root));
        if (kin_is_known_verb_stem(z_root)) {
            obj_cls = 0;
            vext    = z_vext;
            strncpy(bare, z_root, KIN_MAX_STEM - 1);
            bare[KIN_MAX_STEM - 1] = '\0';
            /* The leading 'z' is the contracted future TM 'za' (§1.1:
             * za + vowel-initial root → z + root, 'a' of TM absorbed).
             * Re-tag tense: this is FUTURE, not PRESENT_NORA.           */
            tense = TENSE_FUTURE;
        }
    }

    /* ── LAYER 3c.6: r-drop before passive extension (r → ∅ / __ w) ──────── *
     * In Kinyarwanda, verb roots ending in 'r' lose the 'r' before the     *
     * passive extension -w-: gukura (root kur) + passive -w- → surface kuw  *
     * yakuwe = ya(SP) + kur(root) + w(pass) + e(FV), but surface is yakuwe  *
     * because kur+w → kuw (r deleted before w).                             *
     *                                                                         *
     * Detection: an OM was stripped (obj_cls>0) but bare is not a known stem.*
     * ext_strip(raw_stem) returns VEXT_PASSIVE; bare_nopass + 'r' IS known. *
     * Action: revert OM, restore root to bare_nopass+'r', keep VEXT_PASSIVE. */
    if (obj_cls > 0 && !kin_is_known_verb_stem(bare)) {
        char rdrop_bare[KIN_MAX_STEM];
        VerbExtension rdrop_vext = VEXT_NONE;
        strncpy(rdrop_bare, raw_stem, KIN_MAX_STEM - 1);
        rdrop_bare[KIN_MAX_STEM - 1] = '\0';
        if (ext_strip(raw_stem, &rdrop_vext, rdrop_bare, sizeof(rdrop_bare)) &&
            rdrop_vext == VEXT_PASSIVE) {
            size_t rblen = strlen(rdrop_bare);
            if (rblen > 0 && rblen + 1 < KIN_MAX_STEM) {
                char with_r[KIN_MAX_STEM];
                strncpy(with_r, rdrop_bare, KIN_MAX_STEM - 1);
                with_r[KIN_MAX_STEM - 1] = '\0';
                with_r[rblen]   = 'r';
                with_r[rblen+1] = '\0';
                if (kin_is_known_verb_stem(with_r)) {
                    obj_cls = 0;
                    vext    = VEXT_PASSIVE;
                    strncpy(bare, with_r, KIN_MAX_STEM - 1);
                    bare[KIN_MAX_STEM - 1] = '\0';
                }
            }
        }
    }

    /* ── LAYER 3c.7: Epenthetic 'a' for monosyllabic root (past perfect) ──── *
     * Monosyllabic consonant roots (h=guha, b=kuba, z=kuza) insert an          *
     * epenthetic 'a' before past perfect FV 'ye': root + a + ye (not + ye).   *
     *   guha (root h): ya+h+aye → yahaye, m+bi+h+aye → mbihaye               *
     *   kuba (root b): ya+b+aye → yabaye                                       *
     * After the standard "ye"-strip in verb_match_inner, the bare looks like   *
     * a 2-char form ending in 'a' (e.g. "ha" or "ba").  If bare is unknown    *
     * but bare[0] alone IS a known monosyllabic root, strip the epenthetic 'a'.*
     * Guard: tense==PAST_PERF only (epenthetic 'a' is a past perfect feature). */
    if (tense == TENSE_PAST_PERF &&
        !kin_is_known_verb_stem(bare) &&
        strlen(bare) == 2 && !is_vowel((unsigned char)bare[0]) && bare[1] == 'a') {
        char mono_root[2] = { bare[0], '\0' };
        if (kin_is_known_verb_stem(mono_root)) {
            bare[1] = '\0';   /* "ha"→"h", "ba"→"b": strip epenthetic 'a' */
            vext = VEXT_NONE;
        }
    }

    /* ── LAYER 3d: Reflexive 'i' marker + nd→nz mutation detection ─────── *
     * Pattern: SP + i(REFL) + root_alt + FV                                *
     * After SP + zero-TM, when no OM was found and bare starts with 'i':   *
     *   strip leading 'i' → candidate refl_root                            *
     *   if refl_root ends in 'z', try reversing z→d → base_try             *
     *   if base_try is a known verb stem → underlying root is base_try,     *
     *   and the 'i' was the reflexive marker (from kwi-), nd→nz being the  *
     *   surface mutation in the reflexive context.                          *
     * Example: byigenza → SP=bi, bare="igenz"                               *
     *   strip 'i' → "genz", z→d → "gend" = known stem (kugenda) ✓         *
     * Guard: obj_cls==0 (no OM consumed the 'i') and vext==NONE (no ext).  */
    if (obj_cls == 0 && vext == VEXT_NONE &&
        bare[0] == 'i' && strlen(bare) >= 4) {
        const char *after_i = bare + 1;
        size_t ai_len = strlen(after_i);
        if (ai_len >= 3 && after_i[ai_len - 1] == 'z') {
            char base_try[KIN_MAX_STEM];
            strncpy(base_try, after_i, ai_len);
            base_try[ai_len - 1] = 'd';   /* reverse nd→nz: z→d */
            base_try[ai_len] = '\0';
            if (kin_is_known_verb_stem(base_try)) {
                strncpy(bare, base_try, KIN_MAX_STEM - 1);
                bare[KIN_MAX_STEM - 1] = '\0';
                vext = VEXT_REFLEXIVE;
            }
        }
    }

    /* ── LAYER 3e: ra-prefix false-TM recovery ─────────────────────────── *
     * verb_match_inner greedily matches "ra" as TM (TENSE_PRESENT) because  *
     * it scans for SP+ra+stem+FV before trying SP+stem+FV.  When "ra" is    *
     * actually the start of the root (e.g. rangir in birangira), the bare   *
     * root after stripping comes out unknown.                                *
     *                                                                         *
     * Recovery: prepend "ra" back to after_om and retry as TENSE_PRESENT_NORA.*
     * birangira: after_om="ngir" → nora_cand="rangir" → known stem ✓         *
     * Also tries ext_strip (e.g. "rang" after stripping applicative -ir-).   *
     * Restricted to obj_cls==0 to avoid double-prepending over a real OM.   */
    if (tense == TENSE_PRESENT && obj_cls == 0 && !kin_is_known_verb_stem(bare)) {
        size_t ao_len = strlen(after_om);
        if (2 + ao_len < KIN_MAX_STEM) {
            char nora_cand[KIN_MAX_STEM];
            nora_cand[0] = 'r'; nora_cand[1] = 'a';
            memcpy(nora_cand + 2, after_om, ao_len + 1);   /* +1 for NUL */
            /* Direct lexicon hit: root is the full "ra"+after_om stem */
            if (kin_is_known_verb_stem(nora_cand)) {
                tense = TENSE_PRESENT_NORA;
                vext  = VEXT_NONE;
                strncpy(bare, nora_cand, KIN_MAX_STEM - 1);
                bare[KIN_MAX_STEM - 1] = '\0';
            } else {
                /* Try ext_strip on nora_cand to find root + extension */
                char nora_bare[KIN_MAX_STEM];
                VerbExtension nora_vext = VEXT_NONE;
                strncpy(nora_bare, nora_cand, KIN_MAX_STEM - 1);
                nora_bare[KIN_MAX_STEM - 1] = '\0';
                if (ext_strip(nora_cand, &nora_vext, nora_bare, sizeof(nora_bare)) &&
                    kin_is_known_verb_stem(nora_bare)) {
                    tense = TENSE_PRESENT_NORA;
                    vext  = nora_vext;
                    strncpy(bare, nora_bare, KIN_MAX_STEM - 1);
                    bare[KIN_MAX_STEM - 1] = '\0';
                }
            }
        }
    }

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

/* ══════════════════════════════════════════════════════════════════════════
 * § 3  TREE 2 — NTERA (Adjective)
 *
 *  Formula:   RS  +  C
 *             │      └─ Igicumbi (adjective stem; closed set of ~27 stems)
 *             └──────── Indangasano (concordance prefix = noun's indanganteko)
 *
 *  Rule: RS MUST equal the indanganteko of the noun it modifies.
 *  If RS ≠ noun's RT → ERR_ADJ_AGREEMENT (detected in syntax.c).
 *
 *  Adjective stems (in lexicon.c ADJ_STEMS[]):
 *   -nini (big)    -to (small)    -re (long)     -gufi (short)
 *   -ke (few)      -shya (new)    -bi (bad)       -za (good)
 *   -zima (whole)  -kuru (old)    -bisi (raw)     -sa (like)
 *   -tindi (other) -gari (wide)   -inshi (many)   -eru (white)
 *   -nzima (heavy) -ogo (deep)    -tagatifu (holy) -hire (fast)
 *   -taraga (old/aged)            + augmentative variants (-nzinya etc.)
 *
 *  Reduplication (indorerezi): RS + C + RS + C for emphasis.
 *   e.g. mu+re+mu+re = muremure (very tall/long, Nt.1)
 *        ba+re+ba+re = barebare (Nt.2)
 *
 *  Tree transitions from ntera:
 *   → izina ntera (POS_RELATIVE_NOUN): when a noun plays the RS+C role.
 *     The noun connects via ikinyazina ngenera (possessive connector).
 *     e.g. "igitabo cy'Ikinyarwanda" — Ikinyarwanda is POS_RELATIVE_NOUN.
 *   → igisantera (POS_COMPOUND_ADJ): two noun-pair acting as single adjective.
 *     [planned — tag exists in header, detection not yet implemented]
 * ══════════════════════════════════════════════════════════════════════════ */

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
        { "ry",  5  }, /* ri + vowel-initial stem (i→y): ryiza, ryinshi   */
        /* ── Class 9 (RS=n) nasal surface variants ─────────────────────────
         * The underlying RS is always 'n'.  The surface form changes based
         * on the initial consonant of C (igicumbi):
         *   n + bilabial (b,p,v,f,h)  → m   §3.3   (n→m assimilation)
         *   n + r                     → nd  §3.5   (r→d after nasal)
         *   n + vowel or y            → nz  §2.4   (epenthetic z)
         *   n + n                     → n   (geminate simplification)
         *   n + other consonant       → n   (unchanged)
         * Note: "ny" as a prefix is WRONG — the digraph 'ny' is a phoneme,
         * so n(RS) + nyoni(C) surfaces as "nyoni" with RS stripped.        */
        { "m",   9  }, /* n→m §3.3: mbisi, mbi (n before bilabial)        */
        { "nd",  9  }, /* n+r→nd §3.5: ndere, nderenire (n before r-stem) */
        { "nz",  9  }, /* n+V epenthetic z §2.4: nziza, nzinshi           */
        /* ── Class 12 (RS=ka) voicing before voiced consonant ───────────── */
        { "ga",  12 }, /* k→g §3.7 (ka before voiced-initial C stem)      */
        /* ── a+i→e fusion: prefix ending in 'a' + i-initial stem ──────────
         * Amategeko y'igenamajwi p.7-8: a+i contraction                   */
        { "be",  2  }, /* ba + inshi/iza → benshi/beza                    */
        { "me",  6  }, /* ma + inshi/iza → menshi/meza                    */
        { "ye",  4  }, /* ya + i-stem (Nt.4)                              */
        { "ze",  10 }, /* za + i-stem (Nt.10)                             */
        { "he",  16 }, /* ha + iza/inshi → heza/henshi (Nt.16 a+i fusion) */
        /* ha + vowel-initial stem: final 'a' of 'ha' elides before vowel */
        { "h",   16 }, /* ha + other vowel stems (a→Ø elision): hinshi    */
        { NULL, 0 }
    };

    /* For the a+i→e fused prefixes, the stem stored in ADJ_STEMS starts
     * with 'i'.  We need to prepend 'i' to whatever follows the fused pfx.
     * We handle this by checking both the remainder AND 'i'+remainder.     */
    static const char *FUSED_PREFIXES[] = { "be", "me", "ye", "ze", "he", NULL };

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

        /* ── Class 9 special: n+n→n geminate simplification ──────────────
         * n(RS) + nini(C) → surface "nini" (nn simplified to n).
         * After stripping "n", sfx = "ini" which is not a stem.
         * Reconstruct by prepending 'n' to sfx and recheck.             */
        if (ADJ_PREFIXES[i].cls == 9 &&
            ADJ_PREFIXES[i].pfx[0] == 'n' && ADJ_PREFIXES[i].pfx[1] == '\0') {
            char restored[KIN_MAX_STEM];
            restored[0] = 'n';
            strncpy(restored + 1, sfx, KIN_MAX_STEM - 2);
            restored[KIN_MAX_STEM - 1] = '\0';
            if (kin_is_adj_stem(restored)) {
                if (stem_out)  strncpy(stem_out, restored, KIN_MAX_STEM - 1);
                if (class_out) *class_out = 9;
                return true;
            }
        }

        /* ── Class 9 special: n+r→nd (§3.5) ─────────────────────────────
         * n(RS) + re(C) → surface "nde" (r→d after n).
         * After stripping "nd", sfx = "e" but underlying C = "re".
         * Reconstruct by prepending 'r' to sfx and recheck.             */
        if (ADJ_PREFIXES[i].cls == 9 &&
            ADJ_PREFIXES[i].pfx[0] == 'n' && ADJ_PREFIXES[i].pfx[1] == 'd' &&
            ADJ_PREFIXES[i].pfx[2] == '\0') {
            char restored[KIN_MAX_STEM];
            restored[0] = 'r';
            strncpy(restored + 1, sfx, KIN_MAX_STEM - 2);
            restored[KIN_MAX_STEM - 1] = '\0';
            if (kin_is_adj_stem(restored)) {
                if (stem_out)  strncpy(stem_out, restored, KIN_MAX_STEM - 1);
                if (class_out) *class_out = 9;
                return true;
            }
        }

        /* Reduplication check: RS + stem + RS + stem
         * e.g. barebare = ba + re + ba + re; sfx="rebare", pfx="ba"         */
        {
            char red_stem[KIN_MAX_STEM] = "";
            if (kin_is_adj_reduplicated(sfx, ADJ_PREFIXES[i].pfx, red_stem)) {
                if (stem_out)  strncpy(stem_out, red_stem, KIN_MAX_STEM - 1);
                if (class_out) *class_out = ADJ_PREFIXES[i].cls;
                return true;
            }
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
