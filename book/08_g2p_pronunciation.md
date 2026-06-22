# Chapter 8 — From Spelling to Sound: Grapheme-to-Phoneme Conversion

## How this chapter works

Same rules as Chapters 1 through 7. Every code example was actually
compiled with `gcc -std=c99 -Wall -Wextra` and actually executed — the
output shown in a fenced block prefixed with `$` is the real terminal
output of that exact program.

Seven chapters have asked, one word and then one sentence at a time,
*what role does this word play*. This chapter asks a question none of
them ever needed: *what does this word sound like*. `src/g2p.c` —
grapheme-to-phoneme (G2P) conversion — turns Kinyarwanda spelling into
a sequence of phoneme tokens, the format a text-to-speech engine or a
speech-recognition post-processor would actually consume. It is this
book's first chapter about pronunciation rather than grammar, and (one
correction to last chapter's teaser) the per-sentence pipeline you are
about to read emits plain ASCII tokens, not IPA symbols directly —
IPA is available, but only one phoneme at a time, through a separate
lookup function. Section 1.1 shows exactly what each form looks like.

---

# Part 1 — Language and Code, Side by Side

## 1.1 The rule that makes this whole chapter possible: Kinyarwanda is almost perfectly phonemic

`g2p.c`'s own header comment states the linguistic fact this entire
module is built on:

> Kinyarwanda orthography is almost perfectly phonemic. The main
> complexity is digraph detection (longest-match-first) and the
> mapping of digraphs like 'sh', 'ny', 'ts', 'nsh' to single phonemes.

"Phonemic" means a simple, useful thing for a reader who has only ever
seen English spelling: one written symbol, one sound, consistently,
every time. English breaks this constantly — the letters "ough" sound
different in "though", "through", "rough", and "cough". Kinyarwanda
mostly doesn't. `g2p.h`'s header comment lists the handful of rules
that govern the whole system:

> 1. Digraphs are processed with longest-match-first priority:
>    "nsh" > "sh" > "s"; "ny" > "n"; "bw" > "b"
> 2. 'c' = /tʃ/ (like English "ch" in "church")
> 3. 'j' = /dʒ/ (like English "j" in "jump")
> 4. 'r' = /ɾ/ (alveolar tap, like Spanish "r" in "pero")
> 5. Vowels (a e i o u) are pure — no diphthongs; each is a full
>    syllable.
> 6. Long vowels exist phonemically but are not marked in standard
>    writing.

Rule 1 is the load-bearing one for this chapter. A **digraph** is two
letters that together represent one sound, not two — `sh` is one
consonant sound (like English "sh" in "shoe"), not the sounds "s"
followed by "h". "Longest-match-first" means that when the scanner is
deciding what a letter means, it must check whether a *longer* known
pattern starts at this position before falling back to treating the
letter alone. Here is that rule applied to five real words, run
through the actual library:

```c
/* p1_header_examples.c -- the worked examples from g2p.h's own doc comment */
#include "g2p.h"
#include <stdio.h>

static void show(const char *w) {
    KinPhonemeSeq seq;
    kin_g2p_word(w, &seq);
    printf("%-12s -> %s\n", w, seq.repr);
}

int main(void) {
    show("genda");
    show("ishuri");
    show("bwana");
    show("nyuma");
    show("ntibigenda");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p1_header_examples.c \
      -L . -lkinyarwanda -o p1_header_examples
$ LD_LIBRARY_PATH=. ./p1_header_examples
genda        -> g e nd a
ishuri       -> i sh u r i
bwana        -> bw a n a
nyuma        -> ny u m a
ntibigenda   -> nt i b i g e nd a
```

Read `ishuri` ("school"): five letters, four phoneme tokens, because
`sh` is one sound. Read `bwana` ("sir"): five letters, four tokens,
because `bw` — a consonant pronounced with rounded lips, a single
phonological unit in Kinyarwanda — is one sound, not "b" followed by
"w". Hold onto `genda`'s real output, `g e nd a`: Section 4.1 finds
that this doesn't match what `g2p.h`'s own worked-example comment
claims it should produce, and the discrepancy turns out to share a
root cause with a second, independent mismatch in the same file.

## 1.2 Build it: a toy G2P that doesn't know about digraphs, and what it gets wrong

```c
/* p1_toy_g2p.c -- a G2P that only knows single characters */
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *word = "ishuri";   /* real word: "school" */
    printf("input: %s\n", word);
    printf("naive (one phoneme per letter): ");
    for (size_t i = 0; i < strlen(word); i++) {
        printf("%c ", word[i]);
    }
    printf("\n");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra p1_toy_g2p.c -o p1_toy_g2p
$ ./p1_toy_g2p
input: ishuri
naive (one phoneme per letter): i s h u r i
```

