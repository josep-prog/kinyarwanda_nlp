/*
 * morph_dispatch.c
 * Type-dispatch morphological analysis for Kinyarwanda.
 *
 * After POS tagging has identified the word type, this module performs a
 * second, TYPE-SPECIFIC pass that fills Token.morph with the correct
 * morpheme breakdown and the orthographic rules that apply to that type.
 *
 * Design principle (from REB textbooks S3/S4):
 *   Each word type has its own morpheme formula and its own set of
 *   igenamajwi (orthographic rules).  Treating all words the same is
 *   incorrect.  This module dispatches to the right analyser:
 *
 *     Noun  (izina mbonera)   → analyse_noun()   → D + RT + C
 *     Adj   (ntera)           → analyse_adj()    → RS + C
 *     V.conj (inshinga itond.)→ analyse_vconj()  → SP+(TM)+(OM)+root+(EXT)+FV
 *     V.inf  (imbundo)        → analyse_vinf()   → PREF+root+FV
 *
 * Orthographic rules cited (RALC 2017 / REB S4 p.499-501):
 *   §1.1  u→w / _V    (mu+ana → mwana; ku+iga → kwiga)
 *   §1.1  i→y / _V    (ki+atsi → cyatsi; mi+uko → myuko)
 *   §1.1  a→∅ / _V    (ba+ana → bana; a+ari → abari)
 *   §1.1  a+i→e       (ba+inshi → benshi; ka+ibo → kebo)
 *   §3.3  n→m / _b    (n+baga → mbaga)
 *   §3.3  n→m / _f    (n+fwati → mfwati)
 *   §3.3  n→m / _h→mp (n+hinja → mpinja)
 *   §3.3  n→m / _p    (n+papuro → mpapuro)
 *   §3.3  n→m / _v    (n+vura → mvura)
 *   §3.5  r→d / n_    (n+ruru → nduru)
 *   §3.7  k→g / _GR   (ki+haza → gihaza; ka+... → ga...)
 *   §3.7  t→d / _GR   (tu+shaza → dushaza? — rare but cited)
 *   §2.4  n+y→nz      (n+yoga → nzoga; Nt.9/10 only)
 *   §3.6  t→∅ / n_s   (n+tsibo → nsibo)
 *   §3.6.2 c→sh / n_  (n+curo → nshuro)
 */

#include <string.h>
#include <stdio.h>
#include "../include/kinyarwanda.h"

/* ── helpers ──────────────────────────────────────────────────────────────── */

static bool mv(char c) { return c=='a'||c=='e'||c=='i'||c=='o'||c=='u'; }

/* Verbs whose lexicon stem looks consonant-initial but whose TRUE underlying
 * root is vowel-initial.  Historical phonology: *kwubaka = ku + ubak + a, with
 * u+u→u (§6.6) giving modern kubaka.  The surface root slot therefore begins
 * with a consonant (bak) even though the underlying root is vowel-initial (ubak).
 *
 * Map: lexicon stem  →  true underlying root
 * Add new entries alphabetically as more such verbs are confirmed.             */
static const struct { const char *lex_stem; const char *true_root; } uu_stems[] = {
    { "bak",  "ubak"  },   /* kubaka  ← *kwubaka (ku + ubak + a)  §6.6 */
    { NULL,   NULL    }
};

/* Return the true vowel-initial root for a given lexicon stem, or NULL. */
static const char *find_uu_root(const char *stem) {
    for (int i = 0; uu_stems[i].lex_stem; i++)
        if (strcmp(stem, uu_stems[i].lex_stem) == 0)
            return uu_stems[i].true_root;
    return NULL;
}

/* Fill one KinMorpheme slot */
static void set_morph(KinMorpheme *m,
                      const char *label,
                      const char *form,
                      const char *surface,
                      const char *rule)
{
    strncpy(m->label,   label,   KIN_MORPH_LABEL_LEN - 1);
    strncpy(m->form,    form,    KIN_MORPH_FORM_LEN  - 1);
    strncpy(m->surface, surface, KIN_MORPH_FORM_LEN  - 1);
    strncpy(m->rule,    rule,    KIN_MORPH_RULE_LEN  - 1);
    m->label  [KIN_MORPH_LABEL_LEN - 1] = '\0';
    m->form   [KIN_MORPH_FORM_LEN  - 1] = '\0';
    m->surface[KIN_MORPH_FORM_LEN  - 1] = '\0';
    m->rule   [KIN_MORPH_RULE_LEN  - 1] = '\0';
}

/* ── Noun class tables (underlying D and RT for each class 1-16) ──────────── */

static const char *NOUN_D [17] = {
    "?",                          /* index 0 unused */
    "u","a","u","i","i","a","i","i","i","i","u","a","u","u","u","a"
};
static const char *NOUN_RT[17] = {
    "?",
    "mu","ba","mu","mi","ri","ma","ki","bi","n","zi","ru","ka","tu","bu","ku","ha"
};

/* ══════════════════════════════════════════════════════════════════════════
 * analyse_noun()
 *
 * Decomposes an izina mbonera into D + RT + C following REB S4 p.480-501.
 *
 * All 16 noun classes are handled including:
 *   - Nt.9/10 nasal assimilation (§3.3, §3.5, §2.4, §3.6, §3.6.2)
 *   - Vowel-contact rules at RT-C boundary (§1.1)
 *   - Voicing of k/t before voiced-consonant stems (§3.7)
 *
 * tok->stem and tok->noun_class must be set (by kin_tag_token).
 * For Nt.9/10 the stem stored is "surface_after_i" (e.g. "mvura" for imvura);
 * we call kin_ortho_nt9_stem() to recover the underlying C.
 * ══════════════════════════════════════════════════════════════════════════ */
