/*
 * kinyarwanda.h
 * Rule-based Kinyarwanda NLP engine
 * Based on: "Ikinyarwanda Amashuri Nderabarezi (TTC)" - REB 2020
 *
 * Grammar terms used throughout (from the book):
 *   Izina mbonera   = Common noun      (D + RT + C structure)
 *   Ntera           = Adjective        (RS + C structure)
 *   Izina ntera     = Relative noun    (noun used as qualifier)
 *   Igisantera      = Compound adj     (noun pair acting as adjective)
 *   Inshinga        = Verb
 *   Ikinyazina      = Pronoun          (many subtypes)
 *   Amagambo adahinduka = Invariable words (prepositions, conjunctions, etc.)
 *   Inteko          = Noun class (1-16)
 *   Indanganteko    = Class marker (RT)
 *   Indomo (D)      = Prefix vowel
 *   Igicumbi (C)    = Stem/root
 *   Indangasano(RS) = Concordance prefix (for adjectives)
 */

#ifndef KINYARWANDA_H
#define KINYARWANDA_H

#include <stdbool.h>
#include <stddef.h>

/* ─── limits ─────────────────────────────────────────────────────────────── */
#define KIN_MAX_WORD      128
#define KIN_MAX_STEM       96
#define KIN_MAX_PREFIX     32
#define KIN_MAX_TOKENS    256
#define KIN_MAX_ERRORS     64
#define KIN_MAX_MSG       256

/* ─── Part-of-speech tags ─────────────────────────────────────────────────── */
typedef enum {
    POS_UNKNOWN        = 0,
    POS_NOUN,              /* Izina mbonera                                */
    POS_ADJECTIVE,         /* Ntera                                        */
    POS_RELATIVE_NOUN,     /* Izina ntera                                  */
    POS_COMPOUND_ADJ,      /* Igisantera                                   */
    POS_VERB_INF,          /* Inshinga – infinitive (ku-/gu-/kw- form)     */
    POS_VERB_CONJ,         /* Inshinga – conjugated form                   */
    POS_PRONOUN,           /* Ikinyazina (any type)                        */
    POS_PREPOSITION,       /* Umugereka / Ingera                           */
    POS_CONJUNCTION,       /* Icyungo                                      */
    POS_INTERJECTION,      /* Irangamutima                                 */
    POS_ADVERB,            /* Akamamo                                      */
    POS_LOCATIVE,          /* Indangahantu                                 */
    POS_VERB_PARTICLE,     /* Ikegeranshinga: ngo, ko                      */
    POS_FOREIGN,           /* Word not matching any Kinyarwanda pattern    */
} POS;

/* ─── Verb tense (ibihe by'inshinga) ─────────────────────────────────────────
 * REB 2020 "Amashuri yisumbuye" terminology (section 1.8):
 *   Indagihe   = Present tense  (3 sub-types: ako kanya / ubusanzwe / ikomeza)
 *   Impitagihe = Past tense     (Impitakare = recent; Impitakera = remote)
 *   Inzagihe   = Future tense   (Inzahato = near; Inzakera = remote)
 *
 * REB Year-4 book (section ITONDAGURANSHINGA) adds verb MODES (uburyo):
 *   Ikirango    = Indicative  (assertion – uses all tenses above)
 *   Inyifurizo  = Optative    (wish/blessing: SP+ra+ka+stem+a  urakabyara)
 *   Integeko    = Imperative  (command: stem+a  genda / nimu+stem+e  nimugende)
 *   Inkurikizo  = Sequential  (narr. sequence: SP+ka+stem+a  akagenda)  ← = NARRATIVE
 *   Ikigombero  = Subjunctive (obligation/desire: SP+stem+e  agende)    ← = SUBJUNCTIVE
 *   Inziganyo   = Conditional (if/would: SP+a+stem+a  twize/SP+nta+ku+stem+a)
 */
