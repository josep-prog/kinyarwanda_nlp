/*
 * tokenizer.c
 * Splits Kinyarwanda text into word tokens.
 *
 * Kinyarwanda orthographic notes (from book p.4-12):
 *  - Apostrophe contracts: n'uku, k'ibuye, b'abana (the elided form of
 *    the final vowel of the preceding word).  We split on apostrophe,
 *    yielding two tokens (the clitic attaches to the next word).
 *  - Both ASCII apostrophe (U+0027 ') and Unicode curly apostrophe
 *    (U+2019 ', UTF-8: 0xE2 0x80 0x99) are treated as split points.
 *  - Hyphen in reduplicated adjective stems: to-to, re-re, sa-sa.
 *    We keep these as single tokens.
 *  - Punctuation (.,?!;:) ends a token but is discarded.
 *  - Upper-case letters: preserved in surface form; used to detect proper
 *    nouns (words capitalised mid-sentence).
 */

#include <string.h>
#include <ctype.h>
#include "../include/kinyarwanda.h"

/* Return pointer to the next apostrophe (ASCII ' or UTF-8 curly ')
 * within [p, word_end), or NULL if none found.
 * *apos_len is set to the byte length of the apostrophe found (1 or 3). */
static const char *find_apostrophe(const char *p, const char *word_end,
                                   size_t *apos_len) {
    for (const char *q = p; q < word_end; q++) {
        if ((unsigned char)*q == 0x27) {           /* ASCII apostrophe ' */
            *apos_len = 1;
            return q;
        }
        /* UTF-8 U+2019 RIGHT SINGLE QUOTATION MARK: E2 80 99 */
        if ((unsigned char)q[0] == 0xE2 &&
            q + 2 < word_end &&
            (unsigned char)q[1] == 0x80 &&
            (unsigned char)q[2] == 0x99) {
            *apos_len = 3;
            return q;
        }
    }
    return NULL;
}

/* Returns true if p points to an ASCII or UTF-8 double-quote character:
 *   ASCII " (U+0022)
 *   UTF-8 U+201C LEFT  DOUBLE QUOTATION MARK  E2 80 9C  "
 *   UTF-8 U+201D RIGHT DOUBLE QUOTATION MARK  E2 80 9D  "
 * Safe to call with any non-NULL pointer (checks before dereferencing p[1]/p[2]). */
static bool is_dquote(const char *p) {
    if ((unsigned char)*p == '"') return true;
    if ((unsigned char)p[0] == 0xE2 && (unsigned char)p[1] == 0x80 &&
        ((unsigned char)p[2] == 0x9C || (unsigned char)p[2] == 0x9D))
        return true;
    return false;
}

/* Emit a single punctuation token.  Returns 1 on success, 0 if at capacity. */
static int emit_punct(Token *out, int count, int max_tokens,
                      const char *ch, size_t nbytes, PunctType pt) {
    if (count >= max_tokens) return 0;
    memset(&out[count], 0, sizeof(Token));
    memcpy(out[count].surface, ch, nbytes);
    out[count].surface[nbytes] = '\0';
    memcpy(out[count].lower,   ch, nbytes);
    out[count].lower[nbytes]   = '\0';
    out[count].pos             = POS_PUNCTUATION;
    out[count].punct_type      = pt;
    out[count].is_clause_boundary = (pt == PUNCT_COMMA || pt == PUNCT_SEMICOLON);
    out[count].is_sent_boundary   = (pt == PUNCT_PERIOD || pt == PUNCT_QUESTION ||
                                     pt == PUNCT_EXCLAIM);
    out[count].is_quote_open      = (pt == PUNCT_QUOTE_OPEN);
    out[count].is_quote_close     = (pt == PUNCT_QUOTE_CLOSE);
    out[count].is_kinyarwanda     = false;
    return 1;
}

/* Known stative-possessive (-fite) prefix forms that may appear merged with a
 * following noun (e.g. "bifiteubugingo" written as one word instead of two).
 * When detected, the tokenizer splits them at the boundary so each part is
 * analysed independently.
 * Ordered longest-first to prevent prefix mismatch (bafite before afite). */
static const char * const FITE_FORMS[] = {
    "bafite", "bifite", "zifite", "rufite", "gafite", "dufite",
    "mufite", "bufite", "gifite", "nfite",  "ufite",  "afite",
    "ifite",  NULL
};

