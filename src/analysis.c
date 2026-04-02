/*
 * analysis.c
 * Top-level pipeline: tokenize → tag → check syntax → suggest corrections.
 * Also contains the pretty-print output function.
 */

#include <stdio.h>
#include <string.h>
#include "../include/kinyarwanda.h"

/*
 * kin_resolve_sp_ambiguity() – post-processing pass to fix ambiguous SP classes.
 *
 * In Kinyarwanda the "ya" subject prefix is shared by two grammatical contexts:
 *
 *   1. Nt.6 PRESENT habitual:  ya + stem + a     (yamara, yagenda)
 *   2. Nt.1/3 PAST:            a(SP) + a(past) → ya + stem + ye/tse/aga
 *                               (yagiye, yaremye, yagendaga, yabonye)
 *
 * The verb morphology tagger has no sentence context, so it always assigns
 * class 6 when it sees "ya" as SP.  After syntax checking we know whether
 * the subject noun is Nt.1/3 (human/tree class) or Nt.6 (mass/plural class).
 * This pass corrects the verb's stored noun_class to match the actual subject
 * so the output display and any downstream processing shows the right class.
 *
 * Similarly "i" SP is stored as class 4 but class 9 also uses "i"; we resolve
 * that in context too.
 *
 * Only verbs whose class changed from "ya→Nt.1" or "i→Nt.9" and whose
 * IMMEDIATE predecessor is a noun of the matching class are updated.
 * Verbs with already-correct class, or without a directly preceding noun,
 * are left unchanged.
 */
/*
 * scan_back_noun() — scan backwards from token[from] to find the most recent
 * noun whose class is one of the candidates in cls[] (length n).
 * Returns NULL if none found.
 */
static const Token *scan_back_noun(const SentenceAnalysis *sa, int from,
                                   const int *cls, int n) {
    for (int j = from; j >= 0; j--) {
        const Token *t = &sa->tokens[j];
        if (t->pos != POS_NOUN || t->noun_class == 0) continue;
        for (int k = 0; k < n; k++) {
            if (t->noun_class == cls[k]) return t;
        }
    }
    return NULL;
}

static void kin_resolve_sp_ambiguity(SentenceAnalysis *sa) {
    for (int i = 1; i < sa->token_count; i++) {
        Token *verb = &sa->tokens[i];
        if (verb->pos != POS_VERB_CONJ) continue;

        int vc = verb->noun_class;

        /* "ya" SP (stored cls 6) + Nt.1/3/9 subject + PAST tense → reclassify.
         * In Kinyarwanda the "ya" SP is shared by:
         *   Nt.1 past:  umuntu yagiye  (a+past-a → ya)
         *   Nt.3 past:  umuti waguye    (... but also uses 'wa', less common)
         *   Nt.9 past:  Imana yaremye   (inka ya- in past)
         *   Nt.6 pres:  amazu yagenda   (genuine Nt.6 present)
         * We reclassify "ya" to match the subject class when tense is past.
         * Scan back for the nearest Nt.1/3/9 noun. */
        if (vc == 6 &&
            (verb->verb_tense == TENSE_PAST_PERF  ||
             verb->verb_tense == TENSE_PAST_IMPF  ||
             verb->verb_tense == TENSE_COPULA_PAST)) {
            static const int ya_cls[] = {1, 3, 9};
            const Token *subj = scan_back_noun(sa, i - 1, ya_cls, 3);
            if (subj) verb->noun_class = subj->noun_class;
        }

        /* "i" SP (stored cls 4) — valid antecedents: Nt.4 or Nt.9.
         * Scan back; if the closest matching noun is class 9, reclassify. */
        if (vc == 4) {
            static const int i_cls[] = {4, 9};
            const Token *subj = scan_back_noun(sa, i - 1, i_cls, 2);
            if (subj && subj->noun_class == 9)
                verb->noun_class = 9;
        }

        /* "y" SP (stored cls 0, ambiguous) arises from i→y glide before a
         * vowel-initial root (§1.1: i+V → y+V). The stored class is 0 because
         * the "y" prefix is shared by Nt.1/6/9 present and Nt.1 past.
         * Scan back for nearest Nt.9 or Nt.1 noun and resolve:
         *   Nt.9 subject → SP was "i" → class 9  (e.g. Imana yita)
         *   Nt.1 subject + present → SP was "a" → class 1  (e.g. umuntu yiga) */
        if (vc == 0) {
            static const int y_cls[] = {9, 1};
            const Token *subj = scan_back_noun(sa, i - 1, y_cls, 2);
            if (subj) {
                if (subj->noun_class == 9) {
                    verb->noun_class = 9;
                } else if (subj->noun_class == 1 &&
                           (verb->verb_tense == TENSE_PRESENT_NORA ||
                            verb->verb_tense == TENSE_PRESENT)) {
                    verb->noun_class = 1;
                }
            }
        }
    }
}