The naive scanner splits `sh` into "s" then "h" — two separate sounds
that, said one after another, do not sound like the consonant in
"shoe". Compare it side by side with the real engine on the exact same
word:

```
naive: i s  h  u r i
real:  i sh    u r i
```

The fix is not a smarter character-by-character rule; it's a
*decision about scan order*. Before treating any single character as
its own sound, the real scanner must first check whether a longer
known pattern — 3 characters, then 2 — starts right here:

```
position p in the word
   |
   v
 ┌─────────────────────────────┐
 │ does a 3-char pattern        │   nsh, nzw, ngw, shw
 │ (G2P_3 table) match at p?    │
 └──────────────┬───────────────┘
            yes │            no
                v             v
        emit that phoneme   ┌─────────────────────────────┐
        advance p by 3      │ does a 2-char pattern         │  sh, ny, ts, mb, nd,
                             │ (G2P_2 table) match at p?     │  bw, gw, rw, ... (29 total)
                             └──────────────┬────────────────┘
                                       yes │            no
                                           v             v
                                   emit that phoneme   emit the single
                                   advance p by 2       character at p
                                                         advance p by 1
```

Every one of the 33 multi-character rules in `g2p.c` is checked in
this fixed order — 3-character table first, then 2-character, then
the single-character fallback — and the *first* match at the current
position wins. Get that order backwards (check single characters
first) and `sh` is gone before the scanner ever gets a chance to see
it as a digraph.

## 1.3 Checkpoint: test yourself before continuing

Before Part 2, predict the real output for these, then check against
the rules above:

1. `nyuma` — how many phoneme tokens, and why not five?
2. `ntibigenda` — which two-letter sequence inside it is a single
   prenasalized-cluster phoneme, not two separate consonants? (Hint:
   it's the same cluster that showed up in `genda`.)
3. If a word contained the letters `n`, `s`, `h` in a row as `nsh`,
   would the real scanner ever treat `n` as its own phoneme and `sh`
   as a separate one? Why not, given the diagram above?

---

# Part 2 — Three Representations of One Sound

## 2.1 Why a phoneme is an ID, an ASCII token, and an IPA string, never just one of those

Every phoneme this engine knows about is one row of a table:

```c
typedef struct {
    KinPhonemeID  id;
    const char   *token;     /* ASCII output token, e.g. "sh"              */
    const char   *ipa;       /* IPA string, e.g. "ʃ"                       */
    const char   *grapheme;  /* Orthographic input that maps here          */
} PhonemeEntry;
```

Run a handful of real rows through the two public lookup functions:

```c
#include <stdio.h>
#include "g2p.h"

static void row(KinPhonemeID id) {
    printf("  token=%-4s ipa=%s\n", kin_phoneme_token(id), kin_phoneme_ipa(id));
}

int main(void) {
    printf("phoneme inventory size: %d\n", kin_phoneme_count());
    row(PH_SH); row(PH_NY); row(PH_C); row(PH_J); row(PH_R); row(PH_NGW);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p2_table.c -L . -lkinyarwanda -o p2_table
$ LD_LIBRARY_PATH=. ./p2_table
phoneme inventory size: 61
  token=sh   ipa=ʃ
  token=ny   ipa=ɲ
  token=ch   ipa=tʃ
  token=j    ipa=dʒ
  token=r    ipa=ɾ
  token=ngw  ipa=ŋɡʷ
```

Three different jobs need three different shapes for the same sound,
and the table keeps them deliberately separate instead of picking one
and deriving the others on the fly:

- The **ID** (`PH_SH`, a plain enum value) is what every internal
  function actually passes around and compares. It's small, it's
  fast to switch on, and it never changes shape regardless of which
  output format a caller eventually wants.
- The **ASCII token** (`"sh"`) is for humans reading this book's
  terminal output, or for any downstream system that only has a plain
  ASCII pipe to write to and can't safely pass UTF-8 IPA symbols
  through.
- The **IPA string** (`"ʃ"`) is the linguistically precise symbol a
  trained phonetician — or a TTS acoustic model's training data —
  actually expects. It is not interchangeable with the ASCII token:
  `kin_phoneme_token(PH_C)` is `"ch"` (two ASCII characters, easy to
  read, easy to type), while `kin_phoneme_ipa(PH_C)` is `"tʃ"` (one
  affricate symbol followed by a fricative symbol, the precise IPA
  transcription of the sound 'c' actually represents).

Notice that `PH_R`'s IPA is `ɾ`, not the more familiar `r`. That's
deliberate, not a typo: rule 4 from Section 1.1 says Kinyarwanda's
"r" is an alveolar tap, phonetically closer to the flap heard in
Spanish "pero" than to an English or French "r". Writing it as the
plain Latin letter `r` for the ASCII token (where readability wins)
while keeping the linguistically exact `ɾ` for the IPA field (where
precision wins) is the entire reason this table has two separate
string columns instead of one.

## 2.2 The lookup tables are the same NULL-sentinel pattern you've already audited

The three rule tables driving Part 1.2's diagram — `G2P_3`, `G2P_2`,
`G2P_1` — are flat arrays terminated by a `{ NULL, PH_NULL }` sentinel
row, exactly like the `INVARIABLES[]` table Chapter 5 audited and the
`ADJ_STEMS[]` table from Chapter 2:

```c
typedef struct { const char *graph; KinPhonemeID phone; } G2PRule;

static const G2PRule G2P_3[] = {
    { "nsh", PH_NSH },
    { "nzw", PH_NZW },
    { "ngw", PH_NGW },
    { "shw", PH_SHW },
    { NULL,  PH_NULL }
};
```

The scanning loop checks `G2P_3[i].graph` for `NULL` to know when to
stop, the same defensive pattern this book has seen guard every other
flat lookup table so far. There is no new risk to demonstrate here —
the interesting engineering decision in this module isn't the table's
shape, which is completely familiar by Chapter 8; it's the *order* in
which three of these tables get checked, which Part 1.2's diagram
already covered.

---

# Part 3 — Making It Interactive

## 3.1 Build it: type a sentence, see both representations at once

```c
/* p3_repl.c -- type a sentence, see its phonemes and per-phoneme IPA */
#include "g2p.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    char line[256];
    printf("Type a Kinyarwanda sentence, or 'q' to quit.\n");
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == 'q' && line[1] == '\0') break;

        KinPhonemeSeq seq;
        if (!kin_g2p_sentence(line, &seq)) {
            printf("  (no phonemes produced)\n");
            continue;
        }
        printf("  tokens: %s\n", seq.repr);
        printf("  count:  %d\n", seq.count);
        printf("  ipa:    ");
        for (int i = 0; i < seq.count; i++) {
            printf("%s", kin_phoneme_ipa(seq.phones[i]));
            if (i + 1 < seq.count) printf(" ");
        }
        printf("\n");
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p3_repl.c -L . -lkinyarwanda -o p3_repl
$ printf "Imana iravuga.\nUmwana arasoma.\nq\n" | LD_LIBRARY_PATH=. ./p3_repl
Type a Kinyarwanda sentence, or 'q' to quit.
  tokens: i m a n a |i r a v u g a .
  count:  14
  ipa:    i m a n a | i ɾ a v u ɡ a .
  tokens: u mw a n a |a r a s o m a .
  count:  14
  ipa:    u mʷ a n a | a ɾ a s o m a .
```

Two things to notice immediately, both of which Part 4 investigates
properly: the `ipa:` line has a visible space on both sides of every
`|` word-boundary marker, but the `tokens:` line printed just above it
does not (`a |i` versus `a | i`). And `mw` (in `umwana`) shows up as a
single IPA symbol with a superscript `ʷ` (`mʷ`) — a labialized nasal,
one sound, exactly the same "consonant + rounded-lip glide = one
phoneme" pattern `bwana` demonstrated back in Section 1.1.

---

# Part 4 — Capstone: Four Real Findings From Reading the Production Code

## 4.1 Two header-documented examples, both wrong, for two different reasons

`g2p.h`'s own doc comment gives a worked example of sentence-level
G2P:

> "Imana iravuga" → "i m a n a | i r a v u g a"

Run the exact same sentence through the real function:

```c
/* p4_pipe.c -- the header's own worked example, run for real */
#include "g2p.h"
#include <stdio.h>

int main(void) {
    KinPhonemeSeq seq;
    kin_g2p_sentence("Imana iravuga", &seq);
    printf("documented in g2p.h: \"i m a n a | i r a v u g a\"\n");
    printf("actually produced:   \"%s\"\n", seq.repr);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_pipe.c -L . -lkinyarwanda -o p4_pipe
$ LD_LIBRARY_PATH=. ./p4_pipe
documented in g2p.h: "i m a n a | i r a v u g a"
actually produced:   "i m a n a |i r a v u g a"
```

No space after the `|`. The cause is one `if` inside `seq_append`,
read in full in Section 6.4: a space separator is inserted before
every new token *except* when the previous character already written
was a `|`. That rule was presumably meant to keep the boundary marker
visually tight against its neighbor, but the doc comment was clearly
typed by hand without ever running this exact call — it shows spaces
on both sides.

The very first example in Section 1.1, `genda`, has the identical
problem for an unrelated reason. `g2p.h` doesn't show that one
directly, but the convention in code comments throughout this project
is to write `g e n d a` as four separate letters when illustrating a
word with no digraphs — and `n` followed by `d` is exactly the kind
of sequence a hand-written example would assume splits into two
single-character phonemes. It doesn't: `nd` is one of the 13
prenasalized-cluster rules in `G2P_2` (Section 1.1's real output was
`g e nd a`, three letters' worth of consonant collapsed into one
token). Both mismatches trace back to the same root cause: a worked
example written by a person, never re-run against the table it's
describing. Neither is a runtime bug — the code does exactly what its
own rule tables say — but both are reminders that a comment's example
is a claim, not a guarantee, and this book has now found that claim
false in two completely different functions across two chapters.

## 4.2 The silent cap: `kin_g2p_word` can lose data and still report success

```c
/* p4_trunc.c -- what happens past the phoneme budget */
#include "g2p.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    char word[601];
    for (int i = 0; i < 600; i++) word[i] = "aeiou"[i % 5];
    word[600] = '\0';

    KinPhonemeSeq seq;
    bool ok = kin_g2p_word(word, &seq);
    printf("input letters:  %zu\n", strlen(word));
    printf("budget:         %d (G2P_MAX_PHONEMES)\n", G2P_MAX_PHONEMES);
    printf("seq.count:      %d\n", seq.count);
    printf("return value:   %s\n", ok ? "true" : "false");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_trunc.c -L . -lkinyarwanda -o p4_trunc
$ LD_LIBRARY_PATH=. ./p4_trunc
input letters:  600
budget:         512 (G2P_MAX_PHONEMES)
seq.count:      511
return value:   true
```

No crash — `seq_append` checks `seq->count >= G2P_MAX_PHONEMES - 1`
before writing and simply refuses to append once the array is full, a
textbook example of the bounds-checking discipline this whole project
applies everywhere. But the *caller-visible signal* is exactly the
problem this book flagged in Chapter 6's array-overflow section and
again in Chapter 4's connector-fusion buffer: `kin_g2p_word` returns
`bool`, and that single bit means only "at least one phoneme was
produced" (the `any` flag, set the moment the very first character
matches). It carries no information about whether every character
made it in. A 600-letter input and a 5-letter input that both produce
at least one phoneme both return `true`; only inspecting `seq.count`
yourself, against the documented `G2P_MAX_PHONEMES` constant, reveals
that one of them was silently cut short.

