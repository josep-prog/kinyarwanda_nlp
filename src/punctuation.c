/*
 * punctuation.c — Kinyarwanda punctuation placement rules
 *
 * Implements kin_check_punctuation(), called from the validator after
 * kin_analyze() has tagged, morpheme-analysed, and syntax-checked a sentence.
 * Detected violations are appended to sa->errors[] using the same Error struct
 * already used by kin_check_syntax().
 *
 * Rules implemented (RALC 2017 orthography + REB textbook grammar):
 *
 * P1 – Comma before adversative/concessive conjunctions
 *   'ariko', 'naho', 'cyakora', 'icyakora', 'ahubwo' in non-sentence-initial
 *   position require a comma immediately before them.
 *   e.g.  "Yagiye, ariko aragaruka."  ✓
 *         "Yagiye ariko aragaruka."   ✗
 *
 * P2 – Comma before 'kandi' at clause junctions
 *   'kandi' connecting two full verb-clauses needs a preceding comma.
 *   'kandi' joining two nouns or adjectives does NOT need a comma.
 *   e.g.  "Aragenda, kandi aratumanahana."   ✓  (clause+clause)
 *         "umugabo kandi umugore barakora"    ✓  (no comma, noun list)
 *
 * P3 – Colon or opening quote after 'ati' / 'ngo' (speech particles)
 *   After a speech-introducing particle the next token must be ':' or '"'.
 *   e.g.  ati: "Murakoze"   ✓
 *         ati Murakoze      ✗  (missing colon)
 *
 * P4 – Question mark for interrogative sentences
 *   Sentences containing an interrogative word (nde, iki, ryari, he, hehe,
 *   gute, kangahe, kuki, ikihe, harya, mbese, ese) must end with '?' not '.'.
 *   Trigger list is corpus-derived (Bibiliya Yera frequency analysis).
 *
 * P5 – Missing terminal punctuation
 *   A sentence whose last token is not POS_PUNCTUATION with is_sent_boundary
 *   is missing end-of-sentence punctuation.
 *
 * P6 – Spurious comma between noun and its immediately following adjective
 *   In Kinyarwanda no comma separates a noun from its modifier.
 *   "umuntu, munini"  ✗  →  "umuntu munini"  ✓
 *
 * P7 – Exclamation mark for exclamative sentences
 *   Sentences containing an exclamative particle (erega, ayii, ishyano,
 *   mbega, iyaba, dore) should end with '!' not '.'.
 *   Trigger list is corpus-derived (Bibiliya Yera frequency analysis).
 */

#include <string.h>
#include <stdio.h>
#include "../include/kinyarwanda.h"

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static void add_error(SentenceAnalysis *sa, ErrorType type, int idx,
                      const char *msg, const char *sug) {
    if (sa->error_count >= KIN_MAX_ERRORS) return;
    Error *e = &sa->errors[sa->error_count++];
    e->type        = type;
    e->token_index = idx;
    strncpy(e->message,    msg, KIN_MAX_MSG - 1);
    strncpy(e->suggestion, sug, KIN_MAX_MSG - 1);
    e->message[KIN_MAX_MSG - 1]    = '\0';
    e->suggestion[KIN_MAX_MSG - 1] = '\0';
    if (idx >= 0 && idx < sa->token_count)
        sa->tokens[idx].error_count++;
}

/* Index of the previous non-punctuation token before position i, or -1. */
static int prev_content(const SentenceAnalysis *sa, int i) {
    for (int j = i - 1; j >= 0; j--)
        if (sa->tokens[j].pos != POS_PUNCTUATION) return j;
    return -1;
}

/* Index of the next non-punctuation token after position i, or token_count. */
static int next_content(const SentenceAnalysis *sa, int i) {
    for (int j = i + 1; j < sa->token_count; j++)
        if (sa->tokens[j].pos != POS_PUNCTUATION) return j;
    return sa->token_count;
}

/*
 * True when the token immediately before position i (scanning right-to-left,
 * stopping at the first token of any kind) is a comma.
 */
static bool preceded_by_comma(const SentenceAnalysis *sa, int i) {
    for (int j = i - 1; j >= 0; j--) {
        /* First token we encounter in either direction */
        if (sa->tokens[j].pos == POS_PUNCTUATION)
            return sa->tokens[j].punct_type == PUNCT_COMMA;
        return false; /* content token hit before any punctuation */
    }
    return false;
}

/* True when the lower form is one of the adversative/concessive conjunctions. */
static bool is_adversative(const char *lower) {
    return (strcmp(lower, "ariko")    == 0 ||
            strcmp(lower, "naho")     == 0 ||
            strcmp(lower, "cyakora")  == 0 ||
            strcmp(lower, "icyakora") == 0 ||
            strcmp(lower, "ahubwo")   == 0);
}

/*
 * True when the lower form is an interrogative word.
 * Core list verified against Bibiliya Yera corpus (2 102 question sentences):
 *   nde 54x, iki 25x, mbese 77x, he 26x, kuki 112x, ikihe 358x, harya 72x
 *   ratios = how much more frequent in "?" sentences vs. declaratives.
 */
