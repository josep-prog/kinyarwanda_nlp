# Chapter 16 — `g2p.c`: Another Full Table-Driven Conversion Pass

## A familiar shape, one genuinely new idea

`g2p.c` (626 lines) converts written Kinyarwanda into a phoneme sequence
for text-to-speech and speech-recognition use. Most of its machinery —
longest-match-first table lookup, `NULL`/sentinel-terminated tables,
`strncmp`-based digraph matching — is technique you've already mastered
from Chapters 6, 7, and 14. This chapter moves quickly through the
familiar parts and spends its real attention on the one idea this file
introduces that no earlier chapter needed: maintaining **two different
representations of the same sequence at once**, kept in sync from a
single chokepoint function.

## 16.1 `KinPhonemeSeq`: a struct holding two views of the same data

```c
typedef struct {
    KinPhonemeID  phones[G2P_MAX_PHONEMES];  /* structured: one ID per phoneme */
    int           count;
    char          repr[G2P_MAX_REPR];        /* flat: "g e n d a" as one string */
} KinPhonemeSeq;
```

`phones[]` + `count` is Chapter 3's capacity+count idiom, holding the
phoneme sequence as **structured data** — an array of small integer IDs,
exactly the form a downstream TTS acoustic model or speech-recognition
system would want to consume directly, one ID at a time, with no parsing
required. `repr` holds the *identical* sequence as a **flat,
human-readable string** (`"g e n d a"`) — the form a developer debugging
the pipeline, or a test comparing expected output, would actually want to
read or print. Both fields describe the same underlying phoneme sequence;
they just serve two different audiences.

## 16.2 `seq_append`: one function, keeping both views honest

```c
static bool seq_append(KinPhonemeSeq *seq, KinPhonemeID ph) {
    if (seq->count >= G2P_MAX_PHONEMES - 1) return false;
    seq->phones[seq->count++] = ph;

    const char *tok = kin_phoneme_token(ph);
    size_t repr_len = strlen(seq->repr);
    size_t tok_len  = strlen(tok);

    if (repr_len > 0 && seq->repr[repr_len-1] != '|' &&
        repr_len + 1 + tok_len + 1 < G2P_MAX_REPR) {
        seq->repr[repr_len++] = ' ';
        seq->repr[repr_len]   = '\0';
    }
    if (repr_len + tok_len + 1 < G2P_MAX_REPR) {
        strcpy(seq->repr + repr_len, tok);
    }
    return true;
}
```

Because `phones[]` and `repr` are two separate fields describing the same
logical sequence, *every* place in this file that adds one phoneme has to
update **both** of them, or the two views would silently drift out of
sync — `repr` claiming a different sequence than `phones[]` actually
holds. Rather than repeating that two-part update (append to the array,
append to the string) at every one of the many call sites in
`kin_g2p_word()` and `kin_g2p_sentence()`, it's written exactly once,
here, and every other part of the file calls `seq_append()` instead of
touching either field directly.

This completes a pattern worth naming explicitly, because you've now
seen it three times, in three different shapes, across three different
chapters: Chapter 11's `set_morph()` centralized a repeated *copy*
operation; Chapter 15's `scan_back_noun()` centralized a repeated
*search* algorithm; `seq_append()` here centralizes a repeated
*dual-state update*. All three are the same underlying engineering
instinct — DRY, "don't repeat yourself" — applied to three different
*kinds* of repetition. Being able to point at all three and say "this is
the same principle, applied to copying, then to searching, then to
keeping two derived views consistent" is a strong, synthesizing answer if
you're asked to identify recurring design patterns across the codebase.

## 16.3 Building a delimited string without a `join()` function

Notice exactly how the space separator gets added:

```c
if (repr_len > 0 && seq->repr[repr_len-1] != '|' &&
    repr_len + 1 + tok_len + 1 < G2P_MAX_REPR) {
    seq->repr[repr_len++] = ' ';
    seq->repr[repr_len]   = '\0';
}
```

C has no `", ".join(list)`-style function. To build a space-separated
list incrementally, one token at a time, the code has to decide, **at
the moment each new token is appended**, whether a separator is needed —
and the only information available to make that decision is whatever is
*already* sitting in the buffer so far. `repr_len > 0` answers "is this
the very first token" (don't prepend a space before the first one).
`seq->repr[repr_len-1] != '|'` answers a second, more specific question:
don't add a space right after a word-boundary marker either, since the
`|` itself already visually separates words. This is the general shape
of incremental delimiter-joining in C: inspect the **last character
already written**, and let that — not a loop counter or an externally
tracked "is this the first iteration" flag — decide whether a separator
belongs before the next piece.

