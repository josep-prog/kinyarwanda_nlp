/*
 * validator.c — Validation report output and multi-format file handling
 *
 * Public entry points:
 *   kin_validate_text(text)  — validate a sentence or paragraph string
 *   kin_validate_file(path)  — auto-detect .pdf / .doc[x] / plain-text and validate
 *
 * For each sentence the full pipeline runs:
 *   kin_analyze() → kin_check_punctuation() → print_validation_report()
 *
 * Output is bilingual (Kinyarwanda / English), styled to match the rest of
 * the engine's terminal output.  Violations are split into grammar/spelling
 * and punctuation categories in the per-sentence summary.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>    /* getpid */
#include "../include/kinyarwanda.h"

#define VAL_WIDTH   70          /* report box width in characters */
#define VAL_MAX_SEG 4096        /* max characters per sentence segment */

/* ── Error metadata ───────────────────────────────────────────────────────── */

static const char *error_type_tag(ErrorType t) {
    switch (t) {
        case ERR_ADJ_AGREEMENT:       return "ADJ_AGREEMENT";
        case ERR_POSS_AGREEMENT:      return "POSS_AGREEMENT";
        case ERR_NO_VERB:             return "NO_VERB";
        case ERR_UNKNOWN_WORD:        return "UNKNOWN_WORD";
        case ERR_INVALID_CLUSTER:     return "INVALID_CLUSTER";
        case ERR_SPELLING:            return "SPELLING";
        case ERR_SUBJ_VERB_AGREEMENT: return "SUBJ_VERB_AGREEMENT";
        case ERR_OBJ_VERB_AGREEMENT:  return "OBJ_VERB_AGREEMENT";
        case ERR_VOWEL_HIATUS:        return "VOWEL_HIATUS";
        case ERR_VERB_SELECTION:      return "VERB_SELECTION";
        case ERR_MISSING_PERIOD:      return "MISSING_PERIOD";
        case ERR_MISSING_COMMA:       return "MISSING_COMMA";
        case ERR_MISSING_QMARK:       return "MISSING_QMARK";
        case ERR_MISSING_COLON:       return "MISSING_COLON";
        case ERR_WRONG_PUNCT:         return "WRONG_PUNCT";
        case ERR_EXTRA_PUNCT:         return "EXTRA_PUNCT";
        default:                      return "ERROR";
    }
}

static const char *error_type_rw(ErrorType t) {
    switch (t) {
        case ERR_ADJ_AGREEMENT:       return "Indangasano y'intera";
        case ERR_POSS_AGREEMENT:      return "Ikinyazina ngenera";
        case ERR_NO_VERB:             return "Nta nshinga";
        case ERR_UNKNOWN_WORD:        return "Ijambo ridaziwe";
        case ERR_INVALID_CLUSTER:     return "Inzarara mbi";
        case ERR_SPELLING:            return "Imyandikire";
        case ERR_SUBJ_VERB_AGREEMENT: return "Indangasubizi";
        case ERR_OBJ_VERB_AGREEMENT:  return "Indangasobwa";
        case ERR_VOWEL_HIATUS:        return "Iranya ry'impanvu";
        case ERR_VERB_SELECTION:      return "Guhitamo inshinga";
        case ERR_MISSING_PERIOD:      return "Nta kirango cy'iherezo";
        case ERR_MISSING_COMMA:       return "Nta koma";
        case ERR_MISSING_QMARK:       return "Nta kibazo";
        case ERR_MISSING_COLON:       return "Nta koma ndende";
        case ERR_WRONG_PUNCT:         return "Ikirango kidahuye";
        case ERR_EXTRA_PUNCT:         return "Ikirango kirenze";
        default:                      return "Ikindi";
    }
}

static bool is_punct_error(ErrorType t) {
    return (t == ERR_MISSING_PERIOD || t == ERR_MISSING_COMMA  ||
            t == ERR_MISSING_QMARK  || t == ERR_MISSING_COLON  ||
            t == ERR_WRONG_PUNCT    || t == ERR_EXTRA_PUNCT);
}

/* ── Formatting helpers ───────────────────────────────────────────────────── */

static void print_rule(char c) {
    printf("  ");
    for (int i = 0; i < VAL_WIDTH - 2; i++) putchar(c);
    putchar('\n');
}

/*
 * Build a one-line context string from the token array.
 * The token at err_tok is wrapped in >>...<<.
 */
static void build_context(const SentenceAnalysis *sa, int err_tok,
                           char *out, size_t outsz) {
    out[0] = '\0';
    for (int i = 0; i < sa->token_count; i++) {
        const Token *t = &sa->tokens[i];
        /* Attach punctuation directly to previous token (no leading space) */
        if (t->pos == POS_PUNCTUATION) {
            strncat(out, t->surface, outsz - strlen(out) - 1);
            continue;
        }
        if (out[0] != '\0' && i > 0 && sa->tokens[i-1].pos != POS_PUNCTUATION)
            strncat(out, " ", outsz - strlen(out) - 1);

        if (i == err_tok) strncat(out, ">>", outsz - strlen(out) - 1);
        strncat(out, t->surface, outsz - strlen(out) - 1);
        if (i == err_tok) strncat(out, "<<", outsz - strlen(out) - 1);
    }
}

