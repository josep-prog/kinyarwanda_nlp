/*
 * main.c
 * CLI entry point for the Kinyarwanda NLP engine.
 *
 * Usage:
 *   ./kinyarwanda_nlp                    → interactive mode
 *   ./kinyarwanda_nlp -s "text here"     → analyze a single sentence
 *   ./kinyarwanda_nlp -f file.txt        → analyze a text file
 *   ./kinyarwanda_nlp -p file.pdf        → analyze a PDF (needs pdftotext)
 *   ./kinyarwanda_nlp -v -s "text"       → verbose output
 *   ./kinyarwanda_nlp --help             → show help
 *   ./kinyarwanda_nlp --version          → show version
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../include/kinyarwanda.h"
#include "../include/g2p.h"

#define MAX_LINE   4096
#define VERSION    "1.2.0"
#define BUILD_DATE "2026"

static void print_help(const char *prog) {
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║        KINYARWANDA NLP ENGINE  v" VERSION "                         ║\n");
    printf("║  Imashini isesengura Ikinyarwanda / Kinyarwanda Language Tool   ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");
    printf("IMIKORESHEREZE / USAGE:\n");
    printf("  %s                        Uburyo bw'ikiganiro (interactive)\n", prog);
    printf("  %s -s \"interuro\"          Sesengura interuro imwe (one sentence)\n", prog);
    printf("  %s -f dosiye.txt          Sesengura dosiye txt (text file)\n", prog);
    printf("  %s -p dosiye.pdf          Sesengura dosiye PDF (PDF file)\n", prog);
    printf("  %s -v -s \"interuro\"       Ibisobanuro by'inizi (verbose)\n", prog);
    printf("  %s -h, --help             Uru rupapuro rw'ubufasha (this help)\n", prog);
    printf("  %s --version              Werekana version\n\n", prog);

    printf("IBIRANGA IBISOBANURO / OUTPUT FIELDS:\n");
    printf("  Ijambo/Word      – Ijambo nk'uko ryanditse (word as written)\n");
    printf("  Ubwoko/Type      – Ubwoko bw'ijambo (part of speech):\n");
    printf("    Izina mbonera  – Izina (Noun)\n");
    printf("    Ntera          – Ntera (Adjective)\n");
    printf("    Inshinga       – Inshinga (Verb, infinitive or conjugated)\n");
    printf("    Ikinyazina     – Ikinyazina (Pronoun)\n");
    printf("    Umugereka      – Ingera (Preposition)\n");
    printf("    Icyungo        – Icyungo (Conjunction)\n");
    printf("    Akamamo        – Akamamo (Adverb)\n");
    printf("    Indangahantu   – Locative (Place word)\n");
    printf("  Inteko/Class     – Inteko y'izina (Noun class Nt.1–Nt.16)\n");
    printf("  Igicumbi/Stem    – Igicumbi (root after prefix removal)\n");
    printf("  └─ Igihe/Tense   – Igihe cy'inshinga (verb tense, shown below verb)\n\n");

    printf("IBIHE BY'INSHINGA / VERB TENSES (REB 2020):\n");
    printf("  Indagihe y'ako kanya  – Present immediate:  aragenda, iravuga\n");
    printf("  Indagihe y'ubusanzwe  – Habitual present:   ibona, barima\n");
    printf("  Impitakere            – Recent past:        yaremye, bagiye\n");
    printf("  Impitakera            – Remote/habit. past: yagendaga\n");
    printf("  Inzagihe              – Future:             azagenda, tuzakora\n");
    printf("  Isabira               – Subjunctive:        agende\n");
    printf("  Inshinga y'imigani    – Narrative:          akagenda\n\n");

    printf("AMAKOSA ABONEKA / ERRORS DETECTED:\n");
    printf("  ! Mbere y'ijambo   – Icyo jambo gifite ikosa (error marker)\n");
    printf("  ERR_ADJ_AGREEMENT  – Ntera ntishyikiye inteko y'izina\n");
    printf("                       (adjective doesn't agree with noun class)\n");
    printf("  ERR_POSS_AGREEMENT – Ikinyazina ngenera ntishyikiye inteko\n");
    printf("                       (possessive doesn't agree with noun)\n");
    printf("  ERR_NO_VERB        – Interuro ntagira inshinga (no verb found)\n");
    printf("  ERR_UNKNOWN_WORD   – Ijambo ntiriziwe (unrecognised word)\n\n");

    printf("INGERO / EXAMPLES:\n");
    printf("  %s -s \"Umuntu munini aragenda\"\n", prog);
    printf("  %s -s \"Imana yaremye ijuru n'isi\"\n", prog);
    printf("  %s -v -s \"Urugo rwacu rurabaho\"\n", prog);
    printf("  %s -f inkuru.txt\n", prog);
    printf("  %s -p Bibiliya.pdf\n\n", prog);

    printf("DOSIYE ZIHABWA / SUPPORTED FILE TYPES:\n");
    printf("  .txt  – Analyzed line by line\n");
    printf("  .pdf  – Converted via pdftotext (must be installed)\n\n");

    printf("Soma urupapuro rw'ubufasha: man kinyarwanda_nlp\n");
    printf("Soma igitabo: Ikinyarwanda Amashuri Nderabarezi (TTC) – REB 2020\n");
}

static void analyse_line(const char *line, bool verbose) {
    /* Skip empty lines and comments */
    if (line[0] == '\0' || line[0] == '#') return;

    SentenceAnalysis sa = kin_analyze(line);
    printf("\nInput: %s\n", line);
    kin_print_analysis(&sa, verbose);
}

