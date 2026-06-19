# Chapter 9 — `ortho.c` (I): Mutable Buffers, `memmove`, and Bounds-Checked Editing

## A different kind of file

Every file so far has *read* a word and decided something about it.
`ortho.c` — 1,330 lines implementing the RALC 2017 sound-change rules — is
the first file that **writes**: given an underlying morpheme sequence, it
produces the correct surface spelling by applying rule after rule,
actually changing the characters in a buffer as it goes. That's a
fundamentally different kind of C code from anything in Chapters 4–8, and
it needs its own vocabulary: in-place editing, overlap-safe copying, and
sentinel return values. This chapter covers the low-level toolbox;
Chapter 10 covers how that toolbox gets organized into a 16-pass pipeline.

## 9.1 The buffer format: one string, with `|` marking morpheme seams

`kin_ortho_gen()` takes an input like `"bi-a-tek-w-ye"` (morphemes
separated by `-`) and is expected to apply rules exactly at those
boundaries. Rather than keeping a separate array of "where the boundaries
are," the boundary marker is normalized into the buffer itself, as a
literal character:

```c
for (int i = 0; morphemes[i] && len+1 < OB; i++) {
    char c = morphemes[i];
    if (c == '0') continue;          /* skip empty/zero morphemes */
    if (c == '-') c = '|';           /* normalise the separator    */
    buf[len++] = (char)tolower((unsigned char)c);
}
```

This is a deliberate simplification: every rule function in this file
only ever needs to scan for the single byte `'|'` to know "a morpheme
boundary is here" — no second array, no struct of offsets, just one
string where the seams are visible characters. The cost of this
simplicity is that, once every rule that needs to *see* the boundaries
has run, those `|` characters have to be stripped back out before the
result is a real word — you'll meet `strip_boundaries()` in Section 9.4,
and see it called as its own numbered pass in Chapter 10.

## 9.2 `OB`: a fixed buffer, completing the storage-duration picture

```c
#define OB  512          /* ortho internal buffer size */

void kin_ortho_gen(const char *morphemes, bool noun_class_9,
                   char *surface, size_t size) {
    char buf[OB];
    int  len = 0;
    /* ... */
}
```

`char buf[OB];` declared as a plain local variable (no `static`) has
**automatic storage duration** — the third and final storage-duration
concept in this book, completing the set Chapter 8 started: a `static`
local (Ch.8) is built once and lives for the whole program; a plain local
like this one is created fresh on the stack every time `kin_ortho_gen()`
is called, and is gone the instant the function returns. Every single
call gets its own private 512-byte scratch space, with zero risk of one
call's leftover data leaking into the next call's — at the cost of that
512 bytes being set up and torn down every single call, which on a stack
allocation is essentially free (no system call, no bookkeeping — moving
the stack pointer is the entire cost). This is the same no-heap-
allocation discipline from Chapter 1, now visible at the smallest
possible scale: even a temporary scratch buffer used only inside one
function avoids `malloc`.

## 9.3 `del_at` and `repl_at`: editing a buffer without rebuilding it

```c
static void del_at(char *buf, int pos, int n, int *len) {
    memmove(buf+pos, buf+pos+n, (size_t)(*len-pos-n+1));
    *len -= n;
}
```