static void analyse_noun(Token *tok)
{
    MorphBreakdown *mb = &tok->morph;
    memset(mb, 0, sizeof(*mb));

    int cls = tok->noun_class;
    if (cls < 1 || cls > 16) return;

    const char *d_under  = NOUN_D [cls];   /* underlying D  */
    const char *rt_under = NOUN_RT[cls];   /* underlying RT */

    const char *word = tok->lower;
    if (!word[0]) return;

    /* ── NT.9 / NT.10: special nasal-prefix analysis ────────────────────── */
    if (cls == 9 || cls == 10) {
        /* Surface structure: i(D) + [nasal cluster](RT surface) + C
         * The stored tok->stem = everything after the outer 'i'.
         * Recover underlying C via kin_ortho_nt9_stem().               */
        const char *after_i = tok->stem;   /* e.g. "mvura", "nka", "nzira" */
        char c_underlying[KIN_MAX_STEM];
        kin_ortho_nt9_stem(after_i, c_underlying, sizeof(c_underlying));

        /* Determine surface RT and rule applied */
        char rt_surface[8] = "n";
        char rule[KIN_MORPH_RULE_LEN] = "";

        if (after_i[0] == 'm') {
            /* n→m assimilation before bilabial (§3.3) */
            strncpy(rt_surface, "m", sizeof(rt_surface)-1);
            char bilabial[4] = {after_i[1], '\0'};
            snprintf(rule, sizeof(rule), "n→m §3.3 (before bilabial '%s')", bilabial);
        } else if (after_i[0]=='n' && after_i[1]=='z') {
            /* n+y→nz (§2.4) or n+V epenthetic z */
            strncpy(rt_surface, "nz", sizeof(rt_surface)-1);
            if (c_underlying[0] && mv(c_underlying[0]))
                snprintf(rule, sizeof(rule), "n+V→nz §2.4 (epenthetic z before vowel)");
            else
                snprintf(rule, sizeof(rule), "n+y→nz §2.4 (before y-initial stem)");
        } else if (after_i[0]=='n' && after_i[1]=='s' && after_i[2]=='h') {
            /* nc→nsh (§3.6.2) */
            strncpy(rt_surface, "nsh", sizeof(rt_surface)-1);
            snprintf(rule, sizeof(rule), "n+c→nsh §3.6.2");
        } else if (after_i[0]=='n' && after_i[1]=='d') {
            /* r→d after n (§3.5): n+ruru → nduru */
            strncpy(rt_surface, "nd", sizeof(rt_surface)-1);
            snprintf(rule, sizeof(rule), "n+r→nd §3.5 (r→d after nasal)");
        } else if (after_i[0]=='n') {
            strncpy(rt_surface, "n", sizeof(rt_surface)-1);
            /* no change to nasal */
        }

        /* Verify: build expected surface from D + rt_surface + c_underlying */
        char expected[KIN_MAX_WORD];
        snprintf(expected, sizeof(expected), "i%s%s", rt_surface, c_underlying);
        mb->verified = (strcmp(expected, word) == 0);

        set_morph(&mb->m[0], "D",  "i",          "i",         "");
        set_morph(&mb->m[1], "RT", "n",           rt_surface,  rule);
        set_morph(&mb->m[2], "C",  c_underlying,  c_underlying,"");
        mb->n = 3;
        return;
    }

    /* ── All other classes: standard D + RT + C analysis ─────────────────── */

    /* Derive C from the surface word by stripping D+RT.
     * We use the surface prefix detected by kin_strip_noun_prefix logic.   */
    const char *c_start = NULL;   /* pointer into word where C begins        */
    char rt_surface[8]  = "";
    char rule_rt   [KIN_MORPH_RULE_LEN] = "";

    /* Clone RT and D for mutable comparison */
    char d_s[4];
    strncpy(d_s, d_under, sizeof(d_s)-1);
    d_s[sizeof(d_s)-1] = '\0';

    switch (cls) {
        /* ── Classes with D=u, RT=mu ─────────────────────────────────────── */
        case 1: case 3:
            if (kin_starts_with(word, "umw") && mv(word[3])) {
                c_start = word + 3;
                strncpy(rt_surface, "mw", sizeof(rt_surface)-1);
                snprintf(rule_rt, sizeof(rule_rt),
                         "u→w §1.1 (mu+'%c' vowel → mw)", word[3]);
            } else if (kin_starts_with(word, "umu")) {
                c_start = word + 3;
                strncpy(rt_surface, "mu", sizeof(rt_surface)-1);
            } else if (kin_starts_with(word, "umw")) {
                /* umw before consonant (unlikely but guard) */
                c_start = word + 3;
                strncpy(rt_surface, "mw", sizeof(rt_surface)-1);
            } else {
                /* bare/dropped-D forms: start from stem stored by tagger */
                strncpy(rt_surface, "mu", sizeof(rt_surface)-1);
                c_start = NULL;  /* fall through to stem-based path */
            }
            break;

        /* ── Class 2: D=a, RT=ba ─────────────────────────────────────────── */
        case 2:
            if (kin_starts_with(word, "ab") && mv(word[2])) {
                /* a+ba+V: 'a' of ba elided, surface "ab"+V */
                c_start = word + 2;
                strncpy(rt_surface, "b", sizeof(rt_surface)-1);
                snprintf(rule_rt, sizeof(rule_rt),
                         "a→∅ §1.1 (ba+'%c' vowel → b, a elides)", word[2]);
            } else if (kin_starts_with(word, "aba")) {
                c_start = word + 3;
                strncpy(rt_surface, "ba", sizeof(rt_surface)-1);
            } else {
                strncpy(rt_surface, "ba", sizeof(rt_surface)-1);
                c_start = NULL;
            }
            break;

        /* ── Class 4: D=i, RT=mi ─────────────────────────────────────────── */
        case 4:
            if (kin_starts_with(word, "imy") && mv(word[3])) {
                c_start = word + 3;
                strncpy(rt_surface, "my", sizeof(rt_surface)-1);
                snprintf(rule_rt, sizeof(rule_rt),
                         "i→y §1.1 (mi+'%c' vowel → my)", word[3]);
            } else if (kin_starts_with(word, "imi")) {
                c_start = word + 3;
                strncpy(rt_surface, "mi", sizeof(rt_surface)-1);
            } else {
                strncpy(rt_surface, "mi", sizeof(rt_surface)-1);
                c_start = NULL;
            }
            break;

        /* ── Class 5: D=i, RT=ri ─────────────────────────────────────────── */
        case 5:
            if (kin_starts_with(word, "iry") && mv(word[3])) {
                c_start = word + 3;
                strncpy(rt_surface, "ry", sizeof(rt_surface)-1);
                snprintf(rule_rt, sizeof(rule_rt),
                         "i→y §1.1 (ri+'%c' vowel → ry)", word[3]);
            } else if (kin_starts_with(word, "iri")) {
                c_start = word + 3;
                strncpy(rt_surface, "ri", sizeof(rt_surface)-1);
            } else if (word[0] == 'i') {
                /* RT 'ri' fully elided (common: ishuri, ibuye) */
                c_start = word + 1;
                strncpy(rt_surface, "∅", sizeof(rt_surface)-1);
                strncpy(rule_rt, "ri→∅ §1.1 (elision before C-initial stem)", sizeof(rule_rt)-1);
            } else {
                strncpy(rt_surface, "ri", sizeof(rt_surface)-1);
                c_start = NULL;
            }
            break;

        /* ── Class 6: D=a, RT=ma ─────────────────────────────────────────── */
        case 6:
            if (kin_starts_with(word, "ama")) {
                c_start = word + 3;
                strncpy(rt_surface, "ma", sizeof(rt_surface)-1);
            } else {
                strncpy(rt_surface, "ma", sizeof(rt_surface)-1);
                c_start = NULL;
            }
            break;

        /* ── Class 7: D=i, RT=ki ─────────────────────────────────────────── */
        case 7:
            if (kin_starts_with(word, "icy") && mv(word[3])) {
                c_start = word + 3;
                strncpy(rt_surface, "cy", sizeof(rt_surface)-1);
                snprintf(rule_rt, sizeof(rule_rt),
                         "i→y §1.1 + k→c §3.9 (ki+'%c' → cy)", word[3]);
            } else if (kin_starts_with(word, "igi")) {
                c_start = word + 3;
                strncpy(rt_surface, "gi", sizeof(rt_surface)-1);
                snprintf(rule_rt, sizeof(rule_rt),
                         "k→g §3.7 (ki before voiced '%c'→gi)", word[3]);
            } else if (kin_starts_with(word, "iki")) {
                c_start = word + 3;
                strncpy(rt_surface, "ki", sizeof(rt_surface)-1);
            } else {
                strncpy(rt_surface, "ki", sizeof(rt_surface)-1);
                c_start = NULL;
            }
            break;

        /* ── Class 8: D=i, RT=bi ─────────────────────────────────────────── */
        case 8:
            if (kin_starts_with(word, "iby") && mv(word[3])) {
                c_start = word + 3;
                strncpy(rt_surface, "by", sizeof(rt_surface)-1);
                snprintf(rule_rt, sizeof(rule_rt),
                         "i→y §1.1 (bi+'%c' vowel → by)", word[3]);
            } else if (kin_starts_with(word, "ibi")) {
                c_start = word + 3;
                strncpy(rt_surface, "bi", sizeof(rt_surface)-1);
            } else {
                strncpy(rt_surface, "bi", sizeof(rt_surface)-1);
                c_start = NULL;
            }
            break;

        /* ── Class 11: D=u, RT=ru ─────────────────────────────────────────── */
        case 11:
            if (kin_starts_with(word, "urw") && mv(word[3])) {
                c_start = word + 3;
                strncpy(rt_surface, "rw", sizeof(rt_surface)-1);
                snprintf(rule_rt, sizeof(rule_rt),
                         "u→w §1.1 (ru+'%c' vowel → rw)", word[3]);
            } else if (kin_starts_with(word, "uru")) {
                c_start = word + 3;
                strncpy(rt_surface, "ru", sizeof(rt_surface)-1);
            } else {
                strncpy(rt_surface, "ru", sizeof(rt_surface)-1);
                c_start = NULL;
            }
            break;

        /* ── Class 12: D=a, RT=ka ─────────────────────────────────────────── */
        case 12:
            if (kin_starts_with(word, "aga")) {
                c_start = word + 3;
                strncpy(rt_surface, "ga", sizeof(rt_surface)-1);
                snprintf(rule_rt, sizeof(rule_rt),
                         "k→g §3.7 (ka before voiced '%c'→ga)", word[3]);
            } else if (kin_starts_with(word, "aka")) {
                c_start = word + 3;
                strncpy(rt_surface, "ka", sizeof(rt_surface)-1);
            } else {
                strncpy(rt_surface, "ka", sizeof(rt_surface)-1);
                c_start = NULL;
            }
            break;

        /* ── Class 13: D=u, RT=tu ─────────────────────────────────────────── */
        case 13:
            if (kin_starts_with(word, "utw") && mv(word[3])) {
                c_start = word + 3;
                strncpy(rt_surface, "tw", sizeof(rt_surface)-1);
                snprintf(rule_rt, sizeof(rule_rt),
                         "u→w §1.1 (tu+'%c' vowel → tw)", word[3]);
            } else if (kin_starts_with(word, "utu")) {
                c_start = word + 3;
                strncpy(rt_surface, "tu", sizeof(rt_surface)-1);
            } else {
                strncpy(rt_surface, "tu", sizeof(rt_surface)-1);
                c_start = NULL;
            }
            break;

        /* ── Class 14: D=u, RT=bu ─────────────────────────────────────────── */
        case 14:
            if (kin_starts_with(word, "ubw") && mv(word[3])) {
                c_start = word + 3;
                strncpy(rt_surface, "bw", sizeof(rt_surface)-1);
                snprintf(rule_rt, sizeof(rule_rt),
                         "u→w §1.1 (bu+'%c' vowel → bw)", word[3]);
            } else if (kin_starts_with(word, "ubu")) {
                c_start = word + 3;
                strncpy(rt_surface, "bu", sizeof(rt_surface)-1);
            } else {
                strncpy(rt_surface, "bu", sizeof(rt_surface)-1);
                c_start = NULL;
            }
            break;

        /* ── Class 15: D=u, RT=ku ─────────────────────────────────────────── */
        case 15:
            if (kin_starts_with(word, "ukw") && mv(word[3])) {
                c_start = word + 3;
                strncpy(rt_surface, "kw", sizeof(rt_surface)-1);
                snprintf(rule_rt, sizeof(rule_rt),
                         "u→w §1.1 (ku+'%c' vowel → kw)", word[3]);
            } else if (kin_starts_with(word, "uku")) {
                c_start = word + 3;
                strncpy(rt_surface, "ku", sizeof(rt_surface)-1);
            } else {
                strncpy(rt_surface, "ku", sizeof(rt_surface)-1);
                c_start = NULL;
            }
            break;

        /* ── Class 16: D=a, RT=ha ─────────────────────────────────────────── */
        case 16:
            if (kin_starts_with(word, "aha")) {
                c_start = word + 3;
                strncpy(rt_surface, "ha", sizeof(rt_surface)-1);
            } else {
                strncpy(rt_surface, "ha", sizeof(rt_surface)-1);
                c_start = NULL;
            }
            break;

        default:
            return;
    }

    /* Determine C: either from surface-word pointer or from stored tok->stem */
    char c_form[KIN_MAX_STEM];
    if (c_start && c_start[0]) {
        strncpy(c_form, c_start, sizeof(c_form) - 1);
        c_form[sizeof(c_form) - 1] = '\0';
    } else if (tok->stem[0]) {
        strncpy(c_form, tok->stem, sizeof(c_form) - 1);
        c_form[sizeof(c_form) - 1] = '\0';
    } else {
        return;  /* cannot determine C */
    }

    if (!c_form[0]) return;

    /* Build underlying string and verify via kin_ortho_gen */
    char underlying[128];
    snprintf(underlying, sizeof(underlying), "%s|%s|%s", d_under, rt_under, c_form);
    char reconstructed[KIN_MAX_WORD];
    kin_ortho_gen(underlying, (cls == 9 || cls == 10), reconstructed, sizeof(reconstructed));
    mb->verified = (strcmp(reconstructed, word) == 0);

    /* Store morphemes */
    set_morph(&mb->m[0], "D",  d_under,  d_under,   "");
    set_morph(&mb->m[1], "RT", rt_under, rt_surface, rule_rt);
    set_morph(&mb->m[2], "C",  c_form,   c_form,     "");
    mb->n = 3;
}