typedef enum {
    TENSE_NONE         = 0,
    TENSE_PRESENT,         /* Indagihe y'ako kanya: SP+ra+stem+a    aragenda */
    TENSE_PRESENT_NORA,    /* Indagihe y'ubusanzwe: SP+stem+a       ibona    */
    TENSE_PAST_PERF,       /* Impitakare: SP+stem+ye                yaremye  */
    TENSE_PAST_IMPF,       /* Impitakera: SP+stem+aga               yagendaga*/
    TENSE_FUTURE,          /* Inzagihe: SP+za+stem+a                azagenda */
    TENSE_SUBJUNCTIVE,     /* Ikigombero/Isabira: SP+stem+e         agende   */
    TENSE_NARRATIVE,       /* Inkurikizo: SP+ka+stem+a              akagenda */
    TENSE_OPTATIVE,        /* Inyifurizo: SP+ra+ka+stem+a           urakabyara*/
    TENSE_IMPERATIVE,      /* Integeko: bare stem+a                 genda    */
    TENSE_CONDITIONAL,     /* Inziganyo: SP+a+Ø+stem+a              twatsinda*/
} VerbTense;

/* ─── Verb derivational extensions (itondaguranshinga) ───────────────────────
 * REB Year-2 book, chapter 26:
 *   Imbundo  = Passive      stem + -w-          gukor-w-a
 *   Integeko = Causative    stem + -ish-/-esh-  gukor-ish-a
 *   Ikirango = Applicative  stem + -ir-/-er-    gukor-er-a
 * Plus the reciprocal (sociative):
 *   Igisubizo = Reciprocal  stem + -an-         gukor-an-a
 */
typedef enum {
    VEXT_NONE        = 0,
    VEXT_PASSIVE,        /* Imbundo:   -w-    gukorwa, yakorwaga     */
    VEXT_CAUSATIVE,      /* Integeko:  -ish-/-esh-  gukorisha        */
    VEXT_APPLICATIVE,    /* Ikirango:  -ir-/-er-    gukorera         */
    VEXT_RECIPROCAL,     /* Igisubizo: -an-         gukorana         */
} VerbExtension;

/* ─── Pronoun sub-types (amoko y'ibinyazina) ─────────────────────────────── */
typedef enum {
    PRON_NONE          = 0,
    PRON_DEMONSTRATIVE,    /* Ikinyazina nyereka:  uyu, uwo, uno...        */
    PRON_PERSONAL,         /* Ikinyazina ngenga:   nge, mwe, we, bo...     */
    PRON_POSSESSIVE,       /* Ikinyazina ngenera:  wa, ya, rya...          */
    PRON_REFLEXIVE,        /* Ikinyazina ngenera ngenga: wange, wacu...    */
    PRON_RELATIVE,         /* Ikinyazina ngenera (relative connector)      */
    PRON_INTERROGATIVE,    /* Ikinyazina kibaza:   nde, iki, iyihe...      */
    PRON_INDEFINITE,       /* Ikinyazina ndafutura: umwe, bamwe...         */
    PRON_CONCORDANCE,      /* Ikinyazina mboneranteko                      */
    PRON_NUMERICAL,        /* Ikinyazina nyamubaro                         */
    PRON_VOCATIVE,         /* Ikinyazina mpamagazi                         */
} PronounType;

/* ─── Noun class (Inteko) table entry ──────────────────────────────────────
 *
 * The book (p.61-62) defines 16 noun classes. Structure is D + RT + C.
 * We store the combined surface prefix for fast matching.
 */
typedef struct {
    int   num;                   /* 1-16                                   */
    char  prefix[8];             /* Combined D+RT surface form, e.g. "umu" */
    char  rt[4];                 /* Indanganteko alone, e.g. "mu"          */
    char  concordance_adj[8];    /* Indangasano for ntera (adj) agreement  */
    char  concordance_poss[8];   /* Possessive connector (ikinyazina ngenera) */
    char  subj_prefix[8];        /* Verb subject agreement prefix          */
    const char *description;
} NounClass;

/* ─── Orthographic violation types (igenantego) ──────────────────────────── *
 * Each constant maps to a specific RALC rule section.                        */
typedef enum {
    ORTHO_OK             = 0,
    ORTHO_VV_HIATUS,         /* §1.1/§2.1: two adjacent vowels (VV not resolved) */
    ORTHO_NASAL_ASSIM,       /* §3.3:  n before f/p/b/v/h must become m/mp       */
    ORTHO_NASAL_ELISION,     /* §3.1:  n before m/n/ny must elide                */
    ORTHO_CY_UNFUSED,        /* §3.9:  C+y at morpheme boundary must fuse        */
    ORTHO_STOP_UNDELETED,    /* §3.6:  epenthetic stop (t in nts, p in mpf) kept */
    ORTHO_C_NOT_SH,          /* §3.6.2: nc must become nsh                       */
    ORTHO_VOWEL_ASSIM,       /* §1.3:  -ir-/-ish- before o-stem must be -er-/-esh-*/
} OrthoViolationType;