## 16.4 `PH_COUNT`: counting enum values without `sizeof`

```c
typedef enum {
    PH_NULL = 0,
    PH_A, PH_E, PH_I, PH_O, PH_U,
    /* ... every phoneme type ... */
    PH_WORD_BOUND, PH_PAUSE_SHORT, PH_PAUSE_LONG,
    PH_COUNT        /* total number of phoneme types */
} KinPhonemeID;
```

```c
int kin_phoneme_count(void) { return (int)PH_COUNT; }
/* ... */
if (id < 0 || id >= PH_COUNT) return "?";
```

This is a different technique from Chapter 7's `sizeof(arr)/
sizeof(arr[0])` array-length idiom, solving the identical *meta*-problem
("how many of these are there, without hardcoding a number that has to
be kept in sync by hand") for a fundamentally different kind of
collection. An `enum`'s members auto-increment from whatever the first
explicit value was (Chapter 2) — so by deliberately placing one **extra**
member at the very end, with no explicit value of its own, that member's
value automatically becomes exactly equal to "however many real members
came before it." `PH_COUNT` is never used as an actual phoneme anywhere
in the program; its only job is to *equal* the total count, automatically,
forever, no matter how many phoneme types get added or removed above it
in the list. This is a name worth recognizing on sight in other people's
C code: a trailing, deliberately-unassigned enum constant — frequently
spelled `_COUNT`, `_MAX`, or `_LAST` — almost always exists purely to
answer "how many of these enum values exist," the enum equivalent of
Chapter 7's `sizeof` trick for arrays.

## 16.5 Longest-match-first, one more time, in a new domain

```c
static const G2PRule G2P_3[] = { /* 3-character digraph/cluster rules */ };
static const G2PRule G2P_2[] = { /* 2-character rules                 */ };
static const G2PRule G2P_1[] = { /* single-character fallback rules   */ };
```

The file's own header comment states the principle directly: *"Digraphs
are processed with longest-match-first priority: `nsh` > `sh` > `s`."*
This is Chapter 6's longest-match algorithm, unchanged in spirit, applied
to a third linguistic level — Chapter 6 used it to identify noun-class
prefixes, this file uses the identical discipline to decide whether three
consecutive letters form one prenasalized cluster phoneme, two letters
form a labialized consonant, or a single letter stands alone — by simply
trying the 3-character table first, falling back to the 2-character
table, and only then the 1-character table. The fact that the *same*
ordering principle reappears, unprompted, in a file dealing with sound
rather than word-internal structure is a good, concrete illustration that
this isn't a trick specific to noun classes — it's a general strategy for
any "find the longest pattern that matches here" problem, and Kinyarwanda
gives you several unrelated instances of exactly that problem at
different linguistic levels.

## Key takeaways

- A struct can deliberately hold two different *representations* of the
  same logical sequence (a structured array for machine consumption, a
  flat string for human consumption) — as long as one centralized
  function is the only thing allowed to update either of them, keeping
  them from drifting apart.
- You've now seen the DRY principle applied to three different shapes of
  repetition: a copy operation (Ch.11's `set_morph`), a search algorithm
  (Ch.15's `scan_back_noun`), and a dual-state update (this chapter's
  `seq_append`) — the same underlying instinct, three different
  applications.
- Building a delimited string incrementally in C means inspecting the
  last character already written to decide whether a separator is
  needed next — there is no built-in "join" function to lean on.
- A trailing, deliberately unassigned enum member (commonly named
  `_COUNT`/`_MAX`/`_LAST`) automatically equals the number of real
  members before it, thanks to enum auto-increment — the enum-specific
  counterpart to `sizeof(arr)/sizeof(arr[0])` for arrays.
- Longest-match-first is not a one-off trick for noun-class prefixes; it
  reappears, unprompted, anywhere a "find the longest pattern that fits
  here" problem shows up — including, in this file, phoneme/digraph
  matching.

## Search YouTube for

- "enum auto increment trick C count members"
- "building delimited strings in C without a join function"
- "data structures with multiple representations of the same data"
- "longest match tokenization algorithm"

## Coming up in Chapter 17

`api.c` is the smallest file in the project (100 lines) — a thin
convenience layer wrapping `kin_analyze()` and `kin_g2p_sentence()` for
callers who don't want to learn the full `Token`/`SentenceAnalysis`
model. Chapter 17 covers why a project bothers writing a *second*, much
simpler public interface on top of an already-complete one, and what
that decision says about designing a library for more than one kind of
caller.