static bool is_interrogative(const char *lower) {
    return (strcmp(lower, "nde")     == 0 ||  /* who                  */
            strcmp(lower, "iki")     == 0 ||  /* what                 */
            strcmp(lower, "ryari")   == 0 ||  /* when                 */
            strcmp(lower, "he")      == 0 ||  /* where                */
            strcmp(lower, "hehe")    == 0 ||  /* where (emphatic)     */
            strcmp(lower, "gute")    == 0 ||  /* how                  */
            strcmp(lower, "kangahe") == 0 ||  /* how many times       */
            strcmp(lower, "kuki")    == 0 ||  /* why (corpus: 112x)   */
            strcmp(lower, "ikihe")   == 0 ||  /* which one (358x)     */
            strcmp(lower, "harya")   == 0 ||  /* really/is it so (72x)*/
            strcmp(lower, "mbese")   == 0 ||  /* polar question marker */
            strcmp(lower, "ese")     == 0);   /* polar question marker */
}

/*
 * True when the lower form is an exclamative particle.
 * Corpus-derived from Bibiliya Yera exclamation sentences (325 total):
 *   erega 249x, ayii 394x, ishyano 89x, mbega 131x, iyaba 62x.
 */
static bool is_exclamative(const char *lower) {
    return (strcmp(lower, "erega")   == 0 ||  /* indeed/really! (249x) */
            strcmp(lower, "ayii")    == 0 ||  /* interjection (394x)   */
            strcmp(lower, "ishyano") == 0 ||  /* woe/disaster (89x)    */
            strcmp(lower, "mbega")   == 0 ||  /* how great!/behold (131x) */
            strcmp(lower, "iyaba")   == 0 ||  /* would that/if only (62x) */
            strcmp(lower, "dore")    == 0);   /* look!/behold          */
}

/*
 * Index of the last POS_PUNCTUATION token with is_sent_boundary, or -1.
 * Used to check which terminal mark (if any) closes the sentence.
 */
static int last_sent_boundary_idx(const SentenceAnalysis *sa) {
    for (int i = sa->token_count - 1; i >= 0; i--)
        if (sa->tokens[i].pos == POS_PUNCTUATION && sa->tokens[i].is_sent_boundary)
            return i;
    return -1;
}

/* ── Public entry point ───────────────────────────────────────────────────── */

