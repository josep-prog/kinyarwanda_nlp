# Chapter 3 — Structs: Composing the Data the Whole Project Runs On

## Why this comes right after enums

Chapter 2 covered the *tag* — `POS`, `VerbTense`, and friends. A tag alone
is useless without something to attach it to. That something is the
`struct`: C's only built-in way to group different pieces of data — a
string, some numbers, some enum tags, even other structs — into one named
unit. Six structs carry this entire project: `NounClass`, `KinMorpheme`,
`MorphBreakdown`, `Error`, `Token`, and `SentenceAnalysis`. By the end of
this chapter you'll be able to read any of them and explain every field's
purpose and every sizing decision.

## 3.1 The simplest one: `NounClass`

```c
typedef struct {
    int   num;                   /* 1-16                                   */
    char  prefix[8];             /* Combined D+RT surface form, e.g. "umu" */
    char  rt[4];                 /* Indanganteko alone, e.g. "mu"          */
    char  concordance_adj[8];    /* Indangasano for ntera (adj) agreement  */
    char  concordance_poss[8];   /* Possessive connector (ikinyazina ngenera) */
    char  subj_prefix[8];        /* Verb subject agreement prefix          */
    const char *description;
} NounClass;
```

This is a struct in its purest form here: a few small fixed-size `char`
arrays for short strings, an `int`, and one pointer. Three things worth
noticing immediately:

- **Different array sizes for different fields.** `prefix[8]` and `rt[4]`
  aren't both sized to some generic "string buffer" constant — `rt` only
  ever needs to hold something like `"mu"` or `"ki"` (a couple of bytes
  plus the terminator), so it's sized tighter than `prefix`, which holds
  the combined, slightly longer `"umu"`-style form. Sizing each buffer to
  its *actual* maximum content, rather than reusing one large constant
  everywhere, keeps each `NounClass` instance small — and there will be
  16 of them sitting in a table you'll meet in Chapter 7.
- **`const char *description` is the one exception to "use a fixed
  array."** This field never needs to be copied or mutated — it always
  points at a string literal baked into the program (e.g.
  `"Nt.1 – human singular (umuntu)"`), which lives for the entire program
  run in read-only memory. A raw pointer is safe here specifically
  *because* nothing ever needs to own, free, or rewrite what it points
  to. Contrast this with `prefix`/`rt`/etc., which **are** copied (every
  time a `NounClass` value is copied, assigned, or returned) — a `char`
  array's contents copy along with the struct; a pointer field would only
  copy the address, not the data it points to.
- `sizeof(NounClass)` on this build is **48 bytes** — small enough that an
  array of 16 of these (the full noun-class table, Chapter 7) costs well
  under a kilobyte total.

### 3.1.1 What D, RT, and C actually mean — the noun formula behind every field