/* ══════════════════════════════════════════════════════════════════════════
 * analyse_adj()
 *
 * Adjective (ntera) structure: RS + C
 * RS (Indangasano) matches the noun class it modifies.
 * Concordance prefixes follow the same phonological rules as RT for nouns.
 * ══════════════════════════════════════════════════════════════════════════ */
static void analyse_adj(Token *tok)
{
    MorphBreakdown *mb = &tok->morph;
    memset(mb, 0, sizeof(*mb));

    int cls = tok->noun_class;
    if (cls < 1 || cls > 16) return;
    if (!tok->stem[0]) return;

    /* Underlying RS (same as noun RT) */
    const char *rs_under = NOUN_RT[cls];

    const char *word = tok->lower;
    const char *c    = tok->stem;   /* Igicumbi – already stored by tagger  */

    /* Determine surface RS and any rule applied */
    char rs_surface[8];
    char rule_rs[KIN_MORPH_RULE_LEN] = "";

    strncpy(rs_surface, rs_under, sizeof(rs_surface)-1);
    rs_surface[sizeof(rs_surface)-1] = '\0';

    /* Check for phonological changes at RS-C boundary */
    if (c[0] && mv(c[0])) {
        size_t rlen = strlen(rs_under);
        char last = rs_under[rlen - 1];
        if (last == 'u') {
            /* u→w (§1.1) */
            char tmp[8];
            strncpy(tmp, rs_under, sizeof(tmp)-1);
            tmp[rlen-1] = 'w';
            tmp[rlen] = '\0';
            strncpy(rs_surface, tmp, sizeof(rs_surface)-1);
            snprintf(rule_rs, sizeof(rule_rs),
                     "u→w §1.1 (%s+'%c' vowel → %s)", rs_under, c[0], rs_surface);
        } else if (last == 'i') {
            /* i→y (§1.1) */
            char tmp[8];
            strncpy(tmp, rs_under, sizeof(tmp)-1);
            if (strcmp(rs_under, "ki") == 0) {
                strncpy(tmp, "cy", sizeof(tmp)-1);
            } else {
                tmp[rlen-1] = 'y';
                tmp[rlen] = '\0';
            }
            strncpy(rs_surface, tmp, sizeof(rs_surface)-1);
            snprintf(rule_rs, sizeof(rule_rs),
                     "i→y §1.1 (%s+'%c' vowel → %s)", rs_under, c[0], rs_surface);
        } else if (last == 'a') {
            /* a+i→e fusion (§1.1) or a→∅ elision */
            if (c[0] == 'i') {
                /* a+i→e */
                char tmp[8];
                strncpy(tmp, rs_under, sizeof(tmp)-1);
                tmp[rlen-1] = 'e';
                tmp[rlen] = '\0';
                strncpy(rs_surface, tmp, sizeof(rs_surface)-1);
                snprintf(rule_rs, sizeof(rule_rs),
                         "a+i→e §1.1 (%s+i fusion → %s)", rs_under, rs_surface);
            }
        }
    }

    /* Verify */
    char expected[KIN_MAX_WORD];
    snprintf(expected, sizeof(expected), "%s%s", rs_surface, c);
    mb->verified = (strcmp(expected, word) == 0);

    set_morph(&mb->m[0], "RS", rs_under,  rs_surface, rule_rs);
    set_morph(&mb->m[1], "C",  c,         c,          "");
    mb->n = 2;
}

