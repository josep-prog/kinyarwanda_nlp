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
 *   'ariko', 'naho', 'cyakora', 'icyakora', 'ahubwo', 'nyamara'
 *   in non-sentence-initial position require a comma immediately before them.
 *   e.g.  "Yagiye, ariko aragaruka."   ✓
 *         "Yagiye ariko aragaruka."    ✗
 *         "Yagiye, nyamara aragaruka." ✓
 *         "Yagiye nyamara aragaruka."  ✗
 *   Corpus: nyamara precedes comma in 83% of occurrences (Bibiliya Yera).
 *
 * P2 – Comma before coordinators at clause junctions
 *   'kandi', 'cyangwa', 'ndetse' connecting two full verb-clauses need a comma.
 *   When joining two nouns/adjectives, no comma is needed.
 *   e.g.  "Aragenda, kandi aratumanahana."   ✓  (clause+clause)
 *         "umugabo kandi umugore barakora"    ✓  (no comma, noun list)
 *         "Azagenda, cyangwa azatuza."        ✓  (clause+clause)
 *         "pani cyangwa icupa"                ✓  (no comma, noun list)
 *
 * P3 – Colon or opening quote after speech particles (ati family + ngo)
 *   After any speech-introducing particle the next token must be ':' or '"'.
 *   The full '-ti' conjugation family is covered (corpus: 89–100% before quote):
 *     ati(97.9%), bati(99.6%), uti(90.6%), nti(93.7%), iti(94.1%),
 *     riti(95.6%), muti(89.2%), tuti(96.2%), ziti/kiti/hati/kuti(100%)
 *   'biti' excluded — homonym with "trees/branches" (igiti pl., 65.5% non-speech).
 *   e.g.  ati: "Murakoze"   ✓     bati "Yagiye."   ✓
 *         ati Murakoze      ✗     riti Yagiye.      ✗
 *
 * P12 – Unbalanced quotation marks
 *   Every opening quote ('"', '«') must have a matching closing quote
 *   ('"', '»') within the same sentence.  Fires ERR_UNBALANCED_QUOTE.
 *   e.g.  ati: "Murakoze."          ✓
 *         ati: "Murakoze.           ✗  (no closing quote)
 *         Yagiye" aragaruka.        ✗  (closing quote without opening)
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
 *   mbega, iyaba, ntibikabeho, yemwe, nkanswe, haleluya, nimwumve) or
 *   starting with the degree exclamative 'ko' should end with '!' not '.'.
 *   Trigger list is corpus-derived (Bibiliya Yera frequency analysis).
 *
 * P8 – Comma before sequential 'maze' (narrative "then")
 *   'maze' used as a narrative connector ("then/and so") requires a preceding
 *   comma when it follows a conjugated verb clause.
 *   Fires only when prev_content is POS_VERB_CONJ, which distinguishes
 *   connector 'maze' from adverbial 'maze' meaning "already" (which follows
 *   particles like 'ko': "ko maze gukecura" = "that I am already old").
 *   Corpus: 'maze' precedes comma in 69% of occurrences.
 *   e.g.  "Arakoze, maze atuza."   ✓
 *         "Arakoze maze atuza."    ✗
 *
 * P9 – Comma before causal 'kuko' (because)
 *   'kuko' introducing a reason clause after a complete predicate requires
 *   a preceding comma.  Fires when prev_content is POS_VERB_CONJ.
 *   Does not fire when 'kuko' is sentence-initial or follows a noun/adverb
 *   (complement-clause uses: "ntinya kuko..." are less certain).
 *   Corpus: 'kuko' precedes comma in 63% of occurrences.
 *   e.g.  "Yagiye, kuko yari nushye."   ✓
 *         "Yagiye kuko yari nushye."    ✗
 *
 * P10 – Semicolon before a known clause connector is wrong (use comma instead)
 *   In Kinyarwanda ';' never appears before adversative/causal connectors
 *   (ariko, naho, ahubwo, nyamara, kandi, cyangwa, ndetse, kuko, maze).
 *   Those connectors require a comma, not a semicolon.
 *   e.g.  "Yagiye; ariko aragaruka."   ✗  →  "Yagiye, ariko aragaruka."  ✓
 *         "Yagiye; kuko yari nushye."  ✗  →  "Yagiye, kuko yari nushye." ✓
 *
 * P11 – Semicolon must separate two complete clauses (each with a predicate)
 *   Both the left and right clause segments around ';' must contain at least
 *   one conjugated verb (POS_VERB_CONJ) or the copula 'ni'/'si'.
 *   If either side is a bare fragment, the ';' is used incorrectly.
 *   Correct uses:
 *     Clause;clause  — "Sara arapfa; Aburahamu agura ubuvumo."
 *     Enumeration    — "mu Barubeni ni Elisuri; mu Basimeyoni ni Shelumiyeli"
 *                       (both sides have copula 'ni')
 *   e.g.  "Sara arapfa; Aburahamu agura ubuvumo."   ✓
 *         "Sara arapfa; ariko"                       ✗  (right side = fragment)
 *
 * P13 – Unbalanced parentheses
 *   Every opening '(' must have a matching closing ')' in the same sentence,
 *   and vice versa.  Fires ERR_UNBALANCED_PAREN.
 *   e.g.  "Yagiye (aragaruka neza) vuba."   ✓
 *         "Yagiye (aragaruka neza vuba."     ✗  (missing close)
 *         "Yagiye aragaruka) neza."          ✗  (close without open)
 *
 * P14 – Terminal punctuation inside a mid-sentence parenthetical
 *   If a '.' / '?' / '!' appears between '(' and ')' and the sentence
 *   continues after the ')', the inner terminal mark is wrong.
 *   In Kinyarwanda a parenthetical that is NOT a complete aside sentence
 *   does not end with terminal punctuation.
 *   e.g.  "Yagiye (aragaruka neza) vuba."   ✓
 *         "Yagiye (aragaruka neza.) vuba."   ✗
 *
 * Notes on « » and << >>
 *   Unicode guillemets '«' (U+00AB) and '»' (U+00BB) are the formal Kinyarwanda
 *   outer-quote characters, used for:
 *     1. Titles of works:  igitabo «Ukwibyara»
 *     2. Terms/glosses:    ijambo «renaissance»
 *     3. Direct speech:    ati «Murakoze»  (same role as "...")
 *   ASCII << and >> are treated as equivalent to « and » by the tokenizer.
 *   Both map to PUNCT_QUOTE_OPEN / PUNCT_QUOTE_CLOSE, so P3 (speech particle
 *   check) and P12 (balanced-quote check) apply automatically to all four forms.
 *   No additional rules are needed beyond P3 and P12.
 *
 * P16 – Unbalanced parenthetical dash
 *   In Kinyarwanda a dash used to open a parenthetical aside must be
 *   closed by a matching dash:  "Yagiye – nk'uko byagenze – aragaruka."
 *   If a sentence contains exactly one PUNCT_DASH that is flanked by
 *   content tokens on both sides (i.e. not a line-initial bullet and
 *   not a morpheme marker attached at word-start), it is unbalanced.
 *   Corpus: 65% of dashes are bullets (excluded by "content before" check);
 *   parenthetical prose dashes are rare but this fires cleanly when used.
 *   e.g.  "Yagiye – nk'uko byagenze – aragaruka."   ✓
 *         "Yagiye – nk'uko byagenze aragaruka."      ✗ (opening dash, no close)
 *
 * P15 – List-introducing predicate needs ':' not '.'
 *   The -kurikira verb family used as a list predicate must be followed by ':'
 *   not '.'. These words are: bikurikira, akurikira, zikurikira, ikurikira,
 *   bikurikiraho, ikurikiraho, zikurikiraho.
 *   Fires when the word is the LAST content token in the sentence and the
 *   terminal mark is '.'.  When used as a modifier ("do the following X"),
 *   content tokens follow the word and the rule does NOT fire.
 *   Corpus: 34–87% of these words precede ':'; when at sentence-end before '.'
 *   they are almost always predicate list-introducers that need ':'.
 *   e.g.  "Subiza ibibazo bikurikira:"   ✓
 *         "Subiza ibibazo bikurikira."    ✗
 *         "Kora iyi myitozo ikurikira ya mbere."  ✓  (modifier, no colon)
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