int kin_tokenize(const char *text, Token *out, int max_tokens) {
    int count = 0;
    const char *p = text;

    while (*p && count < max_tokens) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;

        /* ── Punctuation tokens ──────────────────────────────────────────── *
         * Emit punctuation marks as explicit POS_PUNCTUATION tokens instead  *
         * of silently discarding them.  This lets the analyser know about    *
         * clause and sentence boundaries, and about direct-speech openings.  *
         *                                                                    *
         * ASCII double-quote (") is treated as QUOTE_OPEN or QUOTE_CLOSE    *
         * based on context: OPEN when no unmatched open quote precedes it;  *
         * CLOSE otherwise.  Curly " (U+201C) is always OPEN, " (U+201D)    *
         * always CLOSE.                                                      */
        if (*p == ',')  { count += emit_punct(out,count,max_tokens,p,1,PUNCT_COMMA);      p++; continue; }
        if (*p == '.')  { count += emit_punct(out,count,max_tokens,p,1,PUNCT_PERIOD);     p++; continue; }
        if (*p == '?')  { count += emit_punct(out,count,max_tokens,p,1,PUNCT_QUESTION);   p++; continue; }
        if (*p == '!')  { count += emit_punct(out,count,max_tokens,p,1,PUNCT_EXCLAIM);    p++; continue; }
        if (*p == ';')  { count += emit_punct(out,count,max_tokens,p,1,PUNCT_SEMICOLON);  p++; continue; }
        if (*p == ':')  { count += emit_punct(out,count,max_tokens,p,1,PUNCT_COLON);      p++; continue; }
        if (*p == '"')  {
            /* ASCII " is ambiguous: open when no prior unmatched open exists,
             * close otherwise (toggle logic). */
            bool seen_open = false;
            for (int qi = 0; qi < count; qi++) {
                if (out[qi].is_quote_open)  seen_open = true;
                if (out[qi].is_quote_close) seen_open = false;
            }
            PunctType qt = seen_open ? PUNCT_QUOTE_CLOSE : PUNCT_QUOTE_OPEN;
            count += emit_punct(out,count,max_tokens,p,1,qt); p++; continue;
        }
        /* UTF-8 U+201C LEFT DOUBLE QUOTATION MARK  E2 80 9C  " */
        if ((unsigned char)p[0]==0xE2 && (unsigned char)p[1]==0x80 &&
            (unsigned char)p[2]==0x9C) {
            count += emit_punct(out,count,max_tokens,p,3,PUNCT_QUOTE_OPEN);  p+=3; continue;
        }
        /* UTF-8 U+201D RIGHT DOUBLE QUOTATION MARK E2 80 9D  " */
        if ((unsigned char)p[0]==0xE2 && (unsigned char)p[1]==0x80 &&
            (unsigned char)p[2]==0x9D) {
            count += emit_punct(out,count,max_tokens,p,3,PUNCT_QUOTE_CLOSE); p+=3; continue;
        }

        /* Find end of current word (whitespace, punctuation, or double quote) */
        const char *word_end = p;
        while (*word_end && *word_end != ' ' && *word_end != '\t' &&
               *word_end != '\n' && *word_end != '\r' &&
               !(*word_end=='.'||*word_end==','||*word_end=='!'||
                 *word_end=='?'||*word_end==';'||*word_end==':') &&
               !is_dquote(word_end))
            word_end++;

        /* Handle apostrophe: split into two tokens.
         * e.g. "n'abana" → "n" then "abana"
         * e.g. "k'ishuri" → "k" then "ishuri"
         * Works for both ASCII apostrophe and curly apostrophe (U+2019).
         */
        size_t apos_len = 1;
        const char *apos = find_apostrophe(p, word_end, &apos_len);
        if (apos) {
            /* Emit the part before the apostrophe */
            size_t len = (size_t)(apos - p);
            if (len > 0 && len < KIN_MAX_WORD - 1 && count < max_tokens) {
                memset(&out[count], 0, sizeof(Token));
                memcpy(out[count].surface, p, len);
                out[count].surface[len] = '\0';
                kin_strlower(out[count].surface, out[count].lower,
                             KIN_MAX_WORD);
                count++;
            }
            p = apos + apos_len; /* skip past apostrophe bytes */
            continue;
        }

        /* Normal word: emit entire word_end span */
        size_t wlen = (size_t)(word_end - p);
        if (wlen == 0) { p++; continue; }
        if (wlen >= KIN_MAX_WORD) wlen = KIN_MAX_WORD - 1;

        memset(&out[count], 0, sizeof(Token));
        memcpy(out[count].surface, p, wlen);
        out[count].surface[wlen] = '\0';
        kin_strlower(out[count].surface, out[count].lower, KIN_MAX_WORD);

        /* Detect proper noun: capital letter that is NOT the first token  */
        if (count > 0 && isupper((unsigned char)out[count].surface[0]))
            out[count].is_proper_noun = true;

        count++;
        p = word_end;

        /* ── Split -fite compounds ────────────────────────────────────────── *
         * If this word starts with a known -fite stative form (e.g. "bifite")*
         * immediately followed by more characters (another word run on       *
         * without a space, e.g. "bifiteubugingo"), back p up so the main     *
         * loop re-processes the overflow as a separate token.                *
         *                                                                    *
         * e.g. "bifiteubugingo" → emit "bifite", re-read "ubugingo"         */
        {
            Token *last = &out[count - 1];
            for (int fi = 0; FITE_FORMS[fi]; fi++) {
                size_t flen = strlen(FITE_FORMS[fi]);
                if (wlen > flen &&
                    strncmp(last->lower, FITE_FORMS[fi], flen) == 0) {
                    /* Truncate the emitted token to just the -fite part */
                    last->surface[flen] = '\0';
                    last->lower[flen]   = '\0';
                    /* Back p up to re-process the overflow (wlen - flen chars) */
                    p = p - (wlen - flen);
                    break;
                }
            }
        }
    }
    return count;
}
