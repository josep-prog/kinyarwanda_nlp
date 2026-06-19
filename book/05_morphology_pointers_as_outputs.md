# Chapter 5 — `morphology.c` (I): Pointers as Multiple Return Values

## Why this file, and why split into two chapters

`morphology.c` is 3,366 lines — the second-largest file in the project.
Its own header comment divides it into four sections: `§0 UTILITIES`,
`§1 PHONOLOGICAL RULES`, `§2 noun detection`, `§3 adjective detection`,
`§4 verb detection`. This chapter covers `§0` and `§1` — the smaller,
simpler functions — and uses them to teach the general pattern of "how
does a C function hand back more than one piece of information." Chapter
6 then shows that same pattern scaled up to its most demanding form in
`§2`–`§4`.

## 5.1 Three ways a C function can give you a string-shaped answer

C has exactly one return value per function. Every function in this
section answers the question "how do I get more out of a function than
one value" with one of three concrete strategies. Naming these three
strategies precisely will make every function signature in this project
instantly readable.

**A. Pure predicate — the return value *is* the entire answer.**
Nothing is written anywhere; the caller is only ever told yes/no (or a
single number).

```c
bool kin_starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
```

**B. Mutate-in-place — the caller's own buffer is edited directly.**
No second buffer is involved; the function is given write access to data
the caller already owns and is told to change it.

```c
void kin_str_trim(char *s) { /* edits s directly, no return value needed */ }
```

**C. Write-to-output-buffer — the caller provides a separate destination.**
The input is left untouched; the answer goes into a different buffer the
caller passed in specifically to receive it.

```c
void kin_strlower(const char *src, char *dst, size_t dstlen) { /* ... */ }
```

