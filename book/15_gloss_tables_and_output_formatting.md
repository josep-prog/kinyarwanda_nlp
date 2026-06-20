# Chapter 15 — `gloss.c`: Large Tables, Round Two, and Output Formatting

## What's genuinely new here

`gloss.c` (776 lines) reuses Chapter 7's table-and-linear-search pattern
almost exactly — you'll recognize it instantly. What's new in this
chapter is everything *around* that pattern: a defensive habit Chapter
14 found missing elsewhere, the ternary operator as a compact
fallback idiom, and — the real centerpiece — how a four-line,
column-aligned interlinear gloss display is built using nothing but
`printf` width specifiers, no terminal UI library anywhere in sight.

## 15.0 What an "interlinear gloss" is, and where its abbreviations come from

Up to now, this project has mostly *talked about* morphemes (D, RT, C, SP,
TM...) using whichever vocabulary felt natural per chapter. Linguists have
a standardized way of *displaying* a morpheme-by-morpheme breakdown
alongside its meaning, called an **interlinear gloss**, and the specific
set of abbreviations this project uses (`3SG`, `PRF`, `PASS`, `APPL`,
`RECP`, `SUBJ`, `IMP`, ...) follows the **Leipzig Glossing Rules** — a
widely adopted academic convention for exactly this purpose, used across
linguistics papers on every language family, not invented for this
project. The convention has three stacked lines per word:

```
   Line 1 (surface):    yaremye
   Line 2 (morphemes):  ya  -  rem   -  ye
   Line 3 (gloss):      3SG.HUM - create - PRF
   Line 4 (free translation, once per sentence/clause): "he/she created [it]"
```

Every abbreviation in line 3 is a standardized code, not an ad hoc
shorthand: `3SG` = third person singular, `HUM` = human (naming which of
the 16 noun classes the subject agrees with — Section 7.1.1's table is
exactly where `3SG.HUM` vs. `3SG.CL7` vs. `3SG.LOC` come from), `PRF` =
perfect/completed aspect. A reader who has never seen a single word of
Kinyarwanda, but knows the Leipzig conventions (or is simply told what
each code means, which `kin_print_interlinear`'s own banner does — see the
"Abbreviations:" lines it prints), can read that gloss line and understand
the grammatical structure of the word without needing the surface
spelling decoded at all. This is *why* `gloss.c` exists as its own file,
separate from `morph_dispatch.c`: Chapter 11 was about recovering *which
letters* belong to which morpheme; this file is about translating each
recovered morpheme into this standardized, internationally-readable
notation.

## 15.1 The same table, the same search, one new defensive habit

```c
typedef struct { const char *stem; const char *gloss; } VerbGloss;
static const VerbGloss VERB_GLOSS_TABLE[] = {
    { "som", "read" }, { "bon", "see/get" }, /* ... 200+ entries ... */
};

bool kin_get_verb_gloss(const char *stem, char *out, size_t outsize) {
    if (!stem || !out || outsize == 0) return false;
    out[0] = '\0';
    for (int i = 0; VERB_GLOSS_TABLE[i].stem; i++) {
        if (strcmp(stem, VERB_GLOSS_TABLE[i].stem) == 0) {
            strncpy(out, VERB_GLOSS_TABLE[i].gloss, outsize - 1);
            out[outsize - 1] = '\0';
            return true;
        }
    }
    return false;
}
```

This is Chapter 7's array-of-structs, `NULL`-terminated, linearly searched
— at a larger scale (200+ rows instead of dozens), but structurally
identical. The one new thing worth pointing out: `out[0] = '\0';` runs
**before the search loop even starts**. If no match is ever found, `out`
is still guaranteed to be a valid, empty C string when the function
returns `false` — never untouched, never garbage. This is a direct,
useful contrast with Chapter 14's finding in `corrector.c`, where a
missing explicit terminator left a buffer's safety dependent on an
assumption about prior zero-initialization. Here, the function takes
charge of its own output buffer's validity itself, unconditionally, at
the very top, before anything else runs. Pointing out that the same
codebase is *more* careful in one file and *less* careful in another,
specifically and by name, is a stronger defense answer than claiming
uniform perfection in either direction.

## 15.2 The ternary operator as a "prefer this, else that" idiom