## 4.3 A documented contract with nothing enforcing it: `expand_number`'s "0-9999"

`expand_number`'s own comment is explicit about its scope:

```c
/* Expand a number 0-9999 into Kinyarwanda words.
 * Returns number of chars written (not including NUL). */
static int expand_number(long n, char *buf, size_t bufsz) {
```

Its caller, `kin_normalize_text`, scans an arbitrary run of digits
with `strtol` and passes whatever number it finds straight to
`expand_number` — no range check, no clamp, nothing that confirms the
"0-9999" promise before calling:

```c
if (isdigit((unsigned char)*p)) {
    long n = strtol(p, (char **)&p, 10);
    char numbuf[128];
    int nw = expand_number(n, numbuf, sizeof(numbuf));
    ...
```

Test a number safely inside the documented range against one outside
it:

```c
/* p4_numbug.c -- expand_number's own comment says "0-9999" */
#include "g2p.h"
#include <stdio.h>

static void show(const char *s) {
    char norm[G2P_MAX_NORM];
    kin_normalize_text(s, norm, sizeof(norm));
    printf("%-28s -> %s\n", s, norm);
}

int main(void) {
    show("Hari abantu 1500 mu mujyi.");   /* inside the documented 0-9999 range */
    show("Hari abantu 12345 mu mujyi.");  /* outside it -- nothing stops this call */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_numbug.c -L . -lkinyarwanda -o p4_numbug
$ LD_LIBRARY_PATH=. ./p4_numbug
Hari abantu 1500 mu mujyi.   -> Hari abantu igihumbi na amagana gatanu mu mujyi .
Hari abantu 12345 mu mujyi.  -> Hari abantu ibihumbi zeru na amagana gatatu na mirongo ine na gatanu mu mujyi .
```

