/*
 * morph_dispatch.c  —  Type-dispatch morpheme analysis for Kinyarwanda.
 *
 * ROLE: After pos_tagger.c assigns each token to its tree (word type),
 * this module fills Token.morph with the correct morpheme breakdown.
 * Each tree has its own formula and phonological rules.
 *
 * Dispatch table (kin_morpheme_analyze, bottom of file):
 *   POS_NOUN / POS_RELATIVE_NOUN / POS_COMPOUND_ADJ
 *             → analyse_noun()   → Tree 1: D + RT + C
 *   POS_ADJECTIVE
 *             → analyse_adj()    → Tree 2: RS + C
 *   POS_VERB_CONJ
 *             → analyse_vconj()  → Tree 3b: SP + (TM) + (OM) + C + (EXT) + FV
 *   POS_VERB_INF
 *             → analyse_vinf()   → Tree 3a: PREF + C + (EXT) + FV
 *   All others (pronouns, invariables, foreign): no morpheme breakdown.
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
 * TREE 1 — IZINA MBONERA (Noun)   D + RT + C
 * Also handles: POS_RELATIVE_NOUN (izina ntera) and POS_COMPOUND_ADJ (igisantera)
 * because both share the same D+RT+C morpheme structure.
 *
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

        /* Surface C and rule for C morpheme.  For most Nt.9 words, surface C
         * equals underlying C.  Exception: nz case (n+y→nz §2.4.1) where
         * underlying C starts with 'y' but surface C starts with 'z'.       */
        char c_surface[KIN_MAX_STEM];
        strncpy(c_surface, c_underlying, sizeof(c_surface)-1);
        c_surface[sizeof(c_surface)-1] = '\0';
        char c_rule[KIN_MORPH_RULE_LEN] = "";

        /* Determine surface RT and rule applied */
        char rt_surface[8] = "n";
        char rule[KIN_MORPH_RULE_LEN] = "";

        if (after_i[0] == 'm') {
            /* n→m assimilation (§3.3) — two sub-cases:
             *
             *   (a) after_i[1] is consonant: "mvura", "mbeba"
             *       'm' is the surface of RT 'n'.  C starts at after_i[1].
             *       RT surface = "m".
             *
             *   (b) after_i[1] is vowel: "mana"
             *       Underlying: n(RT) + mana(C).  n→m before bilabial 'm',
             *       then geminate mm→m.  The resulting 'm' belongs to C.
             *       RT 'n' has zero surface representation.
             *       RT surface = "∅",  C = full after_i.                    */
            if (mv(after_i[1])) {
                /* Case (b): n→∅ /_m — RT 'n' elides before geminate m.
                 * §3.3 (n→m before bilabial) + §3.1 geminate elision (mm→m).
                 * Net effect: RT 'n' has zero surface representation.         */
                strncpy(rt_surface, "\xe2\x88\x85", sizeof(rt_surface)-1); /* UTF-8 ∅ */
                snprintf(rule, sizeof(rule),
                         "n→∅ /_m §3.3+§3.1 (n→m→mm→m; RT 'n' elided before geminate)");
                /* c_underlying is the full after_i — set by kin_ortho_nt9_stem */
            } else {
                /* Case (a): n→m §3.3 (bilabial assimilation); RT surfaces as 'm' */
                strncpy(rt_surface, "m", sizeof(rt_surface)-1);
                snprintf(rule, sizeof(rule),
                         "n→m §3.3 (before bilabial '%c')", after_i[1]);
            }
        } else if (after_i[0]=='n' && after_i[1]=='y') {
            /* n→∅ /_ny — RT 'n' elides before the palatal phoneme 'ny'.
             * §3.1 nasal elision: n+ny→ny (geminate simplification).
             * 'ny' is a distinct phoneme; RT 'n' has zero surface form.      */
            strncpy(rt_surface, "\xe2\x88\x85", sizeof(rt_surface)-1); /* UTF-8 ∅ */
            snprintf(rule, sizeof(rule),
                     "n→∅ /_ny §3.1 (RT 'n' elided before palatal phoneme ny)");
        } else if (after_i[0]=='n' && after_i[1]=='z') {
            /* n+y→nz §2.4.1: underlying C starts with 'y'; kin_ortho_nt9_stem
             * has already restored it.  Build surface C by replacing 'y'→'z'. */
            strncpy(rt_surface, "n", sizeof(rt_surface)-1);
            snprintf(rule, sizeof(rule), "n+y→nz §2.4.1 (underlying C starts with 'y')");
            c_surface[0] = 'z';
            strncpy(c_surface + 1, c_underlying + 1, sizeof(c_surface) - 2);
            c_surface[sizeof(c_surface)-1] = '\0';
            strncpy(c_rule, "y→z §2.4.1 (n+y→nz before nasal n)", sizeof(c_rule)-1);
        } else if (after_i[0]=='n' && after_i[1]=='s' && after_i[2]=='h') {
            /* c→sh /_n §3.6.2: underlying c becomes sh after nasal n */
            strncpy(rt_surface, "nsh", sizeof(rt_surface)-1);
            snprintf(rule, sizeof(rule),
                     "c→sh /_n §3.6.2 (n+c→nsh)");
        } else if (after_i[0]=='n' && after_i[1]=='d') {
            /* r→d /_n §3.5: underlying r becomes d after nasal n */
            strncpy(rt_surface, "nd", sizeof(rt_surface)-1);
            snprintf(rule, sizeof(rule),
                     "r→d /_n §3.5 (n+r→nd)");
        } else if (after_i[0]=='n') {
            strncpy(rt_surface, "n", sizeof(rt_surface)-1);
            /* plain nasal n: no rule applied */
        }

        /* Dynamic verification: apply kin_ortho_gen to the underlying morpheme
         * string "i|n|{C_underlying}" with noun_class_9=true.
         * This exercises all §2.4 / §3.x rules through the same engine used
         * for all other noun classes — consistent with classes 1-8, 11-16.   */
        char morph_str[KIN_MAX_WORD];
        snprintf(morph_str, sizeof(morph_str), "i|n|%s", c_underlying);
        char reconstructed_nt9[KIN_MAX_WORD];
        kin_ortho_gen(morph_str, true, reconstructed_nt9, sizeof(reconstructed_nt9));
        mb->verified = (strcmp(reconstructed_nt9, word) == 0);

        set_morph(&mb->m[0], "D",  "i",          "i",         "");
        set_morph(&mb->m[1], "RT", "n",           rt_surface,  rule);
        set_morph(&mb->m[2], "C",  c_underlying,  c_surface,   c_rule);
        mb->n = 3;
        return;
    }

    /* ── All other classes: standard D + RT + C analysis ─────────────────── */

    /* Derive C from the surface word by stripping D+RT.
     * We use the surface prefix detected by kin_strip_noun_prefix logic.   */
    const char *c_start = NULL;   /* pointer into word where C begins        */
    char rt_surface[8]  = "";
    char rule_rt   [KIN_MORPH_RULE_LEN] = "";
    bool d_elided = false;         /* true when D 'u' is dropped after ku/mu  */

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
            } else if (kin_starts_with(word, "mw") && mv(word[2])) {
                /* Bare RT, D 'u' elided after locative ku/mu; vowel-initial C.
                 * e.g. "ku mwana" → "mwana" = mw(mu+V) + ana, D=∅ dropped.  */
                c_start = word + 2;
                strncpy(rt_surface, "mw", sizeof(rt_surface)-1);
                snprintf(rule_rt, sizeof(rule_rt),
                         "u→w §1.1 (mu+'%c' vowel → mw)", word[2]);
                d_elided = true;
            } else if (kin_starts_with(word, "mu")) {
                /* Bare RT, D 'u' elided after locative ku/mu; consonant-initial C.
                 * e.g. "ku munsi" → "munsi" = mu + nsi, D=∅ dropped.
                 * §locative: D elides when ku/mu provides the locative function. */
                c_start = word + 2;
                strncpy(rt_surface, "mu", sizeof(rt_surface)-1);
                /* Set rule_rt as signal to use surface-path reconstruction
                 * (rule text only printed when rt form≠surface, which it is not
                 * here — "mu"→"mu" — so this won't appear in display output).  */
                snprintf(rule_rt, sizeof(rule_rt),
                         "u\xe2\x86\x92\xe2\x88\x85 (D elided after locative ku/mu)");
                d_elided = true;
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
            } else if (kin_starts_with(word, "am") && mv(word[2])) {
                /* a→∅ §1.1 before vowel-initial stem: ma+'o'→m              *
                 * e.g. amoko = a + m + oko  (ubwoko/amoko pair, C=-oko-)    */
                c_start = word + 2;
                strncpy(rt_surface, "m", sizeof(rt_surface)-1);
                snprintf(rule_rt, sizeof(rule_rt),
                         "a\342\206\222\342\210\205 \302\2471.1 (ma+'%c' vowel \342\206\222 m)", word[2]);
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
                         "k→g §3.7 (RT 'ki'→'gi'; Itanisha GR)");
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
                         "k→g §3.7 (RT 'ka'→'ga'; Itanisha GR)");
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

    /* Determine C: primary method = plurality table lookup (kin_lookup_igicumbi).
     * This implements the user's stated method: C is the shared suffix between
     * singular and plural forms, not simply "whatever remains after stripping D+RT".
     * Fall back to the surface-word pointer, then to tok->stem.              */
    char c_form[KIN_MAX_STEM];
    {
        char plural_c[KIN_MAX_STEM];
        int  plural_cls = 0;
        if (kin_lookup_igicumbi(word, plural_c, &plural_cls) && plural_c[0]) {
            strncpy(c_form, plural_c, sizeof(c_form) - 1);
            c_form[sizeof(c_form) - 1] = '\0';
        } else if (c_start && c_start[0]) {
            strncpy(c_form, c_start, sizeof(c_form) - 1);
            c_form[sizeof(c_form) - 1] = '\0';
        } else if (tok->stem[0]) {
            strncpy(c_form, tok->stem, sizeof(c_form) - 1);
            c_form[sizeof(c_form) - 1] = '\0';
        } else {
            return;  /* cannot determine C */
        }
    }

    if (!c_form[0]) return;

    /* Verify the reconstruction matches the surface word.
     * When a phonological rule already fired at the RT boundary (rule_rt set),
     * verify directly via surface-piece concatenation — kin_ortho_gen would
     * re-apply an incorrect rule to the underlying form (e.g. for Nt.6 "a→∅"
     * the generator wrongly turns "ma|o" into "myo" instead of "mo").
     * For no-rule cases, use kin_ortho_gen which handles nasal assimilation etc. */
    /* When D is elided (bare noun after locative ku/mu), the reconstruction
     * uses "" for D so the surface form is correct; but the display shows "∅".  */
    const char *d_recon   = d_elided ? ""                    : d_under;
    const char *d_display = d_elided ? "\xe2\x88\x85"        : d_under;  /* ∅ */

    char reconstructed[KIN_MAX_WORD];
    if (rule_rt[0]) {
        /* Surface: d_recon + rt_surface + c_form — direct check */
        snprintf(reconstructed, sizeof(reconstructed), "%s%s%s",
                 d_recon, rt_surface, c_form);
    } else {
        char underlying[128];
        snprintf(underlying, sizeof(underlying), "%s|%s|%s", d_under, rt_under, c_form);
        kin_ortho_gen(underlying, (cls == 9 || cls == 10), reconstructed, sizeof(reconstructed));
    }
    mb->verified = (strcmp(reconstructed, word) == 0);

    /* Store morphemes */
    set_morph(&mb->m[0], "D",  d_display, d_display, "");
    set_morph(&mb->m[1], "RT", rt_under,  rt_surface, rule_rt);
    set_morph(&mb->m[2], "C",  c_form,    c_form,     "");
    mb->n = 3;
}