/*
 * analyse_text() — split 'text' on sentence boundaries and call analyse_line
 * for each fragment.  This handles pasted multi-sentence input and prevents
 * accidental concatenation when the last pasted line has no trailing newline.
 *
 * Boundary rule: a '.', '!' or '?' followed by a space, newline, or
 * end-of-string ends the current sentence.  Commas never end a sentence
 * so "Buragoroba buracya," is kept intact as one fragment.
 */
static void analyse_text(const char *text, bool verbose) {
    if (!text || !text[0] || text[0] == '#') return;

    char seg[MAX_LINE];
    size_t si = 0;

    for (size_t i = 0; ; i++) {
        char c = text[i];

        /* Accumulate character */
        if (c != '\0' && si < sizeof(seg) - 1)
            seg[si++] = c;

        /* Check for sentence boundary or end of string */
        bool end     = (c == '\0');
        bool is_term = (si > 0 && (seg[si-1] == '.' || seg[si-1] == '!'
                                   || seg[si-1] == '?'));
        bool next_ok = end || text[i+1] == ' ' || text[i+1] == '\n'
                           || text[i+1] == '\0';

        if ((is_term && next_ok) || end) {
            seg[si] = '\0';
            kin_str_trim(seg);
            if (seg[0]) {
                analyse_line(seg, verbose);
                putchar('\n');
            }
            si = 0;
            /* Skip the whitespace separator between sentences */
            while (text[i+1] == ' ' || text[i+1] == '\n') i++;
        }

        if (end) break;
    }

    /* Flush any remainder that had no terminating punctuation */
    if (si > 0) {
        seg[si] = '\0';
        kin_str_trim(seg);
        if (seg[0]) {
            analyse_line(seg, verbose);
            putchar('\n');
        }
    }
}

/* Analyse a stream line by line */
static void analyse_stream(FILE *fp, bool verbose) {
    char line[MAX_LINE];
    int lineno = 0;
    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        size_t l = strlen(line);
        if (l > 0 && line[l-1] == '\n') line[--l] = '\0';
        if (l > 0 && line[l-1] == '\r') line[--l] = '\0';
        /* Skip blank lines and chapter headings (all-caps / digits only) */
        if (l == 0 || line[0] == '#') continue;
        printf("\n[Umurongo %d / Line %d]\n", lineno, lineno);
        analyse_line(line, verbose);
    }
}