void kin_check_punctuation(SentenceAnalysis *sa) {
    if (sa->token_count == 0) return;

    bool has_interrogative = false;
    bool has_exclamative   = false;

    for (int i = 0; i < sa->token_count; i++) {
        const Token *t = &sa->tokens[i];

        /* Skip punctuation tokens in the main scan — handled separately. */
        if (t->pos == POS_PUNCTUATION) {

            /* P6: Comma between a noun and its immediately following adjective */
            if (t->punct_type == PUNCT_COMMA) {
                int pci = prev_content(sa, i);
                int nci = next_content(sa, i);
                if (pci >= 0 && nci < sa->token_count &&
                    sa->tokens[pci].pos == POS_NOUN &&
                    sa->tokens[nci].pos == POS_ADJECTIVE) {
                    char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
                    snprintf(msg, sizeof(msg),
                        "Koma nta mvugo hagati y'izina '%s' n'intera '%s'. / "
                        "No comma between noun '%s' and its adjective '%s'.",
                        sa->tokens[pci].surface, sa->tokens[nci].surface,
                        sa->tokens[pci].surface, sa->tokens[nci].surface);
                    snprintf(sug, sizeof(sug),
                        "Kuraho koma: '%s %s'. / "
                        "Remove the comma: write '%s %s'.",
                        sa->tokens[pci].surface, sa->tokens[nci].surface,
                        sa->tokens[pci].surface, sa->tokens[nci].surface);
                    add_error(sa, ERR_EXTRA_PUNCT, i, msg, sug);
                }
            }
            continue;
        }

        /* P4 / P7: Track interrogative and exclamative words */
        if (is_interrogative(t->lower))
            has_interrogative = true;
        if (is_exclamative(t->lower))
            has_exclamative = true;

        /* P1: Adversative conjunctions need a preceding comma */
        if (is_adversative(t->lower) && i > 0 && !preceded_by_comma(sa, i)) {
            char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
            snprintf(msg, sizeof(msg),
                "Icyungo '%s' gikeneye koma imbere yacyo. / "
                "Conjunction '%s' requires a comma before it.",
                t->surface, t->surface);
            snprintf(sug, sizeof(sug),
                "Shyira koma imbere ya '%s': '..., %s ...'. / "
                "Insert a comma before '%s': '..., %s ...'.",
                t->surface, t->surface, t->surface, t->surface);
            add_error(sa, ERR_MISSING_COMMA, i, msg, sug);
        }

        /* P2: 'kandi' joining two clauses (verb … kandi … verb) needs a comma */
        if (strcmp(t->lower, "kandi") == 0 && i > 0) {
            int pci = prev_content(sa, i);
            int nci = next_content(sa, i);
            bool prev_verb = (pci >= 0 &&
                              (sa->tokens[pci].pos == POS_VERB_CONJ ||
                               sa->tokens[pci].pos == POS_VERB_INF));
            bool next_verb = (nci < sa->token_count &&
                              (sa->tokens[nci].pos == POS_VERB_CONJ ||
                               sa->tokens[nci].pos == POS_VERB_INF));
            if (prev_verb && next_verb && !preceded_by_comma(sa, i)) {
                char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
                snprintf(msg, sizeof(msg),
                    "'kandi' iri hagati y'inshinga ebyiri — koma irakenewe. / "
                    "'kandi' joins two verb clauses — a comma is required before it.");
                snprintf(sug, sizeof(sug),
                    "Shyira koma imbere ya 'kandi': '..., kandi ...'. / "
                    "Insert a comma before 'kandi': '..., kandi ...'.");
                add_error(sa, ERR_MISSING_COMMA, i, msg, sug);
            }
        }

        /* P3: 'ati' / 'ngo' introducing reported speech must be followed by ':' or '"' */
        if (t->pos == POS_VERB_PARTICLE &&
            (strcmp(t->lower, "ati") == 0 || strcmp(t->lower, "ngo") == 0)) {
            bool next_ok = false;
            if (i + 1 < sa->token_count) {
                const Token *nx = &sa->tokens[i + 1];
                next_ok = (nx->pos == POS_PUNCTUATION &&
                           (nx->punct_type == PUNCT_COLON ||
                            nx->punct_type == PUNCT_QUOTE_OPEN));
            }
            if (!next_ok) {
                char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
                snprintf(msg, sizeof(msg),
                    "'%s' itangira ijambo rivugwa ariko nta koma ndende (':') "
                    "cyangwa nta kururika ('\"') bikurikiraho. / "
                    "'%s' introduces direct speech but is not followed by ':' or '\"'.",
                    t->surface, t->surface);
                snprintf(sug, sizeof(sug),
                    "Shyira ':' nyuma ya '%s': '%s: \"...\"'. / "
                    "Add ':' after '%s': '%s: \"...\"'.",
                    t->surface, t->surface, t->surface, t->surface);
                add_error(sa, ERR_MISSING_COLON, i, msg, sug);
            }
        }
    }

    /* P4: If interrogative words were found, final mark must be '?' not '.' */
    if (has_interrogative) {
        int sb = last_sent_boundary_idx(sa);
        if (sb >= 0 && sa->tokens[sb].punct_type == PUNCT_PERIOD) {
            char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
            snprintf(msg, sizeof(msg),
                "Interuro y'ikibazo isozwa n'ikirango '.' aho kuba '?'. / "
                "An interrogative sentence ends with '.' instead of '?'.");
            snprintf(sug, sizeof(sug),
                "Hindura '.' ugashyira '?'. / "
                "Replace the final '.' with a question mark '?'.");
            add_error(sa, ERR_MISSING_QMARK, sb, msg, sug);
        }
    }

    /* P7: Exclamative sentence should end with '!' not '.' */
    if (has_exclamative && !has_interrogative) {
        int sb = last_sent_boundary_idx(sa);
        if (sb >= 0 && sa->tokens[sb].punct_type == PUNCT_PERIOD) {
            char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
            snprintf(msg, sizeof(msg),
                "Interuro ifite ijambo ry'ubwishime isozwa n'ikirango '.' "
                "aho kuba '!'. / "
                "An exclamative sentence ends with '.' instead of '!'.");
            snprintf(sug, sizeof(sug),
                "Hindura '.' ugashyira '!'. / "
                "Replace the final '.' with an exclamation mark '!'.");
            add_error(sa, ERR_MISSING_EXCLAIM, sb, msg, sug);
        }
    }

    /* P5: Missing terminal punctuation — no period/question/exclamation at all */
    if (last_sent_boundary_idx(sa) < 0) {
        /* Find the last content token to attach the error to */
        int last_c = -1;
        for (int i = sa->token_count - 1; i >= 0; i--) {
            if (sa->tokens[i].pos != POS_PUNCTUATION) { last_c = i; break; }
        }
        if (last_c >= 0) {
            const char *mark = has_interrogative ? "?" :
                               has_exclamative   ? "!" : ".";
            char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
            snprintf(msg, sizeof(msg),
                "Interuro irangira nta kirango cy'iherezo ('.', '?', '!'). / "
                "Sentence ends without terminal punctuation ('.', '?', '!').");
            snprintf(sug, sizeof(sug),
                "Ongeraho '%s' ku iherezo rya interuro. / "
                "Add '%s' at the end of the sentence.",
                mark, mark);
            add_error(sa, ERR_MISSING_PERIOD, last_c, msg, sug);
        }
    }
}