`NounClass` is the struct that stores one row of a 16-row table (the full
table is Chapter 7's subject), and every field name in it is an
abbreviation of a Kinyarwanda grammatical term. Kinyarwanda nouns are
traditionally analyzed with the formula:

```
            D       +       RT       +        C
        (indomo)      (indanganteko)      (igicumbi)
       initial vowel    class marker          stem
        i, u, or a      e.g. mu, ba, ki     carries the
       (sometimes        (this is the        actual meaning;
        absent)          part that drives    NEVER changes
                          agreement —
                          see below)

   Example:   u   +   mu   +   ntu     =    umuntu   ("person")
              D       RT        C
```

Every noun belongs to one of 16 **noun classes** (numbered 1–16 by
convention, written `nt.1`...`nt.16`), distinguished by which `RT` they take
— class 1 takes `mu` (singular human: *umuntu*), class 2 takes `ba` (plural
human: *abantu*), class 7 takes `ki` (*ikintu*, "thing"), and so on. This
matters enormously for the rest of the grammar, because **the class marker
doesn't just sit on the noun — it echoes onto every other word that
"agrees with" that noun**: the adjective describing it, the possessive
connecting to it, the verb whose subject it is. That's exactly what the
other four `NounClass` fields are recording:

| Field | Kinyarwanda term | What it lets agree with the noun |
|---|---|---|
| `prefix` | D+RT combined | the noun's own surface form, e.g. `"umu"` |
| `rt` | Indanganteko | the bare class marker, e.g. `"mu"` |
| `concordance_adj` | Indangasano | the matching adjective prefix — *umuntu mwiza* ("a good person"): `mwiza` takes `mu`-, echoing the noun's class |
| `concordance_poss` | (possessive connector base) | the matching possessive form — *umuntu we* ("his/her person"): `we` is built from class 1's possessive connector |
| `subj_prefix` | Indanganshinga ya ruhamwa | the matching verb subject-marker — *Umuntu a-ragenda* ("the person is going"): `a-` is class 1's verb agreement prefix |

So a single `NounClass` row is really "everything every *other* word in
the sentence needs to know in order to agree with this one noun." This is
the linguistic reason `syntax.c` (Chapter 12) can detect a grammar error
just by comparing two strings: if a noun's `concordance_adj` doesn't match
the prefix actually found on the adjective sitting next to it in the
sentence, that's a genuine agreement violation, not a guess.

## 3.2 The fixed-array-as-string idiom, generalized

Why use `char prefix[8]` instead of `char *prefix` everywhere in this
project? You already saw the no-`malloc` rationale in Chapter 1 — but
there's a second reason specific to structs: **copyability**.

In C, assigning one struct to another (`a = b;`) or passing/returning a
struct by value performs a **memberwise copy** — every field is copied,
byte for byte. If a field is a fixed array like `char prefix[8]`, the copy
genuinely duplicates those 8 bytes; the two structs end up with
completely independent data, and modifying one's `prefix` later can never
affect the other's. If that field had instead been `char *prefix`, the
"copy" would only copy the *pointer* — both structs would end up pointing
at the *same* underlying characters, and there'd be no single owner
responsible for eventually freeing that memory (going back to why this
project avoids `malloc`/`free` entirely — owning a `char*` field would
reintroduce exactly that problem). Fixed-size array fields make every
struct in this project **trivially, safely value-copyable** with a plain
`=`, no special copy function, no risk of two structs secretly sharing
memory.

## 3.3 `KinMorpheme` — sizing every field to its real job

```c
#define KIN_MORPH_LABEL_LEN 12   /* "D","RT","C","SP","TM","OM","EXT","FV","RS","PREF" */
#define KIN_MORPH_FORM_LEN  20   /* max length of a single morpheme surface/underlying */
#define KIN_MORPH_RULE_LEN  96   /* rule citation, e.g. "u→w §1.1 (SP 'tu'+'a'→'tw')"   */
#define KIN_MORPH_GLOSS_LEN 48   /* English gloss for this morpheme slot                */

typedef struct {
    char label        [KIN_MORPH_LABEL_LEN];
    char form         [KIN_MORPH_FORM_LEN];
    char surface      [KIN_MORPH_FORM_LEN];
    char rule         [KIN_MORPH_RULE_LEN];
    char english_gloss[KIN_MORPH_GLOSS_LEN];
} KinMorpheme;
```

Same idiom as `NounClass`, but now with named `#define` constants instead
of bare numbers (`8`, `4`) — because these sizes are referenced again
elsewhere (Chapter 1's "single source of truth" point). Each constant's
comment states *exactly* what has to fit: `KIN_MORPH_LABEL_LEN` only ever
holds a short tag like `"SP"` or `"PREF"`, so 12 bytes is generous;
`KIN_MORPH_RULE_LEN` has to hold a full rule citation string, so it gets
96. This is the discipline behind every buffer-size choice in the
project: each constant is sized by *what actually has to fit in it*, with
a comment justifying the number — never a round number picked
arbitrarily. `sizeof(KinMorpheme)` here comes to **196 bytes**.

### 3.3.1 The morpheme-label alphabet: decoding `label[KIN_MORPH_LABEL_LEN]`

Every `KinMorpheme.label` field holds one short abbreviation, and that
short list of abbreviations is the vocabulary the *entire* morphological
side of this project speaks. It's worth having all of them in one place,
since Chapters 6–15 will use them constantly without re-explaining each
time:

| Label | Kinyarwanda term | English | Appears in the formula for |
|---|---|---|---|
| `D` | Indomo | Initial/pre-prefix vowel | Nouns: `D+RT+C` |
| `RT` | Indanganteko | Noun class marker | Nouns: `D+RT+C` |
| `C` | Igicumbi | Stem (noun or adjective) | Nouns and adjectives |
| `RS` | Indangasano | Adjective agreement marker | Adjectives: `RS+C` |
| `PREF` | Indanganshinga | Infinitive verb-class marker (`ku-`/`gu-`/`kw-`) | Infinitive verbs: `PREF+root+FV` |
| `SP` | Indanganshinga ya ruhamwa | Subject-agreement prefix | Conjugated verbs |
| `TM` | Indangagihe | Tense/aspect marker | Conjugated verbs |
| `OM` | Icyuzuzo k'impagike | Object marker (infixed pronoun) | Conjugated verbs |
| `EXT` | Ingereka | Derivational extension (passive, causative, ...) | Conjugated verbs |
| `FV` | Umusozo | Final vowel/suffix | Both verb formulas |

Five word-formulas, ten reusable slot names — every single morpheme this
project ever produces is labeled with one of these ten strings. Once
you've memorized this table, every `tok->morph.m[i].label` value you see
printed in this book's later chapters is immediately readable without
looking anything up.

## 3.4 The "capacity + count" idiom: bounded lists without dynamic memory

```c
#define KIN_MAX_MORPHEMES   8

typedef struct {
    KinMorpheme m[KIN_MAX_MORPHEMES];
    int         n;        /* number of morphemes stored */
    bool        verified;
} MorphBreakdown;
```

This is the single most important pattern in the entire project's data
design, and it shows up at every scale: a fixed-capacity array, paired
with a separate `int` that tracks **how many of those slots are actually
in use**. `m` always has exactly 8 slots, whether a word's breakdown needs
2 morphemes or 7 — but `n` tells every piece of code reading this struct
exactly where the real data stops and stale/unused slots begin.

This is C's hand-rolled substitute for a dynamic, growable list (a
`Vec`/`ArrayList`/Python list) under the "no heap allocation" constraint
from Chapter 1. You cannot `append()` to a fixed array, so instead: write
into `m[n]`, then increment `n`. Reading code never iterates all 8 slots
blindly — it always loops `for (int i = 0; i < morph->n; i++)`, trusting
`n` to mark the real boundary. You will see this *exact* shape two more
times before this chapter ends, and a fourth time as you move through the
rest of the project — it's worth being able to name on sight: **"a
fixed-capacity array plus a count field is this project's substitute for
a dynamic list."**

`sizeof(MorphBreakdown)` works out to **1,576 bytes** — eight 196-byte
`KinMorpheme` slots, plus the `int n` and `bool verified`, plus a small
amount of compiler-inserted padding (more on padding in Section 3.7).

## 3.5 `Error` — the same idiom, applied to diagnostics

```c
typedef struct {
    ErrorType type;
    int       token_index;
    char      message[KIN_MAX_MSG];
    char      suggestion[KIN_MAX_MSG];
} Error;
```

`KIN_MAX_MSG` is `256` (from Chapter 1). `token_index` is how an `Error`
refers back to *which* token in the sentence triggered it — rather than
embedding a copy of the token itself, it stores a plain integer index into
the `tokens[]` array you'll see in `SentenceAnalysis` below. This is a
deliberate choice: storing an index instead of a pointer or a copy means
an `Error` value can be freely copied (Section 3.2's copyability point)
without ever risking a dangling reference to a `Token` that moved or went
out of scope — an index into a fixed, known array is always safe to
resolve later, as long as the array itself is still alive. `sizeof(Error)`
is **520 bytes** — dominated by the two 256-byte message buffers.

## 3.6 `Token` — composition: a struct containing a struct

```c
typedef struct {
    char surface[KIN_MAX_WORD];
    char lower[KIN_MAX_WORD];
    POS  pos;
    PronounType   pron_type;
    VerbTense     verb_tense;
    VerbExtension verb_ext;
    int  noun_class;
    int  obj_class;
    char stem[KIN_MAX_STEM];
    char detected_prefix[KIN_MAX_PREFIX];
    bool is_kinyarwanda;
    bool is_proper_noun;
    bool is_negative;
    bool is_hortative;
    bool is_reduplicated;
    char redup_surface[KIN_MAX_STEM];
    int  error_count;
    bool is_deverbative;
    char verb_root[KIN_MAX_STEM];
    bool is_conditional_clause;
    GramRole  gram_role;
    PunctType  punct_type;
    bool       is_clause_boundary;
    bool       is_sent_boundary;
    bool       is_quote_open;
    bool       is_quote_close;
    MorphBreakdown morph;            /* a WHOLE STRUCT, nested inside this one */
} Token;
```

This is **composition**: `Token` doesn't inherit from `MorphBreakdown`
(C has no inheritance — there are no classes at all), it simply *contains*
one, as a named field (`morph`). When you write `tok->morph.n` or
`tok->morph.m[0].label`, you're reaching through the outer struct into the
inner one. This is how C builds up complex data from simple data: not
through class hierarchies, but by literally nesting one struct's full
contents inside another's memory layout. `sizeof(Token)` includes
`sizeof(MorphBreakdown)` (1,576 bytes) as part of its own total — which
comes to **2,204 bytes** per `Token`.

Notice, too, the run of `bool` fields: `is_kinyarwanda`, `is_proper_noun`,
`is_negative`, `is_hortative`, `is_reduplicated`, `is_clause_boundary`,
`is_sent_boundary`, `is_quote_open`, `is_quote_close`. A more
space-conscious design could pack all nine of these into a single integer
using one bit per flag (a "bitmask") — this would shrink that part of the
struct from 9 bytes down to roughly 2. This project doesn't do that, for
the same reason Chapter 2 gave for not using a `union`: a named `bool`
field is self-documenting and trivial to inspect in a debugger or a
`printf` (`tok->is_negative` reads instantly), where a bitmask field
(`tok->flags & FLAG_NEGATIVE`) requires you to remember which bit means
what. Given there are at most 256 `Token`s alive at once, the few extra
bytes per token this costs is not a real expense — clarity wins.

## 3.7 A short detour: where do those byte counts actually come from?

You may have noticed `sizeof(Token)` (2,204) is *not* simply the sum of
every field's individual size added up by hand. That's because of
**struct padding**: the compiler is allowed to insert unused filler bytes
between fields so that each field starts at a memory address that's a
multiple of its own size — this is called **alignment**, and it lets the
CPU read multi-byte values (like a 4-byte `int`) in a single, fast memory
access instead of several slower ones. For example, a `bool` (1 byte)
immediately followed by an `int` (4 bytes, needing 4-byte alignment) will
typically get 3 padding bytes inserted after the `bool` so the `int` lands
on a 4-byte boundary. You don't control this directly in standard C
(there are compiler-specific ways to disable it, like
`__attribute__((packed))`, which this project does not use anywhere —
correctness and portability matter more here than shaving a few padding
bytes off a struct that's already a few kilobytes at most).

## 3.8 `SentenceAnalysis` — the top of the hierarchy, and a real number worth knowing

```c
typedef struct {
    Token  tokens[KIN_MAX_TOKENS];   /* KIN_MAX_TOKENS = 256 */
    int    token_count;
    Error  errors[KIN_MAX_ERRORS];   /* KIN_MAX_ERRORS = 64  */
    int    error_count;
    bool   has_verb;
    bool   is_complete;
} SentenceAnalysis;
```

This is the capacity+count idiom from Section 3.4, applied twice in one
struct: `tokens`/`token_count` and `errors`/`error_count`. It's also an
**array of structs** (256 `Token` values sitting one after another in
memory) — the opposite layout from a "struct of arrays" (which would
instead have, say, one big `POS pos_array[256]`, one big
`char surface_array[256][128]`, and so on, as separate top-level arrays).
This project consistently chooses array-of-structs: every piece of
information about token #5 — its surface text, its POS tag, its tense,
its morpheme breakdown — lives contiguously together at
`tokens[5]`, which matches how the code actually thinks about the problem
("process *this* token fully, then move to the next") far better than
having to jump between five separate parallel arrays to gather "everything
about token #5."

Here's the number worth memorizing for your defense, measured directly
from this build with `sizeof()`:

```
sizeof(NounClass)        =      48 bytes
sizeof(KinMorpheme)      =     196 bytes
sizeof(MorphBreakdown)   =   1,576 bytes
sizeof(Error)            =     520 bytes
sizeof(Token)            =   2,204 bytes
sizeof(SentenceAnalysis) = 597,516 bytes  (≈ 583.5 KB)
```

**583.5 KB.** That's the size of *one* `SentenceAnalysis` value — almost
entirely the 256-slot `Token` array (256 × 2,204 ≈ 564 KB) plus the 64-slot
`Error` array (64 × 520 ≈ 33 KB). This is the honest cost of Chapter 1's
"no heap allocation" decision: instead of allocating only as much memory
as one actual sentence needs, every `SentenceAnalysis` reserves the full
worst-case capacity, every time, on the stack. For a desktop/server CLI
tool with megabytes of stack space available per thread, half a megabyte
per call is a non-issue. It would be a real concern in a deeply
constrained embedded environment or if this function were called
recursively — and it's a perfectly fair question to be asked in your
defense: *"what's the memory cost of this design, and when would it stop
being acceptable?"* Now you have the exact number and the honest answer.

## 3.9 Case study: filling in one `Token` by hand for a real noun

To see every struct in this chapter holding real data at once, trace the
single word **`umuntu`** ("person", "human being") as the pipeline would
fill in its `Token`. This is the same word used as the running example in
Section 3.1.1 — here it's followed all the way into the actual struct
fields:

```
   surface word:   u  m  u  n  t  u
                   └D┘└RT┘└──C───┘
                    u    mu    ntu
```

| `Token` field | Value after analysis | Why |
|---|---|---|
| `surface` | `"umuntu"` | the raw word exactly as written |
| `lower` | `"umuntu"` | lowercased copy used for case-insensitive matching |
| `pos` | `POS_NOUN` | Chapter 8 decided this is the noun branch of the word-type tree |
| `noun_class` | `1` | class 1 = human singular |
| `stem` | `"ntu"` | the `C` (igicumbi) — the part that never changes |
| `detected_prefix` | `"umu"` | the combined `D+RT` that was stripped off to find the stem |
| `is_kinyarwanda` | `true` | the word matched a known Kinyarwanda pattern |
| `morph.n` | `3` | three morphemes were recorded: D, RT, C |
| `morph.m[0]` | `{label="D", form="u", surface="u", ...}` | the initial vowel |
| `morph.m[1]` | `{label="RT", form="mu", surface="mu", ...}` | the class marker |
| `morph.m[2]` | `{label="C", form="ntu", surface="ntu", english_gloss="person"}` | the stem, with its meaning |
| `morph.verified` | `true` | re-assembling D+RT+C reproduced the original surface string exactly |

Every other field not listed above (`pron_type`, `verb_tense`, `verb_ext`,
`is_negative`, `gram_role`, ...) simply sits at its zero value — `PRON_NONE`,
`TENSE_NONE`, `VEXT_NONE`, `false`, `GRAM_ROLE_NONE` — because none of them
apply to a noun. This is Chapter 2's "zero means nothing happened" idiom
(Section 2.3), now visible as a complete, concrete `Token` rather than an
abstract rule: roughly two-thirds of this one struct's fields are
*correctly* sitting at zero, simply because `umuntu` is a noun and not,
say, a conjugated verb — which would instead leave `noun_class`,
`detected_prefix`, and the noun-specific morpheme labels at zero, and fill
in `verb_tense`, `verb_ext`, and an `SP+TM+OM+root+EXT+FV`-shaped
`morph` breakdown instead.

## 3.10 Try it yourself

Reproduce the table above on your own machine — it's worth seeing these
numbers come out of your own compiler, not just trusting the printed
table:

```c
#include "kinyarwanda.h"
#include <stdio.h>
int main(void) {
    printf("sizeof(Token)            = %zu bytes\n", sizeof(Token));
    printf("sizeof(SentenceAnalysis) = %zu bytes (%.1f KB)\n",
           sizeof(SentenceAnalysis), sizeof(SentenceAnalysis) / 1024.0);
    return 0;
}
```

Compile with `gcc -Iinclude /tmp/your_file.c -o /tmp/sz && /tmp/sz`. Try
changing `KIN_MAX_TOKENS` in your *local copy* of the header from `256` to
`16` and recompiling — watch `sizeof(SentenceAnalysis)` shrink
proportionally. That's Chapter 1's "single source of truth" constant in
action: one number, in one place, sizes an entire struct's memory
footprint.

## Key takeaways

- A `struct` groups heterogeneous fields under one name; C has no classes
  or inheritance — complex types are built purely by **composition**
  (nesting one struct inside another, like `Token` containing a
  `MorphBreakdown`).
- Fixed-size `char` arrays (not `char *`) make structs safely,
  trivially copyable by plain assignment — a pointer field would only
  copy an address, leaving two structs aliasing the same memory.
- The **capacity + count idiom** (`KinMorpheme m[8]` + `int n`;
  `Token tokens[256]` + `int token_count`; `Error errors[64]` +
  `int error_count`) is this project's hand-rolled substitute for a
  dynamic list, given the no-`malloc` constraint from Chapter 1.
- Buffer sizes are never arbitrary round numbers — every `#define` is
  sized to its field's real maximum content, with a comment justifying
  it.
- Struct **padding/alignment** means `sizeof(struct)` is usually larger
  than the naive sum of its fields' sizes — the compiler inserts filler
  bytes so multi-byte fields land on aligned addresses for faster CPU
  access.
- `SentenceAnalysis` is ≈583.5 KB per value — the real, measurable cost of
  choosing fixed worst-case capacity over dynamic allocation. Fine for
  this CLI/library's use case; a number you should be ready to justify.

## Search YouTube for

- "C structs tutorial — struct vs union vs class"
- "struct padding and alignment in C explained"
- "array of structs vs struct of arrays performance"
- "sizeof operator in C explained"
- "passing structs by value vs by pointer in C"

## Coming up in Chapter 4

With the data structures fully understood, Chapter 4 finally reaches the
first file that actually *runs*: `tokenizer.c`. You'll see how raw
`const char *text` becomes a filled-in `Token tokens[]` array — the very
first write into the structures this chapter just took apart — using
nothing but `char`-by-`char` loops, since C has no built-in "split string"
function the way Python or JavaScript do.