/*
 * Print the bilateral message split on the " / " separator that divides
 * the Kinyarwanda from the English portion of every message string.
 */
static void print_bilingual(const char *label_rw, const char *label_en,
                             const char *text) {
    const char *slash = strstr(text, " / ");
    if (slash) {
        printf("  %-10s: %.*s\n", label_rw, (int)(slash - text), text);
        printf("  %-10s: %s\n",   label_en, slash + 3);
    } else {
        printf("  %-10s: %s\n", label_en, text);
    }
}

/* ── Per-sentence report ──────────────────────────────────────────────────── */

static void print_report(const SentenceAnalysis *sa,
                         const char *input_text, int sent_num) {

    if (sent_num > 0)
        printf("  [Interuro %d / Sentence %d]\n  \"%s\"\n\n",
               sent_num, sent_num, input_text);

    if (sa->error_count == 0) {
        printf("  ✓ Ntamukono wabonetse — imvugo inoze. / "
               "No violations — sentence is correct.\n\n");
        return;
    }

    int gram_n = 0, punct_n = 0;
    for (int i = 0; i < sa->error_count; i++) {
        if (is_punct_error(sa->errors[i].type)) punct_n++;
        else gram_n++;
    }

    for (int i = 0; i < sa->error_count; i++) {
        const Error *e = &sa->errors[i];

        putchar('\n');
        print_rule('-');
        printf("  Ikibazo %d/%d  [%s  ·  %s]\n",
               i + 1, sa->error_count,
               error_type_tag(e->type),
               error_type_rw(e->type));
        print_rule('-');

        /* Token position */
        if (e->token_index >= 0 && e->token_index < sa->token_count)
            printf("  %-10s: '%s' (indangiriro %d / position %d)\n",
                   "Ijambo", sa->tokens[e->token_index].surface,
                   e->token_index + 1, e->token_index + 1);
        else
            printf("  %-10s: (interuro yose / whole sentence)\n", "Ijambo");

        /* Context */
        char ctx[VAL_MAX_SEG] = {0};
        build_context(sa, e->token_index, ctx, sizeof(ctx));
        if (ctx[0])
            printf("  %-10s: %s\n", "Context", ctx);

        /* Problem and fix — split on ' / ' for bilingual display */
        if (e->message[0])
            print_bilingual("Ikibazo", "Problem", e->message);
        if (e->suggestion[0])
            print_bilingual("Gusubiza", "Fix", e->suggestion);
    }

    putchar('\n');
    print_rule('=');
    printf("  Incamake: imikono %d — imvugo/imyandikire %d, ibirango %d\n",
           sa->error_count, gram_n, punct_n);
    printf("  Summary:  %d violation(s) — %d grammar/spelling, %d punctuation\n",
           sa->error_count, gram_n, punct_n);
    print_rule('=');
    putchar('\n');
}

/* ── Sentence splitting + single-sentence validation ─────────────────────── */

static void validate_segment(const char *seg, int sent_num, int *total) {
    SentenceAnalysis sa = kin_analyze(seg);
    kin_check_punctuation(&sa);
    print_report(&sa, seg, sent_num);
    *total += sa.error_count;
}

/*
 * Split text on sentence boundaries (. ! ?) and validate each fragment.
 * Uses the same splitting heuristic as analyse_text() in main.c.
 */
static void validate_text_block(const char *text, int *sent_num, int *total) {
    char seg[VAL_MAX_SEG];
    size_t si = 0;

    for (size_t i = 0; ; i++) {
        char c = text[i];
        if (c != '\0' && si < sizeof(seg) - 1)
            seg[si++] = c;

        bool end     = (c == '\0');
        bool is_term = (si > 0 &&
                        (seg[si-1] == '.' || seg[si-1] == '!' || seg[si-1] == '?'));
        bool next_ok = end || text[i+1] == ' ' || text[i+1] == '\n'
                           || text[i+1] == '\0';

        if ((is_term && next_ok) || end) {
            seg[si] = '\0';
            kin_str_trim(seg);
            if (seg[0]) {
                (*sent_num)++;
                validate_segment(seg, *sent_num, total);
            }
            si = 0;
            while (text[i+1] == ' ' || text[i+1] == '\n') i++;
        }
        if (end) break;
    }
    /* Flush any remainder that had no terminal punctuation */
    if (si > 0) {
        seg[si] = '\0';
        kin_str_trim(seg);
        if (seg[0]) {
            (*sent_num)++;
            validate_segment(seg, *sent_num, total);
        }
    }
}

static void print_header(const char *label) {
    putchar('\n');
    for (int i = 0; i < VAL_WIDTH; i++) putchar('=');
    printf("\nISUZUMA RY'IMVUGO — Sentence Validation\n");
    if (label && label[0])
        printf("%s\n", label);
    for (int i = 0; i < VAL_WIDTH; i++) putchar('=');
    printf("\n\n");
}

