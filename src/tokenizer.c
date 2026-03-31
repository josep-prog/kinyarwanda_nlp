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

int kin_tokenize(const char *text, Token *out, int max_tokens) {
    int count = 0;
    const char *p = text;

    while (*p && count < max_tokens) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;

        /* Skip leading double-quote characters (ASCII " and curly " ") so
         * they are not emitted as Foreign tokens.  Trailing quotes are
         * handled below in the post-word skip.                              */
        while (*p == '"' || is_dquote(p))
            p += ((unsigned char)*p == '"') ? 1 : 3;
        if (!*p) break;

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

        /* Skip trailing punctuation and double-quote characters */
        for (;;) {
            if (*p == '.' || *p == ',' || *p == '!' || *p == '?' ||
                *p == ';' || *p == ':' || *p == '"') {
                p++;
            } else if (is_dquote(p)) {
                p += 3;
            } else {
                break;
            }
        }
    }
    return count;
}