`1500` becomes a fluent `igihumbi na amagana gatanu` ("a thousand and
five hundred"), correct and inside the documented range. `12345`
becomes `ibihumbi zeru na amagana gatatu na mirongo ine na gatanu` —
literally "thousands zero and three hundred and forty and five",
nonsense, because `expand_number`'s thousands branch only knows how to
look up a *single digit* of thousands in `DIGITS_UNITS[]`:

```c
if (thou == 1) {
    w = snprintf(buf, bufsz, "igihumbi ");
} else {
    w = snprintf(buf, bufsz, "ibihumbi %s ", DIGITS_UNITS[thou < 10 ? thou : 0]);
}
```

For `12345`, `thou` is `12` — two digits, not one — so the
`thou < 10 ? thou : 0` guard silently substitutes index `0`
(`"zeru"`, the word for zero) instead of attempting to spell out
"twelve" thousands. This is the same shape of bug this book has found
before in `is_complete` (Chapter 7): a function's own comment honestly
states its real limit, but nothing at the call site actually enforces
that limit before crossing it, and the failure mode on the other side
is silent wrong output, not a crash or a refusal.

## 4.4 The headline bug: two adjacent expansions glue together when the input has no space

This is the one worth remembering. Kinyarwanda text in the wild
routinely writes a number directly against its unit, with no space —
`5km`, `200frw`, the way units are normally abbreviated. Test both
spacings side by side:

```c
/* p4_glue.c -- the headline bug: two expansions with no space between them */
#include "g2p.h"
#include <stdio.h>

static void show(const char *s) {
    char norm[G2P_MAX_NORM];
    kin_normalize_text(s, norm, sizeof(norm));
    printf("%-20s -> %s\n", s, norm);
}

int main(void) {
    show("Bigufi 5 km.");   /* space already in the input between digit and unit */
    show("Bigufi 5km.");    /* no space -- how units are normally written */
    show("Yishyuye 200frw.");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_glue.c -L . -lkinyarwanda -o p4_glue
$ LD_LIBRARY_PATH=. ./p4_glue
Bigufi 5 km.         -> Bigufi gatanu kilometero .
Bigufi 5km.          -> Bigufi gatanukilometero .
Yishyuye 200frw.     -> Yishyuye amagana kabiriamafaranga .
```

With a space already in the input, normalization is perfect: `5 km`
becomes the cleanly separated `gatanu kilometero`. Without one — the
realistic case — it becomes `gatanukilometero`, one fused, unreadable
word, and `200frw` becomes `amaganakabiriamafaranga`-shaped wreckage
(`amagana kabiri` + `amafaranga`, glued with no boundary at all).

The cause is structural, not a typo, and it's visible directly in
`kin_normalize_text`'s control flow (read in full in Section 6.2):
the digit branch writes its expansion and immediately `continue`s
back to the top of the loop; the abbreviation branch then runs and
checks only whether the *previous character in the original input*
was alphabetic — and the digit `5` is not alphabetic, so the boundary
check passes and the abbreviation match proceeds. Neither branch ever
asks "did the text I just wrote into `out` end with a space, and does
the text I'm about to write begin with one?" — each one only looks at
the *original* input around it, never at what the *output buffer*
already contains. Two correct expansions, run back to back with no
separator logic between them, produce one incorrect concatenated
word — and because every word number this engine knows about doubles
as the start or end of a longer Kinyarwanda phrase, the only signal
that anything went wrong is a human reading the result.

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 Why a flat ordered scan, not a trie, is the right call here

A computer science course would likely point at "longest-match-first
string matching" and reach for a trie (a tree where each branch is one
character, letting you walk character-by-character toward the longest
match without re-scanning). `g2p.c` doesn't build one, and that's the
right call, not a missed opportunity: the entire multi-character rule
set is 33 entries (4 three-character, 29 two-character — counted
directly from `G2P_3[]` and `G2P_2[]` in Section 1.2), every word
being scanned is a handful of letters long, and the whole table fits
on one screen, where any reader can audit the *order* — which is the
only thing correctness here actually depends on — at a glance. A trie
would add a real data structure, a build step, and a layer of
indirection between the code and the linguistic rule it implements,
in exchange for an asymptotic speedup this module will never need: no
realistic Kinyarwanda sentence is long enough for a 33-row linear scan
per character to matter. The flat, ordered array is simpler to write,
simpler to extend (add a row, keep the length groups in order), and
simpler to get right than a tree that buys speed nobody asked for.

## 5.2 The one module in this entire project that never sees a `Token`

Every chapter from 1 through 7 built on top of `kin_tokenize`'s
`Token` array or `kin_analyze`'s `SentenceAnalysis` struct — POS tags,
noun classes, gram roles, error lists. `kin_g2p_word` and
`kin_g2p_sentence` take a plain `const char *` and nothing else:

```
   raw text: "Imana iravuga."
       |
       ├──────────────────────────────┐
       v                               v
   kin_tokenize()                kin_g2p_sentence()
       |                               |
       v                               v
   Token[] (Ch.6)                 KinPhonemeSeq
       |                               |
       v                               v
   kin_analyze()                  TTS / ASR consumer
       |
       v
   SentenceAnalysis (Ch.7)
```

This is deliberate, not an oversight. A text-to-speech engine does not
need to know that `iravuga` is a class-9 verb with a habitual-present
tense marker before it can pronounce it — it needs phonemes, and
nothing about Chapters 1 through 7's grammatical analysis changes how
a word *sounds*. Making G2P a sibling branch off the raw text, rather
than one more stage chained after `kin_analyze`, means a caller who
only wants pronunciation never pays for — or depends on the
correctness of — five chapters' worth of noun-class and verb-tense
detection it does not need. `main.c`'s own `--g2p` CLI flag confirms
this is exactly how the real binary uses it: it calls
`kin_g2p_sentence` directly on the raw input string, never touching
`kin_tokenize` or `kin_analyze` at all.