Notice the `const` placement is your first clue to which strategy a
function uses, before you even read its body: `const char *s` in
strategy A and B's input means "this won't be modified"; a plain
`char *s` with no `const` (as in `kin_str_trim`'s parameter) is your
signal that the function intends to write through that pointer.

## 5.2 Category A — predicates built from the standard library

```c
bool kin_starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

bool kin_ends_with(const char *s, const char *suffix) {
    size_t sl = strlen(s), pl = strlen(suffix);
    if (pl > sl) return false;
    return strcmp(s + sl - pl, suffix) == 0;
}
```

`kin_starts_with` is a thin, readable wrapper: `strncmp` compares only the
first N bytes of two strings (N = `strlen(prefix)` here), so it can never
look past the length of `prefix` — exactly what "does `s` start with
`prefix`" means. `kin_ends_with` is the more interesting one:
`s + sl - pl` is pointer arithmetic that jumps the pointer directly to
where the suffix *would* begin if `s` ends with `suffix` — `sl - pl` is
how many characters from the start of `s` to skip. There's no loop here
at all; the entire "does this end with that" check is one pointer offset
plus one `strcmp`. The guard `if (pl > sl) return false;` exists because,
without it, `sl - pl` would underflow (both are `size_t`, an *unsigned*
type — `sl - pl` when `pl > sl` would not go negative, it would wrap
around to a huge positive number, and `s + (huge number)` would be a wild,
invalid pointer). This is a real, sharp-edged C gotcha: unsigned
arithmetic never goes negative, it wraps — always check the subtraction
is safe *before* you do it, not after.

## 5.3 Category B — editing the caller's buffer directly

```c
void kin_str_trim(char *s) {
    char *p = s;
    while (*p && (unsigned char)*p <= 0x7F && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t l = strlen(s);
    while (l > 0 && (unsigned char)s[l-1] <= 0x7F && isspace((unsigned char)s[l-1]))
        s[--l] = '\0';
}
```

This trims leading and trailing whitespace **in place** — no second
buffer is allocated anywhere. Two different techniques handle the two
ends, and the difference is worth understanding:

- **Trimming the front requires moving data.** If `s` is `"  umuntu"`,
  the real content starts two bytes in. `p` walks forward past the
  whitespace, then `memmove(s, p, strlen(p) + 1)` slides everything from
  `p` onward back to the start of the buffer (the `+ 1` includes the
  `'\0'` terminator in the move, so the result is still a valid C
  string). `memmove`, not `memcpy`, because the source (`p`) and
  destination (`s`) regions of this single buffer overlap — you saw this
  same justification in Chapter 1's preview of `ortho.c`, and here it is
  again in its simplest form.
- **Trimming the back requires no moving at all.** A C string's length is
  defined entirely by where its `'\0'` sits. To remove trailing
  whitespace, you don't need to shift any bytes — you just need to move
  the terminator *earlier*. `s[--l] = '\0';` is doing two things in one
  expression: `--l` decrements `l` first and produces the decremented
  value, which is then used as the index — so this writes `'\0'` at the
  new last "real" character's position, shrinking the string by exactly
  one character, every time the loop runs. This is asymptotically
  cheaper than the front-trim: no bytes need to move, because nothing
  after the new, shorter end matters anymore — it's still sitting in
  memory, just past the terminator where no C string function will ever
  look.

This function needed no `out` parameter and no return value beyond `void`
— because the caller already owns `s` and expects it to come back
shortened. There's nothing to "return"; the answer *is* the mutation.

## 5.4 Category C — writing into a separate destination buffer

```c
void kin_strlower(const char *src, char *dst, size_t dstlen) {
    size_t i = 0, j = 0;
    while (src[i] && j + 1 < dstlen) {
        unsigned char c0 = (unsigned char)src[i];
        unsigned char c1 = src[i + 1] ? (unsigned char)src[i + 1] : 0;
        if (c0 == 0xC4 && (c1 == 0x80 || c1 == 0x81)) { dst[j++]='a'; i+=2; continue; }
        /* ... four more macron cases ... */
        dst[j++] = (char)tolower(c0);
        i++;
    }
    dst[j] = '\0';
}
```

This is the first function you've seen with **two separate index
variables walking two separate buffers**: `i` tracks position in `src`,
`j` tracks position in `dst`, and they do **not** stay in lockstep. Why?
Because the transformation isn't one-byte-in, one-byte-out: a two-byte
macron sequence like `0xC4 0x80` (Ā) collapses into a single output byte
(`'a'`) — `i` advances by 2 (`i += 2`) while `j` advances by only 1
(`j++`). If this function used one shared index for both buffers (the way
`kin_has_vowel_hiatus` in Section 5.6 can, because its transformation
*is* one-to-one), it would be wrong the moment it hit a macron. Whenever
input and output lengths can differ, you need independent cursors — this
is a generally useful rule, not specific to this function.

Also notice the loop condition: `j + 1 < dstlen`, not `j < dstlen`. This
reserves room for the `'\0'` the function writes after the loop ends —
exactly the same "leave room for the terminator" discipline from Chapter
4's tokenizer.

## 5.5 `kin_vv_join`: guard clauses, and a `printf` trick worth knowing cold

```c
void kin_vv_join(const char *prefix, const char *stem, char *out, size_t out_sz)
{
    if (!prefix || !stem || !out || out_sz == 0) return;

    size_t plen = strlen(prefix);
    size_t slen = strlen(stem);
    if (plen == 0) { snprintf(out, out_sz, "%s", stem);   return; }
    if (slen == 0) { snprintf(out, out_sz, "%s", prefix); return; }

    char p_end   = prefix[plen - 1];
    char p_prev  = plen > 1 ? prefix[plen - 2] : '\0';
    char s_start = stem[0];
    /* ... */
}
```

`if (!prefix || !stem || !out || out_sz == 0) return;` is a **guard
clause**: check every way the inputs could be unusable, up front, and
bail out immediately before any real logic runs. This keeps the rest of
the function free to assume its inputs are sane, instead of interleaving
sanity checks with logic throughout. Notice, too, how the "last
character" and "second-to-last character" of `prefix` are found:
`prefix[plen - 1]` and `prefix[plen - 2]`. No loop is needed to find the
end of the string, because `strlen()` already told you exactly where it
is — direct indexing from a known length is simpler and faster than
walking to the end with a pointer when you already know the length.

Now the trick. Deeper in the function, you'll find lines like this:

```c
snprintf(out, out_sz, "%.*s%s", (int)base, prefix, stem);
```

`%.*s` is a *runtime-supplied precision*. Normally, a format specifier
like `%.5s` hardcodes "print at most 5 characters of this string" directly
in the format string. Writing `*` instead of a literal number means "take
the precision from the next argument in the list" — so `(int)base` is
consumed as the precision (how many characters of `prefix` to print), and
`prefix` is the actual string argument that follows it. The net effect:
this one `snprintf` call prints exactly `base` characters from `prefix`
(silently dropping `prefix`'s last character or two, without needing to
build a separate truncated copy of `prefix` in another buffer first),
immediately followed by the full `stem` string — all written directly and
safely into `out`, bounded by `out_sz`. This is precisely why the function
can resolve vowel-contact rules (Chapter 1 previewed: `mu+iga→mwiga`,
`ku+oma→koma`) without ever allocating a temporary buffer: `%.*s` lets you
print "all but the last N characters of this string" in a single call.

## 5.6 `is_vowel` and indexed lookahead — a different loop style than Chapter 4's

```c
static bool is_vowel(char c) {
    return c=='a'||c=='e'||c=='i'||c=='o'||c=='u';
}

bool kin_has_vowel_hiatus(const char *word) {
    if (!word || !word[0]) return false;
    for (int i = 0; word[i] && word[i+1]; i++) {
        if (is_vowel(word[i]) && is_vowel(word[i+1]))
            return true;
    }
    return false;
}
```

`is_vowel` takes a single `char` **by value** and returns a `bool` — no
pointers at all. This is the simplest possible function shape, used here
because the question ("is this one character a vowel") has nothing to do
with strings, buffers, or ownership; it's a pure, tiny lookup with no side
effects.

`kin_has_vowel_hiatus`'s loop uses **array indexing** (`word[i]`,
`word[i+1]`) rather than Chapter 4's pointer-cursor style (`*p`, `p++`).
Both styles walk a string; the choice here is deliberate: this function
needs to look at *two* characters at once — the current one and the next
one — to detect an illegal adjacent vowel pair. With an index variable
`i`, "the next character" is simply `word[i+1]`, no extra variable needed.
With a pure pointer-cursor style, you'd need a second pointer
(`p` and `p+1`) tracked alongside the first, which is no simpler — so
indexing wins here on readability. Chapter 4's tokenizer needed pointer
arithmetic because it had to jump forward by *variable* amounts (1, 2, or
3 bytes depending on which UTF-8 sequence matched); this function only
ever needs to peek exactly one character ahead, so a plain `int i` index
is the more natural tool. Knowing *which* style to reach for — and being
able to say why — is itself something worth being able to defend.