/*
 * True when the lower form is an adversative/concessive conjunction that
 * always requires a comma when it appears in non-sentence-initial position.
 * 'nyamara' added after corpus analysis: 83% preceded by comma (Bibiliya Yera).
 */
static bool is_adversative(const char *lower) {
    return (strcmp(lower, "ariko")    == 0 ||  /* but/however           */
            strcmp(lower, "naho")     == 0 ||  /* although/even if      */
            strcmp(lower, "cyakora")  == 0 ||  /* but still/however     */
            strcmp(lower, "icyakora") == 0 ||  /* but still/however     */
            strcmp(lower, "ahubwo")   == 0 ||  /* rather/instead        */
            strcmp(lower, "nyamara")  == 0);   /* however/but (83%)     */
}

/*
 * True when the lower form is a clause coordinator that needs a comma before
 * it when both the preceding and following content tokens are conjugated verbs.
 * Extends the P2 logic to 'cyangwa' and 'ndetse' beyond 'kandi'.
 */
static bool is_clause_coordinator(const char *lower) {
    return (strcmp(lower, "kandi")   == 0 ||  /* and (clause+clause)   */
            strcmp(lower, "cyangwa") == 0 ||  /* or  (clause+clause)   */
            strcmp(lower, "ndetse")  == 0);   /* and even/moreover     */
}