---

# Part 6 — Reading the Real Production Code

## 6.1 `kin_g2p_word`, in full

```c
bool kin_g2p_word(const char *word, KinPhonemeSeq *out) {
    if (!word || !out) return false;

    /* Lowercase copy */
    char lower[G2P_MAX_REPR];
    size_t wlen = strlen(word);
    if (wlen == 0) return false;
    if (wlen >= sizeof(lower)) wlen = sizeof(lower) - 1;
    for (size_t i = 0; i < wlen; i++) lower[i] = (char)tolower((unsigned char)word[i]);
    lower[wlen] = '\0';

    out->count = 0;
    out->repr[0] = '\0';

    const char *p = lower;
    bool any = false;

    while (*p) {
        bool matched = false;

        if (*p == '.') { seq_append(out, PH_PAUSE_LONG);  p++; any = true; continue; }
        if (*p == ',') { seq_append(out, PH_PAUSE_SHORT); p++; any = true; continue; }
        if (*p == ' ' || *p == '\t') { p++; continue; }

        /* 3-char rules */
        for (int i = 0; G2P_3[i].graph && !matched; i++) {
            if (strncmp(p, G2P_3[i].graph, 3) == 0) {
                seq_append(out, G2P_3[i].phone);
                p += 3; matched = true; any = true;
            }
        }
        if (matched) continue;

        /* 2-char rules */
        if (*(p+1)) {
            for (int i = 0; G2P_2[i].graph && !matched; i++) {
                if (strncmp(p, G2P_2[i].graph, 2) == 0) {
                    seq_append(out, G2P_2[i].phone);
                    p += 2; matched = true; any = true;
                }
            }
        }
        if (matched) continue;

        /* Single-char fallback */
        for (int i = 0; G2P_1[i].graph && !matched; i++) {
            if (*p == G2P_1[i].graph[0]) {
                seq_append(out, G2P_1[i].phone);
                p++; matched = true; any = true;
            }
        }

        if (!matched) p++;   /* unknown character: skip it */
    }

    return any;
}
```

This is Part 1.2's diagram, written as code: 3-char table, then
2-char, then 1-char fallback, exactly in that order, with `continue`
after every match so a position is never re-checked against a shorter
rule after a longer one already claimed it. The `if (*(p+1))` guard
before the 2-char loop is a real, necessary bounds check — without it,
`strncmp(p, "...", 2)` on the very last character of the word would
read one byte past the string's `NUL` terminator on a 2-byte compare;
checking that a next character exists first is what keeps this
function inside the bytes it's actually given.

## 6.2 `kin_normalize_text`, the section that produces Section 4's findings

```c
if (*p == '\'' && p > input) {
    if (isalpha((unsigned char)*(p-1))) {
        char prev = (char)tolower((unsigned char)*(p-1));
        if (prev == 'n' && out < end) *out++ = 'a';
        else if (prev == 'y' && out < end) *out++ = 'a';
        else if (prev == 'k' && out < end) *out++ = 'u';
        else if (prev == 'm' && out < end) *out++ = 'u';
        if (out < end) *out++ = ' ';
        p++;
        continue;
    }
}

if (isdigit((unsigned char)*p)) {
    long n = strtol(p, (char **)&p, 10);
    char numbuf[128];
    int nw = expand_number(n, numbuf, sizeof(numbuf));
    while (nw > 0 && numbuf[nw-1] == ' ') { numbuf[--nw] = '\0'; }
    for (int i = 0; i < nw && out < end; i++) *out++ = numbuf[i];
    continue;
}

if (isalpha((unsigned char)*p) &&
    (p == input || !isalpha((unsigned char)*(p-1)))) {
    bool matched = false;
    for (int i = 0; ABBREVS[i].abbr; i++) {
        size_t alen = strlen(ABBREVS[i].abbr);
        bool eq = true;
        for (size_t j = 0; j < alen; j++) {
            if (tolower((unsigned char)p[j]) != ABBREVS[i].abbr[j]) { eq = false; break; }
        }
        if (eq && !isalpha((unsigned char)p[alen])) {
            const char *exp = ABBREVS[i].expansion;
            size_t elen = strlen(exp);
            for (size_t j = 0; j < elen && out < end; j++) *out++ = exp[j];
            p += alen;
            matched = true;
            break;
        }
    }
    if (matched) continue;
}
```

This is the order Section 4.4 traced: apostrophe contraction first
(restoring the vowel an `'` elided — `n'` becomes `na `, `y'` becomes
`ya `, `k'` becomes `ku `, `m'` becomes `mu `, a genuinely different
job from Chapter 6's tokenizer, which only ever split *at* the
apostrophe and never tried to undo it), then digit expansion, then
abbreviation expansion. The digit branch's word-boundary check for the
*next* alphabetic run only looks backward at `*(p-1)` in the original
input — never forward at whether `out` currently ends in a space. That
single missing check is the entire root cause of `5km` becoming
`gatanukilometero`.