/* ══════════════════════════════════════════════════════════════════════════
 * analyse_vconj()
 *
 * Conjugated verb (inshinga itondaguye): SP + (TM) + (OM) + root + (EXT) + FV
 *
 * Phonological rule at SP boundary (§1.1 Iranya ry'impanvu):
 *   SP ends in 'i' + next element vowel-initial → i→y
 *   SP ends in 'u' + next element vowel-initial → u→w
 *   SP ends in 'a' + next element i-initial    → a+i→e (e.g. ba+inshi→benshi)
 * ══════════════════════════════════════════════════════════════════════════ */

/* Subject prefix underlying forms (present tense) per class */
static const char *SP_PRES[17] = {
    "?",
    "a","ba","u","i","ri","ya","ki","bi","i","zi","ru","ka","tu","bu","ku","ha"
};
/* Subject prefix underlying forms (past tense) per class */
static const char *SP_PAST[17] = {
    "?",
    "ya","ba","wa","ya","rya","ya","cya","bya","ya","zya","ru","ka","tu","bu","ku","ha"
};

static bool is_past_tense(VerbTense t) {
    return t == TENSE_PAST_PERF || t == TENSE_PAST_IMPF || t == TENSE_COPULA_PAST;
}

static const char *tense_marker(VerbTense t) {
    switch (t) {
        case TENSE_PRESENT:      return "ra";
        case TENSE_FUTURE:       return "za";
        case TENSE_NARRATIVE:    return "ka";
        case TENSE_OPTATIVE:     return "raka";
        case TENSE_NEG_RELATIVE: return "ta";
        /* Conditional: the 'a' TM is fused into the SP surface via vowel
         * contact (tu+a→twa), so the independent TM slot shows "a" while
         * the SP morpheme shows the fused surface form.                  */
        case TENSE_CONDITIONAL:  return "a";
        default:                 return "";
    }
}

