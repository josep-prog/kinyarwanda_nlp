/*
 * api.c — Kinyarwanda NLP library: high-level convenience wrappers.
 *
 * Implements the two string-returning functions declared in kinyarwanda_api.h:
 *   kin_correct()  — ASR post-processor (spelling correction)
 *   kin_g2p()      — G2P phoneme string for TTS
 *
 * kin_analyze() is already implemented in analysis.c.
 */

#include "../include/kinyarwanda_api.h"
#include <string.h>
#include <stdio.h>

/* ── kin_correct ─────────────────────────────────────────────────────────────
 *
 * Run the full NLP pipeline, collect spelling corrections, then reconstruct
 * the sentence substituting corrected forms where available.
 *
 * Algorithm:
 *   1. kin_analyze() → tokenise, tag, morpheme-analyse, syntax-check
 *   2. kin_suggest_corrections() → fill Error entries with suggestions
 *   3. For each token, use the error's corrected_word if one was actually
 *      computed, else the surface form. 'suggestion' is deliberately NOT
 *      used here — it is a bilingual explanatory sentence, not a word, and
 *      is not always populated even when corrected_word is.
 *   4. Join with spaces; punctuation tokens are appended without a leading space
 *
 * Static buffer: 4096 bytes — sufficient for the longest plausible sentence.
 */
const char *kin_correct(const char *text)
{
    static char buf[4096];

    if (!text || !text[0]) {
        buf[0] = '\0';
        return buf;
    }

    SentenceAnalysis sa = kin_analyze(text);
    kin_suggest_corrections(&sa);

    buf[0] = '\0';
    size_t pos = 0;

    for (int i = 0; i < sa.token_count; i++) {
        Token *tok = &sa.tokens[i];

        /* Determine the word to emit: prefer a literal corrected word.
         * 'suggestion' is never used here -- it is bilingual explanatory
         * prose, not a replacement token, and substituting it would corrupt
         * the reconstructed sentence.
         * Apply: ERR_SPELLING (typos) and ERR_ADJ_AGREEMENT (noun-class
         * concordance, e.g. "umugabo yose" → "umugabo wese"). */
        const char *word = tok->surface;
        for (int e = 0; e < sa.error_count; e++) {
            ErrorType et = sa.errors[e].type;
            if (sa.errors[e].token_index == i
                && (et == ERR_SPELLING || et == ERR_ADJ_AGREEMENT)
                && sa.errors[e].corrected_word[0] != '\0') {
                word = sa.errors[e].corrected_word;
                break;
            }
        }

        /* Spacing: apostrophe-split tokens rejoin with apostrophe (no space);
         * closing punctuation attaches directly; everything else gets a space. */
        bool attach_left = (tok->pos == POS_PUNCTUATION
                            && tok->punct_type != PUNCT_QUOTE_OPEN);

        if (pos > 0) {
            if (tok->preceded_by_apostrophe) {
                if (pos < sizeof(buf) - 1)
                    buf[pos++] = '\'';
            } else if (!attach_left) {
                if (pos < sizeof(buf) - 1)
                    buf[pos++] = ' ';
            }
        }

        size_t wlen = strlen(word);
        if (pos + wlen >= sizeof(buf))
            wlen = sizeof(buf) - pos - 1;   /* truncate rather than overflow */

        memcpy(buf + pos, word, wlen);
        pos += wlen;
        buf[pos] = '\0';
    }

    return buf;
}

/* ── kin_g2p ─────────────────────────────────────────────────────────────────
 *
 * Convert text to a space-separated phoneme string via the G2P engine.
 * Normalisation (digit expansion, apostrophes, abbreviations) is handled
 * internally by kin_g2p_sentence().
 *
 * Returns a pointer into KinPhonemeSeq::repr — a static buffer.
 * Returns "" on failure (empty input or non-Kinyarwanda text).
 */
const char *kin_g2p(const char *text)
{
    static KinPhonemeSeq seq;

    if (!text || !text[0]) {
        seq.repr[0] = '\0';
        return seq.repr;
    }

    if (!kin_g2p_sentence(text, &seq))
        seq.repr[0] = '\0';

    return seq.repr;
}
