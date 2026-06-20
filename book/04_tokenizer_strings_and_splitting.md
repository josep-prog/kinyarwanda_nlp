# Chapter 4 — `tokenizer.c`: Strings, Char Arrays, and Splitting Text

## Why this is the first file to actually run

Everything in Chapters 1–3 was groundwork: the contract, the tags, the
containers. `tokenizer.c` is where data first moves — raw text goes in,
a filled `Token` array comes out. It's also the smallest, most
self-contained file in the project (315 lines, no dependency on any
other `.c` file's logic), which makes it the right place to learn the C
string idioms that every later chapter will assume you already know.

## 4.1 C has no `split()` — you walk the string yourself

In Python you'd write `text.split()`. C gives you no such function. A C
string is nothing more than a `char` array whose end is marked by a single
`'\0'` (null) byte — there is no length stored alongside it. The only way
to find where it ends, or to find a particular character inside it, is to
walk forward byte by byte and check. This is the entire shape of
`kin_tokenize()`:

```c
int kin_tokenize(const char *text, Token *out, int max_tokens) {
    int count = 0;
    const char *p = text;

    while (*p && count < max_tokens) {
        /* ... skip whitespace, find boundaries, emit tokens, advance p ... */
    }
    return count;
}
```

`p` is a **cursor** — a pointer that walks along the input one position at
a time. `*p` dereferences it: "the character currently under the cursor."
`while (*p)` is the standard C idiom for "while we haven't hit the
terminating `'\0'` yet" — `'\0'` is the only character that's falsy in C
(it equals integer `0`), so this loop condition reads naturally as "while
there's still a character here." This single pattern — a `const char *`
cursor, advanced by `p++` or `p += n`, tested against `*p` — is the
backbone of every hand-written scanner in C, including, not
coincidentally, the lexical-analysis front end of real compilers.

Notice the function signature itself follows the out-parameter pattern
from Chapter 2/3: `kin_tokenize` returns an `int` (how many tokens it
produced), but the *real* output — the actual tokens — is written through
`Token *out`, a buffer the **caller** owns and allocates (recall
`SentenceAnalysis.tokens[256]` from Chapter 3). `max_tokens` is the
caller telling this function "don't write past this many slots" — the
capacity half of the capacity+count idiom, enforced here at the point
where the array actually gets filled.

## 4.2 Why raw bytes, not "characters": Kinyarwanda needs UTF-8 awareness

Kinyarwanda text in the wild contains curly quotes (`'`, `"`, `"`),
guillemets (`«`, `»`), and en/em dashes (`–`, `—`) — none of which are
plain ASCII. In UTF-8 encoding, any character beyond the original ASCII
set is represented as *multiple* bytes. A C `char` is exactly **one
byte**. There is no built-in C type in this project's chosen standard that
represents "one Unicode character" directly — so the tokenizer works at
the raw byte level and explicitly checks for the *specific byte sequences*
that known multi-byte characters encode to:

```c
/* UTF-8 U+2019 RIGHT SINGLE QUOTATION MARK: E2 80 99 */
if ((unsigned char)q[0] == 0xE2 &&
    q + 2 < word_end &&
    (unsigned char)q[1] == 0x80 &&
    (unsigned char)q[2] == 0x99) {
    *apos_len = 3;
    return q;
}
```

Three things to learn from this one block:

- **`(unsigned char)` casts everywhere.** Whether `char` is signed or
  unsigned is *not* guaranteed by the C standard — it depends on the
  compiler and platform. A byte like `0xE2` (which is 226 as an unsigned
  value) would be interpreted as a **negative** number if `char` happens
  to be signed on a given platform. Comparing a possibly-negative `char`
  against the positive literal `0xE2` would then silently fail on some
  platforms and work on others — a notorious, hard-to-reproduce class of
  C bug. Casting to `unsigned char` first forces the comparison into the
  unambiguous 0–255 range every time, on every platform. This single
  habit is why you'll see `(unsigned char)` before almost every raw byte
  comparison in this file.
- **Bounds checking before reading ahead.** `q + 2 < word_end` is checked
  *before* `q[1]`/`q[2]` are read. Reading past the end of a buffer is
  undefined behavior in C — the language will not stop you, it might
  crash, or it might silently read garbage memory. Every multi-byte check
  in this file confirms there's room for the bytes it's about to read
  before reading them.
- **No real Unicode library used.** This project doesn't link against a
  Unicode-handling library (like ICU) — it hardcodes the specific byte
  patterns for the handful of multi-byte characters that actually show up
  in Kinyarwanda text. That's a deliberate, minimal-dependency choice
  consistent with Chapter 1's "zero runtime dependencies" goal, traded
  against the fact that it would not correctly handle arbitrary Unicode
  beyond the patterns explicitly coded for.

## 4.3 `static` functions: this file's private helpers

```c
static const char *find_apostrophe(const char *p, const char *word_end,
                                    size_t *apos_len) { ... }
static bool is_dquote(const char *p) { ... }
static int emit_punct(Token *out, int count, int max_tokens,
                      const char *ch, size_t nbytes, PunctType pt) { ... }
```

None of these three functions appear in `kinyarwanda.h`. They're marked
`static`, which in C (at file scope) means **internal linkage** — the
symbol exists only within `tokenizer.c`'s compiled object file; no other
`.c` file can call it, even if it somehow guessed the name, because the
linker never exposes it outside this translation unit. This is C's
nearest equivalent to a "private method": `kin_tokenize()` is the one
function this file makes public (declared in the header), and
`find_apostrophe`/`is_dquote`/`emit_punct` are implementation details it
leans on internally. If you ever needed to change how apostrophes are
detected, you could rewrite `find_apostrophe()` entirely and know with
certainty that nothing outside this file could possibly be depending on
its old behavior — the compiler enforces that guarantee for you.

## 4.4 `emit_punct`: writing a token, and the zero-then-fill pattern

```c
static int emit_punct(Token *out, int count, int max_tokens,
                      const char *ch, size_t nbytes, PunctType pt) {
    if (count >= max_tokens) return 0;
    memset(&out[count], 0, sizeof(Token));
    memcpy(out[count].surface, ch, nbytes);
    out[count].surface[nbytes] = '\0';
    memcpy(out[count].lower,   ch, nbytes);
    out[count].lower[nbytes]   = '\0';
    out[count].pos             = POS_PUNCTUATION;
    out[count].punct_type      = pt;
    /* ... more field assignments ... */
    return 1;
}
```

Read this top to bottom as a sequence of deliberate decisions:

1. **Capacity check first.** `if (count >= max_tokens) return 0;` — refuse
   to write before doing anything else. Returning `0` (rather than, say,
   crashing or writing out of bounds) is this function's way of reporting
   "I didn't do it" with no exceptions mechanism available in C.
2. **`memset(&out[count], 0, sizeof(Token));`** — zero out the *entire*
   struct before filling in specific fields. This is exactly Chapter 2's
   "zero means none/unknown" pattern, applied manually: rather than
   relying on the slot having *already* been zero (it might be reused
   memory from a previous, larger sentence's analysis), this explicitly
   re-zeros it, so every `bool` flag and every enum field starts at its
   "nothing" value, and only the fields this function actually cares
   about get overwritten next. `sizeof(Token)` — from Chapter 3, you know
   this is 2,204 bytes — is passed directly to `memset`, so this line
   correctly zeroes the whole struct no matter how many fields `Token`
   ever grows to have; nobody needs to update this line if `Token` changes
   shape.
3. **`memcpy` + manual null terminator.** `memcpy(out[count].surface, ch,
   nbytes)` copies exactly `nbytes` raw bytes — it does **not** know or
   care about C strings, and it does **not** add a `'\0'` for you. The
   very next line, `out[count].surface[nbytes] = '\0';`, does that by
   hand. Forgetting this line is one of the most common C bugs there is:
   the buffer would contain the right bytes, but every C string function
   (`strlen`, `strcmp`, `printf("%s", ...)`) would keep reading past the
   end looking for a terminator that was never written, into whatever
   garbage happens to follow in memory.
4. **The return value doubles as a delta, not just a flag.** Look at how
   every call site uses it:

```c
if (*p == ',')  { count += emit_punct(out,count,max_tokens,p,1,PUNCT_COMMA); p++; continue; }
```

   `emit_punct` returns `1` on success or `0` on failure (capacity
   reached). `count += emit_punct(...)` means: on success, `count`
   advances by exactly one (the slot just written); on failure, `count`
   stays exactly where it was (nothing was written, so don't pretend a
   slot was used). This is a compact way of combining "did it succeed"
   and "advance the counter accordingly" into a single expression, instead
   of writing `if (emit_punct(...)) count++;` at every one of the dozen
   call sites in this function.

## 4.5 Pointer arithmetic: subtracting pointers to get a length

```c
const char *apos = find_apostrophe(p, word_end, &apos_len);
if (apos) {
    size_t len = (size_t)(apos - p);
    /* ... */
}
```

`apos` and `p` are both pointers into the *same* underlying character
array (the input `text`). Subtracting one pointer from another that
points into the same array gives you the number of elements *between*
them — here, since each element is one `char`, that's directly a byte
count. This is one of the most common pointer idioms in C: rather than
manually counting characters in a loop to measure a span, you locate the
*start* and the *end* with two pointers and subtract. The cast to
`(size_t)` is there because pointer subtraction technically produces a
signed type (`ptrdiff_t`), but the code already knows `apos` comes *after*
`p` here, so the result is guaranteed non-negative and is immediately
treated as an unsigned size.

### 4.5.1 Why the tokenizer needs to know about apostrophes at all

`find_apostrophe` exists because of a specific, real fact about written
Kinyarwanda introduced in Part 12 of this project's grammar reference: the
conjunctions `na` ("and") and `nka` ("like/as") **elide their final vowel**
before a word that starts with a vowel, and the official orthography
writes that elision with an apostrophe:

```
   na  + isi   →  n'isi    "and earth"     (na's "a" dropped before isi's "i")
   nka + umwana →  nk'umwana "like a child" (nka's "a" dropped before umwana's "u")
```

Without `find_apostrophe`, the tokenizer would see `"n'isi"` as one
unbroken run of letters and apostrophe-like punctuation and have no
principled way to know it's actually *two words* — the conjunction `na`
(missing its final vowel) plus the noun `isi`. By splitting at the
apostrophe and treating the elided fragment (`n'`) as its own token, every
later chapter's tagging and analysis gets to work with `na` and `isi` as
the two real, separate grammatical units they linguistically are — exactly
the split this book's Chapter 8 case study (Section 8.6) relies on when it
traces this very sentence.

## 4.6 Defensive clamping: no exceptions, so check and clamp by hand

```c
size_t wlen = (size_t)(word_end - p);
if (wlen == 0) { p++; continue; }
if (wlen >= KIN_MAX_WORD) wlen = KIN_MAX_WORD - 1;

memset(&out[count], 0, sizeof(Token));
memcpy(out[count].surface, p, wlen);
out[count].surface[wlen] = '\0';
```

If a "word" found in the input is somehow longer than `KIN_MAX_WORD - 1`
(127 characters, leaving room for the terminator — recall `KIN_MAX_WORD`
is 128 from Chapter 1), this doesn't crash, doesn't throw, and doesn't
silently overflow the buffer. It clamps: `wlen` is forced down to the
maximum that safely fits, and the rest of that absurdly long "word" is
simply not copied. This is the manual, by-hand version of bounds checking
that a language with exceptions or growable strings would give you
automatically — in C, you write the `if` yourself, every time you copy
into a fixed buffer, or you risk a buffer overflow.

## 4.7 A NULL-terminated array as a different "list of unknown length" idiom

Chapter 3 introduced the capacity+count idiom (a fixed array plus a
separate `int n`). Here's a *second* idiom for representing a list of
unknown length, used when there's a natural value that can never be a
real element — a **sentinel**:

```c
static const char * const FITE_FORMS[] = {
    "bafite", "bifite", "zifite", "rufite", "gafite", "dufite",
    "mufite", "bufite", "gifite", "nfite",  "ufite",  "afite",
    "ifite",  NULL
};
```

`FITE_FORMS` is an array of `const char *` — each element is a pointer to
a string literal. The list ends with `NULL`, which can never legitimately
be one of the strings (a real string pointer is never the null pointer).
Code that walks this array doesn't need a separate length variable at
all — it just keeps going until it sees `NULL`:

```c
for (int fi = 0; FITE_FORMS[fi]; fi++) {
    /* FITE_FORMS[fi] is truthy (non-NULL) until the sentinel is reached */
}
```

This exact pattern — array of pointers, `NULL`-terminated, walked with a
`for` loop that uses the array element itself as the loop condition — is
the same idiom C's own `argv` uses in `main(int argc, char *argv[])`
(`argv[argc]` is guaranteed `NULL`). It's worth being able to contrast the
two idioms directly: capacity+count (Chapter 3) is used when the elements
themselves have no natural "this is the end" value (an `int` array can't
use `0` as an end marker if `0` is a valid value); a `NULL`-terminated
array is used when the element type (a pointer) *does* have a value
(`NULL`) that can never be a real element. Both solve the same underlying
problem — "how do I know where this list ends without a separate length
field" — with whichever trick fits the element type.

## 4.8 `ctype.h` functions need `unsigned char` too

```c
if (isupper((unsigned char)out[count].surface[0])) {
```

`isupper()` (and its relatives `tolower()`, `isalpha()`, `isdigit()`, all
from `<ctype.h>`) are defined by the C standard to expect either a value
representable as `unsigned char`, or the special value `EOF` — *not* a
plain, possibly-signed `char`. Passing a plain `char` whose value happens
to be negative (which, per Section 4.2, can happen with high-byte values
on platforms where `char` is signed) is technically undefined behavior.
The fix is the same cast you've now seen several times in this file:
`(unsigned char)` before the call. This is a small habit, but it's one
real C programmers are expected to know cold — it shows up in code review
checklists and static analysis tools for exactly this reason.

## Key takeaways

- C has no built-in string-splitting function; tokenizing means walking a
  `const char *` cursor byte by byte, checking `*p` against known
  characters, and advancing the pointer yourself.
- Multi-byte UTF-8 characters must be matched as explicit byte sequences
  in C, since `char` is one byte; always cast to `(unsigned char)` before
  comparing raw byte values, because plain `char` signedness is platform-
  dependent.
- `static` functions at file scope are invisible outside their `.c` file
  — C's mechanism for private helper functions.
- `memset(&x, 0, sizeof(x))` zero-initializes a whole struct in one call,
  consistent with the "zero means none" convention from Chapter 2.
- `memcpy` never null-terminates a string for you — you must write the
  `'\0'` yourself after copying raw bytes into a `char` buffer.
- Pointer subtraction (`end - start`) is the standard way to measure the
  distance between two positions in the same array.
- Always clamp lengths against your buffer's real capacity before
  copying — C will not stop you from overflowing a fixed buffer.
- A `NULL`-terminated array of pointers is an alternative to
  capacity+count for representing a list of unknown length, usable
  whenever the element type has a value that can never be "real" data.

## Search YouTube for

- "C strings explained — null terminated character arrays"
- "pointer arithmetic in C explained"
- "signed vs unsigned char in C gotchas"
- "memcpy vs strcpy vs memset in C"
- "UTF-8 encoding explained byte by byte"
- "NULL terminated array of strings C (argv pattern)"

## Coming up in Chapter 5

`tokenizer.c` only classified *punctuation* and capitalization. The much
harder problem — deciding whether `"abana"` is a noun, and if so which of
16 classes it belongs to — starts in `morphology.c`. Chapter 5 covers that
file's first half: small utility functions that all share one shape,
returning a `bool` while writing the *real* answer through pointer
parameters, and why that's the idiomatic C way to give a function several
outputs at once.