static const char *final_vowel(VerbTense t, const char *word) {
    size_t wlen = word ? strlen(word) : 0;
    switch (t) {
        case TENSE_PAST_PERF:
            return (wlen >= 3 && word[wlen-3]=='t' && word[wlen-2]=='s' && word[wlen-1]=='e')
                   ? "tse" : "ye";
        case TENSE_PAST_IMPF:     return "aga";
        case TENSE_SUBJUNCTIVE:   return "e";
        case TENSE_SUBJUNCTIVE_LOC:
            if (wlen >= 2) {
                if (word[wlen-2]=='h' && word[wlen-1]=='o') return "eho";
                if (word[wlen-2]=='m' && word[wlen-1]=='o') return "emo";
                if (word[wlen-2]=='y' && word[wlen-1]=='o') return "eyo";
            }
            return "e";
        default:                  return "a";
    }
}

static const char *ext_suffix(VerbExtension e) {
    switch (e) {
        case VEXT_PASSIVE:     return "w";
        case VEXT_CAUSATIVE:   return "ish";
        case VEXT_APPLICATIVE: return "ir";
        case VEXT_RECIPROCAL:  return "an";
        case VEXT_STATIVE:     return "ik";
        case VEXT_REVERSIVE:   return "ur";   /* -ur- is the canonical form;
                                                  -uk- variant also maps here */
        default:               return "";
    }
}