/*
 * kin_tag_gram_roles()
 *
 * Assigns a GramRole to each verb token (POS_VERB_CONJ or POS_VERB_INF)
 * using sentence-level context.  Must be called after kin_tag_sentence()
 * and kin_morpheme_analyze() so that tok->verb_tense and tok->pos are valid.
 *
 * Rules (applied in priority order per token):
 *
 *  GRAM_ROLE_PARTICIPIAL  — tense is TENSE_NEG_RELATIVE (-ta- form)
 *                           e.g. itagira, utagira, atagira
 *
 *  GRAM_ROLE_SEQUENTIAL   — tense is TENSE_NARRATIVE (SP+ka+root+a)
 *                           e.g. akagenda, bakabikorera
 *
 *  GRAM_ROLE_COMPLEMENT   — within 2 tokens after a verb particle (ngo/ko/nuko)
 *                           e.g. "ngo agiye" → agiye is COMPLEMENT
 *
 *  GRAM_ROLE_AUXILIARY    — surface form is a known copula/auxiliary:
 *                           yari, ari, ndi, bari, kuba + copula tenses
 *                           e.g. "yari umuntu" → yari is AUXILIARY
 *
 *  GRAM_ROLE_MAIN_VERB    — any other conjugated or infinitive verb.
 *                           If a sentence has multiple verbs, the first
 *                           non-complement, non-sequential verb is MAIN.
 *                           Later verbs remain MAIN unless context reclassifies.
 */
void kin_tag_gram_roles(SentenceAnalysis *sa) {
    /* First pass: assign roles that are self-evident from the token */
    for (int i = 0; i < sa->token_count; i++) {
        Token *tok = &sa->tokens[i];
        if (tok->pos != POS_VERB_CONJ && tok->pos != POS_VERB_INF) continue;

        /* Neg-participial (-ta- form) */
        if (tok->verb_tense == TENSE_NEG_RELATIVE) {
            tok->gram_role = GRAM_ROLE_PARTICIPIAL;
            continue;
        }

        /* Sequential narrative (inkurikizo: SP+ka+root+a) */
        if (tok->verb_tense == TENSE_NARRATIVE) {
            tok->gram_role = GRAM_ROLE_SEQUENTIAL;
            continue;
        }

        /* Copula / auxiliary: copula tenses or known auxiliary stem */
        if (tok->verb_tense == TENSE_COPULA_PAST ||
            tok->verb_tense == TENSE_COPULA_PRES  ||
            (tok->stem[0] && (strcmp(tok->stem, "b") == 0 ||   /* kuba */
                              strcmp(tok->stem, "ri") == 0))) { /* copular -ri- */
            tok->gram_role = GRAM_ROLE_AUXILIARY;
            continue;
        }

        /* Default: will be refined in second pass */
        tok->gram_role = GRAM_ROLE_MAIN_VERB;
    }

    /* Second pass: upgrade to COMPLEMENT for verbs after ngo/ko/nuko */
    for (int i = 1; i < sa->token_count; i++) {
        Token *tok = &sa->tokens[i];
        if (tok->pos != POS_VERB_CONJ && tok->pos != POS_VERB_INF) continue;
        if (tok->gram_role != GRAM_ROLE_MAIN_VERB) continue;

        /* Scan back up to 3 tokens for a verb particle */
        for (int j = i - 1; j >= 0 && j >= i - 3; j--) {
            const Token *prev = &sa->tokens[j];
            if (prev->pos == POS_VERB_PARTICLE) {
                tok->gram_role = GRAM_ROLE_COMPLEMENT;
                break;
            }
            /* Stop at another verb (can't cross a verb boundary) */
            if (prev->pos == POS_VERB_CONJ || prev->pos == POS_VERB_INF) break;
        }
    }
}

SentenceAnalysis kin_analyze(const char *text) {
    SentenceAnalysis sa;
    memset(&sa, 0, sizeof(sa));
    sa.token_count = kin_tokenize(text, sa.tokens, KIN_MAX_TOKENS);
    kin_tag_sentence(&sa);
    kin_resolve_sp_ambiguity(&sa);   /* resolve ya/i SP class before syntax    */
    /* Type-dispatch morpheme analysis: fills tok->morph per word type.
     * Each type gets its own rules (noun→D+RT+C, verb→SP+TM+C+FV, etc.)   */
    for (int i = 0; i < sa.token_count; i++)
        kin_morpheme_analyze(&sa.tokens[i]);
    kin_tag_gram_roles(&sa);     /* assign sentence role to each verb token    */
    kin_check_syntax(&sa);
    kin_suggest_corrections(&sa);
    return sa;
}

