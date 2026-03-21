/*
 * g2p.h
 * Kinyarwanda Grapheme-to-Phoneme (G2P) converter and text normalizer.
 *
 * This module converts written Kinyarwanda text into a sequence of phoneme
 * tokens that can be consumed by a Text-to-Speech (TTS) acoustic model or
 * used as post-processing validation in an ASR (Speech-to-Text) pipeline.
 *
 * ─── KINYARWANDA PHONEME INVENTORY ──────────────────────────────────────────
 *
 * Kinyarwanda is almost perfectly phonemic: each grapheme (letter or digraph)
 * corresponds to exactly one phoneme and vice versa.  This makes G2P simpler
 * than for languages like English or French.
 *
 * Key rules (from RALC 2017 orthographic standard):
 *   1.  Digraphs are processed with longest-match-first priority:
 *       "nsh" > "sh" > "s";  "ny" > "n";  "bw" > "b"
 *   2.  'c' = /tʃ/  (like English "ch" in "church")
 *   3.  'j' = /dʒ/  (like English "j" in "jump")
 *   4.  'r' = /ɾ/   (alveolar tap, like Spanish "r" in "pero")
 *   5.  Vowels (a e i o u) are pure — no diphthongs; each is a full syllable.
 *   6.  Long vowels exist phonemically but are not marked in standard writing.
 *
 * ─── OUTPUT FORMAT ──────────────────────────────────────────────────────────
 *
 * Each phoneme is represented as a short ASCII token.  Tokens are written
 * space-separated into a char buffer.  Word boundaries are represented by
 * the pipe character '|'.
 *
 * Example:
 *   "genda"   → "g e n d a"
 *   "ishuri"  → "i sh u r i"
 *   "bwana"   → "bw a n a"
 *   "nyuma"   → "ny u m a"
 *   "ntibigenda" → "nt i b i g e n d a"
 *
 * ─── TEXT NORMALIZATION ─────────────────────────────────────────────────────
 *
 * Before G2P, text must be normalized:
 *   - Digits 0-9999 are expanded to Kinyarwanda words
 *   - Common abbreviations are expanded (km → kilometero, kg → kilogiramu)
 *   - Punctuation is converted to prosodic markers (. → pause, , → short pause)
 *   - Apostrophes in contractions are expanded (n'amazi → na amazi)
 */

#ifndef G2P_H
#define G2P_H

#include <stdbool.h>
#include <stddef.h>

/* ─── limits ─────────────────────────────────────────────────────────────── */
#define G2P_MAX_PHONEMES   512
#define G2P_MAX_REPR      2048   /* Max chars in ASCII phoneme string         */
#define G2P_MAX_NORM      1024   /* Max chars after text normalization        */

/* ─── Phoneme IDs ────────────────────────────────────────────────────────── *
 *
 * Grouped by category.  The ASCII repr field in KinPhoneme gives the string
 * token used in output (e.g. PH_SH → "sh", PH_BW → "bw").
 */