```c
const char *surf = t->morph.m[m].surface[0]
                   ? t->morph.m[m].surface
                   : t->morph.m[m].form;
/* ... a few lines later ... */
const char *gl = t->morph.m[m].english_gloss[0]
                 ? t->morph.m[m].english_gloss
                 : t->morph.m[m].label;
```

`condition ? value_if_true : value_if_false` is the **ternary
(conditional) operator** — a compact `if`/`else` that *produces a value*
directly, usable anywhere an expression is expected, instead of needing a
separate variable assigned inside a multi-line `if` block. Both lines
here follow the identical shape: "use the proper field if it's non-empty
(`[0]` checked against `'\0'` — Chapter 2's zero-means-empty convention,
still doing work many chapters later), otherwise fall back to a
less-ideal but always-present field." This "prefer X, fall back to Y"
idiom is common enough, and compact enough with the ternary operator,
that recognizing the shape on sight — rather than mentally expanding it
into an `if`/`else` every time — will make a lot of real-world C (and
C-like) code faster to read.

## 15.3 Building aligned columns with nothing but `printf` width specifiers

```c
printf("  %-18s ", t->surface);
for (int m = 0; m < t->morph.n; m++) {
    if (m > 0) printf(" \xe2\x80\x93 ");
    const char *surf = /* ... ternary fallback ... */;
    printf("%-8s", surf);
}
printf("\n");
printf("  %-18s ", "");
for (int m = 0; m < t->morph.n; m++) {
    if (m > 0) printf(" \xe2\x80\x93 ");
    const char *gl = /* ... ternary fallback ... */;
    printf("%-8s", gl);
}
printf("\n");
```

This produces the classic **interlinear gloss** layout — linguists'
standard way of showing a word's surface form on one line and its
meaning directly beneath it, column for column. There is no curses
library, no terminal UI framework, no grid widget anywhere in this
project. The entire alignment is achieved with one `printf` feature:
`%-18s` and `%-8s`.

- The `-` flag means **left-justify**: pad the string with spaces on the
  *right* until it reaches the given width, instead of `printf`'s default
  of padding on the left (right-justifying). Left-justifying text is what
  makes columns of varying-length words still start at the same visual
  position on the next line.
- The number (`18`, `8`) is a **minimum** width, not a maximum. If
  `t->surface` happens to be longer than 18 characters, `printf` will
  print the entire string anyway, simply without any padding — it never
  truncates. This is worth knowing precisely: column alignment via
  `printf` widths is a best-effort visual aid, not a guarantee, and a
  sufficiently long word can and will push the following columns out of
  visual alignment. That's a fair, accurate limitation to name if asked
  "is this display always perfectly aligned" — no, not for arbitrarily
  long input, by design of how `printf` width specifiers work.
- **`printf("  %-18s ", "");`** is the cleverest single line in this
  block: printing an *empty string* with the exact same `%-18s` width
  used for the real word above it produces a blank field of the
  *identical* width — pure padding, no visible text — which is exactly
  what's needed to keep the second (gloss) row's first column lining up
  underneath the first (surface-word) row's first column, even though the
  second row has nothing of its own to put there.

## 15.4 Hex-escaped UTF-8 bytes in string literals

```c
printf("  \xe2\x95\x90\xe2\x95\x90\xe2\x95\x90 Interlinear Gloss ... \n");
```

Every box-drawing character in this file's banner output (═, –, ─) is
written as an explicit hexadecimal byte escape (`\xe2\x95\x90` is the
three-byte UTF-8 encoding of `═`, U+2550) rather than typed directly as
the literal character in the source file. `\xNN` inside a C string
literal inserts exactly that one byte, verbatim, into the compiled
string — three consecutive `\x` escapes here produce the exact three
bytes that make up one UTF-8 character. Writing it this way, instead of
saving the source file with the real `═` character embedded directly,
guarantees the *exact* bytes end up in the compiled binary regardless of
what text editor, source-file encoding, or locale setting was active when
the file was last saved — a real, practical portability concern for a
project whose entire subject matter is non-ASCII text. The tradeoff is
readability: `\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90...` repeated dozens of
times is far less legible in the source than the actual `═══...`
character would be — a genuine cost paid here in exchange for
byte-exact certainty.

## 15.5 Try it yourself