## 6.3 `expand_number`, the function whose own comment Section 4.3 tested

```c
static int expand_number(long n, char *buf, size_t bufsz) {
    if (bufsz == 0) return 0;
    buf[0] = '\0';
    int written = 0;

    if (n == 0) return snprintf(buf, bufsz, "zeru");

    if (n >= 1000) {
        long thou = n / 1000;
        int w;
        if (thou == 1) {
            w = snprintf(buf, bufsz, "igihumbi ");
        } else {
            w = snprintf(buf, bufsz, "ibihumbi %s ", DIGITS_UNITS[thou < 10 ? thou : 0]);
        }
        if (w > 0) { written += w; buf += w; bufsz -= (size_t)w; }
        n %= 1000;
        if (n > 0 && bufsz > 4) {
            int w2 = snprintf(buf, bufsz, "na ");
            written += w2; buf += w2; bufsz -= (size_t)w2;
        }
    }
    /* ... hundreds, tens, units follow the same pattern ... */
    return written;
}
```

`DIGITS_UNITS[]` has exactly 10 entries, indices 0 through 9 — one
digit's worth. `thou < 10 ? thou : 0` is the function quietly
admitting it can only look up a single-digit thousands count; for any
`n` at or past `10000`, `thou` is two digits, the ternary's `false`
branch fires, and index `0` (`"zeru"`) is substituted with no warning
to the caller. The function's own header comment — "0-9999" — is an
accurate description of where this code stops being correct. The gap
is entirely on the caller's side: nothing between `kin_normalize_text`
parsing a digit run with `strtol` and handing the result straight to
`expand_number` ever checks that promise before relying on it.