static void analyse_vconj(Token *tok)
{
    MorphBreakdown *mb = &tok->morph;
    memset(mb, 0, sizeof(*mb));

    int cls = tok->noun_class;
    if (cls < 0 || cls > 16) return;

    const char *word = tok->lower;
    bool past = is_past_tense(tok->verb_tense);

    /* Underlying SP */
    const char *sp_under = (cls >= 1 && cls <= 16)
                           ? (past ? SP_PAST[cls] : SP_PRES[cls])
                           : "?";
    /* For 1sg/2sg/1pl/2pl (class 0) */
    if (cls == 0) {
        if (tok->verb_tense == TENSE_CONDITIONAL) {
            /* Conditional past-SP absorbs TM 'a' via vowel contact (§1.1):
             * tu+a→twa (u→w),  mu+a→mwa (u→w),  n+a→na,  u+a→wa (u→w)  */
            if      (kin_starts_with(word, "twa")) sp_under = "tu";
            else if (kin_starts_with(word, "mwa")) sp_under = "mu";
            else if (kin_starts_with(word, "na"))  sp_under = "n";
            else if (kin_starts_with(word, "wa"))  sp_under = "u";
            else sp_under = "?";
        } else {
            /* Non-conditional: surface matches underlying */
            if      (kin_starts_with(word, "n"))   sp_under = "n";
            else if (kin_starts_with(word, "tu"))  sp_under = "tu";
            else if (kin_starts_with(word, "u"))   sp_under = "u";
            else if (kin_starts_with(word, "mu"))  sp_under = "mu";
            else sp_under = "?";
        }
    }

    const char *tm  = tense_marker(tok->verb_tense);
    const char *fv  = final_vowel(tok->verb_tense, word);
    const char *root = tok->stem[0] ? tok->stem : "?";
    /* u+u→u fusion: historically vowel-initial roots (e.g. twakubaka ← *twakwubaka).
     * Remap lexicon stem "bak" → true root "ubak" so morpheme display is correct. */
    {
        const char *uu_root = find_uu_root(root);
        if (uu_root) root = uu_root;
    }
    const char *ext  = ext_suffix(tok->verb_ext);
    /* Reversive has two variants: -ur- and -uk-.  ext_suffix() returns the
     * canonical "ur", but we detect the actual surface variant from the word:
     * char immediately before the FV suffix is 'k' (-uk-) or 'r' (-ur-).
     * Past-perf assimilation (uk+tse) hides the 'k', so we keep "ur" as
     * the default when the position is ambiguous.                            */
    static char rev_ext_buf[4];  /* small buffer for "ur" or "uk" */
    if (tok->verb_ext == VEXT_REVERSIVE) {
        size_t fvlen = strlen(fv), wlen = strlen(word);
        if (wlen > fvlen + 1) {
            char elast = word[wlen - fvlen - 1];
            if      (elast == 'k') { strncpy(rev_ext_buf, "uk", 3); ext = rev_ext_buf; }
            else if (elast == 'r') { strncpy(rev_ext_buf, "ur", 3); ext = rev_ext_buf; }
        }
    }
    const char *om   = (tok->obj_class > 0) ? kin_om_str(tok->obj_class) : "";

    /* Determine SP surface: check for phonological change at SP-next boundary */
    const char *next_after_sp = tm[0] ? tm : (om[0] ? om : root);
    char sp_surface[KIN_MORPH_FORM_LEN];
    char sp_rule[KIN_MORPH_RULE_LEN] = "";
    strncpy(sp_surface, sp_under, sizeof(sp_surface)-1);
    sp_surface[sizeof(sp_surface)-1] = '\0';

    if (sp_under[0] && next_after_sp[0]) {
        size_t slen = strlen(sp_under);
        char sp_last = sp_under[slen - 1];
        char next_c  = next_after_sp[0];

        if (sp_last == 'u' && mv(next_c)) {
            sp_surface[slen - 1] = 'w';
            snprintf(sp_rule, sizeof(sp_rule),
                     "u→w §1.1 (SP '%s'+'%c'→'%.*sw')", sp_under, next_c,
                     (int)(slen-1), sp_under);
        } else if (sp_last == 'i' && mv(next_c)) {
            if (slen == 2 && sp_under[0] == 'r' && sp_under[1] == 'i') {
                /* ri → ry */
                strcpy(sp_surface, "ry");
            } else {
                sp_surface[slen - 1] = 'y';
            }
            snprintf(sp_rule, sizeof(sp_rule),
                     "i→y §1.1 (SP '%s'+'%c'→'%s')", sp_under, next_c, sp_surface);
        } else if (sp_last == 'a' && next_c == 'i') {
            /* a+i→e fusion */
            sp_surface[slen - 1] = 'e';
            snprintf(sp_rule, sizeof(sp_rule),
                     "a+i→e §1.1 (SP '%s'+i→'%s')", sp_under, sp_surface);
        }
    }

    /* OM surface (same rule applies at OM-root boundary) */
    char om_surface[KIN_MORPH_FORM_LEN] = "";
    char om_rule[KIN_MORPH_RULE_LEN] = "";
    if (om[0]) {
        strncpy(om_surface, om, sizeof(om_surface)-1);
        size_t olen = strlen(om);
        char om_last = om[olen-1];
        if (root[0] && mv(root[0])) {
            if (om_last == 'u') {
                om_surface[olen-1] = 'w';
                snprintf(om_rule, sizeof(om_rule),
                         "u→w §1.1 (OM '%s'+'%c'→'%.*sw')", om, root[0],
                         (int)(olen-1), om);
            } else if (om_last == 'i') {
                om_surface[olen-1] = 'y';
                snprintf(om_rule, sizeof(om_rule),
                         "i→y §1.1 (OM '%s'+'%c'→'%s')", om, root[0], om_surface);
            }
        } else {
            /* no change */
        }
    }

    /* For CONDITIONAL tense, determine the surface form of the modal particle.
     * The particle is "ku" before voiceless-initial stems and "gu" before
     * voiced-initial stems (k→g §3.7).  When the stem is vowel-initial the
     * particle u→w: ku+V → kw/gw (e.g., twakwubaka but usually twakubaka
     * by secondary elision).  We use the actual text to recover the particle. */
    const char *cond_particle = "";
    char cond_particle_surface[8] = "";
    char cond_particle_rule[KIN_MORPH_RULE_LEN] = "";
    bool cond_tm_elided = false;   /* set when SP ends in 'a' + TM='a' → a+a→a */
    if (tok->verb_tense == TENSE_CONDITIONAL) {
        /* Find the conditional particle by scanning the surface word after the
         * SP+TM ('a') combination.  The SP+TM surfaces as the past-form SP
         * (e.g., twa, ya, wa) so the particle starts right after sp_surface+tm.
         *
         * Special case — a+a→a elision (§1.1):
         * When the SP surface already ends in 'a' (e.g., ya, za, na, bya, cya,
         * rya, zya) and the conditional TM is also 'a', the TM 'a' is phonologically
         * zero in the surface.  Effective SP+TM surface length = len(sp_surface). */
        {
            size_t splen = strlen(sp_surface);
            if (splen > 0 && sp_surface[splen-1] == 'a' && tm[0] == 'a')
                cond_tm_elided = true;
        }
        size_t sp_tm_len = strlen(sp_surface) + (cond_tm_elided ? 0 : strlen(tm));
        const char *after_sptm = (sp_tm_len < strlen(word)) ? word + sp_tm_len : "";
        if (kin_starts_with(after_sptm, "ku") || kin_starts_with(after_sptm, "kw")) {
            cond_particle = "ku";
            strncpy(cond_particle_surface, after_sptm, 2);
            cond_particle_surface[2] = '\0';
            if (cond_particle_surface[1] == 'w')
                snprintf(cond_particle_rule, sizeof(cond_particle_rule),
                         "u→w §1.1 (ku+'%c' vowel → kw)", after_sptm[2]);
        } else if (kin_starts_with(after_sptm, "gu") || kin_starts_with(after_sptm, "gw")) {
            cond_particle = "ku";   /* underlying is always ku */
            strncpy(cond_particle_surface, after_sptm, 2);
            cond_particle_surface[2] = '\0';
            snprintf(cond_particle_rule, sizeof(cond_particle_rule),
                     "k→g §3.7 (ku before voiced '%c' → gu)", after_sptm[2]);
        } else {
            cond_particle = "ku";
            strncpy(cond_particle_surface, "ku", sizeof(cond_particle_surface)-1);
        }

        /* u+u→u fusion at COND+root boundary (§6.6):
         * When the conditional particle surface ends in 'u' and the (possibly
         * remapped) root is vowel-initial (e.g., root="ubak"), the two 'u's
         * merge → the particle contributes only its consonant(s) to the surface.
         * Example: twakubaka = tw+a+[ku+ubak=kubak]+a.
         * We shorten cond_particle_surface by one char and record the rule.    */
        if (cond_particle[0] && root[0] == 'u') {
            size_t cplen = strlen(cond_particle_surface);
            if (cplen > 0 && cond_particle_surface[cplen - 1] == 'u') {
                cond_particle_surface[cplen - 1] = '\0';  /* drop fused 'u' */
                snprintf(cond_particle_rule, sizeof(cond_particle_rule),
                         "u+u\342\206\222u \302\2476.6 (COND 'ku'+'%s'\342\206\222'k%s', from *kw%sa)",
                         root, root, root);
            }
        }
    }

    /* Build expected surface for verification */
    char built[KIN_MAX_WORD];
    if (tok->verb_tense == TENSE_CONDITIONAL) {
        /* When TM 'a' is elided (a+a→a), omit it from the built surface */
        snprintf(built, sizeof(built), "%s%s%s%s%s%s",
                 sp_surface, cond_tm_elided ? "" : tm,
                 cond_particle_surface[0] ? cond_particle_surface : "ku",
                 root, ext, fv);
    } else {
        snprintf(built, sizeof(built), "%s%s%s%s%s%s",
                 sp_surface, tm,
                 om[0] ? om_surface : "",
                 root, ext, fv);
    }
    mb->verified = (strcmp(built, word) == 0);

    /* Store morphemes */
    int n = 0;
    set_morph(&mb->m[n++], "SP", sp_under, sp_surface, sp_rule);
    if (tm[0])
        set_morph(&mb->m[n++], "TM", tm,
                  cond_tm_elided ? "" : tm,   /* surface="" when a+a→a elision */
                  tok->verb_tense == TENSE_CONDITIONAL
                      ? (cond_tm_elided
                         ? "a+a\342\206\222a \302\2471.1 (TM 'a' elided after SP ending in 'a')"
                         : "Inziganyo TM (conditional marker; fused into SP by \302\2471.1)")
                      : "");
    /* Conditional particle (ku/gu) replaces OM slot for CONDITIONAL tense */
    if (tok->verb_tense == TENSE_CONDITIONAL && cond_particle[0]) {
        set_morph(&mb->m[n++], "COND", cond_particle,
                  cond_particle_surface[0] ? cond_particle_surface : cond_particle,
                  cond_particle_rule[0] ? cond_particle_rule
                                        : "Ikivugana cy'inziganyo (conditional modal particle)");
    } else if (om[0]) {
        set_morph(&mb->m[n++], "OM", om, om_surface[0] ? om_surface : om, om_rule);
    }
    set_morph(&mb->m[n++], "root", root, root, "");
    if (ext[0])
        set_morph(&mb->m[n++], "EXT", ext, ext, "");
    set_morph(&mb->m[n++], "FV", fv, fv, "");
    mb->n = n;
}