typedef struct {
    OrthoViolationType type;
    int   pos;                   /* byte offset in the word where violation starts */
    char  rule[16];              /* RALC rule ID, e.g. "§3.9.4"                    */
    char  msg[KIN_MAX_MSG];      /* bilingual explanation (Kinyarwanda / English)  */
} OrthoViolation;

/* ─── Error types ─────────────────────────────────────────────────────────── */
typedef enum {
    ERR_NONE              = 0,
    ERR_ADJ_AGREEMENT,         /* Adj class doesn't match noun class       */
    ERR_POSS_AGREEMENT,        /* Possessive doesn't match noun class      */
    ERR_NO_VERB,               /* Sentence has no verb                     */
    ERR_UNKNOWN_WORD,          /* Word not recognized                      */
    ERR_INVALID_CLUSTER,       /* Invalid consonant cluster                */
    ERR_SPELLING,              /* Likely spelling error                    */
    ERR_SUBJ_VERB_AGREEMENT,   /* Verb SP doesn't match subject noun class */
    ERR_OBJ_VERB_AGREEMENT,    /* Verb OM doesn't match object noun class  */
    ERR_VOWEL_HIATUS,          /* Two adjacent vowels (iranya ry'impanvu)  */
    ERR_VERB_SELECTION,        /* Wrong verb choice (e.g. kugenda vs kujya) */
} ErrorType;

typedef struct {
    ErrorType type;
    int       token_index;
    char      message[KIN_MAX_MSG];
    char      suggestion[KIN_MAX_MSG];
} Error;

/* ─── Analysed token ──────────────────────────────────────────────────────── */
typedef struct {
    char surface[KIN_MAX_WORD];     /* Word as written in the input         */
    char lower[KIN_MAX_WORD];       /* Lowercased form                      */
    POS  pos;                       /* Part-of-speech tag                   */
    PronounType   pron_type;        /* If POS_PRONOUN, which subtype        */
    VerbTense     verb_tense;       /* If POS_VERB_CONJ, detected tense     */
    VerbExtension verb_ext;         /* Derivational extension (imbundo etc) */
    int  noun_class;                /* 1-16 if applicable; 0 = unknown      */
    int  obj_class;                 /* Object marker class (0 = none)       */
    char stem[KIN_MAX_STEM];        /* Igicumbi – bare stem after OM/ext    */
    char detected_prefix[KIN_MAX_PREFIX]; /* D+RT detected                  */
    bool is_kinyarwanda;            /* False = likely foreign/unknown       */
    bool is_proper_noun;            /* Capitalised and not at start         */
    bool is_negative;               /* nt- negative prefix detected         */
    int  error_count;               /* Number of errors on this token       */
} Token;

/* ─── Sentence analysis result ───────────────────────────────────────────── */
typedef struct {
    Token  tokens[KIN_MAX_TOKENS];
    int    token_count;
    Error  errors[KIN_MAX_ERRORS];
    int    error_count;
    bool   has_verb;
    bool   is_complete;   /* Has subject + verb at minimum                  */
} SentenceAnalysis;

/* ══════════════════════════════════════════════════════════════════════════
 * Public API
 * ══════════════════════════════════════════════════════════════════════════ */

/* tokenizer.c */
int  kin_tokenize(const char *text, Token *out, int max_tokens);

/* morphology.c */
int  kin_detect_noun_class(const char *word);
bool kin_strip_noun_prefix(const char *word, char *stem_out, int *class_out);
bool kin_is_verb_infinitive(const char *word, char *stem_out);
bool kin_is_verb_conjugated(const char *word, char *stem_out, int *subj_class,
                            VerbTense *tense_out, int *obj_class_out,
                            VerbExtension *ext_out, bool *neg_out);
bool kin_strip_adj_prefix(const char *word, char *stem_out, int *class_out);

