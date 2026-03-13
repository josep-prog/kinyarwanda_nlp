/*
 * tokenizer.c
 * Splits Kinyarwanda text into word tokens.
 *
 * Kinyarwanda orthographic notes (from book p.4-12):
 *  - Apostrophe contracts: n'uku, k'ibuye, b'abana (the elided form of
 *    the final vowel of the preceding word).  We split on apostrophe,
 *    yielding two tokens (the clitic attaches to the next word).
 *  - Hyphen in reduplicated adjective stems: to-to, re-re, sa-sa.
 *    We keep these as single tokens.
 *  - Punctuation (.,?!;:) ends a token but is discarded.
 *  - Upper-case letters: preserved in surface form; used to detect proper
 *    nouns (words capitalised mid-sentence).
 */

#include <string.h>
#include <ctype.h>
#include "../include/kinyarwanda.h"

int kin_tokenize(const char *text, Token *out, int max_tokens) {
    int count = 0;
    const char *p = text;

    while (*p && count < max_tokens) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;

        /* Handle apostrophe: split into two tokens.
         * e.g. "n'abana" → "n" then "abana"
         * e.g. "k'ishuri" → "k" then "ishuri"
         * The first part is usually a preposition/conjunction clitic.
         */
        if (strchr(p, '\'')) {
            const char *apos = strchr(p, '\'');
            /* Only split if apostrophe is within the current word */
            /* Find where current word ends (whitespace or punct) */
            const char *word_end = p;
            while (*word_end && *word_end != ' ' && *word_end != '\t' &&
                   *word_end != '\n' && *word_end != '\r' &&
                   !(*word_end=='.'||*word_end==','||*word_end=='!'||
                     *word_end=='?'||*word_end==';'||*word_end==':'))
                word_end++;

            if (apos < word_end) {
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
                p = apos + 1; /* skip past apostrophe */
                continue;
            }
        }

        /* Normal word: read until whitespace or punctuation */
        const char *start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' &&
               !(*p=='.'||*p==','||*p=='!'||*p=='?'||*p==';'||*p==':'))
            p++;

        size_t wlen = (size_t)(p - start);
        if (wlen == 0) { p++; continue; }
        if (wlen >= KIN_MAX_WORD) wlen = KIN_MAX_WORD - 1;

        memset(&out[count], 0, sizeof(Token));
        memcpy(out[count].surface, start, wlen);
        out[count].surface[wlen] = '\0';
        kin_strlower(out[count].surface, out[count].lower, KIN_MAX_WORD);

        /* Detect proper noun: capital letter that is NOT the first token  */
        if (count > 0 && isupper((unsigned char)out[count].surface[0]))
            out[count].is_proper_noun = true;

        count++;

        /* Skip trailing punctuation */
        while (*p == '.' || *p == ',' || *p == '!' || *p == '?' ||
               *p == ';' || *p == ':' || *p == '"')
            p++;
    }
    return count;
}