/* ══════════════════════════════════════════════════════════════════════════
 * detect_ext_in_stem()
 *
 * Detect a derivational extension (itondaguranshinga) within an infinitive
 * stem (i.e. the segment between the infinitive prefix and the final vowel).
 *
 * Priority (longest suffix wins to prevent partial matches):
 *   Causative:   -ish- / -esh-  (3 chars)
 *   Applicative: -ir-  / -er-   (2 chars)
 *   Reciprocal:  -an-            (2 chars)
 *   Passive:     -w              (1 char)
 *
 * Safety: the bare root left behind must be ≥ 2 chars.  This prevents
 * false splits when the whole stem is short (e.g. "gir" in "kugira").
 *
 * Returns VEXT_NONE when no extension is found.
 * On success bare_root[] holds the root and ext_str[] the extension.
 * ══════════════════════════════════════════════════════════════════════════ */
static VerbExtension detect_ext_in_stem(const char *stem,
                                         char *bare_root,
                                         char *ext_str, size_t ext_sz)
{
    size_t len = strlen(stem);
    bare_root[0] = '\0';
    ext_str[0]   = '\0';

    /* Causative: stem ends in "ish" or "esh" */
    if (len > 3) {
        const char *s = stem + len - 3;
        if (strcmp(s, "ish") == 0 || strcmp(s, "esh") == 0) {
            size_t rlen = len - 3;
            if (rlen >= 2) {
                strncpy(bare_root, stem, rlen); bare_root[rlen] = '\0';
                strncpy(ext_str,   s,    ext_sz - 1); ext_str[ext_sz-1] = '\0';
                return VEXT_CAUSATIVE;
            }
        }
    }

    /* Applicative: stem ends in "ir" or "er" */
    if (len > 2) {
        const char *s = stem + len - 2;
        if (strcmp(s, "ir") == 0 || strcmp(s, "er") == 0) {
            size_t rlen = len - 2;
            if (rlen >= 2) {
                strncpy(bare_root, stem, rlen); bare_root[rlen] = '\0';
                strncpy(ext_str,   s,    ext_sz - 1); ext_str[ext_sz-1] = '\0';
                return VEXT_APPLICATIVE;
            }
        }
    }

    /* Stative: stem ends in "ik" (Ngirika: guhingika → hing+ik, gufatika → fat+ik)
     * Checked before reversive -uk because both share a final 'k'. */
    if (len > 3) {
        const char *s = stem + len - 2;
        if (strcmp(s, "ik") == 0) {
            size_t rlen = len - 2;
            if (rlen >= 2) {
                strncpy(bare_root, stem, rlen); bare_root[rlen] = '\0';
                strncpy(ext_str,   s,    ext_sz - 1); ext_str[ext_sz-1] = '\0';
                return VEXT_STATIVE;
            }
        }
    }

    /* Reversive -ur (Ngirura: gufungura → fung+ur, guhindura → hind+ur) */
    if (len > 3) {
        const char *s = stem + len - 2;
        if (strcmp(s, "ur") == 0) {
            size_t rlen = len - 2;
            if (rlen >= 2) {
                strncpy(bare_root, stem, rlen); bare_root[rlen] = '\0';
                strncpy(ext_str,   s,    ext_sz - 1); ext_str[ext_sz-1] = '\0';
                return VEXT_REVERSIVE;
            }
        }
    }

    /* Reversive -uk (Ngiruka: gufunguka → fung+uk, guhinduka → hind+uk) */
    if (len > 3) {
        const char *s = stem + len - 2;
        if (strcmp(s, "uk") == 0) {
            size_t rlen = len - 2;
            if (rlen >= 2) {
                strncpy(bare_root, stem, rlen); bare_root[rlen] = '\0';
                strncpy(ext_str,   s,    ext_sz - 1); ext_str[ext_sz-1] = '\0';
                return VEXT_REVERSIVE;
            }
        }
    }

    /* Reciprocal: stem ends in "an" */
    if (len > 2) {
        const char *s = stem + len - 2;
        if (strcmp(s, "an") == 0) {
            size_t rlen = len - 2;
            if (rlen >= 2) {
                strncpy(bare_root, stem, rlen); bare_root[rlen] = '\0';
                strncpy(ext_str,   s,    ext_sz - 1); ext_str[ext_sz-1] = '\0';
                return VEXT_RECIPROCAL;
            }
        }
    }

    /* Passive: stem ends in "w" */
    if (len > 2 && stem[len-1] == 'w') {
        size_t rlen = len - 1;
        if (rlen >= 2) {
            strncpy(bare_root, stem, rlen); bare_root[rlen] = '\0';
            ext_str[0] = 'w'; ext_str[1] = '\0';
            return VEXT_PASSIVE;
        }
    }

    return VEXT_NONE;
}

/* ══════════════════════════════════════════════════════════════════════════
 * analyse_vinf()
 *
 * Verb infinitive (imbundo): PREF + root + (EXT) + FV
 * PREF = ku/gu before consonant-initial stems.
 * PREF = kw/gw before vowel-initial stems (u→w, §1.1).
 *
 * After extracting PREF and FV, detect_ext_in_stem() peels off any
 * derivational extension from the remaining stem so that the true bare
 * root is isolated.  Example:
 *   kugurisha → ku | gur | ish | a   (causative of kugura)
 *   gukorera  → gu | kor | er  | a   (applicative of gukora)
 *   gukorana  → gu | kor | an  | a   (reciprocal of gukora)
 *   gukorwa   → gu | kor | w   | a   (passive of gukora)
 * ══════════════════════════════════════════════════════════════════════════ */