/* ══════════════════════════════════════════════════════════════════════════
 * TREE 2 — NTERA (Adjective)   RS + C
 *
 * analyse_adj()
 *
 * Adjective (ntera) structure: RS + C
 * RS (Indangasano) matches the noun class it modifies.
 * Concordance prefixes follow the same phonological rules as RT for nouns.
 * Note: POS_COMPOUND_ADJ (igisantera) uses analyse_noun() — it has D+RT+C.
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

    /* ── RS surface rules ─────────────────────────────────────────────────────
     *
     * Class 9 (RS=n): nasal assimilation rules mirror those for Nt.9 nouns.
     *   The underlying RS is always 'n'; surface changes based on C's first consonant:
     *   n + bilabial (b,p,v,f,h) → m      §3.3   e.g. n+bi → mbi
     *   n + r                    → nd     §3.5   e.g. n+re → ndere
     *   n + vowel / y            → nz     §2.4   e.g. n+iza → nziza
     *   n + n (C starts with n)  → n      (geminate mm→n: n+nini → nini, RS stays n)
     *   n + other consonant      → n      (unchanged)
     *
     * Class 12 (RS=ka): k→g before voiced consonant-initial C (§3.7).
     *
     * All other classes: standard vowel-contact rules (u→w, i→y, a+i→e).  */

    if (cls == 9) {
        /* Nasal assimilation: determine surface RS from C's initial consonant */
        if (c[0] != '\0') {
            static const char BILABIALS[] = "bpvfh";
            bool is_bilabial = false;
            for (int b = 0; BILABIALS[b]; b++)
                if (c[0] == BILABIALS[b]) { is_bilabial = true; break; }

            if (is_bilabial) {
                strncpy(rs_surface, "m", sizeof(rs_surface)-1);
                snprintf(rule_rs, sizeof(rule_rs),
                         "n→m §3.3 (RS 'n' before bilabial '%c')", c[0]);
            } else if (c[0] == 'r') {
                strncpy(rs_surface, "nd", sizeof(rs_surface)-1);
                snprintf(rule_rs, sizeof(rule_rs),
                         "r→d /_n §3.5 (n+r→nd; RS 'n'+r)");
            } else if (mv(c[0]) || c[0] == 'y') {
                strncpy(rs_surface, "nz", sizeof(rs_surface)-1);
                snprintf(rule_rs, sizeof(rule_rs),
                         "n→nz /_V §2.4 (epenthetic z; RS 'n' before vowel/y)");
            } else if (c[0] == 'n') {
                /* n→∅ /_n (geminate simplification nn→n): RS 'n' elides */
                strncpy(rs_surface, "n", sizeof(rs_surface)-1);
                snprintf(rule_rs, sizeof(rule_rs),
                         "n→∅ /_n §3.1 (geminate nn→n; RS 'n' elided)");
            }
            /* else: RS stays "n" unchanged (before k, g, z, j, t, s, sh, etc.) */
        }
    } else if (cls == 12 && c[0] != '\0') {
        /* Class 12 RS=ka: k→g before ingombajwi z'indagi GR (§3.7).
         * 'h' included: voiced glottal fricative [ɦ] in Kinyarwanda.          */
        static const char VOICED[] = "bdghjmnrvwyz";
        for (int v = 0; VOICED[v]; v++) {
            if (c[0] == VOICED[v]) {
                strncpy(rs_surface, "ga", sizeof(rs_surface)-1);
                snprintf(rule_rs, sizeof(rule_rs),
                         "k→g §3.7 (GR: RS 'ka'→'ga' before '%c')", c[0]);
                break;
            }
        }
    } else if (c[0] && mv(c[0])) {
        /* Standard vowel-contact rules for all other classes */
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
            /* a+i→e fusion (§1.1) */
            if (c[0] == 'i') {
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
            /* Epenthetic 'i' before 'ye' (consonant-final root + past perf):
             * -ir + ye → -iriye (e.g. zikwiriye = zi+kwir+iye)              */
            if (wlen >= 4 && word[wlen-3]=='i' && word[wlen-2]=='y' && word[wlen-1]=='e')
                return "iye";
            return (wlen >= 3 && word[wlen-3]=='t' && word[wlen-2]=='s' && word[wlen-1]=='e')
                   ? "tse" : "ye";
        case TENSE_PAST_IMPF:     return "aga";
        case TENSE_SUBJUNCTIVE:   return "e";
        case TENSE_STATIVE_POSS:  return "e";  /* bifite: SP+fit+e */
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
        case VEXT_CAUSATIVE_Y: return "y";    /* surface: r+y→z; citation: -y- */
        default:               return "";
    }
}