typedef enum {
    PH_NULL  = 0,

    /* ── Vowels (5) ───────────────────────────────────────────── */
    PH_A,           /* /a/ */
    PH_E,           /* /e/ */
    PH_I,           /* /i/ */
    PH_O,           /* /o/ */
    PH_U,           /* /u/ */

    /* ── Simple consonants (20) ──────────────────────────────── */
    PH_B,           /* /b/ */
    PH_D,           /* /d/ */
    PH_F,           /* /f/ */
    PH_G,           /* /ɡ/ */
    PH_H,           /* /h/ */
    PH_K,           /* /k/ */
    PH_L,           /* /l/ (rare in native words; mainly loanwords) */
    PH_M,           /* /m/ */
    PH_N,           /* /n/ */
    PH_P,           /* /p/ */
    PH_R,           /* /ɾ/ alveolar tap */
    PH_S,           /* /s/ */
    PH_T,           /* /t/ */
    PH_V,           /* /v/ */
    PH_W,           /* /w/ glide */
    PH_Y,           /* /j/ palatal glide */
    PH_Z,           /* /z/ */

    /* ── Affricate / digraph consonants ──────────────────────── */
    PH_C,           /* /tʃ/ — written 'c' in Kinyarwanda                    */
    PH_J,           /* /dʒ/ — written 'j' in Kinyarwanda                    */
    PH_SH,          /* /ʃ/  — written 'sh'                                  */
    PH_NY,          /* /ɲ/  — palatal nasal, written 'ny'                   */
    PH_TS,          /* /ts/ — written 'ts'                                  */

    /* ── Labialized consonants (consonant + /w/ glide) ───────── *
     * These are single phonological units in Kinyarwanda, not    *
     * sequences. Written as C+'w' in orthography.                */
    PH_BW,          /* /bʷ/ */
    PH_CW,          /* /tʃʷ/ */
    PH_DW,          /* /dʷ/ */
    PH_FW,          /* /fʷ/ */
    PH_GW,          /* /ɡʷ/ */
    PH_HW,          /* /hʷ/ */
    PH_KW,          /* /kʷ/ */
    PH_MW,          /* /mʷ/ */
    PH_NW,          /* /nʷ/ */
    PH_PW,          /* /pʷ/ */
    PH_RW,          /* /ɾʷ/ */
    PH_SW,          /* /sʷ/ */
    PH_TW,          /* /tʷ/ */
    PH_VW,          /* /vʷ/ */
    PH_YW,          /* /jʷ/ */
    PH_ZW,          /* /zʷ/ */
    PH_SHW,         /* /ʃʷ/ — written 'shw' */

    /* ── Prenasalized consonant clusters ─────────────────────── *
     * Valid nasal-initial clusters (from RALC consonant table).   */
    PH_MB,          /* /mb/ */
    PH_MF,          /* /mf/ */
    PH_MP,          /* /mp/ */
    PH_MV,          /* /mv/ */
    PH_ND,          /* /nd/ */
    PH_NG,          /* /ŋɡ/ */
    PH_NGW,         /* /ŋɡʷ/ */
    PH_NJ,          /* /ndʒ/ */
    PH_NK,          /* /ŋk/ */
    PH_NSH,         /* /nʃ/ */
    PH_NT,          /* /nt/ */
    PH_NZ,          /* /nz/ */
    PH_NZW,         /* /nzʷ/ */

    /* ── Prosodic markers ────────────────────────────────────── */
    PH_WORD_BOUND,  /* '|' — word boundary (for sentence-level G2P)  */
    PH_PAUSE_SHORT, /* ',' — short pause (comma intonation)          */
    PH_PAUSE_LONG,  /* '.' — sentence-final pause                    */

    PH_COUNT        /* total number of phoneme types                  */
} KinPhonemeID;

/* ─── Phoneme sequence ───────────────────────────────────────────────────── */
typedef struct {
    KinPhonemeID  phones[G2P_MAX_PHONEMES];
    int           count;
    char          repr[G2P_MAX_REPR];   /* space-separated ASCII tokens       */
} KinPhonemeSeq;

/* ══════════════════════════════════════════════════════════════════════════
 * Public API
 * ══════════════════════════════════════════════════════════════════════════ */

/* --- Text normalization -------------------------------------------------- */

/*
 * kin_normalize_text()
 *
 * Pre-process raw input text before G2P conversion:
 *   - Expand apostrophe contractions: n'amazi → na amazi
 *   - Expand digit runs: "123" → "ijana na makumyabiri na gatatu"
 *   - Expand common abbreviations: km → kilometero
 *   - Convert punctuation to prosodic markers
 *   - Collapse multiple spaces
 *
 * `output` must be at least G2P_MAX_NORM bytes.
 */
void kin_normalize_text(const char *input, char *output, size_t outsize);

/* --- G2P conversion ------------------------------------------------------ */

/*
 * kin_g2p_word()
 *
 * Convert a single Kinyarwanda word (already normalized, lowercase) to its
 * phoneme sequence.  Returns false if the word is empty or contains only
 * non-Kinyarwanda characters.
 *
 * The phoneme tokens are space-separated in out->repr.
 * Example: "genda" → phones=[G,E,N,D,A], repr="g e n d a"
 */
bool kin_g2p_word(const char *word, KinPhonemeSeq *out);

/*
 * kin_g2p_sentence()
 *
 * Convert a full sentence to a phoneme sequence.  Words are separated by
 * PH_WORD_BOUND markers.  The input is normalized internally.
 *
 * Example:
 *   "Imana iravuga" → "i m a n a | i r a v u g a"
 */
bool kin_g2p_sentence(const char *text, KinPhonemeSeq *out);

/* --- Utility ------------------------------------------------------------- */

/* Human-readable ASCII token for a phoneme ID (e.g. PH_SH → "sh") */
const char *kin_phoneme_token(KinPhonemeID id);

/* IPA string for a phoneme ID (e.g. PH_SH → "ʃ") */
const char *kin_phoneme_ipa(KinPhonemeID id);

/* Number of phonemes in the Kinyarwanda inventory (excluding prosodic) */
int kin_phoneme_count(void);

#endif /* G2P_H */