static void analyse_vinf(Token *tok)
{
    MorphBreakdown *mb = &tok->morph;
    memset(mb, 0, sizeof(*mb));

    const char *word = tok->lower;
    const char *pref = tok->detected_prefix[0] ? tok->detected_prefix : "ku";
    const char *root = tok->stem[0] ? tok->stem : "?";

    /* Determine underlying prefix and any rule */
    char pref_under[8];
    char pref_rule[KIN_MORPH_RULE_LEN] = "";
    strncpy(pref_under, pref, sizeof(pref_under)-1);
    pref_under[sizeof(pref_under)-1] = '\0';

    if (strcmp(pref, "kw") == 0) {
        strncpy(pref_under, "ku", sizeof(pref_under)-1);
        snprintf(pref_rule, sizeof(pref_rule),
                 "u\342\206\222w \302\2471.1 (ku+'%c' vowel \342\206\222 kw)", root[0]);
    } else if (strcmp(pref, "gw") == 0) {
        strncpy(pref_under, "gu", sizeof(pref_under)-1);
        snprintf(pref_rule, sizeof(pref_rule),
                 "u\342\206\222w \302\2471.1 (gu+'%c' vowel \342\206\222 gw)", root[0]);
    }

    /* u+u→u fusion: historically vowel-initial roots (e.g. kubaka ← *kwubaka).
     * The lexicon stores "bak" but the true root is "ubak".  The junction
     * ku+ubak collapses to kubak by §6.6 (u+u→u), so the surface prefix
     * loses its final 'u' (ku → k before the root's leading 'u').              */
    const char *uu_root = find_uu_root(root);
    bool uu_fused = (uu_root != NULL);
    if (uu_fused) {
        root = uu_root;
        /* Build rule text showing the historical origin */
        snprintf(pref_rule, sizeof(pref_rule),
                 "u+u\342\206\222u \302\2476.6 (ku+'%s'+a \342\206\222 '%s', from *kw%s+a)",
                 root, word, root);
    }

    /* Final vowel from surface */
    const char *fv = "a";
    size_t wlen = strlen(word);
    if (wlen >= 3 && word[wlen-3]=='t' && word[wlen-2]=='s' && word[wlen-1]=='e')
        fv = "tse";
    else if (wlen >= 2 && word[wlen-2]=='y' && word[wlen-1]=='e')
        fv = "ye";
    else if (wlen >= 2 && word[wlen-2]=='w' && word[wlen-1]=='e')
        fv = "we";
    else if (wlen >= 1 && word[wlen-1]=='e'
             && (wlen < 2 || (word[wlen-2]!='y' && word[wlen-2]!='w')))
        fv = "e";

    /* Try to detect a derivational extension within the stem.
     * Exception: skip detection for vowel-initial roots (kw-/gw- prefix verbs,
     * and uu_fused verbs like kubaka→ubak).  The initial vowel is PART of the
     * root, not a suffix boundary — splitting e.g. "iruk" as "ir+uk(reversive)"
     * is a false positive.  Derived forms (kwirukana etc.) are separate entries. */
    char bare_root[KIN_MAX_STEM]      = "";
    char ext_str  [KIN_MORPH_FORM_LEN] = "";
    VerbExtension inf_ext = VEXT_NONE;
    if (!mv(root[0])) {
        inf_ext = detect_ext_in_stem(root, bare_root, ext_str, sizeof(ext_str));
    }

    /* Verify against the surface word.
     * For u+u→u fused verbs: drop the prefix's final 'u' before concatenating
     * (ku + ubak + a → k+ubak+a = kubaka).                                     */
    char built[KIN_MAX_WORD];
    if (uu_fused) {
        size_t plen = strlen(pref);
        snprintf(built, sizeof(built), "%.*s%s%s", (int)(plen - 1), pref, root, fv);
    } else {
        snprintf(built, sizeof(built), "%s%s%s", pref, root, fv);
    }
    mb->verified = (strcmp(built, word) == 0);

    /* PREF morpheme surface: for u+u→u, the prefix contributes only its
     * consonant(s) to the surface (the final 'u' fuses with the root's 'u').   */
    char pref_surf[8];
    if (uu_fused) {
        size_t pulen = strlen(pref_under);
        snprintf(pref_surf, sizeof(pref_surf), "%.*s", (int)(pulen - 1), pref_under);
    } else {
        strncpy(pref_surf, pref, sizeof(pref_surf) - 1);
        pref_surf[sizeof(pref_surf) - 1] = '\0';
    }

    /* Store morphemes */
    int n = 0;
    set_morph(&mb->m[n++], "PREF", pref_under, pref_surf, pref_rule);

    if (inf_ext != VEXT_NONE) {
        /* Split: bare_root + extension */
        set_morph(&mb->m[n++], "root", bare_root, bare_root, "");
        set_morph(&mb->m[n++], "EXT",  ext_str,   ext_str,   kin_verb_ext_name(inf_ext));
    } else {
        set_morph(&mb->m[n++], "root", root, root, "");
    }

    set_morph(&mb->m[n++], "FV", fv, fv, "");
    mb->n = n;
}

/* ══════════════════════════════════════════════════════════════════════════
 * kin_morpheme_analyze()   — public dispatcher
 *
 * Called after kin_tag_token() or kin_tag_sentence().
 * Dispatches to the appropriate type-specific analyser and fills tok->morph.
 * ══════════════════════════════════════════════════════════════════════════ */
void kin_morpheme_analyze(Token *tok)
{
    memset(&tok->morph, 0, sizeof(tok->morph));

    switch (tok->pos) {
        case POS_NOUN:
        case POS_RELATIVE_NOUN:
        case POS_COMPOUND_ADJ:
            analyse_noun(tok);
            break;

        case POS_ADJECTIVE:
            analyse_adj(tok);
            break;

        case POS_VERB_CONJ:
            analyse_vconj(tok);
            break;

        case POS_VERB_INF:
            analyse_vinf(tok);
            break;

        default:
            /* Invariables, pronouns, foreign words: no morpheme breakdown */
            break;
    }
}