/* ── Pretty-print helpers ──────────────────────────────────────────────────*/
static void print_separator(char c, int width) {
    for (int i = 0; i < width; i++) putchar(c);
    putchar('\n');
}

/* Returns approximate subject-prefix (SP) surface string for a class/tense.
 * Used for morpheme breakdown display only — not for analysis logic.      */
static const char *sp_display(int cls, VerbTense tense) {
    bool past = (tense == TENSE_PAST_PERF || tense == TENSE_PAST_IMPF ||
                 tense == TENSE_COPULA_PAST);
    switch (cls) {
        case  1: return past ? "ya"  : "a";
        case  2: return "ba";
        case  3: return past ? "wa"  : "u";
        case  4: return "i";
        case  5: return past ? "rya" : "ri";
        case  6: return "ya";
        case  7: return past ? "cya" : "ki";
        case  8: return past ? "bya" : "bi";
        case  9: return past ? "ya"  : "i";
        case 10: return past ? "zya" : "zi";
        case 11: return "ru";
        case 12: return "ka";
        case 13: return "tu";
        case 14: return "bu";
        case 15: return "ku";
        case 16: return "ha";
        default: return "?";
    }
}

/* Returns tense marker (TM) infix between SP and stem, or "" if none.     */
static const char *tm_display(VerbTense tense) {
    switch (tense) {
        case TENSE_PRESENT:    return "ra";
        case TENSE_FUTURE:     return "za";
        case TENSE_NARRATIVE:  return "ka";
        case TENSE_OPTATIVE:   return "raka";
        case TENSE_NEG_RELATIVE: return "ta";
        default:               return "";
    }
}

/* Returns final-vowel/suffix string for each tense.                        */
static const char *fv_display(VerbTense tense) {
    switch (tense) {
        case TENSE_PRESENT:
        case TENSE_PRESENT_NORA:
        case TENSE_FUTURE:
        case TENSE_NARRATIVE:
        case TENSE_OPTATIVE:
        case TENSE_CONDITIONAL:
        case TENSE_NEG_RELATIVE:  return "a";
        case TENSE_PAST_PERF:     return "ye";
        case TENSE_PAST_IMPF:     return "aga";
        case TENSE_SUBJUNCTIVE:   return "e";
        case TENSE_SUBJUNCTIVE_LOC: return "e + ho/mo/yo";
        case TENSE_COPULA_PAST:
        case TENSE_COPULA_PRES:   return "ri + loc";
        default:                  return "a";
    }
}

static bool is_vowel_c(char c) {
    return c=='a'||c=='e'||c=='i'||c=='o'||c=='u';
}

/* ── Noun morphophonological reconstruction ─────────────────────────────────
 * Reads tok->morph (filled by kin_morpheme_analyze) and prints the
 * D + RT + C decomposition with any orthographic rules that applied.
 * Handles all 16 noun classes including Nt.9/10 nasal assimilation.
 * ─────────────────────────────────────────────────────────────────────────*/
static void print_noun_reconstruction(const Token *t) {
    /* Use the pre-computed morph breakdown from kin_morpheme_analyze().
     * This now handles ALL 16 classes (including Nt.9/10 which were
     * previously skipped).                                                   */
    const MorphBreakdown *mb = &t->morph;
    if (mb->n < 3) return;   /* need at least D + RT + C                     */

    const KinMorpheme *d_m  = &mb->m[0];  /* D  (Indomo)        */
    const KinMorpheme *rt_m = &mb->m[1];  /* RT (Indanganteko)  */
    const KinMorpheme *c_m  = &mb->m[2];  /* C  (Igicumbi)      */

    bool rt_changed = (strcmp(rt_m->form, rt_m->surface) != 0);

    printf("  \342\224\224\342\224\200 Gusubiza (Reconstruction):\n");
    if (rt_changed) {
        /* Show underlying, rule, then surface */
        printf("       Ingingo:  [D]%s + [RT]%s + [C]%s\n",
               d_m->form, rt_m->form, c_m->form);
        printf("       Itegeko:  %s\n", rt_m->rule);
        printf("       Guhuza:   %s\xC2\xB7%s\xC2\xB7%s  \342\206\222  %s%s\n",
               d_m->surface, rt_m->surface, c_m->surface,
               t->lower, mb->verified ? "  \342\234\223" : "");
    } else {
        printf("       Ingingo:  [D]%s + [RT]%s + [C]%s  \342\206\222  %s%s\n",
               d_m->form, rt_m->form, c_m->form,
               t->lower, mb->verified ? "  \342\234\223" : "");
    }
}

/* Print verb morpheme breakdown line for a conjugated verb token.
 * Reads from tok->morph (filled by analyse_vconj in morph_dispatch.c) so
 * that EXT, COND particles, and phonological SP surfaces are shown correctly
 * for all tenses and all extension types.                                    */