## 5.7 Try it yourself

Confirm the `%.*s` trick with a tiny standalone program — it's an
obscure-enough feature of `printf`/`snprintf` that seeing it work yourself
is worth the thirty seconds:

```c
#include <stdio.h>
int main(void) {
    char out[32];
    const char *prefix = "kuri";
    int base = 3;   /* print only the first 3 characters: "kur" */
    snprintf(out, sizeof(out), "%.*s-TAIL", base, prefix);
    printf("%s\n", out);   /* prints: kur-TAIL */
    return 0;
}
```

Change `base` to `2` and re-run — confirm the output truncates exactly
where you'd expect, with zero extra buffers involved.

## Key takeaways

- A C function has exactly one return value; when it needs to hand back
  more, it does so through pointer parameters, in one of three shapes:
  pure predicate (return value only), mutate-in-place (`char *`, no
  `const`), or write-to-output-buffer (`const char *src` plus a separate
  `char *dst`).
- Unsigned (`size_t`) subtraction never goes negative — it wraps to a huge
  positive number. Always check `a >= b` before computing `a - b` when
  both are unsigned.
- `memmove` shifts overlapping data safely; shrinking a string from the
  *end* needs no data movement at all — just write `'\0'` earlier.
- When a transformation isn't one-to-one between input and output bytes,
  use two independent index variables, one per buffer — don't assume they
  stay in lockstep.
- Guard clauses (checking every invalid-input case up front and returning
  immediately) keep the rest of a function free to assume sane inputs.
- `%.*s` in a `printf`-family format string takes its precision (max
  characters to print) from an `int` argument at runtime, instead of a
  hardcoded number — useful for printing partial strings without building
  a separate truncated copy.
- Indexed loops (`s[i]`, `s[i+1]`) and pointer-cursor loops (`*p`, `p++`)
  both walk a string; pick indexing when you need to look ahead/behind by
  a fixed, small offset, and pointer-cursor when the advance amount varies
  at runtime.

## Search YouTube for

- "C unsigned integer overflow and underflow explained"
- "memmove vs memcpy when to use which"
- "printf format specifiers explained — precision and width"
- "guard clauses in C programming"
- "C pointers vs array indexing — when to use each"

## Coming up in Chapter 6

Chapter 6 returns to the same three categories from Section 5.1, but now
applied to the actual question this whole project exists to answer: given
a word, which of 16 noun classes is it, or is it a verb, and if so what
tense? You'll meet `kin_strip_noun_prefix()` and
`kin_is_verb_conjugated()` — functions that report success as a `bool`
while writing five or six different pieces of information through five or
six different pointer parameters in a single call — plus the
**longest-match algorithm** that decides which of several overlapping
candidate prefixes is the right one.