/* Forward declaration: detect_ext_in_stem is defined after analyse_vconj
 * but is also needed inside it for conjugated verbs whose stem contains
 * an applicative or other extension (e.g. bivire → stem="vir" = va+ir).  */
static VerbExtension detect_ext_in_stem(const char *stem,
                                         char *bare_root,
                                         char *ext_str, size_t ext_sz);

/* ══════════════════════════════════════════════════════════════════════════
 * TREE 3b — INSHINGA ITONDAGUYE (Conjugated Verb)
 *   Formula:  SP + (TM) + (OM) + C + (EXT) + FV
 *
 * analyse_vconj()
 * ══════════════════════════════════════════════════════════════════════════ */
static void analyse_vconj(Token *tok)
{
    MorphBreakdown *mb = &tok->morph;
    memset(mb, 0, sizeof(*mb));

    int cls = tok->noun_class;
    if (cls < 0 || cls > 16) return;

    const char *word = tok->lower;
    bool past = is_past_tense(tok->verb_tense);

    /* eff_word: effective SP start, stripping negation prefix if present.
     * neg_pfx:  the negation morpheme surface ("nt" or "si") or "".        */
    const char *eff_word = word;
    const char *neg_pfx  = "";
    if (tok->is_negative) {
        if (kin_starts_with(word, "nt") && strlen(word) > 2)
            { eff_word = word + 2; neg_pfx = "nt"; }
        else if (kin_starts_with(word, "si") && strlen(word) > 2)
            { eff_word = word + 2; neg_pfx = "si"; }
    }

    /* Underlying SP */
    const char *sp_under = (cls >= 1 && cls <= 16)
                           ? (past ? SP_PAST[cls] : SP_PRES[cls])
                           : "?";
    /* Defensive SP surface check: if the SP form derived from tok->noun_class
     * doesn't match what the word actually starts with, scan all 16 classes
     * to find one whose past SP (or present SP) matches the surface.
     * This guards against kin_resolve_sp_ambiguity wrongly reclassifying the
     * verb class — e.g. yakoze wrongly assigned cls=3 (SP "wa") when the word
     * starts with "ya" (cls 1/4/6/9). */
    if (past && cls >= 1 && cls <= 16
            && !kin_starts_with(eff_word, SP_PAST[cls])) {
        /* Try present-form first (e.g. zikwiriye: SP_PAST[10]="zya", SP_PRES[10]="zi") */
        if (kin_starts_with(eff_word, SP_PRES[cls])) {
            sp_under = SP_PRES[cls];
        } else {
            /* Word surface doesn't match cls at all — find the class whose
             * past SP actually appears at the start of the word. */
            for (int c = 1; c <= 16; c++) {
                if (kin_starts_with(eff_word, SP_PAST[c])) {
                    sp_under = SP_PAST[c];
                    break;
                }
            }
        }
    }
    /* For 1sg/2sg/1pl/2pl (class 0) */
    if (cls == 0) {
        if (tok->verb_tense == TENSE_CONDITIONAL) {
            /* Conditional past-SP absorbs TM 'a' via vowel contact (§1.1):
             * tu+a→twa (u→w),  mu+a→mwa (u→w),  n+a→na,  u+a→wa (u→w)  */
            if      (kin_starts_with(eff_word, "twa")) sp_under = "tu";
            else if (kin_starts_with(eff_word, "mwa")) sp_under = "mu";
            else if (kin_starts_with(eff_word, "na"))  sp_under = "n";
            else if (kin_starts_with(eff_word, "wa"))  sp_under = "u";
            else sp_under = "?";
        } else {
            /* Non-conditional: detect surface SP form from eff_word.
             * For past tense the SP absorbs the past augment 'a', e.g.:
             *   tu+a→twa, mu+a→mwa, n+a→na, u+a→wa (sp_under set below).
             * "nda" = n(SP) + da(TM immediate present), handled separately.
             * "twa"/"mwa" must be checked before "tu"/"mu" since u→w glide
             * means these words never start with "tu" or "mu".               */
            if      (kin_starts_with(eff_word, "nda"))  sp_under = "n";   /* TM="da" below */
            else if (kin_starts_with(eff_word, "twa"))  sp_under = "tu";  /* 1pl past */
            else if (kin_starts_with(eff_word, "mwa"))  sp_under = "mu";  /* 2pl past */
            else if (kin_starts_with(eff_word, "n"))    sp_under = "n";
            else if (kin_starts_with(eff_word, "tu"))   sp_under = "tu";
            else if (kin_starts_with(eff_word, "u"))    sp_under = "u";
            else if (kin_starts_with(eff_word, "mu"))   sp_under = "mu";
            else if (kin_starts_with(eff_word, "mw"))   sp_under = "mu";  /* 2pl mu+vowel → mw (mwuzure, mwororoke) */
            /* 1sg n→m before bilabials (§3.3): surface "mb/mp/mf/mv" → underlying "n" */
            else if (kin_starts_with(eff_word, "mb"))   sp_under = "n";
            else if (kin_starts_with(eff_word, "mp"))   sp_under = "n";
            else if (kin_starts_with(eff_word, "mf"))   sp_under = "n";
            else if (kin_starts_with(eff_word, "mv"))   sp_under = "n";
            else sp_under = "?";
        }
    }

    const char *tm  = tense_marker(tok->verb_tense);
    /* Special: 1sg immediate present uses "da" as TM (nda = n+da).
     * verb_match_inner stores tense=PRESENT_NORA for "nda" prefix, so
     * tense_marker() returns "". Override tm to "da" for morpheme display. */
    static char nda_tm_buf[4];
    if (cls == 0 && tm[0] == '\0' && kin_starts_with(eff_word, "nda")) {
        strncpy(nda_tm_buf, "da", 3);
        tm = nda_tm_buf;
    }
    /* Contracted future TM: 'za' → 'z' before vowel-initial root (§1.1).
     * When future TM 'za' precedes a vowel-initial root, the TM-final 'a'
     * is absorbed: za + V-root → z + V-root.
     * e.g. azitwa = a(SP) + z(TM·za→z) + it(root) + w(pass) + a(FV).
     * Detect by checking: tense=FUTURE, root is vowel-initial, and the word
     * has only one 'z' char (not the full 'za') after the SP surface.       */
    static char tm_contracted_buf[4];
    char tm_contracted_rule[KIN_MORPH_RULE_LEN] = "";
    bool tm_contracted = false;
    if (tok->verb_tense == TENSE_FUTURE && tok->stem[0] && mv(tok->stem[0])) {
        /* Future + vowel-initial root: TM surface is "z" not "za" */
        strncpy(tm_contracted_buf, "z", 2);
        snprintf(tm_contracted_rule, sizeof(tm_contracted_rule),
                 "a\xe2\x86\x92\xe2\x88\x85 / _V \xc2\xa7" "1.1 "
                 "(TM-final 'a' elides before vowel-initial root '%s': "
                 "za+%s \xe2\x86\x92 z+%s)",
                 tok->stem, tok->stem, tok->stem);
        tm_contracted = true;
    }
    const char *fv  = final_vowel(tok->verb_tense, word);
    /* Override FV for monosyllabic consonant root (h, b, z …) in past perfect:
     * the root inserts an epenthetic 'a' before FV 'ye' → surface FV = "aye".
     * e.g. guha (root h): m+bi+h+aye = mbihaye, ya+h+aye = yahaye.
     * Condition: tense=PAST_PERF, root is exactly one consonant char, word ends "aye". */
    const char *root_tmp = tok->stem[0] ? tok->stem : "";
    {
        size_t wlen = strlen(word);
        if (tok->verb_tense == TENSE_PAST_PERF &&
            strlen(root_tmp) == 1 && !mv((unsigned char)root_tmp[0]) &&
            wlen >= 4 && word[wlen-3]=='a' && word[wlen-2]=='y' && word[wlen-1]=='e') {
            fv = "aye";
        }
    }
    /* Override FV for y-final roots in past perfect:
     * root ends in 'y' + FV = bare 'e' (not 'iye' or 'ye').
     * e.g. giy+e=giye (yagiye), jy+e=jye (bajye)
     * final_vowel() may return "iye" (if word ends ...iye and root has 'i')
     * or "ye" (normal case).  Both are wrong: the 'y' is the root's own final
     * consonant, not the perfectivity glide of the FV 'ye'.                  */
    {
        size_t rtlen = strlen(root_tmp);
        if (tok->verb_tense == TENSE_PAST_PERF &&
            rtlen >= 2 && root_tmp[rtlen-1] == 'y' &&
            (strcmp(fv, "iye") == 0 || strcmp(fv, "ye") == 0)) {
            fv = "e";
        }
    }
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

    /* Root surface: for r-drop passive, stem-final 'r' elides before -w-.
     * yakuwe = ya + kur(root) + w(pass) + e(FV); surface root = "ku" (r-drop).
     * Pre-compute here so the `built` verification string is correct.       */
    char root_surface_buf[KIN_MAX_STEM];
    const char *root_surface = root;
    char root_surface_rule[KIN_MORPH_RULE_LEN] = "";
    if (tok->verb_ext == VEXT_PASSIVE && root[0]) {
        size_t rlen = strlen(root);
        if (rlen >= 2 && root[rlen-1] == 'r') {
            strncpy(root_surface_buf, root, rlen - 1);
            root_surface_buf[rlen-1] = '\0';
            root_surface = root_surface_buf;
            snprintf(root_surface_rule, sizeof(root_surface_rule),
                     "r\xe2\x86\x92\xe2\x88\x85 / __w (r drops before passive -w-): "
                     "-%s+w- \xe2\x86\x92 -%sw-",
                     root, root_surface_buf);
        }
    }

    /* Past-perfect consonant+y→z/ts fusion with FV 'ye':
     *   r+ye→ze  (§1.3): kor+ye=koze   yakoze ← gukora
     *   nd+ye→nze(§1.3): tsind+ye=tsinze  yatsinze ← gutsinda
     *   ng+ye→nze(§1.3): tang+ye=tanze    yatanze ← gutanga
     *   k+ye→tse (§1.3): andik+ye=anditse yanditse ← kwandika
     * The root_surface becomes z/ts-form; FV surface becomes bare 'e'.      */
    bool past_rye_ze  = false;   /* r/d/g-final root: FV 'ye' → surface 'e', root→z */
    bool past_kye_tse = false;   /* k-final root:     FV 'ye' → surface 'e', root→ts */
    char past_fuse_cons = '\0';  /* which consonant fused ('r','d','g','k')          */
    if (is_past_tense(tok->verb_tense) && tok->verb_ext == VEXT_NONE
        && root[0] && !root_surface_rule[0]) {
        size_t rlen = strlen(root);
        size_t wlen = strlen(word);
        char last_c = root[rlen-1];
        if (rlen >= 1 && (last_c == 'r' || last_c == 'd' || last_c == 'g')
            && wlen >= 1 && word[wlen-1] == 'e'
            && (wlen < 2 || word[wlen-2] != 'y')) {
            /* Build z-form: replace last consonant with 'z' */
            strncpy(root_surface_buf, root, rlen - 1);
            root_surface_buf[rlen-1] = 'z';
            root_surface_buf[rlen]   = '\0';
            root_surface = root_surface_buf;
            if (last_c == 'r')
                snprintf(root_surface_rule, sizeof(root_surface_rule),
                         "r+ye\xe2\x86\x92ze \xc2\xa71.3 (r assimilates past FV: "
                         "%sr+ye\xe2\x86\x92%se)", root, root_surface_buf);
            else
                snprintf(root_surface_rule, sizeof(root_surface_rule),
                         "n%c+ye\xe2\x86\x92nze \xc2\xa71.3 (n%c fuses with past FV: "
                         "n%c+ye\xe2\x86\x92nze)", last_c, last_c, last_c);
            past_rye_ze  = true;
            past_fuse_cons = last_c;
        } else if (rlen >= 2 && last_c == 'k'
                   && wlen >= 3 && word[wlen-3]=='t' && word[wlen-2]=='s' && word[wlen-1]=='e') {
            /* k+ye→tse: replace 'k' with "ts"; handle a+a vowel contact
             * (ya + andik: SP underlying ends in 'a', root starts with 'a' → surface "ndits")
             * Use sp_under for the contact check (sp_surface not yet computed here). */
            size_t sp_ulen = strlen(sp_under);
            size_t root_start = (sp_ulen > 0 && sp_under[sp_ulen-1]=='a'
                                 && root[0]=='a') ? 1 : 0;
            size_t copy_len = rlen - 1 - root_start;
            if (copy_len + 3 < (size_t)KIN_MAX_STEM) {
                strncpy(root_surface_buf, root + root_start, copy_len);
                root_surface_buf[copy_len]   = 't';
                root_surface_buf[copy_len+1] = 's';
                root_surface_buf[copy_len+2] = '\0';
                root_surface = root_surface_buf;
                snprintf(root_surface_rule, sizeof(root_surface_rule),
                         "k+ye\xe2\x86\x92tse \xc2\xa71.3 (k palatalized by past FV 'y': "
                         "%s+ye\xe2\x86\x92%stse)", root, root_surface_buf);
                past_kye_tse   = true;
                past_fuse_cons = 'k';
            }
        }
    }

    /* Reflexive -i- present in surface: VEXT_REFLEXIVE with consonant-initial root.
     * The marker 'i' (from kwi-) sits between SP and root, causing:
     *   (a) SP 'bi'+'i' → 'by'   (i→y §1.1)
     *   (b) root-final 'd' → 'z'  (nd→nz mutation in reflexive context)
     * Example: byigenza = bi + i + gend(d→z) + a  →  by + i + genz + a       */
    bool refl_i_present = (tok->verb_ext == VEXT_REFLEXIVE &&
                           root[0] && !mv(root[0]));

    /* Determine SP surface: check for phonological change at SP-next boundary *
     * When the reflexive 'i' is present it is the element that immediately      *
     * follows the SP — use "i" as next_after_sp so the i→y mutation fires.      */
    const char *next_after_sp = refl_i_present ? "i"
        : (tm[0] ? tm : (om[0] ? om : root));
    char sp_surface[KIN_MORPH_FORM_LEN];
    char sp_rule[KIN_MORPH_RULE_LEN] = "";
    char sp_pres_buf[KIN_MORPH_FORM_LEN] = "";  /* buffer for i-final present-form SP */
    bool i_final_pa = false;   /* true → insert ya(PA) slot after SP in chain */
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
        } else if (sp_last == 'n' &&
                   (next_c=='b' || next_c=='p' || next_c=='f' || next_c=='v')) {
            /* n→m §3.3: 1sg SP "n" assimilates to "m" before bilabials/labiodentals */
            sp_surface[slen - 1] = 'm';
            sp_surface[slen] = '\0';
            snprintf(sp_rule, sizeof(sp_rule),
                     "n\xe2\x86\x92m \xc2\xa7" "3.3 (1sg SP before bilabial '%c')", next_c);
        } else if (sp_last == 'a' && next_c == 'i') {
            /* Check whether the surface word uses euphonic-z insertion
             * (SP 'a' + z(euphonic) + i-initial root, e.g. azitwa) rather
             * than the standard a+i→e vowel fusion.
             * Detection: surface word[sp_len] == 'z'.                    */
            if (eff_word[slen] == 'z') {
                /* Euphonic-z: SP stays 'a'; no surface change to SP itself.
                 * The 'z' is a phonological connector, not part of SP.    */
                strncpy(sp_surface, sp_under, sizeof(sp_surface) - 1);
                snprintf(sp_rule, sizeof(sp_rule),
                         "z(euphon.) \xe2\x80\x94 SP '%s' + i-initial root"
                         " \xe2\x86\x92 '%s' + z (euphonic insertion)",
                         sp_under, sp_under);
            } else {
                /* Standard a+i→e fusion */
                sp_surface[slen - 1] = 'e';
                snprintf(sp_rule, sizeof(sp_rule),
                         "a+i\xe2\x86\x92""e \xc2\xa71.1 (SP '%s'+i\xe2\x86\x92'%s')",
                         sp_under, sp_surface);
            }
        }
    }

    /* Post-fix: past-tense SPs where u→w + past-augment 'a' are fused into SP.
     * SP_PAST[] stores the canonical underlying ("ru","bu","tu","mu"), but the
     * surface is the contracted form ("rwa","bwa","twa","mwa").
     * We detect this by checking what eff_word actually starts with.          */
    if (past && tok->verb_tense != TENSE_CONDITIONAL) {
        /* u-final SPs (ru,bu,tu,mu,ku) contract with past augment 'a': u→w+a */
        size_t slen = strlen(sp_under);
        if (slen > 0 && sp_under[slen-1] == 'u') {
            /* Build expected contracted surface: <sp_minus_u>wa */
            char contracted[KIN_MORPH_FORM_LEN];
            strncpy(contracted, sp_under, slen - 1);
            contracted[slen - 1] = '\0';
            strncat(contracted, "wa", sizeof(contracted) - slen);
            if (kin_starts_with(eff_word, contracted)) {
                strncpy(sp_surface, contracted, sizeof(sp_surface) - 1);
                snprintf(sp_rule, sizeof(sp_rule),
                         "u→w §1.1 + a(past augment) → %s (%s past SP)",
                         contracted, sp_under);
            }
        }
        /* 1sg past: n + a(past augment) → na */
        if (cls == 0 && strcmp(sp_under,"n")==0 && kin_starts_with(eff_word,"na")) {
            strncpy(sp_surface, "na", sizeof(sp_surface)-1);
            snprintf(sp_rule, sizeof(sp_rule), "n + a(past augment) → na (1sg past SP)");
        }
        /* 2sg past: u + a → wa (handled above via u-final rule for sp_under="u") */

        /* i-final SPs: bi+a→bya, ri+a→rya, zi+a→zya, ki+a→cya (§1.1 + ky→cy).
         * SP_PAST[] stores the already-contracted surface form ("bya" etc.) for
         * these classes, unlike u-final SPs which store the underlying form.
         * Retroactively expose the underlying present SP and annotate the rule,
         * consistent with how u-final SPs (tu→twa, ru→rwa) are displayed.     */
        if (sp_rule[0] == '\0') {
            static const struct { const char *contracted; const char *pres; } i_map[] = {
                { "bya", "bi" },
                { "rya", "ri" },
                { "zya", "zi" },
                { "cya", "ki" },
                { NULL,  NULL }
            };
            for (int ii = 0; i_map[ii].contracted; ii++) {
                if (strcmp(sp_under, i_map[ii].contracted) == 0) {
                    /* sp_surface already holds the contracted form (initialised
                     * from sp_under before vowel-contact rules ran).           */
                    strncpy(sp_pres_buf, i_map[ii].pres,
                            sizeof(sp_pres_buf) - 1);
                    sp_under = sp_pres_buf;   /* show present form as underlying */
                    i_final_pa = true;        /* insert ya(PA) slot after SP    */
                    if (strcmp(i_map[ii].contracted, "cya") == 0) {
                        snprintf(sp_rule, sizeof(sp_rule),
                                 "i\xe2\x86\x92y \xc2\xa7""1.1 + a(past augment)"
                                 " + ky\xe2\x86\x92""cy \xe2\x86\x92 cya (ki past SP)");
                    } else {
                        snprintf(sp_rule, sizeof(sp_rule),
                                 "i\xe2\x86\x92y \xc2\xa7""1.1 + a(past augment)"
                                 " \xe2\x86\x92 %s (%s past SP)",
                                 i_map[ii].contracted, i_map[ii].pres);
                    }
                    break;
                }
            }
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
                /* i→y §1.1: but "ki"+"y" → "cy" (ky→cy spelling rule in Kinyarwanda)
                 * Similarly "ri"+"y" → "ry" (no change needed for ri).               */
                if (olen == 2 && om[0] == 'k') {
                    strncpy(om_surface, "cy", sizeof(om_surface) - 1); /* ki+V → cy */
                } else {
                    om_surface[olen-1] = 'y';
                }
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

    /* Reflexive root surface: d→z mutation when refl_i_present */
    char root_refl_surface[KIN_MAX_STEM];
    char root_refl_rule[KIN_MORPH_RULE_LEN] = "";
    if (refl_i_present) {
        strncpy(root_refl_surface, root, sizeof(root_refl_surface) - 1);
        root_refl_surface[sizeof(root_refl_surface) - 1] = '\0';
        size_t rlen = strlen(root);
        if (rlen >= 2 && root[rlen - 1] == 'd') {
            root_refl_surface[rlen - 1] = 'z';  /* nd→nz in reflexive context */
            snprintf(root_refl_rule, sizeof(root_refl_rule),
                     "nd\xe2\x86\x92nz (final d\xe2\x86\x92z before reflexive -a in -i- context)");
        }
    }

    /* Causative-y root surface (r+y→z §1.3): pre-compute for built verification.
     * root ends in 'r' (citation); surface form fuses r with -y- → 'z'.
     * e.g. root="er" → root_surface="ez" (kwera/kweza pair)
     *      root="mer"→ root_surface="mez" (kumera/kumeza pair)
     * Also: EXT 'y' is absorbed into the root surface, so ext_surface = "".  */
    const char *ext_surface = ext;   /* normally same as ext; overridden below */
    if (tok->verb_ext == VEXT_CAUSATIVE_Y && root[0]) {
        size_t rlen = strlen(root);
        if (rlen >= 2) {
            strncpy(root_surface_buf, root, rlen - 1);
            root_surface_buf[rlen-1] = 'z';
            root_surface_buf[rlen]   = '\0';
            root_surface = root_surface_buf;
            ext_surface  = "";   /* y absorbed into z-fusion */
        }
    }

    /* TENSE_PRESENT + vowel-initial root surface: TM 'ra' contracts to 'r'.
     * Rule §1.1 (4c): a→∅ before vowel — the 'a' of TM 'ra' elides.
     * e.g. ireza = i(SP) + r(TM ra→r) + ez(root_surface) + a(FV).           */
    if (tok->verb_tense == TENSE_PRESENT && !tm_contracted
        && strcmp(tm, "ra") == 0
        && root_surface[0] && mv(root_surface[0])) {
        strncpy(tm_contracted_buf, "r", 2);
        snprintf(tm_contracted_rule, sizeof(tm_contracted_rule),
                 "a\xe2\x86\x92\xe2\x88\x85 / _V \xc2\xa71.1 "
                 "(TM-final 'a' elides before vowel-initial root '%s': "
                 "ra+%s \xe2\x86\x92 r+%s)",
                 root_surface, root_surface, root_surface);
        tm_contracted = true;
    }

    /* Build expected surface for verification.
     * Include negation prefix in built string so verification works for
     * negative verbs (ntaragenda, sindagenda, etc.).                        */
    char built[KIN_MAX_WORD];
    if (refl_i_present) {
        /* SP + i(REFL) + root_refl_surface + FV */
        snprintf(built, sizeof(built), "%s%si%s%s",
                 neg_pfx, sp_surface, root_refl_surface, fv);
    } else if (tok->verb_tense == TENSE_CONDITIONAL) {
        /* When TM 'a' is elided (a+a→a), omit it from the built surface */
        snprintf(built, sizeof(built), "%s%s%s%s%s%s%s",
                 neg_pfx,
                 sp_surface, cond_tm_elided ? "" : tm,
                 cond_particle_surface[0] ? cond_particle_surface : "ku",
                 root, ext, fv);
    } else {
        /* For r/d/g+ye→ze or k+ye→tse: root_surface already has fusion form;
         * FV surfaces as bare 'e' (the 'y' of 'ye' is absorbed by the fusion). */
        snprintf(built, sizeof(built), "%s%s%s%s%s%s%s",
                 neg_pfx,
                 sp_surface, tm_contracted ? tm_contracted_buf : tm,
                 om[0] ? om_surface : "",
                 root_surface, ext_surface, (past_rye_ze || past_kye_tse) ? "e" : fv);
    }
    mb->verified = (strcmp(built, word) == 0);

    /* Store morphemes: prepend NEG morpheme for negative verbs */
    int n = 0;
    if (neg_pfx[0])
        set_morph(&mb->m[n++], "NEG", neg_pfx, neg_pfx,
                  "Ubunyagatifu (Negation prefix)");
    set_morph(&mb->m[n++], "SP", sp_under, sp_surface, sp_rule);
    /* i-final past SP: insert explicit past-augment slot ya(PA) after SP.
     * form="ya" = surface of a(past augment) after §1.1 bi+a→bya.
     * surface="" so it doesn't double-count "ya" already inside sp_surface.   */
    if (i_final_pa)
        set_morph(&mb->m[n++], "PA", "ya", "", "");
    if (tm[0])
        set_morph(&mb->m[n++], "TM", tm,
                  tm_contracted          ? tm_contracted_buf
                : cond_tm_elided         ? ""
                :                          tm,
                  tm_contracted          ? tm_contracted_rule
                : tok->verb_tense == TENSE_CONDITIONAL
                      ? (cond_tm_elided
                         ? "a+a\342\206\222a \302\2471.1 (TM 'a' elided after SP ending in 'a')"
                         : "Inziganyo TM (conditional marker; fused into SP by \302\2471.1)")
                      : "");
    else if (tok->verb_tense != TENSE_NONE
             && tok->verb_tense != TENSE_IMPERATIVE
             && tok->verb_tense != TENSE_PAST_PERF
             && tok->verb_tense != TENSE_PAST_IMPF)
        /* Zero tense marker: explicit ∅ in Ingingo/Guhuza display.
         * surface="" (empty) keeps the built verification string correct;
         * form="∅" is used by the printer wherever surface is absent.
         * NOT shown for PAST_PERF / PAST_IMPF: the textbook Impitakare
         * formula is SP(past-form) + root + ye(FV) — there is no TM slot.
         * Showing ∅(TM) there contradicts the tense label; past tense is
         * encoded in the SP's past form and in the FV 'ye', not in a TM. */
        set_morph(&mb->m[n++], "TM", "\342\210\205", "", "");
    /* Conditional particle (ku/gu) replaces OM slot for CONDITIONAL tense */
    if (tok->verb_tense == TENSE_CONDITIONAL && cond_particle[0]) {
        set_morph(&mb->m[n++], "COND", cond_particle,
                  cond_particle_surface[0] ? cond_particle_surface : cond_particle,
                  cond_particle_rule[0] ? cond_particle_rule
                                        : "Ikivugana cy'inziganyo (conditional modal particle)");
    } else if (om[0]) {
        set_morph(&mb->m[n++], "OM", om, om_surface[0] ? om_surface : om, om_rule);
    }
    /* Reflexive 'i' morpheme: insert between TM/OM slot and root */
    if (refl_i_present) {
        set_morph(&mb->m[n++], "REFL", "i", "i",
                  "Imbundo y'ikwisanzura (Reflexive marker i- from kwi-)");
        set_morph(&mb->m[n++], "root", root, root_refl_surface, root_refl_rule);
        set_morph(&mb->m[n++], "FV", fv, fv, "");
        mb->n = n;
        return;  /* skip normal root/ext/FV handling below */
    }
    if (tok->verb_ext == VEXT_CAUSATIVE_Y) {
        /* r+y→z (§1.3): citation root ends in 'r'; -y- causative fuses it to 'z'.
         * Surface root = root[0..n-2]+'z'; EXT is absorbed (surface="").
         * e.g. mer + y → mez  (citation: mer, surface: mez)               */
        size_t rlen = strlen(root);
        char root_surface[KIN_MAX_STEM];
        if (rlen >= 1) {
            strncpy(root_surface, root, rlen - 1);
            root_surface[rlen - 1] = 'z';
            root_surface[rlen]     = '\0';
        } else {
            strncpy(root_surface, root, KIN_MAX_STEM - 1);
            root_surface[KIN_MAX_STEM - 1] = '\0';
        }
        set_morph(&mb->m[n++], "root", root, root_surface,
                  "r+y\342\206\222z \302\2471.3 (causative-y fuses stem-final r: mer+y\342\206\222mez)");
        set_morph(&mb->m[n++], "EXT", "y", "", "");   /* ext absorbed into root surface */
    } else {
        /* When the POS tagger found no extension (VEXT_NONE), try detecting
         * one inside the stem now.  Handles cases like bivire (stem="vir")
         * where the root "va" had its final vowel elided before "-ir-".      */
        if (tok->verb_ext == VEXT_NONE && !ext[0] && root[0]) {
            char det_root[KIN_MAX_STEM] = "";
            char det_ext[8] = "";
            VerbExtension det_vext =
                detect_ext_in_stem(root, det_root, det_ext, sizeof(det_ext));
            if (det_vext != VEXT_NONE && det_root[0] && det_ext[0]) {
                /* Elision check: same logic as analyse_vinf */
                size_t brlen = strlen(det_root);
                char root_surf[KIN_MAX_STEM];
                char root_rule[KIN_MORPH_RULE_LEN] = "";
                char elided_v = '\0';
                if (brlen >= 2 && mv(det_ext[0])) {
                    char last = det_root[brlen - 1];
                    if (last=='a'||last=='u'||last=='i'||last=='e'||last=='o')
                        elided_v = last;
                }
                if (elided_v) {
                    strncpy(root_surf, det_root, brlen - 1);
                    root_surf[brlen - 1] = '\0';
                    snprintf(root_rule, sizeof(root_rule),
                             "%c\342\206\222\342\210\205 \302\2471.1 (root-final '%c' elides"
                             " before vowel-initial extension '-%s-')",
                             elided_v, elided_v, det_ext);
                } else {
                    strncpy(root_surf, det_root, KIN_MAX_STEM - 1);
                    root_surf[KIN_MAX_STEM - 1] = '\0';
                }
                set_morph(&mb->m[n++], "root", det_root, root_surf, root_rule);
                set_morph(&mb->m[n++], "EXT",  det_ext,  det_ext,
                          kin_verb_ext_name(det_vext));
            } else {
                /* Use root_surface/rule if set (e.g. by past_rye_ze r→z) */
                set_morph(&mb->m[n++], "root", root, root_surface,
                          root_surface_rule[0] ? root_surface_rule : "");
            }
        } else {
            /* Deep-root check: when an outer extension exists (e.g. reciprocal
             * -an-), also inspect whether the root itself contains an embedded
             * reversive -uk-/-ur- applied to a nasal-final base via r→d/n_:
             *   gutana (root -tan-) + reversive (-ruk-) → -tan-ruk- →
             *   r→d / n_ rule → surface -tanduk-
             * We detect this by:
             *   1. root ends in -uk or -ur
             *   2. stripping those 2 chars gives inner stem ending in -nd or -mb
             *      (epenthetic stop after nasal: n+d or m+b)
             *   3. removing that stop gives a known verb stem (base root)
             * If confirmed, store: base(root) + ruk/rur(REV, with rule) + ext(EXT) */
            bool wrote_deep = false;
            size_t rlen = strlen(root);
            if (rlen > 4 && ext[0] &&
                ((root[rlen-1]=='k' && root[rlen-2]=='u') ||
                 (root[rlen-1]=='r' && root[rlen-2]=='u'))) {
                bool is_uk = (root[rlen-1] == 'k');
                char inner[KIN_MAX_STEM];
                size_t ilen = rlen - 2;
                strncpy(inner, root, ilen); inner[ilen] = '\0';
                if (ilen >= 3) {
                    char last = inner[ilen-1];
                    char prev = inner[ilen-2];
                    if ((last == 'd' && prev == 'n') ||
                        (last == 'b' && prev == 'm')) {
                        char base[KIN_MAX_STEM];
                        strncpy(base, inner, ilen-1); base[ilen-1] = '\0';
                        if (kin_is_known_verb_stem(base)) {
                            /* Underlying reversive: -ruk- or -rur-
                             * Surface after r→d/n_: -duk- or -dur-         */
                            char und_rev[4], srf_rev[4];
                            snprintf(und_rev, sizeof(und_rev), "r%s", is_uk ? "uk" : "ur");
                            snprintf(srf_rev, sizeof(srf_rev), "d%s", is_uk ? "uk" : "ur");
                            char rev_rule[KIN_MORPH_RULE_LEN];
                            snprintf(rev_rule, sizeof(rev_rule),
                                "r\xe2\x86\x92""d / n_ (ingombajwi r ihinduka d inyuma "
                                "y'ingombajwi n): -%s-%s- \xe2\x86\x92 -%s%s-",
                                base, und_rev, base, srf_rev);
                            set_morph(&mb->m[n++], "root", base,    base,    "");
                            set_morph(&mb->m[n++], "REV",  und_rev, srf_rev, rev_rule);
                            wrote_deep = true;
                        }
                    }
                }
            }
            if (!wrote_deep)
                set_morph(&mb->m[n++], "root", root, root_surface,
                          root_surface_rule[0] ? root_surface_rule : "");
            if (ext[0])
                set_morph(&mb->m[n++], "EXT", ext, ext, "");
        }
    }
    /* TENSE_SUBJUNCTIVE_LOC: fv = "eho"/"emo"/"eyo" — split into FV + LOC.
     * The subjunctive final vowel is 'e'; the locative suffix (ho/mo/yo)
     * is a separate morpheme (ahantu).  This mirrors the infinitive treatment
     * in analyse_vinf() where LOC is always a distinct slot.
     * e.g. habeho = ha + ∅ + b + e(FV·subj) + ho(LOC)                      */
    if (tok->verb_tense == TENSE_SUBJUNCTIVE_LOC && strlen(fv) > 1) {
        set_morph(&mb->m[n++], "FV", "e", "e", "");
        /* No phonological rule for the locative suffix — it is appended
         * directly without consonant mutation.  Empty rule prevents it
         * from appearing in the Itegeko (phonological-rule) display.    */
        set_morph(&mb->m[n++], "LOC", fv + 1, fv + 1, "");
    } else if (past_rye_ze) {
        /* r/d/g+ye→ze in past perfect: underlying FV is 'ye', surface is 'e'.
         * The 'y' is absorbed by the consonant fusion shown on root.          */
        {
            char fv_rule[KIN_MORPH_RULE_LEN];
            if (past_fuse_cons == 'r')
                snprintf(fv_rule, sizeof(fv_rule),
                         "y\xe2\x86\x92\xe2\x88\x85 / r_ (y elided: r+ye\xe2\x86\x92ze, \xc2\xa71.3)");
            else
                snprintf(fv_rule, sizeof(fv_rule),
                         "y\xe2\x86\x92\xe2\x88\x85 / n%c_ (y elided: n%c+ye\xe2\x86\x92nze, \xc2\xa71.3)",
                         past_fuse_cons, past_fuse_cons);
            set_morph(&mb->m[n++], "FV", "ye", "e", fv_rule);
        }
    } else if (past_kye_tse) {
        /* k+ye→tse in past perfect: underlying FV is 'ye', surface 'e'.
         * The 'k+y' fusion is shown on root as →ts; only 'e' remains as FV. */
        set_morph(&mb->m[n++], "FV", "ye", "e",
                  "y\xe2\x86\x92\xe2\x88\x85 / k_ (y elided: k+ye\xe2\x86\x92tse, \xc2\xa71.3)");
    } else if (strcmp(fv, "aye") == 0) {
        /* Monosyllabic root (h, b, z…) + epenthetic 'a' before past perfect FV.
         * The Kinyarwanda verb always ends in a vowel; '-y-' is the perfectivity
         * marker; '-e' is the true final vowel (iherezo).  The 'a' is an
         * icyungo cy'ijwi (liaison vowel) inserted to avoid an illegal
         * consonant cluster when the root is a single consonant (h+ye→ *hye).
         * Display as two slots: EPEN('a') + FV('ye').                        */
        set_morph(&mb->m[n++], "EPEN", "a", "a",
                  "Icyungo cy'ijwi (epenthesis: root+a+ye avoids *root+ye cluster)");
        set_morph(&mb->m[n++], "FV", "ye", "ye", "");
    } else {
        set_morph(&mb->m[n++], "FV", fv, fv, "");
    }
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

    /* Short-circuit: if the whole stem is a known verb root, it is integral —
     * do not attempt to split it into root + extension.
     * e.g. "uzur" (kuzura = to fill) must not be split into "uz" + "-ur-"
     *      (reversive), since "uz" is not a valid root.                      */
    if (kin_is_known_verb_stem(stem)) return VEXT_NONE;

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
            /* Single-consonant bare root: the stem's root ends in 'a' which
             * was elided before the vowel-initial extension (a→∅ §1.1).
             * e.g. stem="vir": bare="v" → restored root="va" (kuva exists)
             *      stem="rir": bare="r" → restored root="ra" (kura exists)
             * Restore the 'a' and verify the result is a known verb stem.   */
            if (rlen == 1) {
                /* Try restoring the elided root-final vowel.
                 * Common vowels: 'a' (most roots), 'u' (e.g. vu from kuva),
                 * 'i' (rare).  The elided vowel assimilates before the
                 * vowel-initial extension (V→∅ §1.1).                       */
                char restored[KIN_MAX_STEM];
                static const char try_vowels[] = {'a', 'u', 'i', 'e', 'o'};
                for (int vi = 0; vi < (int)(sizeof try_vowels); vi++) {
                    restored[0] = stem[0];
                    restored[1] = try_vowels[vi];
                    restored[2] = '\0';
                    if (kin_is_known_verb_stem(restored)) {
                        strncpy(bare_root, restored, KIN_MAX_STEM - 1);
                        bare_root[KIN_MAX_STEM - 1] = '\0';
                        strncpy(ext_str, s, ext_sz - 1); ext_str[ext_sz-1] = '\0';
                        return VEXT_APPLICATIVE;
                    }
                }
            }
        }
    }

    /* Stative: stem ends in "ik" or "ek" (Ngirika vowel harmony)
     *   -ik-: non-mid stem vowel (a/i/u) → guhingika (hing+ik), gufatika (fat+ik)
     *   -ek-: mid stem vowel (e/o)        → gutekeka  (tek+ek),  gusomeka (som+ek)
     * Checked before reversive -uk/-ok because both share a final 'k'. */
    if (len > 3) {
        const char *s = stem + len - 2;
        if (strcmp(s, "ik") == 0 || strcmp(s, "ek") == 0) {
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

    /* Causative-y (Ngiza): stem ends in 'z' ← r+y→z rule (§1.3).
     * Replace final 'z' with 'r'; if result is a known stem it was the -y- causative.
     * e.g. "mez" → "mer" (kumera→kumeza); bare_root = "mer" (citation form).
     * e.g. "ez"  → "er"  (kwera→kweza, len=2): guard relaxed to >= 2 so that
     * short-root causative pairs like er/ez are handled correctly.              */
    if (len >= 2 && stem[len-1] == 'z') {
        char try_r[KIN_MAX_STEM];
        strncpy(try_r, stem, len - 1); try_r[len-1] = 'r'; try_r[len] = '\0';
        if (kin_is_known_verb_stem(try_r)) {
            strncpy(bare_root, try_r, KIN_MAX_STEM - 1); bare_root[KIN_MAX_STEM-1] = '\0';
            ext_str[0] = 'z'; ext_str[1] = '\0';   /* surface consonant */
            return VEXT_CAUSATIVE_Y;
        }
    }

    return VEXT_NONE;
}

/* ══════════════════════════════════════════════════════════════════════════
 * TREE 3a — INSHINGA IMBUNDO (Verb Infinitive)
 *   Formula:  PREF + root + (EXT) + FV
 *   PREF = Indanganshinga (ku/gu/kw/gw — marks Nt.15 infinitive class)
 *
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

    /* Detect locative suffix (-ho/-mo/-yo) appended after final 'a'.
     * e.g. guturaho = gu+tur+a+ho, kwigeraho = kw+iger+a+ho.
     * If detected, strip it for FV and reconstruction, then add as LOC morpheme. */
    size_t wlen = strlen(word);
    const char *loc = "";   /* locative suffix string, or "" */
    char work_word[KIN_MAX_WORD];
    strncpy(work_word, word, sizeof(work_word) - 1);
    work_word[sizeof(work_word) - 1] = '\0';
    size_t work_wlen = wlen;
    if (wlen > 5 && wlen >= 3 && word[wlen - 3] == 'a' &&
        (kin_ends_with(word, "ho") || kin_ends_with(word, "mo") ||
         kin_ends_with(word, "yo"))) {
        loc = word + wlen - 2;   /* points to "ho", "mo", or "yo" in word[] */
        work_wlen = wlen - 2;    /* length without locative */
        work_word[work_wlen] = '\0';
    }

    /* Final vowel from surface (use work_word which has locative stripped) */
    const char *fv = "a";
    if (work_wlen >= 3 && work_word[work_wlen-3]=='t' && work_word[work_wlen-2]=='s'
        && work_word[work_wlen-1]=='e')
        fv = "tse";
    else if (work_wlen >= 2 && work_word[work_wlen-2]=='y' && work_word[work_wlen-1]=='e')
        fv = "ye";
    else if (work_wlen >= 2 && work_word[work_wlen-2]=='w' && work_word[work_wlen-1]=='e')
        fv = "we";
    else if (work_wlen >= 1 && work_word[work_wlen-1]=='e'
             && (work_wlen < 2 || (work_word[work_wlen-2]!='y' && work_word[work_wlen-2]!='w')))
        fv = "e";

    /* Try to detect a derivational extension within the stem.
     * detect_ext_in_stem() begins with:
     *   if (kin_is_known_verb_stem(stem)) return VEXT_NONE;
     * so all integral vowel-initial roots already in VERB_STEMS (iruk, emer,
     * izer, emez …) exit immediately — no false positives possible.
     *
     * The previous "!mv(root[0])" guard was added to stop false splits on
     * those roots, but it also prevented causative-y detection for roots like
     * "ez" (from kweza = ku+er+y+a).  Removing the guard is safe because the
     * inner guard already handles every known vowel-initial root correctly:
     *   kwiruka  (root "iruk") → known → VEXT_NONE  ✓
     *   kwemera  (root "emer") → known → VEXT_NONE  ✓
     *   kweza    (root "ez")   → unknown → causative-y: ez→er (known) → VEXT_CAUSATIVE_Y ✓ */
    char bare_root[KIN_MAX_STEM]      = "";
    char ext_str  [KIN_MORPH_FORM_LEN] = "";
    VerbExtension inf_ext = VEXT_NONE;
    inf_ext = detect_ext_in_stem(root, bare_root, ext_str, sizeof(ext_str));

    /* Verify against the surface word (including locative if present).
     * For u+u→u fused verbs: drop the prefix's final 'u' before concatenating
     * (ku + ubak + a → k+ubak+a = kubaka).                                     */
    char built[KIN_MAX_WORD];
    if (uu_fused) {
        size_t plen = strlen(pref);
        snprintf(built, sizeof(built), "%.*s%s%s%s",
                 (int)(plen - 1), pref, root, fv, loc);
    } else {
        snprintf(built, sizeof(built), "%s%s%s%s", pref, root, fv, loc);
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
        if (inf_ext == VEXT_CAUSATIVE_Y) {
            /* r+y→z (§1.3): bare_root ends in 'r' (citation form); the causative
             * morpheme -y- fuses with that 'r' to yield 'z' on the surface.
             * Display: root underlying="gwir", surface="gwiz"; EXT underlying="y",
             * surface="" (absorbed).  This shows y is present without r and z
             * coexisting — the rule consumes both r and y to produce z.            */
            size_t brlen = strlen(bare_root);
            char root_surface[KIN_MAX_STEM];
            strncpy(root_surface, bare_root, brlen - 1);
            root_surface[brlen - 1] = 'z';   /* replace final 'r' with 'z' */
            root_surface[brlen]     = '\0';
            set_morph(&mb->m[n++], "root", bare_root, root_surface,
                      "r+y\342\206\222z \302\2471.3 (causative -y- fuses with"
                      " stem-final r \342\206\222 z)");
            set_morph(&mb->m[n++], "EXT", "y", "", ""); /* -y- absorbed into root surface */
        } else {
        /* Split: bare_root + extension.
         *
         * Elision check: if bare_root ends in 'a' and ext_str is vowel-initial
         * (e.g. bare_root="va", ext_str="ir"), the root-final 'a' was elided
         * before the extension (a→∅ §1.1).  The surface root is bare_root minus
         * its final 'a'; we store the underlying form and set a phonological rule.
         * e.g. kuvira: bare_root="va", ext_str="ir"
         *      surface root = "v", rule = "a→∅ §1.1 (root-final 'a' elides…)"  */
        size_t brlen = strlen(bare_root);
        char root_surface[KIN_MAX_STEM];
        char root_elision_rule[KIN_MORPH_RULE_LEN] = "";
        char elided_vowel = '\0';
        if (brlen >= 2 && ext_str[0] != '\0' && mv(ext_str[0])) {
            char last = bare_root[brlen - 1];
            if (last == 'a' || last == 'u' || last == 'i' || last == 'e' || last == 'o')
                elided_vowel = last;
        }
        bool elided = (elided_vowel != '\0');
        if (elided) {
            strncpy(root_surface, bare_root, brlen - 1);
            root_surface[brlen - 1] = '\0';
            snprintf(root_elision_rule, sizeof(root_elision_rule),
                     "%c\342\206\222\342\210\205 \302\2471.1 (root-final '%c' elides"
                     " before vowel-initial extension '-%s-')",
                     elided_vowel, elided_vowel, ext_str);
        } else {
            strncpy(root_surface, bare_root, KIN_MAX_STEM - 1);
            root_surface[KIN_MAX_STEM - 1] = '\0';
        }
        set_morph(&mb->m[n++], "root", bare_root, root_surface, root_elision_rule);
        set_morph(&mb->m[n++], "EXT",  ext_str,   ext_str,   kin_verb_ext_name(inf_ext));
        } /* end non-CAUSATIVE_Y */
    } else {
        set_morph(&mb->m[n++], "root", root, root, "");
    }

    set_morph(&mb->m[n++], "FV", fv, fv, "");
    if (loc[0])
        set_morph(&mb->m[n++], "LOC", loc, loc, "Umugereka w'ahantu (Locative suffix)");
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