static void print_footer(int sent_num, int total) {
    if (sent_num > 1) {
        for (int i = 0; i < VAL_WIDTH; i++) putchar('=');
        printf("\nINCAMAKE RUSANGE / Grand Total: "
               "interuro %d, imikono %d myose / "
               "%d sentence(s), %d total violation(s)\n",
               sent_num, total, sent_num, total);
        for (int i = 0; i < VAL_WIDTH; i++) putchar('=');
        putchar('\n');
    }
}

/* ── Public: validate an inline text string ───────────────────────────────── */

void kin_validate_text(const char *text) {
    if (!text || !text[0]) return;
    print_header(NULL);
    int sent_num = 0, total = 0;
    /* For a single sentence (no trailing punctuation) send_num stays 0 in
     * print_report, so the "[Interuro N]" prefix is suppressed.  We correct
     * this by using sent_num=0 for the first call when input is one sentence.
     * To detect: count terminal marks in text.                               */
    validate_text_block(text, &sent_num, &total);
    print_footer(sent_num, total);
}

/* ── Public: validate a file (.pdf / .doc / .docx / plain text) ───────────── */

/* True when path ends with ext_lower (case-insensitive, e.g. ".pdf"). */
static bool has_ext(const char *path, const char *ext_lower) {
    const char *dot = strrchr(path, '.');
    if (!dot) return false;
    char low[16] = {0};
    int  k = 0;
    while (k < 15 && dot[k]) {
        char ch = dot[k];
        low[k++] = (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
    }
    return strcmp(low, ext_lower) == 0;
}

/* Validate every non-blank line of fp as text. */
static void validate_stream(FILE *fp, const char *label) {
    print_header(label);
    char line[VAL_MAX_SEG];
    int sent_num = 0, total = 0;
    while (fgets(line, sizeof(line), fp)) {
        size_t l = strlen(line);
        if (l > 0 && line[l-1] == '\n') line[--l] = '\0';
        if (l > 0 && line[l-1] == '\r') line[--l] = '\0';
        kin_str_trim(line);
        if (!line[0] || line[0] == '#') continue;
        validate_text_block(line, &sent_num, &total);
    }
    print_footer(sent_num, total);
}

void kin_validate_file(const char *path) {
    char tmpfile[512] = {0};
    FILE *fp          = NULL;
    bool  is_tmp      = false;

    if (has_ext(path, ".pdf")) {
        snprintf(tmpfile, sizeof(tmpfile), "/tmp/kin_val_%d.txt", (int)getpid());
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
            "pdftotext -layout \"%s\" \"%s\" 2>/dev/null", path, tmpfile);
        if (system(cmd) != 0) {
            fprintf(stderr,
                "Ikosa: pdftotext yaretse. Shyiraho: sudo apt install poppler-utils\n"
                "Error: pdftotext failed. Install with: sudo apt install poppler-utils\n");
            return;
        }
        fp = fopen(tmpfile, "r");
        is_tmp = true;

    } else if (has_ext(path, ".doc") || has_ext(path, ".docx")) {
        snprintf(tmpfile, sizeof(tmpfile), "/tmp/kin_val_%d.txt", (int)getpid());
        char cmd[1024];
        /* Try antiword first; fall back to libreoffice */
        snprintf(cmd, sizeof(cmd),
            "antiword \"%s\" > \"%s\" 2>/dev/null", path, tmpfile);
        if (system(cmd) != 0) {
            snprintf(cmd, sizeof(cmd),
                "libreoffice --headless --convert-to txt:Text "
                "\"%s\" --outdir /tmp/ 2>/dev/null", path);
            (void)system(cmd);
            /* libreoffice writes <basename>.txt in /tmp/ */
            const char *base = strrchr(path, '/');
            base = base ? base + 1 : path;
            const char *dot  = strrchr(base, '.');
            int  blen = dot ? (int)(dot - base) : (int)strlen(base);
            char lo_out[512];
            snprintf(lo_out, sizeof(lo_out), "/tmp/%.*s.txt", blen, base);
            rename(lo_out, tmpfile);
        }
        fp = fopen(tmpfile, "r");
        is_tmp = true;
        if (!fp) {
            fprintf(stderr,
                "Ikosa: Ntibishoboka gufungura '%s'. "
                "Shyiraho 'antiword' cyangwa 'libreoffice'.\n"
                "Error: cannot convert '%s'. "
                "Install antiword or libreoffice.\n", path, path);
            return;
        }

    } else {
        fp = fopen(path, "r");
        if (!fp) {
            fprintf(stderr,
                "Ikosa: Ntibishoboka gufungura '%s'\n"
                "Error: cannot open '%s'\n", path, path);
            return;
        }
    }

    char label[512];
    snprintf(label, sizeof(label), "Dosiye / File: %s", path);
    validate_stream(fp, label);
    fclose(fp);
    if (is_tmp && tmpfile[0]) remove(tmpfile);
}