To delete `n` characters starting at position `pos`, every character
*after* the deleted span has to slide left by `n` positions to close the
gap. `memmove(buf+pos, buf+pos+n, ...)` does exactly that: it asks for
everything from `buf+pos+n` onward to be copied to `buf+pos`. The two
regions — source (`buf+pos+n` onward) and destination (`buf+pos`
onward) — **overlap**, because they're both inside the same buffer, just
offset by `n`. This is precisely why `memmove` is required and `memcpy`
would not be safe here: `memcpy`'s behavior is undefined when source and
destination overlap, while `memmove` is specifically guaranteed by the C
standard to produce the correct result even when they do (typically by
internally detecting the overlap direction and copying in whichever
order — forward or backward — avoids corrupting data it hasn't read yet).
The length to move, `*len-pos-n+1`, covers everything from the deletion
point to the end of the string *including* the terminating `'\0'`
(the `+1`) — so the string stays correctly terminated after the shift,
with no separate step needed to re-add it.

The very last line, `*len -= n;`, is the out-parameter pattern from
Chapters 5–6 again, now updating an `int` that tracks the buffer's
**current length** rather than tracking morphological data — every rule
function in this file takes `int *len` for exactly this reason: editing
the buffer's contents and editing its remembered length must always
happen together, or the rest of the pipeline would be scanning past the
real content into stale leftover bytes.

```c
static int repl_at(char *buf, int pos, int old_n, const char *repl,
                   int cur_len, int cap) {
    int rlen = (int)strlen(repl);
    int new_len = cur_len - old_n + rlen;
    if (new_len >= cap) return -1;
    memmove(buf+pos+rlen, buf+pos+old_n, (size_t)(cur_len-pos-old_n+1));
    memcpy(buf+pos, repl, (size_t)rlen);
    return new_len;
}
```

`repl_at` is the more general operation: replace `old_n` characters at
`pos` with an arbitrary replacement string `repl`, which might be a
different length entirely (replacing one character with a two-character
cluster, say). It computes the resulting length *before* touching the
buffer (`new_len = cur_len - old_n + rlen`) and checks that against the
buffer's real capacity (`cap`) — if the replacement would overflow, it
returns `-1` and **does nothing at all** to the buffer. Only once that
check passes does it perform the edit: `memmove` first, to open up (or
close) exactly the right amount of space for the new text by shifting
everything after the edit point, then a plain `memcpy` to drop the
replacement text into the now-correctly-sized gap (this final copy has no
overlap concern, since `repl` is a completely separate string, not a
pointer into `buf` itself).

The `-1` return is worth naming precisely: this function returns an
`int` representing a length, and every real length is `>= 0` — so `-1` is
a value that could never be mistaken for a real answer, used purely as
an **out-of-band sentinel** meaning "this failed." You've now seen this
same idea, the same shape, expressed with three completely different
types: a `NULL` pointer (Ch.4, Ch.6 — "no string here"), an enum value of
`0` (Ch.2 — "no tag here"), and now an impossible integer (`-1` — "no
valid length here"). Different types, identical underlying idea: reserve
one value from the type's normal range to mean "this operation did not
produce a real answer," and have every caller check for it before trusting
the result.

## 9.4 Two editing strategies in the same file: point-edit vs. filter-and-rebuild

`pass_wy_metathesis` makes a small, localized change at specific matched
positions:

```c
static void pass_wy_metathesis(char *buf, int *len) {
    for (int i = 1; i+3 < *len; i++) {
        if (buf[i]=='|' && buf[i+1]=='w' && buf[i+2]=='|' && buf[i+3]=='y') {
            buf[i+1] = 'y';   /* w → y  */
            buf[i+3] = 'w';   /* y → w  */
        }
    }
}
```

This scans for the exact four-character pattern `|w|y` and, when found,
overwrites two specific positions directly. Notice this is **not** a
generic three-line swap (`tmp = a; a = b; b = tmp;`) — it doesn't need to
be, because the `if` condition has already confirmed exactly what's
sitting at each position before the assignment runs: position `i+1` is
known to currently hold `'w'` and is being set to `'y'`; position `i+3`
is known to hold `'y'` and is being set to `'w'`. Once both values are
already known constants, you can just write the literals directly — a
temporary variable would only be needed if you didn't already know what
was being overwritten. This function also never changes `*len` — it only
overwrites existing bytes in place, with no characters added or removed,
so the buffer's length never moves.

Contrast that with `strip_boundaries`, which has to look at and decide on
**every single character** in the buffer:

```c
static void strip_boundaries(char *buf, int *len) {
    char tmp[OB];
    int j = 0;
    for (int i = 0; buf[i] && j+1 < OB; i++)
        if (buf[i] != '|') tmp[j++] = buf[i];
    tmp[j] = '\0';
    memcpy(buf, tmp, (size_t)(j+1));
    *len = j;
}
```

Rather than calling `del_at` once per `'|'` found (each call shifting the
remaining tail of the buffer left, over and over — for a buffer with many
boundary markers, that's a lot of repeated shifting of the same trailing
bytes), this builds the entire result into a **second buffer** (`tmp`,
itself a plain automatic-storage array — Section 9.2's lesson, used
again) in a single forward pass: walk `buf` once, copy every character
that *isn't* `'|'` into `tmp`, skip every character that is. Once the
filtering pass is done, the whole result is copied back over `buf` in one
`memcpy` (no overlap here — `tmp` and `buf` are two entirely separate
arrays). This is a genuinely different strategy from `del_at`/`repl_at`,
and the right one for a different shaped problem: **point-edit a known,
specific location** with `memmove`-based shifting; **filter or rebuild
the entire contents** by writing a fresh copy in a single linear pass.
Recognizing which situation you're in — "I need to change one known spot"
versus "I need to decide, character by character, what survives" — is
the actual engineering judgment behind choosing between these two
strategies, and it's a fair question to be asked directly: "why does this
file edit some things in place and rebuild others from scratch?"

## 9.5 Try it yourself: prove `memmove` is the safe one

```c
#include <string.h>
#include <stdio.h>

int main(void) {
    char a[20] = "kintu-ki-wye";
    /* Shift "ki-wye" (from index 6) left to index 3, closing a 3-byte gap */
    memmove(a + 3, a + 6, strlen(a + 6) + 1);
    printf("memmove result: %s\n", a);   /* "kinki-wye" — correct */

    char b[20] = "kintu-ki-wye";
    memcpy(b + 3, b + 6, strlen(b + 6) + 1);
    printf("memcpy result:  %s\n", b);   /* undefined behavior — don't trust this output */
    return 0;
}
```

Compile and run this yourself. The `memmove` line is guaranteed correct
by the C standard regardless of which way the overlap goes. The `memcpy`
line is calling a function on overlapping regions in a way the standard
explicitly leaves undefined — on many real implementations you will
likely observe corrupted, duplicated, or truncated output, but the
honest, technically precise statement is that the standard does not
promise *any particular* result, correct or otherwise. That distinction —
"likely wrong in practice" versus "undefined, so no result is guaranteed
at all" — is exactly the level of precision worth having ready if this
comes up in your defense.

## Key takeaways

- Embedding a delimiter character (`|`) directly inside the buffer being
  edited is a simple way to mark structure without a second data
  structure — at the cost of needing an explicit pass later to strip the
  markers back out.
- C has three storage durations for variables: automatic (a plain local,
  created and destroyed every call — `char buf[OB]` here), static
  (Ch.8 — created once, lives for the whole program), and (not used
  anywhere in this project) dynamic/heap, via `malloc`.
- `memmove` is required, not optional, whenever source and destination
  regions can overlap — its behavior is defined for that case by the C
  standard; `memcpy`'s is not.
- An `int *len` parameter threaded through every editing function is the
  out-parameter pattern again, now tracking a buffer's current length
  rather than morphological data.
- A function can signal failure with an otherwise-impossible value from
  its own return type (`-1` for a length-returning `int`) — the same
  underlying idea as a `NULL` pointer or a zero-valued enum, just
  expressed through a different type each time.
- Point-editing a known location (`memmove`-based shifting) and
  filtering/rebuilding an entire buffer (a fresh linear pass into a
  second buffer) are two different strategies for two different shaped
  problems — picking the wrong one isn't incorrect, just less efficient
  or less clear than the alternative.

## Search YouTube for

- "memmove explained — why it's overlap safe"
- "C automatic storage duration vs static vs dynamic"
- "sentinel values in C return codes"
- "in-place array filtering algorithm C"
- "undefined behavior in C explained"

## Coming up in Chapter 10

You've now seen every individual tool `ortho.c` uses to edit a buffer.
Chapter 10 zooms out to `kin_ortho_gen()` itself: 16 numbered passes
applied in a fixed sequence, several of them wrapped in a small
`for (iter = 0; iter < N; iter++)` loop with an `any` flag — a
**fixed-point iteration**, re-running a rule until it stops finding
anything left to change. You'll see why some phonological rules can
trigger each other in a chain reaction that a single pass over the buffer
cannot fully resolve, and why looping "until stable" is the correct
response to that.