static void print_verb_morphemes(const Token *t) {
    const MorphBreakdown *mb = &t->morph;

    if (mb->n > 0) {
        printf("  \342\224\224\342\224\200 Uturemajambo (Morphemes):");
        for (int i = 0; i < mb->n; i++) {
            const KinMorpheme *m = &mb->m[i];
            /* Uturemajambo always shows the UNDERLYING form (m->form), never
             * the phonological surface form.  Rule: individual morphemes in the
             * breakdown must not contain compound consonants (tw, kw, ry, etc.);
             * those are surface artefacts explained in the Itegeko (rules) line.
             * Phonological rules (u→w, i→y, a+a→a) are shown in Ingingo/Guhuza. */
            const char *form = m->form[0] ? m->form : m->surface;
            if (i > 0) printf(" +");
            if (strcmp(m->label, "SP") == 0) {
                if (t->noun_class > 0)
                    printf(" %s(SP\xC2\xB7Nt.%d)", form, t->noun_class);
                else
                    printf(" %s(SP)", form);   /* personal pronoun class */
            } else if (strcmp(m->label, "OM") == 0) {
                printf(" %s(OM\xC2\xB7Nt.%d)", form, t->obj_class);
            } else if (strcmp(m->label, "root") == 0) {
                printf(" %s(root)", form);
            } else if (strcmp(m->label, "EXT") == 0) {
                printf(" %s(EXT)", form);
            } else {
                /* TM, COND, FV: show underlying form */
                printf(" %s(%s)", form, m->label);
            }
        }
        /* Reflexive note: the i- prefix was elided (kwi- → i- dropped) */
        if (t->verb_ext == VEXT_REFLEXIVE)
            printf(" + [i-(imbundo, elided from kwi-)]");
        printf("\n");
        return;
    }

    /* Fallback (tok->morph not filled — should not normally occur) */
    const char *sp  = sp_display(t->noun_class, t->verb_tense);
    const char *tm  = tm_display(t->verb_tense);
    const char *fv  = fv_display(t->verb_tense);
    const char *om  = (t->obj_class > 0) ? kin_om_str(t->obj_class) : "";
    const char *ext = "";
    switch (t->verb_ext) {
        case VEXT_PASSIVE:     ext = "-w-";       break;
        case VEXT_CAUSATIVE:   ext = "-ish-";     break;
        case VEXT_APPLICATIVE: ext = "-ir-";      break;
        case VEXT_RECIPROCAL:  ext = "-an-";      break;
        case VEXT_STATIVE:     ext = "-ik-";      break;
        case VEXT_REVERSIVE:   ext = "-ur-/-uk-"; break;
        default: break;
    }
    printf("  \342\224\224\342\224\200 Uturemajambo (Morphemes): %s(SP\xC2\xB7Nt.%d)", sp, t->noun_class);
    if (tm[0])  printf(" + %s(TM)", tm);
    if (om[0])  printf(" + %s(OM\xC2\xB7Nt.%d)", om, t->obj_class);
    if (t->verb_ext == VEXT_REFLEXIVE) printf(" + [i-(imbundo, elided from kwi-)]");
    printf(" + %s(root)", t->stem[0] ? t->stem : "?");
    if (ext[0]) printf(" + %s(EXT)", ext);
    printf(" + %s(FV)\n", fv);
}

/* ── Tense marker key ───────────────────────────────────────────────────────
 * Returns a short note identifying which morpheme position(s) confirm the
 * tense — shown indented under the tense label so the reader sees the flow:
 * morphemes detected → position of TM/FV confirms → tense name.             */
static const char *tense_marker_key(VerbTense t) {
    switch (t) {
        case TENSE_PRESENT:         return "TM='ra' iboneka mu mwanya wa 2";
        case TENSE_PRESENT_NORA:    return "TM=\342\210\205 (nta ntera y'igihe), FV='a'";
        case TENSE_PAST_PERF:       return "FV='ye'/'tse' mu iherezo ry'ijambo";
        case TENSE_PAST_IMPF:       return "FV='aga' mu iherezo ry'ijambo";
        case TENSE_FUTURE:          return "TM='za' iboneka mu mwanya wa 2";
        case TENSE_NARRATIVE:       return "TM='ka' iboneka mu mwanya wa 2";
        case TENSE_OPTATIVE:        return "TM='raka' iboneka mu mwanya wa 2";
        case TENSE_SUBJUNCTIVE:     return "FV='e' mu iherezo ry'ijambo";
        case TENSE_SUBJUNCTIVE_LOC: return "FV='e' + ahantu (ho/mo/yo)";
        case TENSE_CONDITIONAL:     return "SP + intera 'a' + root + FV='a'";
        case TENSE_NEG_RELATIVE:    return "NEG='ta' iboneka mu mwanya wa 2";
        case TENSE_COPULA_PAST:     return "SP(impitagihe) + 'ri' + ahantu";
        case TENSE_COPULA_PRES:     return "SP(indagihe) + 'ri' + ahantu";
        default:                    return "";
    }
}