/*
 * True when the lower form is a -kurikira list predicate that requires ':'.
 * Only fires when the word is the last content token before '.'.
 * Corpus: these forms are predicate list-introducers in that position.
 * Modifier uses ("kora iyi myitozo ikurikira") have content following them.
 */
static bool is_list_predicate(const char *lower) {
    return (strcmp(lower, "bikurikira")   == 0 ||
            strcmp(lower, "akurikira")    == 0 ||
            strcmp(lower, "zikurikira")   == 0 ||
            strcmp(lower, "ikurikira")    == 0 ||
            strcmp(lower, "bikurikiraho") == 0 ||
            strcmp(lower, "ikurikiraho")  == 0 ||
            strcmp(lower, "zikurikiraho") == 0);
}

/*
 * True when the lower form is a direct-speech introducing particle.
 * Covers the full Kinyarwanda -ti- conjugation family (SP + ti):
 *   ati (3sg), bati (3pl), uti (2sg), nti (1sg), iti (cl.7/9),
 *   riti (cl.5), muti (2pl), tuti (1pl incl.), ziti (cl.10),
 *   kiti (cl.7), hati (locative), kuti (cl.15/inf.), ngo (evidential).
 * 'biti' deliberately excluded — homonym with igiti (tree) plural.
 * Corpus rates (% of occurrences that precede a quote): all ≥89%.
 */