/* ── Vowel & consonant phonology rules ─────────────────────────────────────
 * Amategeko y'igenamajwi (phonological rules) – REB book p.7-8, p.62
 *
 * VOWEL CONTACT RULE (Iranya ry'impanvu):
 *   In Kinyarwanda, two vowels CANNOT appear adjacent within a word.
 *   When morpheme boundaries would create VV contact, one of these applies:
 *
 *   1. u → w  (before any vowel)    ku+eza  → kweza
 *   2. i → y  (before any vowel)    ki+eza  → kyeza / ikyeza
 *   3. a → Ø  (elision before V)    na+amazi→ n'amazi;  ba+eza → b'eza
 *   4. a+i→ e (vowel fusion)        ba+inja → benja;    mu+inja → menja
 *   5. a+e→ e (a drops before e)    ya+eza  → yeza
 *   6. a+o→ o (a drops before o)    ya+oya  → yoya (rarely contracted)
 *
 * CONSONANT CLUSTER RULE (Iteganyo ry'inzarara z'inkongi):
 *   Valid clusters in Kinyarwanda (all nasal-initial):
 *     mb  mp  mv  mf            (m before bilabials)
 *     nd  ng  nk  nz  nj  nt   (n before dentals/velars)
 *     nsh nzw ngw               (n + consonant cluster)
 *   Invalid: any other CC combination (e.g. str, bl, tr, sk, ks...)
 *
 * These functions return true when the word VIOLATES the rule (error).
 * ─────────────────────────────────────────────────────────────────────── */
bool kin_has_vowel_hiatus(const char *word);
bool kin_has_invalid_cluster(const char *word);

/* lexicon.c */
bool kin_is_invariable(const char *word, POS *pos_out);
bool kin_is_pronoun(const char *word, PronounType *type_out, int *class_out);
bool kin_is_adj_stem(const char *stem);
bool kin_is_adj_reduplicated(const char *sfx, const char *pfx, char *stem_out);
bool kin_is_known_verb_stem(const char *stem);
bool kin_is_known_noun_stem(const char *stem, int *class_out);
bool kin_is_known_full_word(const char *word, int *class_out, char *stem_out);
const NounClass *kin_get_noun_class(int num);
const char *kin_pos_name(POS pos);
const char *kin_class_name(int class_num);
const char *kin_pron_type_name(PronounType t);
const char *kin_verb_tense_name(VerbTense t);
const char *kin_verb_ext_name(VerbExtension e);

/* pos_tagger.c */
void kin_tag_token(Token *tok);
void kin_tag_sentence(SentenceAnalysis *sa);

/* syntax.c */
void kin_check_syntax(SentenceAnalysis *sa);

/* corrector.c */
void kin_suggest_corrections(SentenceAnalysis *sa);

/* ortho.c  –  orthographic rule engine (RALC 2017) */

/* Forward generation: apply all rules to '|'-delimited morpheme string.
 * noun_class_9=true activates §2.4 (n+y→nz) for Nt.9/10 prefix context. */
void kin_ortho_gen(const char *morphemes, bool noun_class_9,
                   char *surface, size_t size);

/* Validate a surface word; fill viol[0..max-1], return violation count. */
int  kin_ortho_validate(const char *word, OrthoViolation *viol, int max);

/* Recover candidate underlying verb roots from a surface conjugated stem.
 * `raw_stem` is the stem AFTER stripping SP, tense markers, and final vowel.
 * Returns number of candidates written to roots[][KIN_MAX_STEM] (0 = none). */
int  kin_ortho_recover_verb_root(const char *raw_stem,
                                  char roots[][KIN_MAX_STEM], int max_roots);

/* Extract bare stem from a Nt.9/10 noun (the part after the outer 'i' D-vowel).
 * Reverses §2.4, §3.1, §3.3, §3.6.2 prefix transformations. */
void kin_ortho_nt9_stem(const char *after_i, char *stem_out, size_t size);

/* Human-readable name for an OrthoViolationType. */
const char *kin_ortho_rule_name(OrthoViolationType t);

/* analysis.c  (main pipeline) */
SentenceAnalysis kin_analyze(const char *text);
void kin_print_analysis(const SentenceAnalysis *sa, bool verbose);

/* utils */
void kin_strlower(const char *src, char *dst, size_t dstlen);
bool kin_starts_with(const char *s, const char *prefix);
bool kin_ends_with(const char *s, const char *suffix);
void kin_str_trim(char *s);

#endif /* KINYARWANDA_H */