Confirm the "minimum, not maximum width" claim directly:

```c
#include <stdio.h>
int main(void) {
    printf("[%-8s][%-8s]\n", "hi", "averylongword");
    return 0;
}
```

Run it and look at the output: `"hi"` gets padded out to 8 characters
inside its brackets, but `"averylongword"` (13 characters) prints in
full, blowing past the requested width entirely, with no truncation and
no error. This is exactly the behavior `kin_print_interlinear` is relying
on, and exactly the limitation worth being able to name if a long
Kinyarwanda compound word ever visibly throws off the interlinear
display's column alignment.

## 15.6 Case study: the real interlinear output for `yaremye`

Putting Sections 15.0 and 15.3 together, here is what `kin_print_interlinear`
actually produces for the verb `yaremye` ("he/she created") from this
book's running example sentence — reconstructed field by field from the
gloss functions shown above:

```
  yaremye            ya       – rem      – ye
                      3SG.HUM  – create   – PRF
```

Tracing where each piece of the second and third lines comes from:

- `ya` is the SP morpheme; `sp_class_gloss(1)` (class 1, the noun class
  `yaremye`'s subject agrees with) returns `"3SG.HUM"` — third person
  singular, human class.
- `rem` is the root; its gloss, `"create"`, comes from `VERB_GLOSS_TABLE`
  (Section 15.1) — the same 200+-row table, the same linear search,
  just looked up for this specific stem.
- `ye` is the final-vowel/aspect morpheme; `fv_gloss(TENSE_PAST_PERF)`
  returns `"PRF"` (perfect/completed aspect) — note there's no separate
  `TM` morpheme shown here at all, because this particular tense
  (Impitakare, the recent past) is marked entirely by the `-ye` ending,
  with no distinct tense-marker syllable the way `-ra-` or `-za-` mark
  other tenses; `tm_gloss` would return the empty-set symbol `∅` for this
  tense if a TM slot were present in the breakdown at all, precisely
  because there is nothing there to gloss.

This three-row, table-driven, `printf`-aligned output is the complete,
literal product of every chapter from Chapter 7 onward: Chapter 7's
lookup-table technique, Chapter 8's tagging that decided this token was a
class-1-agreeing conjugated verb in the past perfect, Chapter 11's
morpheme recovery that split `yaremye` into `ya`/`rem`/`ye` in the first
place, and finally this chapter's gloss tables and aligned `printf` calls
turning that breakdown into the standardized notation a linguist anywhere
in the world could read without ever having seen Kinyarwanda before.

## Key takeaways

- A 200+ row lookup table searched linearly is the same technique from
  Chapter 7 at a larger scale — no new data structure is needed just
  because a table grows.
- Pre-clearing an output buffer (`out[0] = '\0';`) before a search loop
  guarantees a valid result even on failure — a defensive habit this
  file follows that Chapter 14 found missing in `corrector.c`, a useful,
  honest point of comparison between two files in the same project.
- The ternary operator (`cond ? a : b`) is a compact way to express
  "prefer this value, fall back to that one" without a multi-line
  `if`/`else` — recognize the shape on sight.
- `printf`'s `%-Ns` format aligns columns by left-justifying and padding
  with spaces up to a *minimum* width `N` — it never truncates a longer
  string, which means column alignment can visually break for unusually
  long input by design, not by bug.
- Printing an empty string with the same width specifier as a real value
  above it is a simple trick for keeping a second, content-free row
  aligned under the first.
- Multi-byte UTF-8 characters can be written as explicit `\xNN` byte
  escapes in a C string literal to guarantee exact byte output regardless
  of source file encoding, at a real cost to source readability.

## Search YouTube for

- "printf format specifiers width and left justify"
- "ternary operator C explained"
- "UTF-8 encoding bytes explained"
- "hex escape sequences in C strings"

## Coming up in Chapter 16

`analysis.c` is the orchestrator — the one function, `kin_analyze()`,
that calls every stage from every chapter so far, in order, and returns
a complete `SentenceAnalysis`. Chapter 16 covers what it actually means
in C to *return a struct by value* (as opposed to filling one through a
pointer, the pattern every previous chapter has used) — and, as promised
in Chapter 14, settles exactly how and where a fresh `SentenceAnalysis`
gets its initial zero-fill.