static bool is_speech_particle(const char *lower) {
    return (strcmp(lower, "ati")  == 0 ||
            strcmp(lower, "bati") == 0 ||
            strcmp(lower, "uti")  == 0 ||
            strcmp(lower, "nti")  == 0 ||
            strcmp(lower, "iti")  == 0 ||
            strcmp(lower, "riti") == 0 ||
            strcmp(lower, "muti") == 0 ||
            strcmp(lower, "tuti") == 0 ||
            strcmp(lower, "ziti") == 0 ||
            strcmp(lower, "kiti") == 0 ||
            strcmp(lower, "hati") == 0 ||
            strcmp(lower, "kuti") == 0 ||
            strcmp(lower, "ngo")  == 0);
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
 * True when the lower form is an exclamative particle or interjection.
 * Corpus-derived from Bibiliya Yera (325 "!" sentences, ratio vs. other
 * sentences — higher ratio = safer to include):
 *
 *   Strong interjections (essentially only appear in "!" sentences):
 *     ayii        394x   — surprise/distress interjection
 *     ntibikabeho  —     — "may it never happen!" (absolute refusal)
 *     ntibikambeho —     — variant spelling of ntibikabeho
 *     haleluya     —     — "hallelujah!" praise interjection
 *     nimwumve     —     — "listen!/hear!" (emphatic imperative with ni-)
 *
 *   Degree/lament particles (strong, but erega occasionally heads declaratives):
 *     erega        197x  — "indeed/really!" (also explanatory in prose)
 *     mbega        104x  — "how great!/behold" (exclamative degree marker)
 *     ishyano       89x  — "woe/disaster to..." (lament formula)
 *     yemwe         —    — "alas/oh my!" (lament particle)
 *     nkanswe       —    — "woe is me/I am undone!" (lament)
 *     iyaba         62x  — "would that/if only..." (optative)
 *
 *   EXCLUDED: 'dore' (behold/here is) — corpus ratio only 2.6x; it heads
 *   many declarative sentences ("Dore ibyanditswe...") with no "!" needed.
 *
 *   Degree exclamative 'ko' (sentence-initial) is handled separately by
 *   is_ko_exclamative_initial() because it is a positional test, not lexical.
 */
static bool is_exclamative(const char *lower) {
    return (strcmp(lower, "erega")        == 0 ||
            strcmp(lower, "ayii")         == 0 ||
            strcmp(lower, "ishyano")      == 0 ||
            strcmp(lower, "mbega")        == 0 ||
            strcmp(lower, "iyaba")        == 0 ||
            strcmp(lower, "ntibikabeho")  == 0 ||
            strcmp(lower, "ntibikambeho") == 0 ||
            strcmp(lower, "haleluya")     == 0 ||
            strcmp(lower, "nimwumve")     == 0 ||
            strcmp(lower, "yemwe")        == 0 ||
            strcmp(lower, "nkanswe")      == 0);
}

/*
 * True when the token is the first content word of the sentence AND equals
 * 'ko', which signals a Kinyarwanda degree exclamative:
 *   "Ko rikomeye!"  =  "How heavy it is!"
 *   "Ko bataka!"    =  "How they are crying!"
 * Sentence-initial 'ko' is distinct from the subordinating 'ko' ("that/because")
 * which always follows a main-clause verb and is never sentence-initial.
 */
static bool is_ko_exclamative_initial(const SentenceAnalysis *sa) {
    for (int i = 0; i < sa->token_count; i++) {
        if (sa->tokens[i].pos == POS_PUNCTUATION) continue;
        return strcmp(sa->tokens[i].lower, "ko") == 0;
    }
    return false;
}

/* ── Semicolon helpers (P10 / P11) ───────────────────────────────────────── */

/*
 * Return the index of the first content token in the clause that ENDS at
 * token semi_idx (i.e., start of the left clause).  Scans backward past
 * punctuation other than another ';' or a sentence-boundary mark.
 */
static int semi_clause_left_start(const SentenceAnalysis *sa, int semi_idx) {
    for (int j = semi_idx - 1; j >= 0; j--) {
        if (sa->tokens[j].pos == POS_PUNCTUATION &&
            (sa->tokens[j].is_sent_boundary ||
             sa->tokens[j].punct_type == PUNCT_SEMICOLON))
            return j + 1;
    }
    return 0;
}

/*
 * Return the index one past the last content token in the clause that
 * BEGINS after token semi_idx (i.e., end of the right clause).
 */
static int semi_clause_right_end(const SentenceAnalysis *sa, int semi_idx) {
    for (int j = semi_idx + 1; j < sa->token_count; j++) {
        if (sa->tokens[j].pos == POS_PUNCTUATION &&
            (sa->tokens[j].is_sent_boundary ||
             sa->tokens[j].punct_type == PUNCT_SEMICOLON))
            return j;
    }
    return sa->token_count;
}

/*
 * True when the token range [from, to) contains a conjugated verb OR the
 * copula 'ni'/'si'.  Used to confirm that each side of ';' is a full clause.
 */
static bool has_predicate(const SentenceAnalysis *sa, int from, int to) {
    for (int i = from; i < to && i < sa->token_count; i++) {
        if (sa->tokens[i].pos == POS_VERB_CONJ)
            return true;
        if (strcmp(sa->tokens[i].lower, "ni") == 0 ||
            strcmp(sa->tokens[i].lower, "si") == 0)
            return true;
    }
    return false;
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
    bool has_exclamative   = is_ko_exclamative_initial(sa);
    int  open_quotes       = 0;
    int  close_quotes      = 0;
    int  open_parens       = 0;
    int  close_parens      = 0;
    int  prose_dashes      = 0;   /* dashes flanked by content on both sides */

    for (int i = 0; i < sa->token_count; i++) {
        const Token *t = &sa->tokens[i];

        /* Skip punctuation tokens in the main scan — handled separately. */
        if (t->pos == POS_PUNCTUATION) {

            /* P12: track quote balance */
            if (t->punct_type == PUNCT_QUOTE_OPEN)  open_quotes++;
            if (t->punct_type == PUNCT_QUOTE_CLOSE) close_quotes++;

            /* P13: track paren balance */
            if (t->punct_type == PUNCT_PAREN_OPEN)  open_parens++;
            if (t->punct_type == PUNCT_PAREN_CLOSE) close_parens++;

            /* P16: count prose dashes (flanked by content on both sides).
             * A bullet dash has no content before it; a morpheme marker
             * has no content after it — neither counts. */
            if (t->punct_type == PUNCT_DASH) {
                int pci = prev_content(sa, i);
                int nci = next_content(sa, i);
                if (pci >= 0 && nci < sa->token_count)
                    prose_dashes++;
            }

            /* P14: terminal punctuation inside a mid-sentence parenthetical.
             * If we see '.'/'?'/'!' between '(' and ')', and there are
             * content tokens after the matching ')', the inner mark is wrong. */
            if ((t->punct_type == PUNCT_PERIOD   ||
                 t->punct_type == PUNCT_QUESTION  ||
                 t->punct_type == PUNCT_EXCLAIM)  &&
                open_parens > close_parens) {
                /* We're inside an open paren — check if sentence continues */
                bool content_after_close = false;
                int depth = open_parens - close_parens;
                for (int j = i + 1; j < sa->token_count; j++) {
                    if (sa->tokens[j].pos == POS_PUNCTUATION &&
                        sa->tokens[j].punct_type == PUNCT_PAREN_CLOSE) {
                        depth--;
                        if (depth == 0) {
                            /* Look for content tokens after the close paren */
                            int nci = next_content(sa, j);
                            content_after_close = (nci < sa->token_count);
                            break;
                        }
                    }
                    if (sa->tokens[j].pos == POS_PUNCTUATION &&
                        sa->tokens[j].punct_type == PUNCT_PAREN_OPEN)
                        depth++;
                }
                if (content_after_close) {
                    const char *mark = t->surface;
                    char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
                    snprintf(msg, sizeof(msg),
                        "Ikirango cy'iherezo '%s' kirimo mu rugabane rw'inzira "
                        "naho interuro ikomeza. / "
                        "Terminal mark '%s' inside parentheses while the sentence continues.",
                        mark, mark);
                    snprintf(sug, sizeof(sug),
                        "Kuraho '%s' imbere y'igisinde gifunga: '(ingingo)'. / "
                        "Remove '%s' before ')': write '(content)' without inner terminal mark.",
                        mark, mark);
                    add_error(sa, ERR_WRONG_PUNCT, i, msg, sug);
                }
            }

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

            /* P10: ";" before a known connector → should be "," */
            if (t->punct_type == PUNCT_SEMICOLON) {
                int nci = next_content(sa, i);
                if (nci < sa->token_count) {
                    const char *nw = sa->tokens[nci].lower;
                    if (is_adversative(nw) || is_clause_coordinator(nw) ||
                        strcmp(nw, "kuko") == 0 || strcmp(nw, "maze") == 0) {
                        char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
                        snprintf(msg, sizeof(msg),
                            "';' imbere ya '%s' ntabwo bihuye — '%s' ikeneye koma, si semicoloni. / "
                            "';' before '%s' is incorrect — this connector requires ',' not ';'.",
                            sa->tokens[nci].surface, sa->tokens[nci].surface,
                            sa->tokens[nci].surface);
                        snprintf(sug, sizeof(sug),
                            "Hindura ';' ugashyira ',': '..., %s ...'. / "
                            "Replace ';' with ',': '..., %s ...'.",
                            sa->tokens[nci].surface, sa->tokens[nci].surface);
                        add_error(sa, ERR_WRONG_PUNCT, i, msg, sug);
                    }
                }

                /* P11: both sides of ";" must contain a predicate */
                int left_from  = semi_clause_left_start(sa, i);
                int right_to   = semi_clause_right_end(sa, i);
                bool left_ok   = has_predicate(sa, left_from, i);
                bool right_ok  = has_predicate(sa, i + 1, right_to);
                if (!left_ok || !right_ok) {
                    char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
                    const char *which_rw = (!left_ok && !right_ok) ? "impande zombi" :
                                          (!left_ok)               ? "ingingo y'ibumoso" :
                                                                     "ingingo y'iburyo";
                    const char *which_en = (!left_ok && !right_ok) ? "both sides" :
                                          (!left_ok)               ? "the left clause" :
                                                                     "the right clause";
                    snprintf(msg, sizeof(msg),
                        "Semicoloni (';') igomba gutandukanya interuro ebyiri zuzuye; "
                        "%s nta nshinga ifite. / "
                        "A semicolon must separate two complete clauses; "
                        "%s has no predicate.",
                        which_rw, which_en);
                    snprintf(sug, sizeof(sug),
                        "Suzuma ko impande zombi z'iri kirango ';' ari interuro zuzuye "
                        "(zifite inshinga). Niba atari byo, koresha ',' cyangwa '.'. / "
                        "Ensure both sides of ';' are complete clauses (with a verb). "
                        "If not, use ',' or '.' instead.");
                    add_error(sa, ERR_WRONG_PUNCT, i, msg, sug);
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

        /* P2: coordinator (kandi/cyangwa/ndetse) joining two verb clauses needs comma */
        if (is_clause_coordinator(t->lower) && i > 0) {
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
                    "'%s' iri hagati y'inshinga ebyiri — koma irakenewe. / "
                    "'%s' joins two verb clauses — a comma is required before it.",
                    t->surface, t->surface);
                snprintf(sug, sizeof(sug),
                    "Shyira koma imbere ya '%s': '..., %s ...'. / "
                    "Insert a comma before '%s': '..., %s ...'.",
                    t->surface, t->surface, t->surface, t->surface);
                add_error(sa, ERR_MISSING_COMMA, i, msg, sug);
            }
        }

        /* P8/P9: 'maze' (sequential "then") and 'kuko' (causal "because") need a
         * comma when they follow a conjugated verb clause.  Checking prev_content
         * is POS_VERB_CONJ distinguishes sequential 'maze' from the adverbial
         * "already" sense ("ko maze gukecura") where 'maze' follows a particle. */
        if ((strcmp(t->lower, "maze") == 0 || strcmp(t->lower, "kuko") == 0) &&
            i > 0 && !preceded_by_comma(sa, i)) {
            int pci = prev_content(sa, i);
            if (pci >= 0 && sa->tokens[pci].pos == POS_VERB_CONJ) {
                char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
                const char *rw_meaning = (strcmp(t->lower, "maze") == 0)
                    ? "guhuza imirimo ('then')"
                    : "impamvu ('because')";
                snprintf(msg, sizeof(msg),
                    "Icyungo '%s' (%s) gikeneye koma imbere yacyo nyuma y'inshinga. / "
                    "'%s' (%s) requires a comma after the preceding verb clause.",
                    t->surface, rw_meaning,
                    t->surface,
                    (strcmp(t->lower, "maze") == 0) ? "sequential 'then'" : "causal 'because'");
                snprintf(sug, sizeof(sug),
                    "Shyira koma imbere ya '%s': '...inshinga, %s ...'. / "
                    "Insert a comma before '%s': '...verb, %s ...'.",
                    t->surface, t->surface, t->surface, t->surface);
                add_error(sa, ERR_MISSING_COMMA, i, msg, sug);
            }
        }

        /* P15: List-introducing predicate at end of clause needs ':' not '.'.
         * is_list_predicate() matches only when this is the last content word
         * (next_content returns token_count) and sentence ends with '.'.    */
        if (is_list_predicate(t->lower)) {
            int nci = next_content(sa, i);
            int sb  = last_sent_boundary_idx(sa);
            if (nci >= sa->token_count &&
                sb >= 0 && sa->tokens[sb].punct_type == PUNCT_PERIOD) {
                char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
                snprintf(msg, sizeof(msg),
                    "Ijambo '%s' ritangira urutonde ariko risozwa na '.' aho ':', / "
                    "'%s' introduces a list but ends with '.' instead of ':'.",
                    t->surface, t->surface);
                snprintf(sug, sizeof(sug),
                    "Hindura '.' ugashyira ':' nyuma ya '%s': '...%s:'. / "
                    "Replace '.' with ':' after '%s': '...%s:'.",
                    t->surface, t->surface, t->surface, t->surface);
                add_error(sa, ERR_MISSING_COLON, i, msg, sug);
            }
        }

        /* P3: speech particle must be followed by ':' or an opening quote.
         * Covers the full -ti family: see is_speech_particle() above. */
        if (is_speech_particle(t->lower)) {
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

    /* P12: Unbalanced quotation marks */
    if (open_quotes != close_quotes) {
        bool missing_close = (open_quotes > close_quotes);
        /* Attach to the last token in the sentence */
        int attach = sa->token_count - 1;
        while (attach > 0 && sa->tokens[attach].pos == POS_PUNCTUATION &&
               sa->tokens[attach].is_sent_boundary)
            attach--;
        char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
        if (missing_close) {
            snprintf(msg, sizeof(msg),
                "Interuro ifite ikururika rivugurwa ridafungwa ('%d' ivugura, '%d' ifunga). / "
                "Sentence has %d opening quote(s) but only %d closing quote(s).",
                open_quotes, close_quotes, open_quotes, close_quotes);
            snprintf(sug, sizeof(sug),
                "Ongeraho kururika gifunga ('\"' cyangwa '»') ku iherezo rya buri quote. / "
                "Add a closing quote ('\"' or '»') at the end of each quoted span.");
        } else {
            snprintf(msg, sizeof(msg),
                "Interuro ifite ikururika rifunga ridafite rivugurwa ('%d' ivugura, '%d' ifunga). / "
                "Sentence has %d closing quote(s) but only %d opening quote(s).",
                open_quotes, close_quotes, open_quotes, close_quotes);
            snprintf(sug, sizeof(sug),
                "Ongeraho kururika givugura ('\"' cyangwa '«') imbere ya buri quote. / "
                "Add an opening quote ('\"' or '«') before each quoted span.");
        }
        add_error(sa, ERR_UNBALANCED_QUOTE, attach, msg, sug);
    }

    /* P13: Unbalanced parentheses */
    if (open_parens != close_parens) {
        bool missing_close = (open_parens > close_parens);
        int attach = sa->token_count - 1;
        while (attach > 0 && sa->tokens[attach].pos == POS_PUNCTUATION &&
               sa->tokens[attach].is_sent_boundary)
            attach--;
        char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
        if (missing_close) {
            snprintf(msg, sizeof(msg),
                "Interuro ifite '(' idafite ')' izifunga ('%d' ivugura, '%d' ifunga). / "
                "Sentence has %d opening parenthes%s but only %d closing.",
                open_parens, close_parens,
                open_parens, open_parens == 1 ? "is" : "es", close_parens);
            snprintf(sug, sizeof(sug),
                "Ongeraho ')' ku iherezo ry'ica ngombwa ('...' → '...)'). / "
                "Add a closing ')' at the end of each opened parenthetical.");
        } else {
            snprintf(msg, sizeof(msg),
                "Interuro ifite ')' idafite '(' ivugura ('%d' ivugura, '%d' ifunga). / "
                "Sentence has %d closing parenthes%s but only %d opening.",
                open_parens, close_parens,
                close_parens, close_parens == 1 ? "is" : "es", open_parens);
            snprintf(sug, sizeof(sug),
                "Ongeraho '(' imbere y'ica ngombwa ('...' → '(...'). / "
                "Add an opening '(' before each parenthetical that lacks one.");
        }
        add_error(sa, ERR_UNBALANCED_PAREN, attach, msg, sug);
    }

    /* P16: Unbalanced parenthetical dash (odd number of prose dashes) */
    if (prose_dashes % 2 != 0) {
        int attach = sa->token_count - 1;
        while (attach > 0 && sa->tokens[attach].pos == POS_PUNCTUATION &&
               sa->tokens[attach].is_sent_boundary)
            attach--;
        char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
        snprintf(msg, sizeof(msg),
            "Interuro ifite umurongo w'inzira utuzuye (%d): buri nzira igomba "
            "gufungurwa no gufungwa. / "
            "Sentence has %d parenthetical dash(es) — dashes must come in pairs.",
            prose_dashes, prose_dashes);
        snprintf(sug, sizeof(sug),
            "Ongeraho umurongo w'inzira wo gufunga: '... – inzira – ...'. / "
            "Add a closing dash to complete the pair: '... – aside – ...'.");
        add_error(sa, ERR_WRONG_PUNCT, attach, msg, sug);
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