/* ── Conjugated-verb morpheme reconstruction ────────────────────────────────
 * Reads tok->morph (filled by analyse_vconj in morph_dispatch.c) and shows:
 *   – underlying morpheme template (Ingingo)
 *   – any phonological rule that fired (Itegeko), if applicable
 *   – the reconstructed surface form with ✓ when it matches the input word
 *
 * Template: SP + (TM) + (COND|OM) + root + (EXT) + FV
 *   COND = conditional modal particle ku/gu (Inziganyo only)
 *   EXT  = -w- / -ish- / -ir- / -an- / -ik- / -ur-/-uk-
 *
 * Phonological rules stored per-morpheme in tok->morph.m[i].rule.           */
static void print_verb_reconstruction(const Token *t) {
    if (t->pos != POS_VERB_CONJ) return;

    /* Copula forms use a different template (SP + ri + locative) */
    if (t->verb_tense == TENSE_COPULA_PAST ||
        t->verb_tense == TENSE_COPULA_PRES) {
        printf("  \342\224\224\342\224\200 Gusubiza (Reconstruction):\n");
        printf("       Ingingo:  [SP]%s(Nt.%d) + [copula]ri + [ahantu]  \342\206\222  %s\n",
               sp_display(t->noun_class, t->verb_tense), t->noun_class, t->surface);
        return;
    }

    const MorphBreakdown *mb = &t->morph;

    if (mb->n > 0) {
        char lword[KIN_MAX_WORD];
        kin_strlower(t->surface, lword, sizeof(lword));

        /* Check if any phonological rules fired */
        bool has_rule = false;
        for (int i = 0; i < mb->n; i++)
            if (mb->m[i].rule[0]) { has_rule = true; break; }

        /* Build concatenated surface form for verification.
         * Use m->surface directly — an empty surface means the morpheme is
         * phonologically zero (e.g., conditional TM 'a' elided by a+a→a). */
        char built[KIN_MAX_WORD] = "";
        for (int i = 0; i < mb->n; i++) {
            const KinMorpheme *m = &mb->m[i];
            const char *surf = m->surface;  /* "" = intentionally elided */
            size_t blen = strlen(built), slen = strlen(surf);
            if (slen > 0 && blen + slen < sizeof(built) - 1)
                memcpy(built + blen, surf, slen + 1);
        }
        bool matches = (strcmp(built, lword) == 0);

        printf("  \342\224\224\342\224\200 Gusubiza (Reconstruction):\n");

        /* Ingingo: underlying forms with labels */
        printf("       Ingingo: ");
        for (int i = 0; i < mb->n; i++) {
            const KinMorpheme *m = &mb->m[i];
            if (i > 0) printf(" + ");
            printf("[%s]%s", m->label, m->form);
            if (strcmp(m->label, "SP") == 0)
                printf("(Nt.%d)", t->noun_class);
            else if (strcmp(m->label, "OM") == 0)
                printf("(Nt.%d)", t->obj_class);
        }

        if (has_rule) {
            printf("\n");
            /* Itegeko: each rule that fired */
            for (int i = 0; i < mb->n; i++)
                if (mb->m[i].rule[0])
                    printf("       Itegeko:  %s\n", mb->m[i].rule);
            /* Guhuza: surface pieces → word  (skip elided/zero morphemes) */
            printf("       Guhuza:  ");
            bool first_g = true;
            for (int i = 0; i < mb->n; i++) {
                const KinMorpheme *m = &mb->m[i];
                /* Use surface directly; skip if zero (e.g., conditional TM elided) */
                const char *surf = m->surface[0] ? m->surface : m->form;
                if (strcmp(m->label, "TM") == 0 && !m->surface[0]) continue;
                if (!first_g) printf(" + ");
                printf("%s", surf);
                first_g = false;
            }
            printf("  \342\206\222  %s%s\n", lword, matches ? "  \342\234\223" : "");
        } else {
            printf("  \342\206\222  %s%s\n", built, matches ? "  \342\234\223" : "");
        }
        return;
    }

    /* Fallback: tok->morph not filled (should not occur for conjugated verbs) */
    char lword[KIN_MAX_WORD];
    kin_strlower(t->surface, lword, sizeof(lword));
    const char *sp   = sp_display(t->noun_class, t->verb_tense);
    const char *tm   = tm_display(t->verb_tense);
    const char *root = t->stem[0] ? t->stem : "?";
    const char *om   = (t->obj_class > 0) ? kin_om_str(t->obj_class) : "";
    size_t llen = strlen(lword);
    const char *fv = (t->verb_tense == TENSE_PAST_PERF)
                   ? ((llen >= 3 && kin_ends_with(lword, "tse")) ? "tse" : "ye")
                   : (t->verb_tense == TENSE_PAST_IMPF)  ? "aga"
                   : (t->verb_tense == TENSE_SUBJUNCTIVE) ? "e"
                   : "a";
    const char *ext_sfx = "";
    switch (t->verb_ext) {
        case VEXT_PASSIVE:     ext_sfx = "w";   break;
        case VEXT_CAUSATIVE:   ext_sfx = "ish"; break;
        case VEXT_APPLICATIVE: ext_sfx = "ir";  break;
        case VEXT_RECIPROCAL:  ext_sfx = "an";  break;
        case VEXT_STATIVE:     ext_sfx = "ik";  break;
        case VEXT_REVERSIVE:
            ext_sfx = (llen >= 2 && lword[llen-3] == 'u' && lword[llen-2] == 'k')
                      ? "uk" : "ur";
            break;
        default: break;
    }
    char built[KIN_MAX_WORD];
    snprintf(built, sizeof(built), "%s%s%s%s%s%s", sp, tm, om, root, ext_sfx, fv);
    bool matches = (strcmp(built, lword) == 0);
    printf("  \342\224\224\342\224\200 Gusubiza (Reconstruction):\n");
    printf("       Ingingo:  [SP]%s(Nt.%d)", sp, t->noun_class);
    if (tm[0])      printf(" + [TM]%s", tm);
    if (om[0])      printf(" + [OM]%s(Nt.%d)", om, t->obj_class);
    printf(" + [root]%s", root);
    if (ext_sfx[0]) printf(" + [EXT]%s", ext_sfx);
    printf(" + [FV]%s  \342\206\222  %s%s\n", fv, built, matches ? "  \342\234\223" : "");
}

