# Chapter 17 — `api.c`: Designing a Thin Convenience Layer

## Why a second, simpler interface on top of a complete one

`kinyarwanda.h` and `kin_analyze()` (Chapter 15) already give you
everything: a full `SentenceAnalysis`, every token's grammar tree,
tense, morpheme breakdown, every detected error. But most real callers
of this library — an ASR pipeline cleaning up speech-to-text output, a
TTS frontend needing phonemes — don't want to learn that whole model.
They want one function: "give me corrected text back," or "give me a
phoneme string back." `api.c` (100 lines) exists purely to be that
second, smaller interface, built *on top of* the complete one, for
exactly this kind of caller. In design terminology this is sometimes
called a **facade**: a small, simplified interface placed in front of a
larger, more capable subsystem, for callers who only need a fraction of
what that subsystem can do.

## 17.1 Returning a pointer to a `static` local buffer — and why it's the only safe choice

```c
const char *kin_correct(const char *text)
{
    static char buf[4096];
    /* ... fill buf ... */
    return buf;
}
```

This is the most important single line in this chapter: `static char
buf[4096];`, declared *inside* the function, with a pointer into it
handed back to the caller via `return buf;` (an array name, used here,
decays to a pointer to its first element). Recall Chapter 9's complete
trio of storage durations: automatic (plain locals — destroyed the
instant the function returns), static (built once, lives for the
program's entire run), and dynamic/heap (this project uses none).

If `buf` had been declared *without* `static`, this function would be
committing one of the most classic mistakes in C: returning a pointer to
memory that no longer exists. The instant `kin_correct()` returns, an
ordinary automatic-duration local array's storage is gone — the stack
space gets reused by whatever the program calls next. A pointer into it
would be **dangling**: it might appear to still work by sheer luck (if
nothing has overwritten that stack memory yet), or it might silently
return garbage, or it might crash — all three are "valid" outcomes of
undefined behavior, and which one you get is not something you can rely
on. `static` is what makes this function legal and correct: a `static`
local variable's lifetime is the entire program, not just one call, so
the memory `buf` occupies is still there, still holding what was just
written into it, long after this particular call to `kin_correct()` has
returned. This is *the* canonical reason a C programmer reaches for a
`static` local: when a function must hand back a pointer to something,
and there's no heap allocation involved, `static` storage is the only
one of the three durations that outlives the function call.

## 17.2 The trade-off, and how this project documents it

That safety comes at a real cost, and this project's own header is
explicit about it — read `kinyarwanda_api.h`'s comment directly:

```
Thread safety: kin_correct() and kin_g2p() use static buffers — call
from one thread at a time (or copy the result before the next call).
kin_analyze() returns a value (SentenceAnalysis on the stack) — safe
to call from multiple threads simultaneously.
```

Because `buf` is a single, shared piece of memory — there is exactly
**one** `buf` for the entire program, not one per call — every call to
`kin_correct()` overwrites the *same* memory the previous call's
returned pointer still points to. Two calls in a row:

```c
const char *a = kin_correct("Umuntu mugni aragenda");
const char *b = kin_correct("Indi nteruro");
/* 'a' now ALSO reads whatever 'b' produced — both point at the same buf */
```

`a` is not a separate string anymore the moment `b`'s call runs — it's
the *same* pointer, into the *same* memory, which `kin_correct()` has
since overwritten. This is exactly why the header says the result is
"valid until the next call," and exactly why it can't safely be called
from two threads at once (two threads writing into the same `buf`
simultaneously would interleave their writes into one corrupted result).

This is the precise, direct payoff of Chapter 15's discussion of
`kin_analyze()` returning a struct **by value**: that design choice gives
every caller their own independent copy (Chapter 3's struct-copy
semantics), safe across threads, at the cost of a large value being
returned. `kin_correct()`'s design instead returns a pointer into shared
storage — cheaper in the sense that no large struct needs to be
constructed and handed back, but unsafe to call concurrently and only
valid for a limited window. Neither design is unconditionally "better" —
they're two different, legitimate answers to "how do I hand data back to
a caller in C," and this project deliberately uses *both*, in different
files, for different reasons, and documents the difference plainly so
callers know which guarantee they're getting from which function. Being
able to explain why two functions in the same library make two different
choices — and that the difference is intentional, not inconsistent — is
exactly the kind of question worth being ready for.

## 17.3 Building the corrected sentence: preference lookup, plus manual delimiter logic

```c
const char *word = tok->surface;
for (int e = 0; e < sa.error_count; e++) {
    if (sa.errors[e].token_index == i
        && sa.errors[e].type == ERR_SPELLING
        && sa.errors[e].suggestion[0] != '\0') {
        word = sa.errors[e].suggestion;
        break;
    }
}
```

This is the same "prefer the better value if one exists, otherwise fall
back" idiom from Chapter 14 — there it was a single ternary expression;
here, because the lookup condition needs three separate checks (the
right token, the right error type, a non-empty suggestion), it's spelled
out as a small loop with a `break` the moment a match is found, rather
than squeezed into one `?:` expression. Same underlying idea, different
syntactic shape, chosen because the condition genuinely needed more than
a one-line test.

```c
bool attach_left = (tok->pos == POS_PUNCTUATION
                    && tok->punct_type != PUNCT_QUOTE_OPEN);
if (pos > 0 && !attach_left) {
    if (pos < sizeof(buf) - 1)
        buf[pos++] = ' ';
}
size_t wlen = strlen(word);
if (pos + wlen >= sizeof(buf))
    wlen = sizeof(buf) - pos - 1;   /* truncate rather than overflow */
memcpy(buf + pos, word, wlen);
pos += wlen;
buf[pos] = '\0';
```

This is the third time in this book you've built a delimited string by
hand, one piece at a time, with no `join()` function to lean on
(Chapter 14's two-row gloss display, Chapter 16's `seq_append`): track a
running position (`pos`), decide whether the *next* piece needs a
separator before it (`attach_left` answers exactly that, this time
based on whether the upcoming token is punctuation that should hug the
previous word rather than float a space away from it), then copy the
piece itself with an explicit truncate-rather-than-overflow guard —
Chapter 4 and Chapter 9's bounds-clamping discipline, applied here to
the one shared `static` buffer this entire function writes into.

## 17.4 `kin_g2p`: the same static-storage trick, applied to a whole struct

```c
const char *kin_g2p(const char *text)
{
    static KinPhonemeSeq seq;
    if (!text || !text[0]) { seq.repr[0] = '\0'; return seq.repr; }
    if (!kin_g2p_sentence(text, &seq)) seq.repr[0] = '\0';
    return seq.repr;
}
```

Same reasoning as Section 17.1, applied one level up: `static
KinPhonemeSeq seq;` gives the *entire* struct (Chapter 16's dual-view
phoneme sequence) program-long lifetime, not just its `char buf[]`
field. The function then returns `seq.repr` — a pointer to one **field**
of that static struct, not the struct itself. This is worth confirming
explicitly: static storage duration applies to the *whole* object,
which means every field inside it, including nested arrays, is equally
safe to take the address of and return — `&seq.repr[0]` is just as valid
a pointer to hand back as `&seq` itself would be, for exactly the same
reason.

## 17.5 Try it yourself: reproduce the "valid until next call" gotcha

```c
#include "kinyarwanda_api.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *a = kin_correct("Umuntu munini aragenda");
    char a_copy[256];
    strncpy(a_copy, a, sizeof(a_copy) - 1);   /* save a's content NOW */
    a_copy[sizeof(a_copy)-1] = '\0';

    const char *b = kin_correct("Indi nteruro runaka");

    printf("a (live pointer, now stale): %s\n", a);       /* prints b's text */
    printf("a_copy (saved earlier):      %s\n", a_copy);  /* prints a's real text */
    printf("b:                           %s\n", b);
    return 0;
}
```

Run this and confirm: `a`, read *after* the second call, no longer shows
the first sentence's corrected text — it shows whatever `kin_correct()`
most recently wrote into the one shared `buf`. Only `a_copy`, which you
deliberately copied out *before* the second call, still holds the
original result. This is precisely the discipline the header comment
asks of every caller: copy the result before calling again, if you need
to keep it.

## Key takeaways

- A function that must return a pointer to data it builds, with no heap
  allocation involved, has to use a `static` local — it's the only
  storage duration that outlives the function call itself.
- That convenience comes with a real cost: a single shared buffer means
  the result is only valid until the next call, and the function is not
  safe to call from multiple threads at once — a trade-off this project
  documents explicitly rather than hiding.
- Returning a struct **by value** (Ch.15's `kin_analyze`) and returning a
  **pointer to static storage** (this chapter's `kin_correct`/`kin_g2p`)
  are two legitimate, different answers to "how do I hand data back to a
  caller" — safe-but-larger versus cheap-but-shared — and a real library
  can deliberately use both, for different functions, for different
  reasons.
- Static storage duration applies to an entire object, including every
  field inside it — returning a pointer to one field of a `static`
  struct is exactly as safe as returning a pointer to the struct itself.
- A thin "convenience" layer in front of a more complete API (a facade)
  is a legitimate design choice when most real callers only need a
  fraction of what the full interface offers.

## Search YouTube for

- "returning pointers to local variables in C — dangling pointers"
- "static local variables explained C"
- "thread safety static buffers C"
- "facade design pattern explained"

## Coming up in Chapter 18

`main.c` is the one file in this entire project that is *not* part of
the library (recall Chapter 1's explanation of why it's excluded from
`LIB_SRCS`). Chapter 18 covers how it parses command-line arguments
(`argv`/`argc`), reads files line by line, and runs an interactive REPL
loop — the only place in the whole codebase that talks directly to a
terminal or a filesystem.