/* Convert PDF to text via pdftotext and analyse */
static int analyse_pdf(const char *pdfpath, bool verbose) {
    /* Build a temp file path */
    char tmpfile[512];
    snprintf(tmpfile, sizeof(tmpfile), "/tmp/kin_nlp_%d.txt", (int)getpid());

    /* Shell command: pdftotext -layout <pdf> <tmp> */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "pdftotext -layout \"%s\" \"%s\" 2>/dev/null", pdfpath, tmpfile);

    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr,
            "Ikosa: pdftotext yaretse. Reba ko 'poppler-utils' yashyizweho.\n"
            "Error: pdftotext failed. Make sure 'poppler-utils' is installed.\n"
            "  sudo apt install poppler-utils\n");
        return 1;
    }

    FILE *fp = fopen(tmpfile, "r");
    if (!fp) {
        fprintf(stderr, "Ikosa: Ntashobora gufungura dosiye y'agateganyo.\n"
                        "Error: Cannot open temp file '%s'.\n", tmpfile);
        return 1;
    }

    printf("PDF: %s\n", pdfpath);
    analyse_stream(fp, verbose);
    fclose(fp);
    remove(tmpfile);
    return 0;
}

int main(int argc, char *argv[]) {
    bool verbose  = false;
    bool g2p_mode = false;
    const char *sentence = NULL;
    const char *filename = NULL;
    const char *pdffile  = NULL;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("kinyarwanda_nlp v" VERSION " (%s)\n", BUILD_DATE);
            printf("Corpus: Bibiliya Yera 2001 + REB TTC 2020\n");
            return 0;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--g2p") == 0) {
            g2p_mode = true;
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            sentence = argv[++i];
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            filename = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            pdffile = argv[++i];
        } else {
            fprintf(stderr, "Ijambo ridaziwe: '%s'\nUnknown option: '%s'\n"
                            "Gerageza: %s --help\n", argv[i], argv[i], argv[0]);
            return 1;
        }
    }

    /* G2P / phoneme mode: --g2p -s "text" */
    if (g2p_mode && sentence) {
        char norm[G2P_MAX_NORM];
        kin_normalize_text(sentence, norm, sizeof(norm));
        printf("Input:      %s\n", sentence);
        printf("Normalized: %s\n", norm);

        KinPhonemeSeq seq;
        if (kin_g2p_sentence(sentence, &seq)) {
            printf("Phonemes:   %s\n", seq.repr);
            printf("Count:      %d phoneme tokens\n", seq.count);
        } else {
            printf("G2P failed: no phonemes produced.\n");
        }
        return 0;
    }

    /* Single sentence mode */
    if (sentence) {
        analyse_line(sentence, verbose);
        return 0;
    }

    /* Text file mode */
    if (filename) {
        FILE *fp = fopen(filename, "r");
        if (!fp) {
            fprintf(stderr, "Ikosa: Ntashobora gufungura '%s'\n"
                            "Error: cannot open '%s'\n", filename, filename);
            return 1;
        }
        analyse_stream(fp, verbose);
        fclose(fp);
        return 0;
    }

    /* PDF file mode */
    if (pdffile) {
        return analyse_pdf(pdffile, verbose);
    }

    /* Interactive mode */
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║   Kinyarwanda NLP Engine v" VERSION " – Uburyo bw'Ikiganiro           ║\n");
    printf("║   Interactive Mode                                               ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("Andika interuro ushinze Enter kugira ngo isesengurwe.\n");
    printf("Type a sentence and press Enter to analyse it.\n");
    printf("Andika 'help' kubona amabwiriza. Type 'quit' or Ctrl+D to exit.\n\n");

    char line[MAX_LINE];
    while (1) {
        printf(">>> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        size_t l = strlen(line);
        if (l > 0 && line[l-1] == '\n') line[--l] = '\0';
        if (l > 0 && line[l-1] == '\r') line[--l] = '\0';
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0 ||
            strcmp(line, "urabeho") == 0) break;
        if (strcmp(line, "help") == 0 || strcmp(line, "ubufasha") == 0) {
            print_help(argv[0]);
            continue;
        }
        if (l == 0) continue;
        analyse_text(line, verbose);
    }
    printf("Murakoze! / Thank you.\n");
    return 0;
}