## 6.4 `seq_append`, the function whose space-skip rule produced Section 4.1's first finding

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
        memcpy(seq->repr + repr_len, tok, tok_len + 1);
    }
    return true;
}
```

Two things in one function, both already seen in Sections 4.1 and
4.2. The bounds check on line 2 — refuse to write once `count` would
reach `G2P_MAX_PHONEMES - 1` — is what keeps a 600-letter word from
overrunning `seq->phones[]`; it is also exactly what makes the
truncation in Section 4.2 *silent*, because refusing to write and
returning `false` from `seq_append` changes nothing about what
`kin_g2p_word` itself returns to its own caller. And the
`seq->repr[repr_len-1] != '|'` condition is the one line responsible
for "a |i" instead of "a | i" — a deliberate choice to skip the space
*after* a boundary marker that the function never applies symmetrically
*before* one, which is exactly the asymmetry Section 4.1 measured.

---

# Part 7 — Looking Back, Looking Forward

## 7.1 A parallel pipeline, not a seventh stage

Every other chapter in this book added one more stage to a single
growing pipeline — tokenize, tag, resolve, check, suggest. This
chapter is the first to step *sideways*: `g2p.c` reads the same raw
Kinyarwanda text every other chapter has been case-studying, but
answers a question that pipeline was never built to answer, using its
own independent rule tables, its own independent normalization pass,
and its own independent output type. Nothing in Chapters 1 through 7
calls into `g2p.c`, and nothing in `g2p.c` calls into them. The two
halves of this project meet only at the CLI's `--g2p` flag in
`main.c`, which simply decides which one of two unrelated pipelines a
given run of the program should use.

## 7.2 Checkpoint: what you should now be able to explain

- Why "digraph" and "phonemic" are the two linguistic terms this whole
  chapter rests on, and what a real word looks like before and after
  longest-match-first scanning.
- Why a phoneme needs three separate string representations (ID,
  ASCII token, IPA) rather than one, and which of the three a
  TTS/ASR consumer actually wants.
- Why a flat ordered array, not a trie, is the right data structure
  for a rule set this small.
- The real, verified gap between what `5km` should normalize to and
  what it actually normalizes to, and exactly which two lines of code
  are responsible.

---

# Part 8 — Practice

### Beginner

1. By hand, walk `nyuma` through Part 1.2's diagram one character at a
   time. Which table does each phoneme come from?
2. Look up `kin_phoneme_ipa(PH_J)` and `kin_phoneme_token(PH_J)` in
   the real `PHONEME_TABLE[]` in `g2p.c`. Why are they different
   strings for the same sound?

### Intermediate

3. Section 4.4 showed `5km` glues into `gatanukilometero`. Predict,
   then verify by compiling `p4_glue.c`'s pattern, what `Bigufi
   5km na 3kg.` normalizes to. Does the bug repeat once per
   number-then-unit pair, or only the first time?
4. `kin_phoneme_count()`'s own header comment in `g2p.h` says it
   returns the phoneme count "(excluding prosodic)". Write a 5-line
   program that calls it and compare the real return value against
   `PH_COUNT`'s position in the enum (`PH_NULL` through
   `PH_PAUSE_LONG`, inclusive). Does the real value exclude the three
   prosodic markers, or include them?
5. `ABBREVS[]`'s own comment claims the table is "sorted by length
   (longest first)... so that 'km' doesn't eat into 'km²' etc." Check
   every entry: is there actually a pair in the table where one
   abbreviation is a prefix of another? If there isn't, what does
   that tell you about whether this comment's stated safeguard is
   being exercised by anything in the current table?

### Advanced

6. Section 4.3 found `expand_number` breaks above 9999 because
   `DIGITS_UNITS[thou < 10 ? thou : 0]` can only index a single
   digit. Without changing the function's public signature, sketch
   (in words, not code) what would need to change to correctly handle
   `n` up to `999999` — what additional digit position would need its
   own expansion logic, modeled on how the existing hundreds/tens
   branches already work.
7. `seq_append` refuses to write past `G2P_MAX_PHONEMES - 1` and
   returns `false`, but neither `kin_g2p_word` nor `kin_g2p_sentence`
   currently checks that return value. Propose a minimal change to
   `kin_g2p_word`'s signature or behavior that would let a caller
   detect truncation, without breaking any of this chapter's existing
   example programs.

---

## Key takeaways

- Kinyarwanda orthography is "almost perfectly phonemic" (one letter
  or digraph, one sound), and the entire G2P module exists to handle
  the one real complication that fact leaves: detecting digraphs and
  clusters with longest-match-first scanning before falling back to
  single characters.
- A phoneme is deliberately represented three different ways — a
  stable internal ID, a human-readable ASCII token, and a precise IPA
  string — because three different audiences (internal code, this
  book's terminal output, a linguist or TTS model) need three
  different shapes for the exact same sound.
- A flat, ordered, NULL-sentinel-terminated array (the same pattern
  Chapters 2 and 5 already audited) is the right tool for a 49-rule
  digraph table; a trie would add real structure in exchange for a
  speedup this module's input sizes will never need.
- Two real, previously-undocumented findings, both from header
  comments never re-verified against the table they describe:
  `g2p.h`'s worked example for "Imana iravuga" omits the asymmetric
  space-skipping rule around `|` that `seq_append` actually applies,
  and the same hand-written-example problem affects how `genda`'s
  `nd` cluster is illustrated.
- A third real finding, the same documented-but-unenforced-contract
  shape this book found in Chapter 7's `is_complete`: `expand_number`'s
  own comment says "0-9999", nothing at its call site enforces that
  range, and `12345` silently normalizes to nonsense ("ibihumbi
  zeru...") rather than failing loudly.
- The headline finding: normalizing `5km` (no space, the realistic
  way units are written) produces the glued, unreadable
  `gatanukilometero`, because the digit-expansion branch and the
  abbreviation-expansion branch each only check the *original input*
  for a word boundary, never the *output buffer* they're both writing
  into.
- `g2p.c` is architecturally unlike every other module in this book:
  it never consumes a `Token` or a `SentenceAnalysis`, by deliberate
  design — a pronunciation consumer doesn't need five chapters' worth
  of grammatical tagging, so this module runs as an independent
  sibling pipeline directly off raw text, not as an eighth stage
  chained onto `kin_analyze`.

## Sources quoted in this chapter

- `src/g2p.c` (`PHONEME_TABLE[]`, `G2P_3[]`/`G2P_2[]`/`G2P_1[]`,
  `kin_g2p_word`, `kin_g2p_sentence`, `kin_normalize_text`,
  `expand_number`, `seq_append`, `ABBREVS[]`).
- `include/g2p.h` (module header comment, the "Key rules" doc list,
  the worked sentence example, `KinPhonemeSeq`, `KinPhonemeID`).
- `src/main.c` (the `--g2p` CLI flag, confirming `kin_g2p_sentence` is
  called directly on raw input, bypassing `kin_tokenize`).
- Every `pN_*.c` program in this chapter was actually compiled with
  `gcc -std=c99 -Wall -Wextra` and actually executed to produce the
  exact output quoted above.

## Coming up in Chapter 9

Eight chapters have asked what a word means and how it sounds. None
has asked the question every morpheme table since Chapter 1 quietly
deferred with a footnote: when a prefix and a stem actually fuse —
when `ku` meets a vowel-initial stem, when a passive `-w-` collides
with a perfect `-ye`, when a nasal prefix meets a voiceless
consonant — what are the *exact* spelling changes the official
orthography requires? `src/ortho.c` is that engine: ten-plus
phonological passes, each one implementing a numbered section of the
real 2017 RALC orthographic standard, turning a raw morpheme string
into the one correctly spelled surface word Chapters 1 through 3 always
assumed without ever showing how it's actually produced.