/* ── Infinitive morpheme breakdown ──────────────────────────────────────────
 * Template: INF.PREF + root + (EXT) + FV
 *   INF.PREF = ku/gu (before consonant) / kw/gw (before vowel, §1.1)
 *   root     = bare verb root (igicumbi cy'inshinga)
 *   EXT      = derivational extension if present (-ish-, -ir-, -an-, -w-)
 *   FV       = final vowel (a, e, ye, tse…)
 *
 * Reads from tok->morph (filled by analyse_vinf in morph_dispatch.c).      */
static void print_inf_morphemes(const Token *t) {
    if (t->pos != POS_VERB_INF) return;

    char lword[KIN_MAX_WORD];
    kin_strlower(t->surface, lword, sizeof(lword));

    const MorphBreakdown *mb = &t->morph;

    /* Use structured breakdown when available.
     * Find morphemes by label to handle optional EXT and LOC morphemes.      */
    if (mb->n >= 3) {
        const KinMorpheme *pref_m = NULL, *root_m = NULL,
                          *ext_m  = NULL, *fv_m   = NULL, *loc_m = NULL;
        for (int i = 0; i < mb->n; i++) {
            const char *lbl = mb->m[i].label;
            if (strcmp(lbl, "PREF") == 0) pref_m = &mb->m[i];
            else if (strcmp(lbl, "root") == 0) root_m = &mb->m[i];
            else if (strcmp(lbl, "EXT")  == 0) ext_m  = &mb->m[i];
            else if (strcmp(lbl, "FV")   == 0) fv_m   = &mb->m[i];
            else if (strcmp(lbl, "LOC")  == 0) loc_m  = &mb->m[i];
        }
        if (!pref_m || !root_m || !fv_m) goto inf_fallback;

        /* Morpheme line (Uturemajambo) */
        printf("  \342\224\224\342\224\200 Uturemajambo (Morphemes): %s(INF.PREF) + %s(root)",
               pref_m->form, root_m->form);
        if (ext_m)  printf(" + %s(EXT)", ext_m->form);
        printf(" + %s(FV)", fv_m->form);
        if (loc_m)  printf(" + %s(LOC)", loc_m->form);
        printf("\n");

        /* Reconstruction */
        printf("  \342\224\224\342\224\200 Gusubiza (Reconstruction):\n");
        /* Build Ingingo line (underlying labels) */
        printf("       Ingingo:  [PREF]%s + [root]%s", pref_m->form, root_m->form);
        if (ext_m)  printf(" + [EXT]%s", ext_m->form);
        printf(" + [FV]%s", fv_m->form);
        if (loc_m)  printf(" + [LOC]%s", loc_m->form);

        if (pref_m->rule[0]) {
            /* Prefix phonological rule fired: show Itegeko + Guhuza lines */
            printf("\n       Itegeko:  %s\n", pref_m->rule);
            printf("       Guhuza:   %s%s", pref_m->surface, root_m->form);
            if (ext_m)  printf("%s", ext_m->form);
            printf("%s", fv_m->form);
            if (loc_m)  printf("%s", loc_m->form);
            printf("  \342\206\222  %s%s\n", lword, mb->verified ? "  \342\234\223" : "");
        } else {
            /* No prefix rule: inline reconstruction on same line */
            printf("  \342\206\222  %s%s", pref_m->surface, root_m->form);
            if (ext_m)  printf("%s", ext_m->form);
            printf("%s", fv_m->form);
            if (loc_m)  printf("%s", loc_m->form);
            printf("%s\n", mb->verified ? "  \342\234\223" : "");
            if (ext_m && ext_m->rule[0])
                printf("       Itegeko:  EXT: %s\n", ext_m->rule);
        }
        return;
    }

    inf_fallback:;

    /* Fallback: morph breakdown not available */
    const char *pref = t->detected_prefix[0] ? t->detected_prefix : "ku";
    const char *root = t->stem[0] ? t->stem : "?";
    const char *fv = "a";
    size_t llen = strlen(lword);
    if (llen >= 3 && kin_ends_with(lword, "tse"))      fv = "tse";
    else if (llen >= 2 && kin_ends_with(lword, "ye"))  fv = "ye";
    else if (llen >= 2 && kin_ends_with(lword, "we"))  fv = "we";
    else if (llen >= 1 && lword[llen-1] == 'e' &&
             (llen < 2 || (lword[llen-2] != 'y' && lword[llen-2] != 'w')))
        fv = "e";
    printf("  \342\224\224\342\224\200 Uturemajambo (Morphemes): %s(INF.PREF) + %s(root) + %s(FV)\n",
           pref, root, fv);
    char built[KIN_MAX_WORD];
    snprintf(built, sizeof(built), "%s%s%s", pref, root, fv);
    bool matches = (strcmp(built, lword) == 0);
    printf("  \342\224\224\342\224\200 Gusubiza (Reconstruction):\n");
    printf("       Ingingo:  [PREF]%s + [root]%s + [FV]%s  \342\206\222  %s%s\n",
           pref, root, fv, built, matches ? "  \342\234\223" : "");
}

