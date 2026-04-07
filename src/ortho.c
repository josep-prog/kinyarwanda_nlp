/*
 * ortho.c  –  Kinyarwanda orthographic rule engine
 *
 * Full implementation of ALL rules from:
 *   Inteko Nyarwanda y'Ururimi n'Umuco (RALC) 2017
 *   "Amategeko y'igenantego ry'Ikinyarwanda"
 *
 * Three sections matching the book structure:
 *   1.  Amategeko yerekeye inyajwi       (vowel rules)       §1
 *   2.  Amategeko yerekeye inyerera      (consonant rules)   §2
 *   3.  Amategeko yerekeye ingombajwi    (nasal rules)       §3
 *
 * Public API (declared in kinyarwanda.h):
 *
 *   kin_ortho_gen()              morpheme string with '|' → surface form
 *   kin_ortho_validate()         surface word  → OrthoViolation list
 *   kin_ortho_recover_verb_root()surface verb  → candidate underlying roots
 *   kin_ortho_nt9_stem()         Nt.9/10 noun  → stem after n-prefix rules
 *
 * Rule application order in kin_ortho_gen() (pass numbers in comments):
 *   P1  §2.3  w-y metathesis           (passive -w- before perfect -ye)
 *   P2  §3.9  C+y fusions              (d/g/r/s/k/c/t/z/sh/j/h/n + y)
 *   P3  §3.8  h+y, n+y fusions
 *   P4  §3.4  b→m before n
 *   P5  §3.3  nasal assimilation       (n→m/ny before various consonants)
 *   P6  §3.5  r→d before n
 *   P7  §3.1  nasal elision            (n→ø before m/n/ny/nny)
 *   P8  §3.7  consonant voicing        (k→g, t→d before voiced root)
 *   P9  §2.2  consonant loss at bdy    (y→ø, w→ø in contexts)
 *   P10 §2.4  n+y→nz  (Nt.9/10 only when flag set)
 *   P11 §1.2  vowel fusion             (a+i→e, a+u→o)
 *   P12 §1.1/§2.1  vowel contact       (glide formation / elision)
 *   P13       remove remaining '|' markers
 *   P14 §3.6  epenthetic stop deletion (nt/s, nc/n, mpf – global)
 *   P15 §3.2  plosive assimilation     (z/sh, s/j, s/sh – global)
 *   P16 §1.3  vowel assimilation       (i→e, u→o before o-stems – global)
 */

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "../include/kinyarwanda.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * Private helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

#define OB  512          /* ortho internal buffer size */

static bool ov(char c)  { return c=='a'||c=='e'||c=='i'||c=='o'||c=='u'; }

/* Ingombajwi z'indagi (GR) — voiced consonants in Kinyarwanda.
 * 'h' is the voiced glottal fricative [ɦ] in Kinyarwanda (ki+haza→gihaza).
 * Vowels count as voiced.  Used for validation; §3.7 itself is unconditional. */
static bool voiced(char c) {
    return c=='b'||c=='d'||c=='g'||c=='h'||c=='j'||c=='l'||c=='m'||c=='n'||
           c=='r'||c=='v'||c=='w'||c=='y'||c=='z'||ov(c);
}

/* Delete n characters at position pos (buf[pos..pos+n-1]). */
static void del_at(char *buf, int pos, int n, int *len) {
    memmove(buf+pos, buf+pos+n, (size_t)(*len-pos-n+1));
    *len -= n;
}

/* Replace old_n chars at pos with string repl.
 * Returns new length or -1 on overflow (buf must have OB capacity). */
