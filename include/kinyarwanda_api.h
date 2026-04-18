/*
 * kinyarwanda_api.h — Public C API for the Kinyarwanda NLP library.
 *
 * Include this header (and link with -lkinyarwanda) in downstream projects:
 *   - ASR post-processor  (whisper.cpp → kin_correct → clean transcript)
 *   - TTS frontend        (text → kin_g2p → phoneme string → acoustic model)
 *   - Any C/C++ project   (full analysis via kin_analyze)
 *
 * C++ callers: extern "C" is handled automatically via the guards below.
 * No C++ headers are included; safe to use in a C++ translation unit.
 *
 * Thread safety: kin_correct() and kin_g2p() use static buffers — call
 * from one thread at a time (or copy the result before the next call).
 * kin_analyze() returns a value (SentenceAnalysis on the stack) — safe
 * to call from multiple threads simultaneously.
 */

#ifndef KINYARWANDA_API_H
#define KINYARWANDA_API_H

#include "kinyarwanda.h"   /* SentenceAnalysis, Token, Error, POS, … */
#include "g2p.h"           /* KinPhonemeSeq, kin_g2p_sentence, …     */

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 1. ASR post-processor
 * ══════════════════════════════════════════════════════════════════════════
 *
 * kin_correct(text)
 *
 *   Runs the NLP engine on `text`, collects per-token spelling suggestions,
 *   and returns a corrected string with suggestions substituted in place.
 *   Words without a correction are returned as-is.
 *
 *   Use case:
 *     char *raw  = whisper_transcribe(audio);   // "Umuntu mugni aragenda"
 *     const char *fixed = kin_correct(raw);     // "Umuntu munini aragenda"
 *     send_to_downstream(fixed);
 *
 *   Return value: pointer to an internal static buffer.
 *   Valid until the next call to kin_correct().
 */
const char *kin_correct(const char *text);

/* ══════════════════════════════════════════════════════════════════════════
 * 2. TTS phoneme frontend (G2P)
 * ══════════════════════════════════════════════════════════════════════════
 *
 * kin_g2p(text)
 *
 *   Normalises `text` (digit expansion, abbreviation expansion, apostrophe
 *   splitting) then converts it to a space-separated ASCII phoneme string.
 *   Word boundaries are marked with '|'.
 *
 *   Use case:
 *     const char *phones = kin_g2p("Imana iravuga");
 *     // → "i m a n a | i r a v u g a"
 *     feed_to_acoustic_model(phones);
 *
 *   Return value: pointer to an internal static buffer (inside KinPhonemeSeq).
 *   Valid until the next call to kin_g2p().
 */
const char *kin_g2p(const char *text);

/* ══════════════════════════════════════════════════════════════════════════
 * 3. Full sentence analysis
 * ══════════════════════════════════════════════════════════════════════════
 *
 * kin_analyze(text)  — declared in kinyarwanda.h, re-documented here.
 *
 *   Runs the full pipeline:
 *     tokenize → POS-tag → morpheme-analyse → syntax-check → grammar roles
 *
 *   Returns a SentenceAnalysis value (stack-allocated, caller owns it).
 *   Safe to call from multiple threads simultaneously.
 *
 *   Quick-access fields:
 *     sa.tokens[i].pos          — part of speech (POS enum)
 *     sa.tokens[i].verb_tense   — tense (VerbTense enum, if verb)
 *     sa.tokens[i].noun_class   — noun class 1-16 (if noun)
 *     sa.tokens[i].stem         — bare stem (igicumbi)
 *     sa.tokens[i].morph        — morpheme breakdown (D+RT+C / SP+TM+…)
 *     sa.errors[i]              — grammar/spelling errors detected
 *     sa.has_verb               — true if sentence contains a verb
 *
 *   SentenceAnalysis kin_analyze(const char *text);   ← see kinyarwanda.h
 */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* KINYARWANDA_API_H */
