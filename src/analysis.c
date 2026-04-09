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

        /* "ya" SP (stored cls 6) + past tense → reclassify to correct antecedent.
         * SP_PAST table: classes 1, 4, 6, 9 ALL share past SP "ya".
         * Class 3 past SP is "wa" — it must NEVER be in this list.
         * Class 4 (imi- nouns, e.g. imirimo) uses "ya" past SP and must be included.
         *   Nt.1 past:  umuntu yagiye      (SP ya = class 1)
         *   Nt.4 past:  imirimo yakoze     (SP ya = class 4)
         *   Nt.9 past:  Imana yaremye      (SP ya = class 9)
         *   Nt.6 pres:  amazu yagenda      (genuine Nt.6 present, not reclassified)
         * Scan back for nearest noun whose past SP is "ya". */
        if (vc == 6 &&
            (verb->verb_tense == TENSE_PAST_PERF  ||
             verb->verb_tense == TENSE_PAST_IMPF  ||
             verb->verb_tense == TENSE_COPULA_PAST)) {
            static const int ya_cls[] = {1, 4, 6, 9};
            const Token *subj = scan_back_noun(sa, i - 1, ya_cls, 4);
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
         *   Nt.1 subject + present → SP was "a" → class 1  (e.g. umuntu yiga)
         * Guard: only apply when the surface word actually starts with "y".
         * Words starting with n/m/mb/mp/mf/mv/tu/mu/du carry unambiguous person
         * prefixes (1sg/2sg/1pl) and must NOT be reclassified as a noun class. */
        if (vc == 0 && kin_starts_with(verb->lower, "y")) {
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

        /* NOTE: "a" SP (class 1) is also valid for Nt.6 (ama- plural) in
         * present tense.  We do NOT reclassify here because SP_PRES[6]="ya"
         * in the code's convention — changing noun_class to 6 would cause
         * morph_dispatch to display "ya" as SP, which is wrong for words
         * that actually start with "a" (e.g. azitwa).  The ambiguity is
         * instead shown in the display via sp_nc_label() as "Nt.1 / Nt.6". */
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
        /* Verbal nouns detected in Pass C must keep their role */
        if (tok->gram_role == GRAM_ROLE_VERBAL_NOUN) continue;

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
        if (tok->gram_role == GRAM_ROLE_VERBAL_NOUN) continue;
        if (tok->gram_role != GRAM_ROLE_MAIN_VERB) continue;

        /* Scan back up to 3 tokens for a complement-introducing particle.
         * Only "ngo", "ko", "kuti" introduce complement clauses.
         * Quotative particles (ati/iti/bati/ruti/etc.) introduce direct speech
         * and must NOT cause the following verb to be marked as complement. */
        for (int j = i - 1; j >= 0 && j >= i - 3; j--) {
            const Token *prev = &sa->tokens[j];
            if (prev->pos == POS_VERB_PARTICLE &&
                (strcmp(prev->lower, "ngo")  == 0 ||
                 strcmp(prev->lower, "ko")   == 0 ||
                 strcmp(prev->lower, "kuti") == 0)) {
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

/* SP 'a' (class 1 present) is homophonous with class 6 (ama- plural).
 * SP 'ya' (class 6 present) is also the past SP for Nt.1/Nt.4/Nt.9.
 * Returns the appropriate label; buf must be at least 20 bytes.            */
static const char *sp_nc_label(int cls, VerbTense tense, char *buf, size_t n) {
    bool past = (tense == TENSE_PAST_PERF || tense == TENSE_PAST_IMPF ||
                 tense == TENSE_COPULA_PAST);
    if (cls == 1 && !past)
        snprintf(buf, n, "Nt.1 / Nt.6");
    else if (cls == 6 && past)
        /* "ya" in past tense: Nt.1 (he/she) most common; Nt.4/Nt.6/Nt.9 also use ya-past.
         * Context reclassification in kin_tag_sentence() sets the correct class when a
         * preceding subject noun is present.  Without context, show the ambiguity. */
        snprintf(buf, n, "Nt.1/Nt.6\xC2\xB7past");
    else if (cls == 0)
        snprintf(buf, n, "Pers.");   /* person prefix: 1sg/2sg/1pl/2pl */
    else
        snprintf(buf, n, "Nt.%d", cls);
    return buf;
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

    /* Detect deverbative C split: [C·root]sanzur + [C·FV]e */
    size_t vrl = t->verb_root[0] ? strlen(t->verb_root) : 0;
    bool c_split = (t->is_deverbative && vrl > 0
                    && strncmp(c_m->form, t->verb_root, vrl) == 0
                    && strlen(c_m->form) > vrl);
    char c_fv = c_split ? c_m->form[vrl] : '\0';

    printf("  \342\224\224\342\224\200 Gusubiza (Reconstruction):\n");
    if (rt_changed) {
        /* Show underlying, rule, then surface */
        if (c_split) {
            printf("       Ingingo:  [D]%s + [RT]%s + [C\xC2\xB7root]%s + [C\xC2\xB7" "FV]%c\n",
                   d_m->form, rt_m->form, t->verb_root, c_fv);
        } else {
            printf("       Ingingo:  [D]%s + [RT]%s + [C]%s\n",
                   d_m->form, rt_m->form, c_m->form);
        }
        printf("       Itegeko:  %s\n", rt_m->rule);
        if (c_split) {
            printf("       Guhuza:   %s\xC2\xB7%s\xC2\xB7%s\xC2\xB7%c  \342\206\222  %s%s\n",
                   d_m->surface, rt_m->surface, t->verb_root, c_fv,
                   t->lower, mb->verified ? "  \342\234\223" : "");
        } else {
            printf("       Guhuza:   %s\xC2\xB7%s\xC2\xB7%s  \342\206\222  %s%s\n",
                   d_m->surface, rt_m->surface, c_m->surface,
                   t->lower, mb->verified ? "  \342\234\223" : "");
        }
    } else {
        if (c_split) {
            printf("       Ingingo:  [D]%s + [RT]%s + [C\xC2\xB7root]%s + [C\xC2\xB7" "FV]%c"
                   "  \342\206\222  %s%s\n",
                   d_m->form, rt_m->form, t->verb_root, c_fv,
                   t->lower, mb->verified ? "  \342\234\223" : "");
        } else {
            printf("       Ingingo:  [D]%s + [RT]%s + [C]%s  \342\206\222  %s%s\n",
                   d_m->form, rt_m->form, c_m->form,
                   t->lower, mb->verified ? "  \342\234\223" : "");
        }
    }
}

/* Print verb morpheme breakdown line for a conjugated verb token.
 * Reads from tok->morph (filled by analyse_vconj in morph_dispatch.c) so
 * that EXT, COND particles, and phonological SP surfaces are shown correctly
 * for all tenses and all extension types.                                    */
static void print_verb_morphemes(const Token *t) {
    /* Equative copula "ni" / negative copula "si" are invariable forms.
     * They do not decompose into SP+TM+root+FV — show them as frozen copulas. */
    if (strcmp(t->lower, "ni") == 0 || strcmp(t->lower, "si") == 0) {
        const char *gloss = (t->lower[0] == 'n')
            ? "Inshinga nkene y'ubwitabire \xe2\x80\x93 invariable equative copula (is/am/are)"
            : "Inshinga nkene y'ubunyagatifu \xe2\x80\x93 invariable negative copula (is not)";
        printf("  \342\224\224\342\224\200 Uturemajambo (Morphemes): %s (%s)\n",
               t->lower, gloss);
        return;
    }

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
                if (t->noun_class > 0) {
                    char nc_buf[16];
                    printf(" %s(SP\xC2\xB7%s)", form,
                           sp_nc_label(t->noun_class, t->verb_tense, nc_buf, sizeof(nc_buf)));
                } else
                    printf(" %s(SP)", form);   /* personal pronoun class */
            } else if (strcmp(m->label, "OM") == 0) {
                printf(" %s(OM\xC2\xB7Nt.%d)", form, t->obj_class);
            } else if (strcmp(m->label, "root") == 0) {
                printf(" %s(root)", form);
            } else if (strcmp(m->label, "REV") == 0) {
                /* Reversive with nasal+r→d rule: show underlying/surface pair */
                printf(" %s\xe2\x86\x92%s(REV\xc2\xb7Ngiruka)", form, m->surface);
            } else if (strcmp(m->label, "EXT") == 0) {
                printf(" %s(EXT)", form);
            } else if (strcmp(m->label, "LOC") == 0) {
                printf(" %s(ahantu)", form);
            } else {
                /* TM, COND, FV: show underlying form */
                printf(" %s(%s)", form, m->label);
            }
        }
        /* Reflexive note: when morph chain is filled, the REFL slot already
         * appears in the chain above; this fallback line is only for the old
         * bare-subjunctive case where 'i' was elided entirely.              */
        if (t->verb_ext == VEXT_REFLEXIVE) {
            bool refl_i_present = t->stem[0] && !is_vowel_c(t->stem[0]);
            if (!refl_i_present)
                printf(" + [i-(imbundo, elided from kwi-)]");
        }
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
    {
        char nc_buf[16];
        printf("  \342\224\224\342\224\200 Uturemajambo (Morphemes): %s(SP\xC2\xB7%s)", sp,
               sp_nc_label(t->noun_class, t->verb_tense, nc_buf, sizeof(nc_buf)));
    }
    if (tm[0])  printf(" + %s(TM)", tm);
    if (om[0])  printf(" + %s(OM\xC2\xB7Nt.%d)", om, t->obj_class);
    if (t->verb_ext == VEXT_REFLEXIVE) {
        bool refl_i_present = t->stem[0] && !is_vowel_c(t->stem[0]);
        if (refl_i_present) printf(" + i(REFL)");
        else                printf(" + [i-(imbundo, elided from kwi-)]");
    }
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
        case TENSE_PAST_PERF:
            return "SP imere y'igihe gishize (ya/wa/ba/...) + FV='ye'/'iye'/'tse'\n"
                   "       Nta mwanya wa TM kuri Impitakare \xe2\x80\x94 igihe gitangaza mu meso\n"
                   "       y'SP (ya \xe2\x86\x92 Nt.1 past; wa \xe2\x86\x92 Nt.3; ba \xe2\x86\x92 Nt.2...) no mu FV 'ye'";
        case TENSE_PAST_IMPF:
            return "SP imere y'igihe gishize + FV='aga'\n"
                   "       Nta mwanya wa TM kuri Imvangura \xe2\x80\x94 igihe gitangaza mu SP + FV 'aga'";
        case TENSE_FUTURE:          return "TM='za' iboneka mu mwanya wa 2";
        case TENSE_NARRATIVE:       return "TM='ka' iboneka mu mwanya wa 2";
        case TENSE_OPTATIVE:        return "TM='raka' iboneka mu mwanya wa 2";
        case TENSE_SUBJUNCTIVE:     return "FV='e' mu iherezo ry'ijambo";
        case TENSE_SUBJUNCTIVE_LOC: return "FV='e' + ahantu (ho/mo/yo)";
        case TENSE_CONDITIONAL:     return "SP + intera 'a' + root + FV='a'";
        case TENSE_NEG_RELATIVE:    return "NEG='ta' iboneka mu mwanya wa 2";
        case TENSE_COPULA_PAST:     return "SP(impitagihe) + 'ri' + ahantu";
        case TENSE_COPULA_PRES:     return "SP(indagihe) + 'ri' + ahantu";
        case TENSE_STATIVE_POSS:    return "SP + -fite (FV='e', igicumbi cya kugira)";
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

    /* Equative/negative copula: invariable, no SP+TM+root+FV reconstruction. */
    if (strcmp(t->lower, "ni") == 0 || strcmp(t->lower, "si") == 0) {
        printf("  \342\224\224\342\224\200 Gusubiza (Reconstruction):\n");
        printf("       Ingingo: [copula]%s (invariable)  \342\206\222  %s  \342\234\223\n",
               t->lower, t->surface);
        return;
    }

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
            if (strcmp(m->label, "SP") == 0) {
                char nc_buf[16];
                printf("(%s)", sp_nc_label(t->noun_class, t->verb_tense,
                                           nc_buf, sizeof(nc_buf)));
            } else if (strcmp(m->label, "OM") == 0)
                printf("(Nt.%d)", t->obj_class);
        }

        if (has_rule) {
            printf("\n");
            /* Itegeko: each rule that fired */
            for (int i = 0; i < mb->n; i++)
                if (mb->m[i].rule[0])
                    printf("       Itegeko:  %s\n", mb->m[i].rule);
            /* Guhuza: underlying morphemes → surface word.
             * Amategeko y'igenamajwi rule: compound consonants (by, tw, kw,
             * ry, etc.) are surface artefacts — they can only appear in the
             * igicumbi (root).  All other slots (SP, TM, OM, EXT, FV) MUST
             * show the underlying form, never the phonological surface form.
             * The Itegeko line above already shows which rule fired and how
             * the surface is derived.  Guhuza = underlying pieces → word.
             *
             * Skipping rule:
             *   • Skip when BOTH form and surface are empty (truly invisible,
             *     e.g. conditional TM 'a' elided by a+a→a).
             *   • Explicit-zero TM (form="∅", surface="") IS shown.
             *   • Absorbed EXT (e.g. "y" absorbed into root via r+y→z) IS
             *     shown via its form — the Itegeko explains the fusion. */
            printf("       Guhuza:  ");
            bool first_g = true;
            for (int i = 0; i < mb->n; i++) {
                const KinMorpheme *m = &mb->m[i];
                /* Determine what to display: always prefer underlying form */
                const char *disp = m->form[0] ? m->form : m->surface;
                /* Skip truly invisible morphemes (both form and surface empty) */
                if (!disp[0]) continue;
                if (!first_g) printf(" + ");
                printf("%s", disp);
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
                   ? ((llen >= 3 && kin_ends_with(lword, "tse")) ? "tse"
                    : (llen >= 3 && kin_ends_with(lword, "iye")) ? "iye" : "ye")
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
        case VEXT_CAUSATIVE_Y: ext_sfx = "y"; break;
        default: break;
    }
    /* For VEXT_CAUSATIVE_Y the reconstruction must apply r+y→z:
     * root ends in 'r' (citation form); root[-1]='r' + 'y' → surface 'z'.
     * Replace: built = sp+tm+om + root[0..n-2]+'z' + fv  (drop both r and y) */
    char built[KIN_MAX_WORD];
    if (t->verb_ext == VEXT_CAUSATIVE_Y) {
        size_t rlen = strlen(root);
        char root_z[KIN_MAX_STEM];
        if (rlen >= 1) {
            strncpy(root_z, root, rlen - 1);
            root_z[rlen - 1] = 'z';
            root_z[rlen] = '\0';
        } else {
            strncpy(root_z, root, KIN_MAX_STEM - 1);
            root_z[KIN_MAX_STEM-1] = '\0';
        }
        snprintf(built, sizeof(built), "%s%s%s%s%s", sp, tm, om, root_z, fv);
    } else {
        snprintf(built, sizeof(built), "%s%s%s%s%s%s", sp, tm, om, root, ext_sfx, fv);
    }
    bool matches = (strcmp(built, lword) == 0);
    printf("  \342\224\224\342\224\200 Gusubiza (Reconstruction):\n");
    {
        char nc_buf[16];
        printf("       Ingingo:  [SP]%s(%s)", sp,
               sp_nc_label(t->noun_class, t->verb_tense, nc_buf, sizeof(nc_buf)));
    }
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

        /* Check if any morpheme has a phonological rule */
        bool any_rule = false;
        for (int i = 0; i < mb->n; i++)
            if (mb->m[i].rule[0]) { any_rule = true; break; }

        /* Helper: surface of a morpheme (fall back to form when surface is empty).
         * Exception: for absorbed morphemes (e.g. EXT "y" in VEXT_CAUSATIVE_Y),
         * the surface is intentionally "" — callers should guard with [0] check
         * before printing so the morpheme is silently omitted from Guhuza.      */
/* For Guhuza: always prefer UNDERLYING form.  Compound consonants (kw, gw,
 * by, ry...) are surface artefacts — they may only appear in the igicumbi
 * (root).  All other slots (PREF, EXT, FV) must show underlying forms;
 * the Itegeko line already explains any phonological changes.             */
#define INF_FORM(m) ((m)->form[0] ? (m)->form : (m)->surface)
/* For the inline (no-rule) path we still reconstruct what was actually
 * written, so surface is fine there — compound consonants in the prefix
 * only arise when a rule fired, which takes the any_rule branch anyway.  */
#define INF_SURF(m) ((m)->surface[0] ? (m)->surface : (m)->form)

        if (any_rule) {
            /* Separate Itegeko + Guhuza lines */
            printf("\n");
            for (int i = 0; i < mb->n; i++)
                if (mb->m[i].rule[0])
                    printf("       Itegeko:  %s\n", mb->m[i].rule);
            /* Guhuza: UNDERLYING forms on the left, surface word on the right.
             * Compound consonants (kw, gw...) must not appear in non-root slots.
             * For absorbed EXT (CAUSATIVE_Y: surface=""), still show underlying
             * form so the reader sees what was absorbed; Itegeko explains fusion. */
            printf("       Guhuza:   %s", INF_FORM(pref_m));
            printf(" + %s", INF_FORM(root_m));
            if (ext_m && (ext_m->form[0] || ext_m->surface[0]))
                printf(" + %s", INF_FORM(ext_m));
            printf(" + %s", INF_FORM(fv_m));
            if (loc_m)  printf(" + %s", INF_FORM(loc_m));
            printf("  \342\206\222  %s%s\n", lword, mb->verified ? "  \342\234\223" : "");
        } else {
            /* No rules fired: inline reconstruction on same Ingingo line */
            printf("  \342\206\222  %s", INF_SURF(pref_m));
            printf("%s", INF_SURF(root_m));
            if (ext_m)  printf("%s", INF_SURF(ext_m));
            printf("%s", INF_SURF(fv_m));
            if (loc_m)  printf("%s", INF_SURF(loc_m));
            printf("%s\n", mb->verified ? "  \342\234\223" : "");
        }
#undef INF_FORM
#undef INF_SURF
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

/* Print adjective (ntera) morpheme breakdown: RS + C.
 * Reads tok->morph filled by analyse_adj in morph_dispatch.c.
 * Formula (Tree 2): RS (Indangasano) + C (Igicumbi)                       */
static void print_adj_morphemes(const Token *t) {
    const MorphBreakdown *mb = &t->morph;
    if (mb->n < 2) {
        /* Fallback if morph not filled */
        if (t->detected_prefix[0] && t->stem[0])
            printf("  \342\224\224\342\224\200 Uturemajambo (Morphemes): %s(RS\xC2\xB7Nt.%d) + %s(C)\n",
                   t->detected_prefix, t->noun_class, t->stem);
        else if (t->stem[0])
            printf("  \342\224\224\342\224\200 Igicumbi (Stem): %s\n", t->stem);
        return;
    }

    const KinMorpheme *rs_m = &mb->m[0];   /* RS (Indangasano)  */
    const KinMorpheme *c_m  = &mb->m[1];   /* C  (Igicumbi)     */

    /* Detect reduplication: RS + C + RS + C (indorerezi)
     * e.g. muremure = mu+re+mu+re, byorobyoro = by+oro+by+oro
     * Must be checked BEFORE printing Uturemajambo so we can show all 4 slots. */
    char red_check[KIN_MAX_WORD];
    snprintf(red_check, sizeof(red_check), "%s%s%s%s",
             rs_m->surface, c_m->surface, rs_m->surface, c_m->surface);
    bool is_reduplicated = (!mb->verified && strcmp(red_check, t->lower) == 0);

    /* Uturemajambo line: show ALL morpheme slots.
     * Reduplicated form shows 4 slots (RS+C+RS+C); plain form shows 2 (RS+C). */
    if (is_reduplicated) {
        printf("  \342\224\224\342\224\200 Uturemajambo (Morphemes): "
               "%s(RS\xC2\xB7Nt.%d) + %s(C) + %s(RS\xC2\xB7Nt.%d) + %s(C)"
               "  [indorerezi]\n",
               rs_m->form, t->noun_class, c_m->form,
               rs_m->form, t->noun_class, c_m->form);
    } else {
        printf("  \342\224\224\342\224\200 Uturemajambo (Morphemes): %s(RS\xC2\xB7Nt.%d) + %s(C)\n",
               rs_m->form, t->noun_class, c_m->form);
    }

    /* Reconstruction */
    bool rs_changed = (strcmp(rs_m->form, rs_m->surface) != 0);
    printf("  \342\224\224\342\224\200 Gusubiza (Reconstruction):\n");
    if (is_reduplicated) {
        /* Reduplicated: show underlying 4-slot Ingingo then surface Guhuza */
        printf("       Ingingo: [RS]%s(Nt.%d) + [C]%s + [RS]%s(Nt.%d) + [C]%s\n",
               rs_m->form, t->noun_class, c_m->form,
               rs_m->form, t->noun_class, c_m->form);
        if (rs_changed)
            printf("       Itegeko:  %s\n", rs_m->rule);
        /* Guhuza shows UNDERLYING forms; Itegeko already explains surface changes */
        printf("       Guhuza:  %s + %s + %s + %s  \342\206\222  %s  \342\234\223\n",
               rs_m->form, c_m->form, rs_m->form, c_m->form, t->lower);
    } else if (rs_changed) {
        printf("       Ingingo: [RS]%s(Nt.%d) + [C]%s\n",
               rs_m->form, t->noun_class, c_m->form);
        printf("       Itegeko:  %s\n", rs_m->rule);
        /* Guhuza shows UNDERLYING forms; surface fusion is explained by Itegeko above */
        printf("       Guhuza:  %s + %s  \342\206\222  %s%s\n",
               rs_m->form, c_m->form, t->lower,
               mb->verified ? "  \342\234\223" : "");
    } else {
        printf("       Ingingo: [RS]%s(Nt.%d) + [C]%s  \342\206\222  %s%s\n",
               rs_m->form, t->noun_class, c_m->form,
               t->lower, mb->verified ? "  \342\234\223" : "");
    }
}

/* ── Pronoun connector prefix for a given noun class ────────────────────── */
static const char *pron_connector(int cls) {
    switch (cls) {
        case  1: return "wa";  case  2: return "ba";  case  3: return "wa";
        case  4: return "ya";  case  5: return "rya"; case  6: return "ya";
        case  7: return "cya"; case  8: return "bya"; case  9: return "ya";
        case 10: return "za";  case 11: return "rwa"; case 12: return "ka";
        case 13: return "twa"; case 14: return "bwa"; case 15: return "kwa";
        case 16: return "ha";
        default: return "";
    }
}

/* ── Map back-reference suffix to class description ─────────────────────── */
typedef struct { const char *suf; int ref_cls; const char *label; } BackRef;
/* Ordered longest-first to avoid prefix false-matches */
static const BackRef BACKREF_TABLE[] = {
    { "njye",  0, "1sg emp. (wanjye/\"mine\")" },
    { "byo",   8, "Nt.8 ref. (\"of those things\")" },
    { "ryo",   5, "Nt.5 ref. (\"of it/that\")" },
    { "cyo",   7, "Nt.7 ref. (\"of it\")" },
    { "rwo",  11, "Nt.11 ref. (\"of it\")" },
    { "kwo",  15, "Nt.15 ref. (\"of it\")" },
    { "two",  13, "Nt.13 ref. (\"of them\")" },
    { "bwo",  14, "Nt.14 ref. (\"of it\")" },
    { "nge",   0, "1sg (\"mine/my\")" },
    { "nyu",   0, "2pl (\"yours pl./your\")" },
    { "bo",    2, "3pl human Nt.2 (\"their/theirs\")" },
    { "wo",    3, "Nt.3 ref. (\"of it\")" },
    { "yo",    4, "Nt.4/6/9 ref. (\"of them/its\")" },
    { "zo",   10, "Nt.10 ref. (\"of them\")" },
    { "ko",   12, "Nt.12 ref. (\"of it\")" },
    { "ho",   16, "Nt.16 ref. (\"of there\")" },
    { "cu",    0, "1pl (\"ours/our\")" },
    { "we",    0, "2sg (\"yours sg./your\")" },
    { "e",     0, "3sg contracted (\"his/her/its\")" },
    { NULL,    0, NULL }
};

/* Print pronoun morpheme breakdown and class info.                         */
static void print_pronoun_morphemes(const Token *t)
{
    if (t->pron_type == PRON_REFLEXIVE) {
        /* Find the connector prefix used for this head class */
        const char *conn = pron_connector(t->noun_class);
        size_t clen = strlen(conn);
        if (clen == 0 || strncmp(t->lower, conn, clen) != 0) return;
        const char *suffix = t->lower + clen;

        /* Match suffix against back-reference table */
        const char *ref_label = NULL;
        int         ref_cls   = 0;
        for (int i = 0; BACKREF_TABLE[i].suf; i++) {
            if (strcmp(suffix, BACKREF_TABLE[i].suf) == 0) {
                ref_label = BACKREF_TABLE[i].label;
                ref_cls   = BACKREF_TABLE[i].ref_cls;
                break;
            }
        }

        /* Morpheme line */
        if (ref_label) {
            printf("  \342\224\224\342\224\200 Uturemajambo (Morphemes):"
                   " %s(IC\xC2\xB7Nt.%d) + %s(IK\xC2\xB7%s)\n",
                   conn, t->noun_class, suffix, ref_label);
            /* Meaning gloss */
            if (ref_cls > 0)
                printf("  \342\224\224\342\224\200 Ibisobanuro: isano ry'izina Nt.%d"
                       " rifatiye ku nyirabyo Nt.%d\n",
                       t->noun_class, ref_cls);
            else
                printf("  \342\224\224\342\224\200 Ibisobanuro: isano ry'izina Nt.%d"
                       " rifatiye ku %s\n",
                       t->noun_class, ref_label);
        } else {
            /* Unknown suffix: still show connector split */
            printf("  \342\224\224\342\224\200 Uturemajambo (Morphemes):"
                   " %s(IC\xC2\xB7Nt.%d) + %s\n",
                   conn, t->noun_class, suffix);
        }

    } else if (t->pron_type == PRON_POSSESSIVE) {
        /* Bare connector — show class agreement */
        printf("  \342\224\224\342\224\200 Uturemajambo: %s(IC\xC2\xB7Nt.%d)"
               "  [inshingano y'uburonko / possessive connector]\n",
               t->lower, t->noun_class);

    } else if (t->pron_type == PRON_PERSONAL && t->noun_class > 0) {
        printf("  \342\224\224\342\224\200 Inteko: Nt.%d\n", t->noun_class);

    } else if (t->pron_type == PRON_DEMONSTRATIVE && t->noun_class > 0) {
        printf("  \342\224\224\342\224\200 Inteko: Nt.%d\n", t->noun_class);

    } else if (t->pron_type == PRON_RELATIVE && t->noun_class > 0) {
        printf("  \342\224\224\342\224\200 Inteko: Nt.%d"
               "  [isaku nyejuru distinguishes from demonstrative]\n",
               t->noun_class);

    } else if (t->pron_type == PRON_INDEFINITE && t->noun_class > 0) {
        printf("  \342\224\224\342\224\200 Inteko: Nt.%d\n", t->noun_class);

    } else if (t->pron_type == PRON_NUMERICAL && t->noun_class > 0) {
        printf("  \342\224\224\342\224\200 Inteko: Nt.%d\n", t->noun_class);
    }
}

/* Print noun morpheme breakdown line (D + RT + C separately).             */
static void print_noun_morphemes(const Token *t) {
    const MorphBreakdown *mb = &t->morph;
    if (mb->n >= 3) {
        /* Use structured breakdown from kin_morpheme_analyze */
        const KinMorpheme *d  = &mb->m[0];
        const KinMorpheme *rt = &mb->m[1];
        const KinMorpheme *c  = &mb->m[2];
        /* For deverbative nouns the igicumbi is verb_root + FV (e.g. sanzur+e).
         * Show the split as [C·root] + [C·FV] so the verbal origin is explicit. */
        size_t vrl = t->verb_root[0] ? strlen(t->verb_root) : 0;
        bool c_split = (t->is_deverbative && vrl > 0
                        && strncmp(c->form, t->verb_root, vrl) == 0
                        && strlen(c->form) > vrl);
        if (c_split) {
            char c_fv = c->form[vrl];
            printf("  \342\224\224\342\224\200 Uturemajambo (Morphemes): "
                   "[D]%s + [RT]%s(Nt.%d) + [C\xC2\xB7root]%s + [C\xC2\xB7" "FV]%c\n",
                   d->form, rt->form, t->noun_class, t->verb_root, c_fv);
        } else {
            printf("  \342\224\224\342\224\200 Uturemajambo (Morphemes): "
                   "[D]%s + [RT]%s(Nt.%d) + [C]%s\n",
                   d->form, rt->form, t->noun_class, c->form);
        }
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
         *   voiced C (b,d,g,j,r,v,z,m,n,y,c): "ku" + stem + "a"
         *   voiceless C:    "gu" + stem + "a"                              */
        char r0 = t->verb_root[0];
        bool is_vowel = (r0=='a'||r0=='e'||r0=='i'||r0=='o'||r0=='u');
        bool is_voiced = (r0=='b'||r0=='d'||r0=='g'||r0=='j'||r0=='r'||
                          r0=='v'||r0=='z'||r0=='m'||r0=='n'||r0=='y'||
                          r0=='c');   /* c = palatal affricate, uses ku  */
        if (r0 == 'w') {
            /* ku + w-initial stem: drop leading 'w', use "kw" prefix */
            printf("  └─ Ivuye mu nshinga (Deverbative): igicumbi -%s-"
                   " (cf. kw%sa)\n", t->verb_root, t->verb_root + 1);
        } else {
            const char *pfx;
            if (is_vowel && (r0 == 'o' || r0 == 'u'))
                pfx = "k";    /* u→∅/_[o,u] §1.2 */
            else if (is_vowel)
                pfx = "kw";   /* u→w §1.1 */
            else if (is_voiced)
                pfx = "ku";
            else
                pfx = "gu";
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

        /* Verbal noun: displayed class is the OM class (the noun class),
         * not the SP class (which is a verb-analysis artefact).             */
        int display_class = t->noun_class;
        if (t->gram_role == GRAM_ROLE_VERBAL_NOUN && t->obj_class > 0)
            display_class = t->obj_class;

        char class_str[16] = "";
        if (display_class > 0)
            snprintf(class_str, sizeof(class_str), "Nt.%d", display_class);

        /* POS label: verbal noun → special; pronoun → show subtype; else generic */
        const char *pos_label;
        if (t->gram_role == GRAM_ROLE_VERBAL_NOUN)
            pos_label = "Izina mbonera (ivuye mu nshinga)";
        else if (t->pos == POS_PRONOUN && t->pron_type != PRON_NONE)
            pos_label = kin_pron_type_name(t->pron_type);
        else
            pos_label = kin_pos_name(t->pos);

        /* Punctuation tokens: display compactly and skip morpheme detail */
        if (t->pos == POS_PUNCTUATION) {
            const char *pname = "";
            switch (t->punct_type) {
                /* Sources: REB S5 Kinyarwanda SB – Imikoreshereze y'utwatuzo (p.82-83) */
                case PUNCT_COMMA:       pname = "Akitso (`,`)";                  break;
                case PUNCT_PERIOD:      pname = "Akabago / Akadomo (`.`)";       break;
                case PUNCT_QUESTION:    pname = "Akabazo (`?`)";                 break;
                case PUNCT_EXCLAIM:     pname = "Agatangaro (`!`)";              break;
                case PUNCT_SEMICOLON:   pname = "Akabago n'akitso (`;`)";        break;
                case PUNCT_COLON:       pname = "Utubago tubiri (`:`)";          break;
                case PUNCT_QUOTE_OPEN:  pname = "Utwuguruzo (`\"`  ouvrant)";   break;
                case PUNCT_QUOTE_CLOSE: pname = "Utwugarizo (`\"` fermant)";    break;
                default:                pname = "Ikimenyetso cy'utwatuzo";       break;
            }
            printf("  `%s`  %s\n", t->surface, pname);
            if (t->is_sent_boundary && t->punct_type == PUNCT_PERIOD)
                printf("        [Igenanzira: iherezo ry'interuro ihamya/itegeka]\n");
            else if (t->is_sent_boundary && t->punct_type == PUNCT_QUESTION)
                printf("        [Igenanzira: iherezo ry'interuro ibaza]\n");
            else if (t->is_sent_boundary && t->punct_type == PUNCT_EXCLAIM)
                printf("        [Igenanzira: iherezo ry'interuro itangara / inyuma y'amarangamutima]\n");
            else if (t->is_sent_boundary)
                printf("        [Igenanzira: iherezo ry'interuro]\n");
            if (t->is_clause_boundary)
                printf("        [Igenanzira: kuhumeka akanya gato mu nteruro]\n");
            if (t->is_quote_open)
                printf("        [Igenanzira: intangiriro y'amagambo yateruwe cyangwa avuzwe]\n");
            if (t->is_quote_close)
                printf("        [Igenanzira: iherezo ry'amagambo yateruwe cyangwa avuzwe]\n");
            putchar('\n');
            continue;
        }

        /* Mark tokens with errors */
        char marker = (t->error_count > 0) ? '!' : ' ';
        printf("%c%-19s  %-30s  %-8s  %-8s\n",
               marker,
               t->surface,
               pos_label,
               class_str,
               t->stem);

        /* ── Verbal noun: morphologically a verb, syntactically a noun ───── *
         * Show: verb morpheme breakdown (linguistically correct), noun class, *
         * and the verb it was derived from.  Suppress tense info.            */
        if (t->gram_role == GRAM_ROLE_VERBAL_NOUN) {
            /* 1. Verb morpheme breakdown (the analysis is correct) */
            print_verb_morphemes(t);
            /* 2. Nominal class note (kin_class_name already includes "Nt.X –") */
            printf("  \342\224\224\342\224\200 Indanganteko (Nominal class): %s\n",
                   kin_class_name(t->obj_class));
            /* 3. Deverbative origin: build citation infinitive */
            {
                char r0 = t->stem[0];
                bool vn_vowel  = (r0=='a'||r0=='e'||r0=='i'||r0=='o'||r0=='u');
                bool vn_voiced = (r0=='b'||r0=='d'||r0=='g'||r0=='j'||r0=='r'||
                                  r0=='v'||r0=='z'||r0=='m'||r0=='n'||r0=='y'||
                                  r0=='c');
                const char *pfx;
                if (vn_vowel && (r0 == 'o' || r0 == 'u'))
                    pfx = "k";
                else if (vn_vowel)
                    pfx = "kw";
                else if (vn_voiced)
                    pfx = "ku";
                else
                    pfx = "gu";
                printf("  \342\224\224\342\224\200 Ivuye mu nshinga (Derived from verb): %s%sa (igicumbi -%s-)\n",
                       pfx, t->stem, t->stem);
            }
            /* 4. Reconstruction */
            print_verb_reconstruction(t);
            putchar('\n');
            continue;   /* skip the regular verb/noun branches below */
        }

        /* Verb details
         * Flow: type detected → morphemes → tense confirmed from morpheme
         * positions → reconstruction (verify morphemes → exact surface word). */
        if (t->pos == POS_VERB_CONJ || t->pos == POS_VERB_INF) {
            if (t->is_negative)
                printf("  └─ INSHINGA Y'UBUNYAGATIFU (Negative verb)\n");
            if (t->pos == POS_VERB_CONJ) {
                /* 1. Morphemes first */
                print_verb_morphemes(t);
                /* 1b. Citation infinitive (imbundo) derived from the verb root.
                 * Same prefix-selection logic as the verbal-noun branch above:
                 *   vowel-initial root → kw+root+a   (u→w §1.1, e.g. kwiga)
                 *   voiced-initial root → ku+root+a  (e.g. kubara, kubona)
                 *   voiceless-initial root → gu+root+a (e.g. gukwira, gusoma) */
                if (t->stem[0]) {
                    /* TENSE_STATIVE_POSS: the -fite paradigm (bifite, afite…)
                     * is a suppletive possessive form of "kugira" (to have).
                     * The surface root "fit" has no real infinitive "gufita";
                     * cite "kugira" instead with a note on the suppletive root. */
                    if (t->verb_tense == TENSE_STATIVE_POSS) {
                        printf("  \342\224\224\342\224\200 Imbundo (Citation verb): kugira"
                               "  (igicumbi ry'inshinga yo gutunga: -fit-)\n");
                        printf("  \342\224\224\342\224\200 Ibisobanuro: '-fite' ni umuzi w'indangika"
                               " (suppletive root) wa 'kugira' mu nshinga yo gutunga;\n"
                               "       FV='e' ni inyerera y'inshinga ngirika"
                               " \342\200\224 ntaho ihinduka ry'a\342\206\222e ribaho;\n"
                               "       kandi ntakintu kijyana na 'gufata' (to grab).\n");
                    } else {
                    char ci_r0 = t->stem[0];
                    bool ci_vowel  = (ci_r0=='a'||ci_r0=='e'||ci_r0=='i'||
                                      ci_r0=='o'||ci_r0=='u');
                    bool ci_voiced = (ci_r0=='b'||ci_r0=='d'||ci_r0=='g'||
                                      ci_r0=='j'||ci_r0=='r'||ci_r0=='v'||
                                      ci_r0=='z'||ci_r0=='m'||ci_r0=='n'||
                                      ci_r0=='y'||ci_r0=='c');
                    /* Prefix selection: standard rule is kw+V (u→w §1.1).
                     * Exception §1.2 — u elides (u→∅) before back vowels:
                     *   ku + o-initial → ko... (kororoka, not *kwororoka)
                     *   ku + u-initial → ku... (kuzura, not *kwuzura)
                     * In these cases the surface prefix is just 'k', and the
                     * stem's leading vowel is visible: k·ororok·a = kororoka. */
                    const char *ci_pfx;
                    if (ci_vowel && (ci_r0 == 'o' || ci_r0 == 'u'))
                        ci_pfx = "k";       /* u→∅/_[o,u] §1.2 */
                    else if (ci_vowel)
                        ci_pfx = "kw";      /* u→w §1.1 before a/e/i        */
                    else if (ci_voiced)
                        ci_pfx = "ku";
                    else
                        ci_pfx = "gu";
                    /* TENSE_SUBJUNCTIVE_LOC: the locative suffix is part of the
                     * base verb form (e.g. kubaho, kubamo, kubayo).  Detect the
                     * suffix from the last 2 chars of the surface word and append
                     * it to the citation so the reader sees the full infinitive. */
                    const char *ci_loc = "";
                    if (t->verb_tense == TENSE_SUBJUNCTIVE_LOC) {
                        size_t wl = strlen(t->lower);
                        if (wl >= 2) {
                            if      (t->lower[wl-2]=='h' && t->lower[wl-1]=='o') ci_loc = "ho";
                            else if (t->lower[wl-2]=='m' && t->lower[wl-1]=='o') ci_loc = "mo";
                            else if (t->lower[wl-2]=='y' && t->lower[wl-1]=='o') ci_loc = "yo";
                        }
                    }
                    /* Suppletive stem mapping: some roots are suppletive past
                     * stems whose canonical citation is a different infinitive.
                     *   giy → kugenda  (giye/bagiye ← kugenda, not *kugiya)
                     *   jye → kujya    (jyeye/bajye – alternate: kujya is valid)
                     * When a suppletive stem is found, display the canonical
                     * infinitive and note the suppletive root in parentheses. */
                    {
                        static const struct {
                            const char *stem;
                            const char *canonical;  /* full canonical infinitive  */
                            const char *note;        /* brief note for the user    */
                        } SUPPLETIVE_CITATIONS[] = {
                            { "giy", "kugenda",
                              "umuzi w'indangika (suppletive past stem) wa kugenda" },
                            { NULL, NULL, NULL }
                        };
                        bool supp_found = false;
                        for (int si = 0; SUPPLETIVE_CITATIONS[si].stem; si++) {
                            if (strcmp(t->stem, SUPPLETIVE_CITATIONS[si].stem) == 0) {
                                printf("  \342\224\224\342\224\200 Imbundo (Citation verb): %s"
                                       "  (igicumbi -%s-)\n",
                                       SUPPLETIVE_CITATIONS[si].canonical, t->stem);
                                printf("  \342\224\224\342\224\200 Icyitonderwa (Suppletive note):"
                                       " -%s- ni %s\n",
                                       t->stem, SUPPLETIVE_CITATIONS[si].note);
                                supp_found = true;
                                break;
                            }
                        }
                        if (!supp_found)
                    printf("  \342\224\224\342\224\200 Imbundo (Citation verb): %s%sa%s"
                           "  (igicumbi -%s-)\n",
                           ci_pfx, t->stem, ci_loc, t->stem);
                    }

                    /* Homograph disambiguation: some roots correspond to two
                     * distinct verbs that differ only in vowel length, which
                     * standard Kinyarwanda orthography does not mark.
                     * Show both readings so the user is aware of the ambiguity. */
                    {
                        static const struct { const char *root; const char *note; } HOMOGRAPH_ROOTS[] = {
                            { "sig",
                              "gusiga (guturika/gusigara \342\200\224 to leave/to remain, igicumbi fupi)"
                              " | gusiiga (gushora amavuta/gusiga ibara \342\200\224 to paint/anoint,"
                              " igicumbi kirekire) \342\200\224 bitandukanye n'uburebure bw'ijwi gusa"
                              " (distinguished by vowel length in speech only)" },
                            { NULL, NULL }
                        };
                        for (int hi = 0; HOMOGRAPH_ROOTS[hi].root; hi++) {
                            if (strcmp(t->stem, HOMOGRAPH_ROOTS[hi].root) == 0) {
                                printf("  \342\224\224\342\224\200 Icyitonderwa (Homograph note):\n"
                                       "       %s\n", HOMOGRAPH_ROOTS[hi].note);
                                break;
                            }
                        }
                    }

                    /* Reflexive derivative citation: when VEXT_REFLEXIVE with
                     * consonant-initial root (i- present in surface), show
                     * the reflexive infinitive kwi+root_alt+a as a secondary
                     * citation alongside the base verb.                       */
                    if (t->verb_ext == VEXT_REFLEXIVE && !is_vowel_c(ci_r0)) {
                        size_t slen = strlen(t->stem);
                        char refl_root[KIN_MAX_STEM];
                        strncpy(refl_root, t->stem, slen);
                        refl_root[slen] = '\0';
                        /* Apply nd→nz for the reflexive infinitive display */
                        if (slen >= 2 && refl_root[slen - 1] == 'd')
                            refl_root[slen - 1] = 'z';
                        printf("  \342\224\224\342\224\200 Inyandiko yo kwisanzura"
                               " (Reflexive infinitive): kwi%sa"
                               "  (nd\xe2\x86\x92nz mutation)\n", refl_root);
                    }

                    /* Deep root analysis: detect nasal + reversive -uk-/-ur- derivation.
                     * Rule: a nasal-final root (-n or -m) + reversive -uk- triggers
                     * epenthetic -d- insertion between nasal and extension vowel:
                     *   -tan- + -uk- → -tan·d·uk- = -tanduk-   (n+u → ndu)
                     *   -fung- + -uk- → -funguk- (no epenthesis; g is not a nasal)
                     * Strip -uk or -ur from stem; if result ends in -nd or -mb,
                     * remove the epenthetic stop to recover the nasal-final base root.
                     * If the recovered base is a known verb stem, display the chain. */
                    {
                        size_t slen = strlen(t->stem);
                        bool has_rev_uk = slen > 4 &&
                            t->stem[slen-1]=='k' && t->stem[slen-2]=='u';
                        bool has_rev_ur = slen > 4 &&
                            t->stem[slen-1]=='r' && t->stem[slen-2]=='u';
                        if (has_rev_uk || has_rev_ur) {
                            /* Strip the reversive -uk or -ur */
                            char inner[KIN_MAX_STEM];
                            size_t ilen = slen - 2;
                            strncpy(inner, t->stem, ilen);
                            inner[ilen] = '\0';
                            /* Check for epenthetic stop after nasal:
                             *   -nd  ← n + d (before -u- of reversive)
                             *   -mb  ← m + b (before -u- of reversive, rarer) */
                            char base[KIN_MAX_STEM];
                            bool found_base = false;
                            if (ilen >= 3) {
                                char last = inner[ilen-1];
                                char prev = inner[ilen-2];
                                if ((last == 'd' && prev == 'n') ||
                                    (last == 'b' && prev == 'm')) {
                                    strncpy(base, inner, ilen-1);
                                    base[ilen-1] = '\0';
                                    if (kin_is_known_verb_stem(base))
                                        found_base = true;
                                }
                            }
                            if (found_base) {
                                char b0 = base[0];
                                bool bv = (b0=='a'||b0=='e'||b0=='i'||b0=='o'||b0=='u');
                                bool bd = (b0=='b'||b0=='d'||b0=='g'||b0=='j'||
                                           b0=='r'||b0=='v'||b0=='z'||b0=='m'||
                                           b0=='n'||b0=='y'||b0=='c');
                                const char *bpfx = bv ? "kw" : bd ? "ku" : "gu";
                                const char *rev_sfx = has_rev_uk ? "uk" : "ur";
                                printf("  \342\224\224\342\224\200 Inkomoko y'igicumbi (Root etymology):\n");
                                printf("       Igicumbi cy'ibanze: -%s- \342\206\222 %s%sa"
                                       "  (igicumbi -%s-)\n",
                                       base, bpfx, base, base);
                                printf("       Itegeko: r\342\206\222d / n_ (ingombajwi r ihinduka d"
                                       " inyuma y'ingombajwi n):\n");
                                printf("                -%s- + -r%s- \342\206\222 -%s-\n",
                                       base, rev_sfx, t->stem);
                            }
                        }
                    }
                    } /* end else (non-STATIVE_POSS citation) */
                }
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
                if (t->verb_ext != VEXT_NONE) {
                    /* For VEXT_REFLEXIVE: distinguish present-i vs elided-i */
                    if (t->verb_ext == VEXT_REFLEXIVE &&
                        t->stem[0] && !is_vowel_c(t->stem[0]))
                        printf("  \342\224\224\342\224\200 Imbundo y'ikwisanzura"
                               " (Reflexive: i- present, nd\xe2\x86\x92nz mutation)\n");
                    else
                        printf("  \342\224\224\342\224\200 %s\n", kin_verb_ext_name(t->verb_ext));
                }
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
        /* Noun morpheme breakdown (Tree 1: izina mbonera + izina ntera) */
        if (t->pos == POS_NOUN || t->pos == POS_RELATIVE_NOUN) {
            if (t->pos == POS_RELATIVE_NOUN)
                printf("  \342\224\224\342\224\200 Izina ntera: noun qualifying preceding noun (Tree 1 \342\206\222 relative role)\n");
            print_noun_morphemes(t);
        }
        /* Adjective morpheme breakdown (Tree 2: ntera, RS + C) */
        if (t->pos == POS_ADJECTIVE)
            print_adj_morphemes(t);
        /* Pronoun morpheme breakdown (Tree 4: ikinyazina) */
        if (t->pos == POS_PRONOUN && t->pron_type != PRON_NONE)
            print_pronoun_morphemes(t);

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