static int repl_at(char *buf, int pos, int old_n, const char *repl,
                   int cur_len, int cap) {
    int rlen = (int)strlen(repl);
    int new_len = cur_len - old_n + rlen;
    if (new_len >= cap) return -1;
    memmove(buf+pos+rlen, buf+pos+old_n, (size_t)(cur_len-pos-old_n+1));
    memcpy(buf+pos, repl, (size_t)rlen);
    return new_len;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * P1 – §2.3  Consonant metathesis: w-y → y-w   (Igurana ry'imanya y'inyerera)
 *
 * The passive morpheme -w- precedes the perfective suffix -ye.
 * At the boundary stem|w|ye, the -w- and the leading -y- swap positions,
 * so the stem-final consonant can then fuse with y (rules §3.9).
 *
 *   bi|a|tek|w|ye  →  bi|a|tek|y|w|e  →  (k+y→ts)  →  byatetswe
 *   ba|ra|fit|w|ye →  ba|ra|fit|y|w|e →  (t+y→sh)  →  barafitswe
 *
 * Implementation: scan for pattern  "|w|y"  in buffer (two adjacent
 * boundaries around a lone 'w', followed by 'y').  Swap the 'w' and 'y'.
 * ═══════════════════════════════════════════════════════════════════════════ */
static void pass_wy_metathesis(char *buf, int *len) {
    for (int i = 1; i+3 < *len; i++) {
        /* Looking for: buf[i]='|', buf[i+1]='w', buf[i+2]='|', buf[i+3]='y' */
        if (buf[i]=='|' && buf[i+1]=='w' && buf[i+2]=='|' && buf[i+3]=='y') {
            buf[i+1] = 'y';   /* w → y  */
            buf[i+3] = 'w';   /* y → w  */
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * P2+P3 – §3.9 + §3.8   Consonant + y fusions
 *   (Kwiyunga kw'ingombajwi n'inyerera y)
 *
 * Called at each morpheme boundary where the char AFTER the boundary is 'y'.
 * The char(s) BEFORE the boundary determine which fusion rule applies.
 *
 * §3.9 rules (consonant + y fused at boundary):
 *   3.9.1   d + y → z          3.9.2   g + y → z
 *   3.9.3   r + y → y  *       3.9.4   r + y → z  * (context-dependent)
 *   3.9.5   r + y → j  *       3.9.6   s + y → sh
 *   3.9.7   k + y → ts         3.9.8   nk + y → ns
 *   3.9.9   c + y → c *        3.9.10  t + y → s / sh  *
 *   3.9.11  z + y → j          3.9.12  c + y → sh
 *   3.9.13  sh + y → sh        3.9.14  j + y → j
 * §3.8 rules:
 *   3.8.1   h + y → shy         3.8.2   n + y → nny  (stem-internal)
 *
 * Disambiguation heuristics for multi-outcome rules (marked *):
 *   r+y:   → y  when preceded by 'i'/'e' (applicative -ir-/-er- + -ye)
 *          → z  otherwise (root-final r + perfective -ye) [most common]
 *   k+y:   → s  when preceded by 'n' (cluster nk before -ye)  [3.9.8]
 *          → ts otherwise  [3.9.7]
 *   c+y:   → sh when next char after y is a vowel (FV remains) [3.9.12]
 *          → c  otherwise (agentive -yi suffix) [3.9.9]
 *   t+y:   → sh when preceded by 'a' and followed by 'e'  [3.9.10 variant]
 *          → s  otherwise  [3.9.10 default]
 *
 * Returns true if a fusion was applied; buf and *len are updated.
 * ═══════════════════════════════════════════════════════════════════════════ */
static bool apply_cy_fusion(char *buf, int bpos, int *len, bool skip_n_y_nny) {
    if (bpos < 1 || bpos+1 >= *len) return false;
    if (buf[bpos+1] != 'y') return false;

    char c1  = buf[bpos-1];                       /* char just before '|'  */
    char c2  = (bpos >= 2) ? buf[bpos-2] : '\0';  /* char two before '|'   */
    char nxt = (bpos+2 < *len) ? buf[bpos+2] : '\0'; /* char after 'y'     */

    /* §3.9.13: sh + y → sh  (digraph ending: ...sh|y) */
    if (c2=='s' && c1=='h') {
        del_at(buf, bpos, 2, len);   /* remove '|y' */
        return true;
    }
    /* §3.9.14: j + y → j */
    if (c1=='j') { del_at(buf, bpos, 2, len); return true; }

    /* §3.9.1: d + y → z */
    if (c1=='d') {
        buf[bpos-1] = 'z';
        del_at(buf, bpos, 2, len);
        return true;
    }
    /* §3.9.2: g + y → z */
    if (c1=='g') {
        buf[bpos-1] = 'z';
        del_at(buf, bpos, 2, len);
        return true;
    }
    /* §3.9.3/3.9.4/3.9.5: r + y → y (applicative) | z (root-default) | j (passive) */
    if (c1=='r') {
        if (c2=='i' || c2=='e') {
            /* Applicative -ir-/-er- context: r+y → y (3.9.3) */
            buf[bpos-1] = 'y';
        } else {
            /* Root-final r + perfective -ye: r+y → z (3.9.4) */
            buf[bpos-1] = 'z';
        }
        del_at(buf, bpos, 2, len);
        return true;
    }
    /* §3.9.6: s + y → sh */
    if (c1=='s') {
        int nl = repl_at(buf, bpos-1, 1, "sh", *len, OB);
        if (nl < 0) return false;
        *len = nl;
        del_at(buf, bpos+1, 2, len);   /* remove '|y' (shifted by inserted 'h') */
        return true;
    }
    /* §3.9.7/3.9.8: k + y → ts (default) | s (after nasal n) */
    if (c1=='k') {
        if (c2=='n') {
            buf[bpos-1] = 's';
            del_at(buf, bpos, 2, len);
        } else {
            int nl = repl_at(buf, bpos-1, 1, "ts", *len, OB);
            if (nl < 0) return false;
            *len = nl;
            del_at(buf, bpos+1, 2, len);
        }
        return true;
    }
    /* §3.9.9/3.9.12: c + y → c (agentive) | sh (before final vowel) */
    if (c1=='c') {
        if (ov(nxt)) {
            int nl = repl_at(buf, bpos-1, 1, "sh", *len, OB);
            if (nl < 0) return false;
            *len = nl;
            del_at(buf, bpos+1, 2, len);
        } else {
            del_at(buf, bpos, 2, len);   /* c+y→c: just drop '|y' */
        }
        return true;
    }
    /* §3.9.10: t + y → s (default) | sh (a_t context before FV) */
    if (c1=='t') {
        if (c2=='a' && ov(nxt)) {
            int nl = repl_at(buf, bpos-1, 1, "sh", *len, OB);
            if (nl < 0) return false;
            *len = nl;
            del_at(buf, bpos+1, 2, len);
        } else {
            buf[bpos-1] = 's';
            del_at(buf, bpos, 2, len);
        }
        return true;
    }
    /* §3.9.11: z + y → j */
    if (c1=='z') {
        buf[bpos-1] = 'j';
        del_at(buf, bpos, 2, len);
        return true;
    }
    /* §3.8.1: h + y → shy */
    if (c1=='h') {
        int nl = repl_at(buf, bpos-1, 1, "sh", *len, OB);
        if (nl < 0) return false;
        *len = nl;
        del_at(buf, bpos+1, 2, len);
        return true;
    }
    /* §3.8.2: n + y → nny  (stem-internal n, not prefix n)
     * Only fires when the char two before is NOT already 'n'
     * (to avoid tripling for cases like nny already present).
     * Skipped when noun_class_9=true because §2.4.1 (n+y→nz) takes priority. */
    if (c1=='n' && c2!='n' && !skip_n_y_nny) {
        /* n|y → nny: replace boundary+y with 'ny' → total: n+'ny' = nny */
        int nl = repl_at(buf, bpos, 2, "ny", *len, OB);
        if (nl < 0) return false;
        *len = nl;
        return true;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * P4 – §3.4.1  b → m before n at morpheme boundary
 *   "Ishushisha ry'ingombajwi b inyuma y'ingombajwi n"
 *
 *   i|n|banza  →  i|n|manza  →  (n→ø/m by §3.1.1)  →  imanza
 *
 * Must run BEFORE nasal elision so the n that immediately follows sees 'm'.
 * ═══════════════════════════════════════════════════════════════════════════ */
static void pass_b_to_m(char *buf, int *len) {
    for (int i = 1; i < *len; i++) {
        if (buf[i]=='|' && buf[i-1]=='n' && i+1 < *len && buf[i+1]=='b') {
            buf[i+1] = 'm';   /* b → m in stem after n-prefix */
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * P5 – §3.3  Nasal assimilation   (Ishushisha ry'inyamazuru)
 *   3.3.1  n → m  /  _f       3.3.2  n → m  /  _p
 *   3.3.3  n → m  /  _b       3.3.4  n → m  /  _h  (then m+h → mp, §3.3.4)
 *   3.3.5  n → m  /  _v       3.3.6  n → ny /  _V  (before any vowel)
 *   3.3.7  n → ny /  _wu      (class 3 possessive)
 *
 * §3.5: r → d / _ n  (also handled here since r is the preceding char).
 * ═══════════════════════════════════════════════════════════════════════════ */
static bool apply_nasal_assim(char *buf, int bpos, int *len) {
    if (bpos < 1 || bpos+1 >= *len) return false;
    char last = buf[bpos-1];
    char next = buf[bpos+1];
    char nxt2 = (bpos+2 < *len) ? buf[bpos+2] : '\0';

    /* §3.5: r → d before n */
    if (last=='r' && next=='n') {
        buf[bpos-1] = 'd';
        del_at(buf, bpos, 1, len);   /* remove boundary */
        return true;
    }
    if (last != 'n') return false;

    /* §3.3.6: n → ny / _ V */
    if (ov(next)) {
        buf[bpos] = 'y';   /* replace '|' with 'y' → forms 'ny' */
        return true;
    }
    /* §3.3.7: n → ny / _ wu */
    if (next=='w' && nxt2=='u') {
        buf[bpos] = 'y';   /* n|wu → nywu */
        return true;
    }
    /* §3.3.1: n → m / _ f */
    if (next=='f') { buf[bpos-1]='m'; del_at(buf,bpos,1,len); return true; }
    /* §3.3.2: n → m / _ p */
    if (next=='p') { buf[bpos-1]='m'; del_at(buf,bpos,1,len); return true; }
    /* §3.3.3: n → m / _ b */
    if (next=='b') { buf[bpos-1]='m'; del_at(buf,bpos,1,len); return true; }
    /* §3.3.5: n → m / _ v */
    if (next=='v') { buf[bpos-1]='m'; del_at(buf,bpos,1,len); return true; }
    /* §3.3.4: n → m / _ h,  then m+h → mp in writing:
     *   i-n-han-ur-o → i-m-panuro → impanuro  */
    if (next=='h') {
        buf[bpos-1] = 'm';   /* n → m */
        buf[bpos+1] = 'p';   /* h → p  (m+h surface spelling is mp) */
        del_at(buf, bpos, 1, len);
        return true;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * P6 – §3.1  Nasal elision   (Iburizwamo ry'inyamazuru)
 *   3.1.1  n → ø / _ m         3.1.2  n → ø / _ n
 *   3.1.3  n → ø / _ ny        3.1.4  n → ø / _ nny
 * ═══════════════════════════════════════════════════════════════════════════ */
static bool apply_nasal_elision(char *buf, int bpos, int *len) {
    if (bpos < 1 || bpos+1 >= *len) return false;
    if (buf[bpos-1] != 'n') return false;

    char n1 = buf[bpos+1];
    char n2 = (bpos+2 < *len) ? buf[bpos+2] : '\0';
    char n3 = (bpos+3 < *len) ? buf[bpos+3] : '\0';

    /* §3.1.4: n | nny → delete n| */
    if (n1=='n' && n2=='n' && n3=='y') {
        del_at(buf, bpos-1, 2, len); return true;
    }
    /* §3.1.3: n | ny → delete n| */
    if (n1=='n' && n2=='y') {
        del_at(buf, bpos-1, 2, len); return true;
    }
    /* §3.1.2: n | n → delete n| */
    if (n1=='n') { del_at(buf, bpos-1, 2, len); return true; }
    /* §3.1.1: n | m → delete n| */
    if (n1=='m') { del_at(buf, bpos-1, 2, len); return true; }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * P7 – §3.7  Consonant voicing   (Itanisha ry'ingombajwi)
 *   3.7.1  k → g  (prefix RT 'ki'/'ka'/'ku' → 'gi'/'ga'/'gu')
 *   3.7.2  t → d  (prefix RT 'tu' → 'du')
 *
 * This is a MORPHOPHONOLOGICAL rule on 2-char noun/verb prefixes, not a
 * strictly phonological voicing assimilation.  It applies unconditionally
 * when the preceding morpheme is exactly 2 chars and ends in 'k' or 't'.
 * Evidence: "igitabo" (igi+tabo) has gi before voiceless 't'.
 *
 * GR (ingombajwi z'indagi) notation in rule descriptions marks the output
 * class; the voiced() helper is used elsewhere for phonological validation.
 * ═══════════════════════════════════════════════════════════════════════════ */
static bool apply_voicing(char *buf, int bpos, int *len) {
    /* §3.7.1/3.7.2: k→g, t→d for short class-marker prefixes.
     * Applied unconditionally to any 2-char prefix ending in k or t.
     *
     * Algorithm: walk back from bpos to find the start of the preceding morpheme.
     * If (bpos - prec_start) == 2 and buf[prec_start] is 'k' or 't', voice it.
     * The boundary '|' is left in place (strip_boundaries removes it later). */
    if (bpos < 2 || bpos+1 >= *len) return false;

    /* Find start of preceding morpheme */
    int prec_start = bpos - 1;
    while (prec_start > 0 && buf[prec_start-1] != '|') prec_start--;

    int prec_len = bpos - prec_start;   /* length of preceding morpheme */
    if (prec_len != 2) return false;    /* only short (2-char) prefixes  */

    char fc = buf[prec_start];          /* first char of preceding morpheme */
    if (fc == 'k') { buf[prec_start] = 'g'; return true; }
    if (fc == 't') { buf[prec_start] = 'd'; return true; }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * P8 – §2.2  Consonant loss / fusion at boundary
 *   2.2.1  y → ø / _ y     2.2.2  y → ø / _ w
 *   2.2.3  y → ø / k _ i   2.2.4  y → ø / k _ e
 *   2.2.5  y → ø / g _ e   2.2.6  w → ø / k _ u
 *   2.2.7  w → ø / k _ o   2.2.8  w + y → w
 * ═══════════════════════════════════════════════════════════════════════════ */
static bool apply_cons_loss(char *buf, int bpos, int *len) {
    if (bpos < 1 || bpos+1 >= *len) return false;
    char c0  = buf[bpos-1];
    char cm2 = (bpos >= 2) ? buf[bpos-2] : '\0';
    char n1  = buf[bpos+1];

    /* §2.2.8: w|y → w  (w absorbs y) */
    if (c0=='w' && n1=='y') { del_at(buf,bpos,2,len); return true; }
    /* §2.2.1: y|y → y  (first y elides: del y before boundary) */
    if (c0=='y' && n1=='y') { del_at(buf,bpos-1,2,len); return true; }
    /* §2.2.2: y|w → ø+w  (y elides before w) */
    if (c0=='y' && n1=='w') { del_at(buf,bpos-1,2,len); return true; }
    /* §2.2.3: ky|i → ki  (y drops between k and i) */
    if (cm2=='k' && c0=='y' && n1=='i') { del_at(buf,bpos-1,2,len); return true; }
    /* §2.2.4: ky|e → ke */
    if (cm2=='k' && c0=='y' && n1=='e') { del_at(buf,bpos-1,2,len); return true; }
    /* §2.2.5: gy|e → ge */
    if (cm2=='g' && c0=='y' && n1=='e') { del_at(buf,bpos-1,2,len); return true; }
    /* §2.2.6: kw|u → ku */
    if (cm2=='k' && c0=='w' && n1=='u') { del_at(buf,bpos-1,2,len); return true; }
    /* §2.2.7: kw|o → ko */
    if (cm2=='k' && c0=='w' && n1=='o') { del_at(buf,bpos-1,2,len); return true; }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * P9 – §2.4  n + y → nz  (Nt.9/10 noun prefix only)
 *   Iyungana ry'ingombajwi n n'inyerera y itangira ibicumbi by'amazina
 *   yo mu nteko ya 9 ari mu bwinshi mu nteko ya 10.
 *
 *   2.4.1  n + y-initial-stem → nz    (inzira, inzoga, inzara)
 *   Note:  n + V-initial-stem → nz+V  (epenthetic z: inzugi, inzabya)
 *          This epenthesis is handled by kin_ortho_nt9_stem().
 *
 * Only called when noun_class_9 flag is true (to distinguish from §3.3.6).
 * ═══════════════════════════════════════════════════════════════════════════ */
static bool apply_n_y_nz(char *buf, int bpos, int *len) {
    if (bpos < 1 || bpos+1 >= *len) return false;
    if (buf[bpos-1]=='n' && buf[bpos+1]=='y') {
        buf[bpos]  = 'z';                  /* replace '|' with 'z' */
        del_at(buf, bpos+1, 1, len);       /* remove 'y'           */
        return true;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * P10 – §1.2  Vowel fusion at boundary   (Kwiyunga kw'inyajwi)
 *   1.2.1  a + i → e      (ba|iza → beza)
 *   1.2.3  a + u → o      (ba|ura → bora)
 * ═══════════════════════════════════════════════════════════════════════════ */
static bool apply_vowel_fusion(char *buf, int bpos, int *len) {
    if (bpos < 1 || bpos+1 >= *len) return false;
    if (buf[bpos-1]=='a' && buf[bpos+1]=='i') {
        int nl = repl_at(buf, bpos-1, 3, "e", *len, OB);
        if (nl >= 0) { *len = nl; return true; }
        return false;
    }
    if (buf[bpos-1]=='a' && buf[bpos+1]=='u') {
        int nl = repl_at(buf, bpos-1, 3, "o", *len, OB);
        if (nl >= 0) { *len = nl; return true; }
        return false;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * P11 – §1.1 + §2.1  Vowel glide formation / elision at boundary
 *
 * When the morpheme-final char is a vowel and the morpheme-initial char
 * is also a vowel (VV contact at boundary), one of these applies:
 *
 *   §2.1.4  u → w  /  _V     (prefix u before vowel-initial morpheme)
 *   §2.1.5  o → w  /  _V
 *   §2.1.2  i → y  /  _V
 *   §2.1.3  e → y  /  _V
 *   §2.1.1  a → y  /  _V  (SP 'a' forms glide before vowel)
 *   §1.1.1  a → ø  /  _V  (a elides when preceded by another vowel)
 *   §1.2.1  a + i → e      (already handled in pass 10; fallback)
 *   §1.2.3  a + u → o      (already handled in pass 10; fallback)
 *   §1.1.2  i → ø  /  _V  (i elides in some prefix positions)
 *   §1.1.3  u → ø  /  _V  (u elides in some prefix positions)
 *
 * Disambiguation:
 *   u|V  → always w+V  (u→w glide, rule §2.1.4)
 *   o|V  → always w+V  (o→w glide, rule §2.1.5)
 *   i|V  → y+V  (i→y glide, rule §2.1.2)
 *   e|V  → y+V  (e→y glide, rule §2.1.3)
 *   a|i  → e    (fusion §1.2.1, handled in P10)
 *   a|u  → o    (fusion §1.2.3, handled in P10)
 *   a|V (other) when preceded by consonant → y+V  (§2.1.1)
 *   a|V (other) when preceded by vowel     → ø    (§1.1.1, elision)
 * ═══════════════════════════════════════════════════════════════════════════ */
static bool apply_vowel_contact(char *buf, int bpos, int *len) {
    if (bpos < 1 || bpos+1 >= *len) return false;
    char v1 = buf[bpos-1];
    char v2 = buf[bpos+1];
    if (!ov(v1) || !ov(v2)) return false;

    switch (v1) {
        case 'u':
            buf[bpos-1] = 'w';
            del_at(buf, bpos, 1, len);
            return true;
        case 'o':
            buf[bpos-1] = 'w';
            del_at(buf, bpos, 1, len);
            return true;
        case 'i':
            buf[bpos-1] = 'y';
            del_at(buf, bpos, 1, len);
            return true;
        case 'e':
            buf[bpos-1] = 'y';
            del_at(buf, bpos, 1, len);
            return true;
        case 'a':
            if (v2 == 'i') {   /* fallback fusion */
                int nl = repl_at(buf, bpos-1, 3, "e", *len, OB);
                if (nl >= 0) { *len = nl; return true; }
            }
            if (v2 == 'u') {   /* fallback fusion */
                int nl = repl_at(buf, bpos-1, 3, "o", *len, OB);
                if (nl >= 0) { *len = nl; return true; }
            }
            {
                char cm2 = (bpos >= 2) ? buf[bpos-2] : '\0';
                if (!ov(cm2) && cm2 != '\0') {
                    /* Consonant before a: a → y  (§2.1.1) */
                    buf[bpos-1] = 'y';
                } else {
                    /* Vowel or start before a: elide a  (§1.1.1) */
                    del_at(buf, bpos-1, 2, len);
                    return true;
                }
                del_at(buf, bpos, 1, len);
                return true;
            }
        default: break;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * P13 – Strip remaining boundary markers
 * ═══════════════════════════════════════════════════════════════════════════ */
static void strip_boundaries(char *buf, int *len) {
    char tmp[OB];
    int j = 0;
    for (int i = 0; buf[i] && j+1 < OB; i++)
        if (buf[i] != '|') tmp[j++] = buf[i];
    tmp[j] = '\0';
    memcpy(buf, tmp, (size_t)(j+1));
    *len = j;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * P14 – §3.6  Epenthetic stop deletion (global scan)
 *   3.6.1  t → ø / n _ s     (nts → ns:   i-n-tsina → insina)
 *   3.6.2  c → sh / n _      (nc → nsh:   i-n-cuti → inshuti)
 *   3.6.3  p → ø / m _ f     (mpf → mf:   i-m-pfizi → imfizi)
 * ═══════════════════════════════════════════════════════════════════════════ */
static bool pass_epenthetic(char *buf, int *len) {
    bool ch = false;
    for (int i = 0; i+2 <= *len; i++) {
        /* §3.6.1: n+t+s → n+s */
        if (buf[i]=='n' && buf[i+1]=='t' && i+2 < *len && buf[i+2]=='s') {
            del_at(buf, i+1, 1, len); ch = true; continue;
        }
        /* §3.6.2: n+c → n+sh */
        if (buf[i]=='n' && buf[i+1]=='c') {
            int nl = repl_at(buf, i+1, 1, "sh", *len, OB);
            if (nl >= 0) { *len = nl; ch = true; i++; } continue;
        }
        /* §3.6.3: m+p+f → m+f */
        if (buf[i]=='m' && buf[i+1]=='p' && i+2 < *len && buf[i+2]=='f') {
            del_at(buf, i+1, 1, len); ch = true; continue;
        }
    }
    return ch;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * P15 – §3.2  Plosive assimilation (global scan)
 *   3.2.1  z → j / _ sh       3.2.2  s → sh / _ j
 *   3.2.3  s → sh / _ sh
 * ═══════════════════════════════════════════════════════════════════════════ */
static bool pass_plosive_assim(char *buf, int *len) {
    bool ch = false;
    for (int i = 0; i+1 < *len; i++) {
        /* §3.2.1: z+sh → j+sh */
        if (buf[i]=='z' && buf[i+1]=='s' && i+2 < *len && buf[i+2]=='h') {
            buf[i] = 'j'; ch = true; continue;
        }
        /* §3.2.2: s+j → sh+j */
        if (buf[i]=='s' && buf[i+1]=='j') {
            int nl = repl_at(buf, i, 1, "sh", *len, OB);
            if (nl >= 0) { *len = nl; ch = true; i++; } continue;
        }
        /* §3.2.3: s+sh → sh+sh */
        if (buf[i]=='s' && buf[i+1]=='s' && i+2 < *len && buf[i+2]=='h') {
            int nl = repl_at(buf, i, 1, "sh", *len, OB);
            if (nl >= 0) { *len = nl; ch = true; i++; } continue;
        }
    }
    return ch;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * P16 – §1.3  Vowel assimilation (global scan)
 *   1.3.1/1.3.2  i → e / before stem containing 'o'
 *   1.3.3        u → o / before stem containing 'o'
 *
 * Applies specifically to verb extension vowels:
 *   -ir- → -er-  (applicative: kukorera ← ku+kor+ir+a)
 *   -ish- → -esh-  (causative: gukorisha ← gu+kor+ish+a)
 *   -ur- → -or-  (reversive: guturuza ← gu+tur+ur+a)
 *
 * Algorithm: scan for the word containing 'o' in the root; if found,
 * replace extension-position 'i'/'u' with 'e'/'o'.  The extension
 * position is an 'i' or 'u' preceded by a consonant (not word-initial).
 * ═══════════════════════════════════════════════════════════════════════════ */
static bool pass_vowel_assim(char *buf, int *len) {
    /* §1.3.1/1.3.2: i→e in -ir-/-ish- when root (vowels BEFORE extension) has 'e' or 'o'.
     * §1.3.3:        u→o in -ur-        when root has 'o'.
     *
     * Key constraint: only vowels appearing BEFORE the extension position count.
     * This prevents "papuro" (where 'o' is AFTER '-ur-') from triggering the rule.
     */
    bool ch = false;
    int wlen = *len;
    for (int i = 1; i+1 < wlen; i++) {
        /* Extension-position vowel: preceded by consonant */
        if (ov(buf[i-1])) continue;

        bool is_ir  = (buf[i]=='i' && buf[i+1]=='r');
        bool is_ish = (buf[i]=='i' && buf[i+1]=='s' && i+2 < wlen && buf[i+2]=='h');
        bool is_ur  = (buf[i]=='u' && buf[i+1]=='r');
        if (!is_ir && !is_ish && !is_ur) continue;

        /* Check root vowels BEFORE this extension position */
        bool root_has_e_or_o = false, root_has_o = false;
        for (int j = 0; j < i; j++) {
            if (buf[j]=='o') { root_has_o = true; root_has_e_or_o = true; }
            if (buf[j]=='e') { root_has_e_or_o = true; }
        }

        if ((is_ir || is_ish) && root_has_e_or_o) {
            buf[i] = 'e'; ch = true;
        }
        if (is_ur && root_has_o) {
            buf[i] = 'o'; ch = true;
        }
    }
    return ch;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PUBLIC API 1: kin_ortho_gen()
 *
 * Apply all orthographic rules to a '|'-delimited morpheme string.
 * '|' marks morpheme boundaries.  '-' is treated identically to '|'.
 * '0' marks a zero morpheme and is ignored.
 *
 * The flag `noun_class_9` activates rule §2.4 (n+y→nz) which only applies
 * to the Nt.9/10 noun class prefix context, not to verbs.
 *
 * Examples:
 *   kin_ortho_gen("ba|iza",            false, surf, sz) → "beza"
 *   kin_ortho_gen("ku|0|ubak|a",       false, surf, sz) → "kubaka"
 *   kin_ortho_gen("ya|kor|ye",         false, surf, sz) → "yakoze"
 *   kin_ortho_gen("ya|rem|ye",         false, surf, sz) → "yaremye"
 *   kin_ortho_gen("bi|a|tek|w|ye",     false, surf, sz) → "byatetswe"
 *   kin_ortho_gen("ba|ra|mu|ton|ish|ir|ye", false, surf, sz) → "baramutonesheje"
 *   kin_ortho_gen("i|n|banza",         false, surf, sz) → "imanza"
 *   kin_ortho_gen("i|n|yira",          true,  surf, sz) → "inzira"
 *   kin_ortho_gen("mu|tek|yi",         false, surf, sz) → "mutetsi"
 *   kin_ortho_gen("u|mu|ana",          false, surf, sz) → "umwana"
 * ═══════════════════════════════════════════════════════════════════════════ */
void kin_ortho_gen(const char *morphemes, bool noun_class_9,
                   char *surface, size_t size) {
    char buf[OB];
    int  len = 0;

    /* Copy input; normalise '-' → '|'; skip '0' zero morphemes */
    for (int i = 0; morphemes[i] && len+1 < OB; i++) {
        char c = morphemes[i];
        if (c == '0') continue;
        if (c == '-') c = '|';
        buf[len++] = (char)tolower((unsigned char)c);
    }
    buf[len] = '\0';

    /* P1: w-y metathesis */
    pass_wy_metathesis(buf, &len);

    /* P2+P3: C+y fusions (multiple passes until stable)
     * Pass noun_class_9 flag to skip §3.8.2 (n+y→nny) when §2.4.1 should apply */
    for (int iter = 0; iter < 4; iter++) {
        bool any = false;
        for (int i = 0; i < len; i++)
            if (buf[i]=='|') {
                if (apply_cy_fusion(buf, i, &len, noun_class_9)) { any=true; i--; }
            }
        if (!any) break;
    }

    /* P4: b→m before n (§3.4.1)
     * Skipped when noun_class_9=true: for Nt.9/10 nouns, the boundary is
     * prefix-n + stem, so n→m (§3.3.3) applies, NOT b→m (§3.4.1).
     * §3.3.3 fires in P5 below and correctly gives imbabazi not imabazi. */
    if (!noun_class_9) pass_b_to_m(buf, &len);

    /* P5: nasal assimilation (multiple passes for cascades) */
    for (int iter = 0; iter < 4; iter++) {
        bool any = false;
        for (int i = 0; i < len; i++)
            if (buf[i]=='|') { if (apply_nasal_assim(buf, i, &len)) { any=true; i--; } }
        if (!any) break;
    }

    /* P6: nasal elision */
    for (int iter = 0; iter < 4; iter++) {
        bool any = false;
        for (int i = 0; i < len; i++)
            if (buf[i]=='|') { if (apply_nasal_elision(buf, i, &len)) { any=true; i--; } }
        if (!any) break;
    }

    /* P7: consonant voicing */
    for (int i = 0; i < len; i++)
        if (buf[i]=='|') apply_voicing(buf, i, &len);

    /* P8: consonant loss at boundaries */
    for (int iter = 0; iter < 3; iter++) {
        bool any = false;
        for (int i = 0; i < len; i++)
            if (buf[i]=='|') { if (apply_cons_loss(buf, i, &len)) { any=true; i--; } }
        if (!any) break;
    }

    /* P9: n+y→nz for Nt.9/10 */
    if (noun_class_9) {
        for (int i = 0; i < len; i++)
            if (buf[i]=='|') apply_n_y_nz(buf, i, &len);
    }

    /* P10: vowel fusion a+i→e, a+u→o */
    for (int i = 0; i < len; i++)
        if (buf[i]=='|') apply_vowel_fusion(buf, i, &len);

    /* P11: vowel contact (glide / elision) – multiple passes */
    for (int iter = 0; iter < 6; iter++) {
        bool any = false;
        for (int i = 0; i < len; i++)
            if (buf[i]=='|') { if (apply_vowel_contact(buf, i, &len)) { any=true; i--; } }
        if (!any) break;
    }

    /* P12: remove leftover boundaries */
    strip_boundaries(buf, &len);

    /* P13: epenthetic stop deletion */
    pass_epenthetic(buf, &len);

    /* P14: plosive assimilation */
    pass_plosive_assim(buf, &len);

    /* P15: vowel assimilation */
    pass_vowel_assim(buf, &len);

    strncpy(surface, buf, size-1);
    surface[size-1] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PUBLIC API 2: kin_ortho_validate()
 *
 * Scan a surface word for orthographic rule violations.
 * Fills `viol` array (up to `max` entries) and returns violation count.
 *
 * Checks performed (rule → violation type):
 *   §1.1/§2.1  VV hiatus not resolved        → ORTHO_VV_HIATUS
 *   §3.3.1     n before f not → m            → ORTHO_NASAL_ASSIM
 *   §3.3.2     n before p not → m            → ORTHO_NASAL_ASSIM
 *   §3.3.3     n before b not → m            → ORTHO_NASAL_ASSIM
 *   §3.3.4     n before h not → mp           → ORTHO_NASAL_ASSIM
 *   §3.3.5     n before v not → m            → ORTHO_NASAL_ASSIM
 *   §3.9.1     d followed by y  (should fuse) → ORTHO_CY_UNFUSED
 *   §3.9.2     g followed by y  (should fuse) → ORTHO_CY_UNFUSED
 *   §3.9.6     s followed by y  (should fuse) → ORTHO_CY_UNFUSED
 *   §3.9.7/8   k followed by y  (should fuse) → ORTHO_CY_UNFUSED
 *   §3.9.10    t followed by y  (should fuse) → ORTHO_CY_UNFUSED
 *   §3.9.11    z followed by y  (should fuse) → ORTHO_CY_UNFUSED
 *   §3.6.1     n+t+s cluster (t not deleted)  → ORTHO_STOP_UNDELETED
 *   §3.6.2     n+c cluster (c not → sh)       → ORTHO_C_NOT_SH
 *   §3.6.3     m+p+f cluster (p not deleted)  → ORTHO_STOP_UNDELETED
 *   §1.3       i in -ir- before o-stem        → ORTHO_VOWEL_ASSIM
 * ═══════════════════════════════════════════════════════════════════════════ */
int kin_ortho_validate(const char *word, OrthoViolation *viol, int max) {
    int count = 0;
    int wlen = (int)strlen(word);
    if (!wlen) return 0;

    /* Helper: add violation (uses snprintf with explicit message string) */
#define ADD_VIOL(rtype, rpos, rsrc, msg_str) \
    do { if (count < max) { \
        viol[count].type = (rtype); viol[count].pos = (rpos); \
        strncpy(viol[count].rule, (rsrc), sizeof(viol[count].rule)-1); \
        viol[count].rule[sizeof(viol[count].rule)-1] = '\0'; \
        strncpy(viol[count].msg, (msg_str), sizeof(viol[count].msg)-1); \
        viol[count].msg[sizeof(viol[count].msg)-1] = '\0'; \
        count++; } } while(0)

    /* Scan for violations */
    for (int i = 0; i < wlen; i++) {
        char c  = word[i];
        char c1 = (i+1 < wlen) ? word[i+1] : '\0';
        char c2 = (i+2 < wlen) ? word[i+2] : '\0';

        /* §1.1/§2.1: VV hiatus – two adjacent vowels */
        if (ov(c) && ov(c1)) {
            char msg[KIN_MAX_MSG];
            snprintf(msg, sizeof(msg),
                     "Iranya ry'impanvu: '%c%c' – impanvu ebyiri zisubiranya. "
                     "Vowel hiatus: '%c%c' adjacent vowels must be resolved "
                     "(u→w, i→y, a→ø, a+i→e, a+u→o).",
                     c, c1, c, c1);
            ADD_VIOL(ORTHO_VV_HIATUS, i, "§1.1/§2.1", msg);
        }

        /* §3.3 nasal assimilation – n before bilabials/labiodentals must become m */
        if (c=='n') {
            char msg[KIN_MAX_MSG];
            if (c1=='f') {
                snprintf(msg,sizeof(msg),"Ishushisha §3.3.1: 'nf' → 'mf'. "
                         "Nasal assimilation: 'n' before 'f' must become 'm'.");
                ADD_VIOL(ORTHO_NASAL_ASSIM, i, "§3.3.1", msg);
            } else if (c1=='p') {
                snprintf(msg,sizeof(msg),"Ishushisha §3.3.2: 'np' → 'mp'. "
                         "Nasal assimilation: 'n' before 'p' must become 'm'.");
                ADD_VIOL(ORTHO_NASAL_ASSIM, i, "§3.3.2", msg);
            } else if (c1=='b') {
                snprintf(msg,sizeof(msg),"Ishushisha §3.3.3: 'nb' → 'mb'. "
                         "Nasal assimilation: 'n' before 'b' must become 'm'.");
                ADD_VIOL(ORTHO_NASAL_ASSIM, i, "§3.3.3", msg);
            } else if (c1=='h') {
                snprintf(msg,sizeof(msg),"Ishushisha §3.3.4: 'nh' → 'mp'. "
                         "Nasal assimilation: 'n' before 'h' must become 'm', then m+h → mp.");
                ADD_VIOL(ORTHO_NASAL_ASSIM, i, "§3.3.4", msg);
            } else if (c1=='v') {
                snprintf(msg,sizeof(msg),"Ishushisha §3.3.5: 'nv' → 'mv'. "
                         "Nasal assimilation: 'n' before 'v' must become 'm'.");
                ADD_VIOL(ORTHO_NASAL_ASSIM, i, "§3.3.5", msg);
            }
        }

        /* §3.1 nasal elision – n before m/n should elide (only for in- prefix) */
        if (c=='n' && (c1=='m' || c1=='n') && i==1 && word[0]=='i') {
            char msg[KIN_MAX_MSG];
            snprintf(msg,sizeof(msg),"Iburizwamo §3.1: 'in%c' → 'i%c'. "
                     "Nasal elision: prefix 'n' before '%c' must drop.", c1, c1, c1);
            ADD_VIOL(ORTHO_NASAL_ELISION, i, "§3.1", msg);
        }

        /* §3.9 C+y sequences that should have been fused */
        if (c1 == 'y') {
            char msg[KIN_MAX_MSG];
            if (c=='d') {
                snprintf(msg,sizeof(msg),"Kwiyunga §3.9.1: 'd+y' → 'z'. "
                         "Unfused: 'd' and 'y' must fuse to 'z'.");
                ADD_VIOL(ORTHO_CY_UNFUSED, i, "§3.9.1", msg);
            } else if (c=='g') {
                snprintf(msg,sizeof(msg),"Kwiyunga §3.9.2: 'g+y' → 'z'. "
                         "Unfused: 'g' and 'y' must fuse to 'z'.");
                ADD_VIOL(ORTHO_CY_UNFUSED, i, "§3.9.2", msg);
            } else if (c=='s') {
                snprintf(msg,sizeof(msg),"Kwiyunga §3.9.6: 's+y' → 'sh'. "
                         "Unfused: 's' and 'y' must fuse to 'sh'.");
                ADD_VIOL(ORTHO_CY_UNFUSED, i, "§3.9.6", msg);
            } else if (c=='z') {
                snprintf(msg,sizeof(msg),"Kwiyunga §3.9.11: 'z+y' → 'j'. "
                         "Unfused: 'z' and 'y' must fuse to 'j'.");
                ADD_VIOL(ORTHO_CY_UNFUSED, i, "§3.9.11", msg);
            } else if (c=='k') {
                snprintf(msg,sizeof(msg),"Kwiyunga §3.9.7: 'k+y' → 'ts' (or 's' after 'n'). "
                         "Unfused: 'k' and 'y' must fuse.");
                ADD_VIOL(ORTHO_CY_UNFUSED, i, "§3.9.7", msg);
            } else if (c=='t') {
                snprintf(msg,sizeof(msg),"Kwiyunga §3.9.10: 't+y' → 's' or 'sh'. "
                         "Unfused: 't' and 'y' must fuse.");
                ADD_VIOL(ORTHO_CY_UNFUSED, i, "§3.9.10", msg);
            }
        }

        /* §3.6.1: n+t+s – t should be deleted */
        if (c=='n' && c1=='t' && c2=='s') {
            ADD_VIOL(ORTHO_STOP_UNDELETED, i, "§3.6.1",
                     "Izimira §3.6.1: 'nts' → 'ns'. "
                     "Stop deletion: epenthetic 't' in 'nts' must be deleted.");
        }
        /* §3.6.2: n+c – c should become sh */
        if (c=='n' && c1=='c') {
            ADD_VIOL(ORTHO_C_NOT_SH, i, "§3.6.2",
                     "Ishushisha §3.6.2: 'nc' → 'nsh'. "
                     "Assimilation: 'c' after nasal 'n' must become 'sh'.");
        }
        /* §3.6.3: m+p+f – p should be deleted */
        if (c=='m' && c1=='p' && c2=='f') {
            ADD_VIOL(ORTHO_STOP_UNDELETED, i, "§3.6.3",
                     "Izimira §3.6.3: 'mpf' → 'mf'. "
                     "Stop deletion: epenthetic 'p' in 'mpf' must be deleted.");
        }
    }

    /* §1.3 vowel assimilation: extension -ir- should be -er- when root has 'o' */
    bool has_o = false;
    for (int i = 0; word[i]; i++) if (word[i]=='o') { has_o = true; break; }
    if (has_o) {
        for (int i = 1; i+1 < wlen; i++) {
            if (word[i]=='i' && word[i+1]=='r' && !ov(word[i-1])) {
                ADD_VIOL(ORTHO_VOWEL_ASSIM, i, "§1.3.1",
                         "Ishushisha §1.3.1: '-ir-' → '-er-' imbere y'igicumbi kirimo 'o'. "
                         "Vowel assimilation: '-ir-' before o-containing stem must become '-er-'.");
            }
            if (word[i]=='i' && word[i+1]=='s' && i+2 < wlen && word[i+2]=='h'
                && !ov(word[i-1])) {
                ADD_VIOL(ORTHO_VOWEL_ASSIM, i, "§1.3.1",
                         "Ishushisha §1.3.1: '-ish-' → '-esh-' imbere y'igicumbi kirimo 'o'. "
                         "Vowel assimilation: '-ish-' before o-containing stem must become '-esh-'.");
            }
        }
    }

#undef ADD_VIOL
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PUBLIC API 3: kin_ortho_recover_verb_root()
 *
 * Given the "raw stem" extracted by the verb conjugation parser (after
 * stripping SP, tense marker, and final vowel), recover the underlying
 * root by reversing the C+y fusions (§3.9, §3.8) that occurred when the
 * perfective suffix -ye combined with the stem-final consonant.
 *
 * The raw stem is the surface substring SP+tense+...+FV with those parts
 * stripped; for example from "yakoze" (ya=SP, kor=root, ye=FV merged):
 *   morphology strips: ya | ? | e  →  raw_stem = "koz"
 * This function detects "koz" ends in 'z' (from r+y→z) → candidate "kor".
 *
 * Returns: number of candidates written to `roots` (0 = no fusion detected,
 * raw stem is already the root).  When n=0, the caller should use raw_stem.
 *
 * Handles all §3.9 rules plus §3.8.1 (h+y→shy) and §3.8.2 (n+y→nny).
 *
 * PAST TENSE suffixes handled:
 *   The parser strips:
 *     -e   (from   C+y+e  where C+y fused, leaving 'e')
 *     -ye  (from   V+ye or m/n/b+ye where no fusion, leaving 'ye')
 *     -tse (from   k+ye=ts+e)
 *   After stripping, what's left in raw_stem is the surface root.
 *
 * Surface suffix → underlying stem-final consonant(s):
 *   ends in 'z'   → could be: r (§3.9.4), d (§3.9.1), g (§3.9.2)
 *   ends in 'j'   → could be: z (§3.9.11), r (§3.9.5)
 *   ends in 'y'   → could be: r (§3.9.3, applicative)
 *   ends in "sh"  → could be: s (§3.9.6), c (§3.9.12), h (§3.8.1)
 *   ends in "ts"  → could be: k (§3.9.7)
 *   ends in "nny" → could be: n (§3.8.2, stem-internal n)
 *   ends in 's'   → could be: t (§3.9.10), k after n (§3.9.8)
 *   ends in 'c'   → could be: c (§3.9.9, unchanged)
 *   (ends unchanged for: m, b, p, f, v, n, w, r in some contexts)
 * ═══════════════════════════════════════════════════════════════════════════ */
int kin_ortho_recover_verb_root(const char *raw_stem,
                                 char roots[][KIN_MAX_STEM], int max_roots) {
    int n = 0;
    size_t slen = strlen(raw_stem);
    if (!slen || max_roots <= 0) return 0;

    char tmp[KIN_MAX_STEM];

#define ADD_ROOT(suffix_len, replacement) \
    do { if (n < max_roots && slen > (size_t)(suffix_len)) { \
        size_t blen = slen - (size_t)(suffix_len); \
        if (blen < KIN_MAX_STEM - 1) { \
            memcpy(tmp, raw_stem, blen); \
            size_t rlen = strlen(replacement); \
            if (blen + rlen < KIN_MAX_STEM) { \
                memcpy(tmp+blen, (replacement), rlen); \
                tmp[blen+rlen] = '\0'; \
                memcpy(roots[n++], tmp, blen+rlen+1); \
            } } } } while(0)

    char end1 = raw_stem[slen-1];
    char end2 = (slen >= 2) ? raw_stem[slen-2] : '\0';
    char end3 = (slen >= 3) ? raw_stem[slen-3] : '\0';

    /* §3.8.2: ends in "nny" → underlying ends in "n" (n+y=nny, n is stem-final) */
    if (slen >= 3 && end3=='n' && end2=='n' && end1=='y') {
        ADD_ROOT(2, "");   /* remove trailing "ny", keep first "n" */
    }

    /* §3.9.7: ends in "ts" → underlying ends in "k" */
    if (slen >= 2 && end2=='t' && end1=='s') {
        ADD_ROOT(2, "k");
    }

    /* §3.8.1: ends in "sh" after a vowel-ish context → underlying ends in "h" */
    /* §3.9.6: ends in "sh" → underlying ends in "s" */
    /* §3.9.12: ends in "sh" → underlying ends in "c" */
    if (slen >= 2 && end2=='s' && end1=='h') {
        ADD_ROOT(2, "s");   /* §3.9.6 most common */
        ADD_ROOT(2, "c");   /* §3.9.12 */
        ADD_ROOT(2, "h");   /* §3.8.1 */
    }

    /* §3.9.4: ends in "z" → underlying ends in "r" (most common) */
    /* §3.9.1: ends in "z" → underlying ends in "d" */
    /* §3.9.2: ends in "z" → underlying ends in "g" */
    if (end1=='z') {
        ADD_ROOT(1, "r");   /* §3.9.4 – most common (kor+ye=koze) */
        ADD_ROOT(1, "d");   /* §3.9.1 */
        ADD_ROOT(1, "g");   /* §3.9.2 */
    }

    /* §3.9.11: ends in "j" → underlying ends in "z" */
    /* §3.9.5:  ends in "j" → underlying ends in "r" */
    if (end1=='j') {
        ADD_ROOT(1, "z");   /* §3.9.11 (ganz+ye=ganje) */
        ADD_ROOT(1, "r");   /* §3.9.5  (ir+ye=ije, passive/specific) */
    }

    /* §3.9.3: ends in "y" (and preceded by another vowel pattern) → underlying ends in "r"
     * This only fires when the 'y' is likely from r+y→y in applicative context.
     * We check: ends in "iy" or "ey" (applicative vowel + r+y→y result). */
    if (end1=='y' && (end2=='i' || end2=='e')) {
        ADD_ROOT(1, "r");   /* §3.9.3 applicative: -ir+ye → -iye (r+y→y) */
    }

    /* §3.9.10: ends in "s" (not "ts") → underlying ends in "t" */
    /* §3.9.8:  ends in "s" preceded by "n" → underlying ends in "nk" */
    if (end1=='s' && end2!='t' && end2!='n') {
        ADD_ROOT(1, "t");   /* §3.9.10 */
    }
    if (end1=='s' && end2=='n') {
        ADD_ROOT(2, "nk");  /* §3.9.8: nk+y→ns, reverse: ns→nk */
    }

    /* §3.9.9: ends in "c" → could be underlying "c" (no change case) */
    /* (Not added as a recovery candidate since it's identity; caller keeps raw) */

#undef ADD_ROOT
    return n;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PUBLIC API 4: kin_ortho_nt9_stem()
 *
 * Handle the Nt.9/10 prefix rules (§2.4 + §3.3.6).
 *
 * Given the raw word starting after the outer 'i' D-vowel of class 9/10,
 * apply the nasal prefix rules to extract the bare stem.
 *
 * The class 9/10 prefix structure is: i + n + stem.
 * At the n+stem boundary several rules apply:
 *
 *   §2.4.1  n + y-initial  → nz + stem   (inzira ← i-n-yira)
 *   §2.4 (exc) n + V-initial → nz + V    (inzugi ← i-n-ugi;  z epenthetic)
 *   §3.3.6  n + V           → ny + V     (inyama ← i-n-nyama? No—see below)
 *
 * NOTE: §3.3.6 (n→ny/_V) applies to VERB prefixes (1sg n- before vowel-stem
 * verbs). For NOUN class 9/10, §2.4 (n+y→nz) and the epenthetic-z rule
 * take precedence.
 *
 * This function writes the bare stem (after removing the 'n' or 'ny' or 'nz'
 * prefix) into `stem_out` and returns the Nt class (9 or 10).
 *
 * Examples:
 *   "nzira"    →  stem "ira"   (surface nz ← n+y: underlying "yira")
 *   "nzugi"    →  stem "ugi"   (surface nz ← n+V: epenthetic z)
 *   "nyama"    →  stem "ama"   (surface ny ← n+y/V: rule §3.1.3 applied)
 *   "mbeba"    →  stem "beba"  (surface m ← n+b: §3.3.3 + §3.1)
 *   "mpapuro"  →  stem "papuro"(surface mp ← n+p: §3.3.2 + §3.1)
 *   "nshuti"   →  stem "shuti" (surface nsh ← n+c: §3.6.2)
 * ═══════════════════════════════════════════════════════════════════════════ */
void kin_ortho_nt9_stem(const char *after_i, char *stem_out, size_t size) {
    size_t wlen = strlen(after_i);
    stem_out[0] = '\0';
    if (!wlen) return;

    const char *st = after_i;

    /* Surface "nsh": came from n + c-initial stem (§3.6.2: nc→nsh).
     * Check this BEFORE the "n" catch-all.                                  */
    if (wlen >= 3 && st[0]=='n' && st[1]=='s' && st[2]=='h') {
        /* Restore the underlying 'c': */
        strncpy(stem_out, "c", size-1);
        strncat(stem_out, st+3, size-1-strlen(stem_out));
        stem_out[size-1] = '\0';
        return;
    }
    /* Surface "nz": n+y→nz (§2.4.1).  All Nt.9/10 "nz" words have a
     * y-initial underlying stem (inzira←i-n-yira, inzoga←i-n-yoga, …).
     * Restore the underlying 'y': strip "nz", prepend 'y'.
     * Example: "nzira" → C="yira";  "nzoga" → C="yoga".                    */
    else if (wlen >= 2 && st[0]=='n' && st[1]=='z') {
        stem_out[0] = 'y';
        strncpy(stem_out + 1, st + 2, size - 2);
        stem_out[size-1] = '\0';
        return;
    }
    /* Surface "ny": n before a genuine 'ny' phoneme (palatal nasal).
     * 'ny' is a distinct Kinyarwanda phoneme (not n+y glide); the RT 'n'
     * merges into the palatal 'ny' of C (n+ny→ny simplification).
     * C = full after_i; RT 'n' has zero surface representation.
     * Example: inyoni = i + n + nyoni → inyoni (n absorbed into ny).  */
    else if (wlen >= 2 && st[0]=='n' && st[1]=='y') {
        /* Do NOT strip: C retains the initial 'ny' phoneme intact. */
        /* st unchanged */
    }
    /* Surface "m": n→m assimilation (§3.3) before bilabial consonant.
     * Two sub-cases must be distinguished:
     *   a) after_i[1] is a consonant  → 'm' is the surface RT (n→m), C starts
     *      at after_i[1].  Example: "mvura" → C="vura", "mbeba" → C="beba".
     *   b) after_i[1] is a vowel      → the 'm' belongs to C (the underlying
     *      RT 'n' merged into the initial 'm' of C via geminate simplification
     *      nn→n after n→m).  Example: "mana" → C="mana" (not "ana").
     *      The RT 'n' has zero surface representation in this case.          */
    else if (wlen >= 2 && st[0]=='m') {
        char c1 = st[1];
        bool next_is_vowel = (c1=='a'||c1=='e'||c1=='i'||c1=='o'||c1=='u');
        if (next_is_vowel) {
            /* Case (b): m belongs to C; RT 'n' fully absorbed — do NOT strip */
            /* st unchanged → C = full after_i string (e.g. "mana") */
        } else {
            /* Case (a): m is surface of RT 'n'; C starts after the 'm' */
            st += 1;
        }
    }
    /* Plain 'n' prefix before consonant-initial stem (no transformation). */
    else if (wlen >= 2 && st[0]=='n') {
        st += 1;
    }

    strncpy(stem_out, st, size-1);
    stem_out[size-1] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PUBLIC API 5: kin_ortho_rule_name()
 *
 * Return a human-readable name for an OrthoViolationType.
 * ═══════════════════════════════════════════════════════════════════════════ */
const char *kin_ortho_rule_name(OrthoViolationType t) {
    switch (t) {
        case ORTHO_OK:              return "OK";
        case ORTHO_VV_HIATUS:       return "VV hiatus (§1.1/§2.1)";
        case ORTHO_NASAL_ASSIM:     return "Nasal assimilation (§3.3)";
        case ORTHO_NASAL_ELISION:   return "Nasal elision (§3.1)";
        case ORTHO_CY_UNFUSED:      return "C+y unfused (§3.9)";
        case ORTHO_STOP_UNDELETED:  return "Stop not deleted (§3.6)";
        case ORTHO_C_NOT_SH:        return "c not → sh before n (§3.6.2)";
        case ORTHO_VOWEL_ASSIM:     return "Vowel assimilation (§1.3)";
        default:                    return "Unknown";
    }
}