/* Print noun morpheme breakdown line (D + RT + C separately).             */
static void print_noun_morphemes(const Token *t) {
    const MorphBreakdown *mb = &t->morph;
    if (mb->n >= 3) {
        /* Use structured breakdown from kin_morpheme_analyze */
        const KinMorpheme *d  = &mb->m[0];
        const KinMorpheme *rt = &mb->m[1];
        const KinMorpheme *c  = &mb->m[2];
        printf("  \342\224\224\342\224\200 Uturemajambo (Morphemes): "
               "[D]%s + [RT]%s(Nt.%d) + [C]%s\n",
               d->form, rt->form, t->noun_class, c->form);
        print_noun_reconstruction(t);
    } else if (t->detected_prefix[0]) {
        /* Fallback: old combined form */
        printf("  \342\224\224\342\224\200 Uturemajambo (Morphemes): %s(D+RT\xC2\xB7Nt.%d) + %s(igicumbi)\n",
               t->detected_prefix, t->noun_class,
               t->stem[0] ? t->stem : "?");
        print_noun_reconstruction(t);
    } else if (t->stem[0]) {
        printf("  \342\224\224\342\224\200 Igicumbi (Stem): %s\n", t->stem);
        print_noun_reconstruction(t);
    }
    if (t->is_deverbative && t->verb_root[0]) {
        /* Build infinitive for display.
         * Kinyarwanda prefix rules (simplified, for display only):
         *   w-initial stem: ku+w = kw → "kw" + stem[1:] + "a"
         *                   e.g. "wambar" → "kwambara", "wijim" → "kwijima"
         *   vowel-initial:  ku+V = kw+V → "kw" + stem + "a"
         *   voiced C (b,d,g,j,r,v,z,m,n,y,c): "gu" + stem + "a"
         *   voiceless C:    "ku" + stem + "a"                              */
        char r0 = t->verb_root[0];
        bool is_vowel = (r0=='a'||r0=='e'||r0=='i'||r0=='o'||r0=='u');
        bool is_voiced = (r0=='b'||r0=='d'||r0=='g'||r0=='j'||r0=='r'||
                          r0=='v'||r0=='z'||r0=='m'||r0=='n'||r0=='y'||
                          r0=='c');   /* c = palatal affricate, uses gu  */
        if (r0 == 'w') {
            /* ku + w-initial stem: drop leading 'w', use "kw" prefix */
            printf("  └─ Ivuye mu nshinga (Deverbative): igicumbi -%s-"
                   " (cf. kw%sa)\n", t->verb_root, t->verb_root + 1);
        } else {
            const char *pfx = is_vowel ? "kw" : (is_voiced ? "gu" : "ku");
            printf("  └─ Ivuye mu nshinga (Deverbative): igicumbi -%s-"
                   " (cf. %s%sa)\n", t->verb_root, pfx, t->verb_root);
        }
    }
}

void kin_print_analysis(const SentenceAnalysis *sa, bool verbose) {
    print_separator('=', 72);
    printf("ISESENGURA RY'URURIMI / LANGUAGE ANALYSIS\n");
    print_separator('=', 72);

    printf("\nAmagambo / Tokens:  %d\n", sa->token_count);
    printf("Inshinga iboneka / Has verb: %s\n",
           sa->has_verb ? "Yego (Yes)" : "Oya (No)");
    printf("Makosa / Errors:    %d\n\n", sa->error_count);

    /* Token table */
    printf("\n%-20s  %-30s  %-8s  %-8s\n",
           "Ijambo/Word", "Ubwoko/Type", "Inteko/Class", "Igicumbi/Stem");
    print_separator('-', 72);
    putchar('\n');

    for (int i = 0; i < sa->token_count; i++) {
        const Token *t = &sa->tokens[i];
        char class_str[16] = "";
        if (t->noun_class > 0)
            snprintf(class_str, sizeof(class_str), "Nt.%d", t->noun_class);

        /* Mark tokens with errors */
        char marker = (t->error_count > 0) ? '!' : ' ';
        printf("%c%-19s  %-30s  %-8s  %-8s\n",
               marker,
               t->surface,
               kin_pos_name(t->pos),
               class_str,
               t->stem);

        /* Verb details
         * Flow: type detected → morphemes → tense confirmed from morpheme
         * positions → reconstruction (verify morphemes → exact surface word). */
        if (t->pos == POS_VERB_CONJ || t->pos == POS_VERB_INF) {
            if (t->is_negative)
                printf("  └─ INSHINGA Y'UBUNYAGATIFU (Negative verb)\n");
            if (t->pos == POS_VERB_CONJ) {
                /* 1. Morphemes first */
                print_verb_morphemes(t);
                /* 2. Tense label with morpheme-position confirmation */
                if (t->verb_tense != TENSE_NONE) {
                    printf("  └─ %s\n", kin_verb_tense_name(t->verb_tense));
                    const char *mk = tense_marker_key(t->verb_tense);
                    if (mk[0])
                        printf("       [Igenanzira: %s]\n", mk);
                }
                /* 3. Sentence role (GramRole) */
                if (t->gram_role != GRAM_ROLE_NONE && t->gram_role != GRAM_ROLE_MAIN_VERB)
                    printf("  \342\224\224\342\224\200 Inshingwa: %s\n",
                           kin_gram_role_name(t->gram_role));
                /* 4. OM and extension */
                if (t->obj_class > 0)
                    printf("  \342\224\224\342\224\200 OM(Nt.%d/-%s-): %s\n",
                           t->obj_class, kin_om_str(t->obj_class),
                           kin_class_name(t->obj_class));
                if (t->verb_ext != VEXT_NONE)
                    printf("  \342\224\224\342\224\200 %s\n", kin_verb_ext_name(t->verb_ext));
                /* 4. Reconstruction: morphemes + rules → surface verification */
                print_verb_reconstruction(t);
            } else {
                /* POS_VERB_INF: morpheme breakdown + reconstruction */
                if (t->obj_class > 0)
                    printf("  └─ OM(Nt.%d/-%s-): %s\n",
                           t->obj_class, kin_om_str(t->obj_class),
                           kin_class_name(t->obj_class));
                if (t->verb_ext != VEXT_NONE)
                    printf("  └─ %s\n", kin_verb_ext_name(t->verb_ext));
                print_inf_morphemes(t);
            }
        }
        /* Noun morpheme breakdown */
        if (t->pos == POS_NOUN)
            print_noun_morphemes(t);
        /* Extra pronoun info in verbose mode */
        if (verbose && t->pos == POS_PRONOUN && t->pron_type != PRON_NONE) {
            printf("  └─ %s\n", kin_pron_type_name(t->pron_type));
        }

        /* Blank line between tokens for readability */
        putchar('\n');
    }

    /* Errors section */
    if (sa->error_count > 0) {
        print_separator('-', 72);
        printf("\nMAKOSA / ERRORS DETECTED:\n\n");
        for (int i = 0; i < sa->error_count; i++) {
            const Error *e = &sa->errors[i];
            printf("[%d] %s\n", i + 1, e->message);
            if (e->suggestion[0])
                printf("    → %s\n", e->suggestion);
            putchar('\n');
        }
    } else {
        printf("\nNta makosa aboneka / No errors detected.\n");
    }

    print_separator('=', 72);
}
