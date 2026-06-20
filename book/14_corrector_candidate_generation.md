# Chapter 14 — `corrector.c`: Generating Candidate Corrections

## The smallest file, and why that's a good sign

`corrector.c` is 107 lines — the shortest substantive file in the
project. That's not because the problem it solves is trivial; it's
because of a clean separation of responsibility you should be able to
name directly: **`syntax.c` (Chapter 12) detects that something is
wrong and records it; `corrector.c` only has to explain what the right
answer would have been.** Neither file duplicates the other's job. This
chapter is short, because the file is short — but the design lesson is
worth more than its line count suggests.

## 14.1 One shared struct, passed by pointer, read by one file and written by the next

```c
void kin_suggest_corrections(SentenceAnalysis *sa) {
    for (int e = 0; e < sa->error_count; e++) {
        Error *err = &sa->errors[e];
        if (err->type == ERR_ADJ_AGREEMENT) {
            /* ... */
        }
        if (err->type == ERR_POSS_AGREEMENT) {
            /* ... */
        }
    }
}
```

There is no new data structure here at all. `kin_suggest_corrections`
takes the *exact same* `SentenceAnalysis *sa` that `kin_check_syntax`
(Chapter 12) already populated, and walks its `errors[]` array — purely
**reading** `error_count`, `errors[e].type`, and `errors[e].token_index`,
fields Chapter 12 wrote. The only thing this function actually
*writes* is `err->suggestion` — filling in the one field Chapter 12 left
mostly generic. This is the real payoff of Chapter 3's struct design: by
the time you've built one shared, comprehensive struct that every
pipeline stage can see through a pointer, adding a whole new processing
stage (detect → now also correct) costs nothing in plumbing — no new
parameters to invent, no new struct to define, just another function
that takes the same `SentenceAnalysis *` everyone else already takes.

## 14.2 `build_adj`: a second `static` local table, and a different "is this a vowel" idiom

```c
static void build_adj(int noun_class, const char *stem, char *out, size_t outsz) {
    static const char *RS[] = {
        "", "mu","ba","mu","mi","ri","ma","ki","bi",
        "n","zi","ru","ka","tu","bu","ku","ha"
    };
    if (noun_class < 1 || noun_class > 16) {
        strncpy(out, stem, outsz - 1); return;
    }
    const char *pfx = RS[noun_class];
    char vowels[] = "aeiou";
    bool stem_vowel = (stem[0] && strchr(vowels, stem[0]) != NULL);
    /* ... */
}
```

`static const char *RS[]` declared *inside* the function body is Chapter
8's second-meaning-of-`static` lesson, seen again in a completely
different file: this 17-entry table is built once and persists for the
program's lifetime, rather than being reconstructed every time
`build_adj` runs. The index-0-unused convention from Chapters 7 and 12
appears again too — `RS[noun_class]` works directly with the real class
number, 1 through 16, with no adjustment needed at the call site.

Now look at `strchr(vowels, stem[0]) != NULL`. You met `is_vowel()` back
in Chapter 5 — a direct `c=='a'||c=='e'||c=='i'||c=='o'||c=='u'` chain.
Here, the *identical* question ("is this one character a vowel") is
answered a completely different way: `strchr(haystack, c)` searches a
string for the first occurrence of character `c` and returns a pointer
to it, or `NULL` if it's not present anywhere in the string. Treating the
literal `"aeiou"` as an ad hoc "set of valid characters" and asking
`strchr` to check membership is a legitimate, commonly used C idiom —
arguably *more* convenient to extend (add a letter to the string literal,
no new `||` clause needed) at the negligible cost of a function call
scanning at most 5 characters.

Both idioms are completely correct, and both appear in this project, in
different files, for the same underlying check. That's worth being
honest about directly if you're asked "is this codebase internally
consistent": no, not perfectly — `is_vowel()`'s explicit `||` chain and
`build_adj`'s `strchr` call solve the identical problem two different
ways, and neither is wrong. Recognizing this kind of harmless
inconsistency, and being able to explain *both* idioms confidently rather
than being caught off guard by the discrepancy, is a stronger defense
position than pretending the whole codebase follows one uniform style.

## 14.3 A `switch` used only for its exceptions, with one shared fallback after it

```c
if (stem_vowel) {
    switch (noun_class) {
        case 1: case 3:  snprintf(out, outsz, "mw%s",  stem); return;
        case 4:          snprintf(out, outsz, "my%s",  stem); return;
        case 7:          snprintf(out, outsz, "cy%s",  stem); return;
        /* ... a few more cases, each returning directly ... */
        default: break;
    }
}
snprintf(out, outsz, "%s%s", pfx, stem);
```

This is a subtly different `switch` shape than the ones in Chapters 2
and 11. There, the `switch` *was* the entire decision — every branch
mattered equally, and `default` handled the catch-all case. Here, the
`switch` only exists to catch a handful of **special exceptions** (noun
classes whose concordance prefix changes shape before a vowel-initial
stem); every case that matches one of those exceptions `return`s
immediately with its own special-cased result. `default: break;` does
nothing itself — it simply lets execution fall out of the `switch`
block and continue to the line *physically below it*, which is the
**one shared fallback** every other situation uses: plain concatenation
of the ordinary prefix and the stem. The `switch` here is not "the whole
algorithm" — it's "a short list of early exits in front of the real
default behavior, written once, after the switch, instead of being
repeated as yet another `case`." Recognizing this distinction —
switch-as-the-whole-decision versus switch-as-a-few-early-exits-before-a-
shared-fallback — means you can read any `switch` in any C file and
correctly predict what happens for the cases it doesn't explicitly list.

## 14.4 A worth-flagging gap: relying on zero-initialization instead of an explicit terminator

```c
char sug[KIN_MAX_MSG * 2];
snprintf(sug, sizeof(sug), /* ... bilingual suggestion text ... */);
/* ... */
strncpy(err->suggestion, sug, KIN_MAX_MSG - 1);
```

Compare this against Chapter 11's `set_morph()`, which *always* followed
every `strncpy` with an explicit `dst[n] = '\0';`. Here, after the
`strncpy` into `err->suggestion`, there is **no** explicit terminator
line at all, in either the `ERR_ADJ_AGREEMENT` or `ERR_POSS_AGREEMENT`
branch. Per Chapter 6's lesson, if `sug`'s formatted text is `255` bytes
or longer, `strncpy(err->suggestion, sug, KIN_MAX_MSG - 1)` would copy
exactly 255 bytes and leave `err->suggestion[255]` completely untouched —
whatever byte was already sitting there beforehand.

Whether that's an actual live bug depends on one fact this chapter alone
can't settle: is `err->suggestion` guaranteed to already be `'\0'` at
index 255 *before* this line runs (because the whole `SentenceAnalysis`
was zero-initialized earlier in the pipeline), or could it ever contain
leftover, non-zero data from somewhere else? Chapter 16 (`analysis.c`)
will let us check exactly how `SentenceAnalysis` values get constructed
and whether that zero-fill guarantee genuinely holds throughout this
project. For now, the honest, precise thing to say in your defense is:
this code is relying on an *assumption* (the buffer started zeroed)
rather than *guaranteeing* termination the way `set_morph()` does
explicitly — a real, specific difference in defensive discipline between
two files in the same project, and a legitimate thing to be able to point
to and reason about rather than gloss over.

## 14.5 Case study: two corrections, two branches of `build_adj`

To see why `build_adj` needs both the vowel-initial `switch` (Section
14.3) and the plain fallback, trace two different real corrections through
it side by side:

```
   Input error:    "umuntu kiza"        (class-1 noun, wrong RS="ki")
   noun_class = 1, stem = "iza" (vowel-initial: starts with 'i')
   → stem_vowel is TRUE → switch(1): case 1/3 → "mw" + "iza" → "mwiza"
   Corrected:      "umuntu mwiza"       "a good/beautiful person"

   Input error:     "ikintu ya"         (class-7 noun, wrong RS="ya")
   noun_class = 7, stem = "nini" (consonant-initial: starts with 'n')
   → stem_vowel is FALSE → switch is skipped entirely
   → falls through to: snprintf(out, outsz, "%s%s", pfx, stem)
   → pfx = RS[7] = "ki"  →  "ki" + "nini" → "kinini"
   Corrected:      "ikintu kinini"      "a big thing"
```

The first correction needs the vowel-glide exception (`mu`+`iza` would
naively produce the impossible-to-pronounce `muiza`; Kinyarwanda resolves
adjacent vowels with the `u→w` glide rule from Chapter 9, giving `mwiza`).
The second correction needs no such exception — `ki`+`nini` is already a
perfectly normal consonant-then-consonant concatenation, so the function's
plain fallback line handles it correctly without ever entering the
`switch` at all. Both are genuine corrections `kin_suggest_corrections`
would actually produce for a real `ERR_ADJ_AGREEMENT`; the difference in
which code path each one takes is entirely explained by one fact about the
*stem*'s first letter — exactly the `stem_vowel` check Section 14.2
introduced.

## Key takeaways

- A clean split between "detect and record" (Ch.12) and "explain the
  fix" (this chapter) means the corrector needs no new data structures —
  it reads and extends the exact same shared struct the detector already
  populated.
- `static const` tables can be declared locally inside any function that
  needs them (Ch.8's lesson, reused here in a second file) — not just at
  file scope.
- Testing "is this character in a set" can be done with an explicit `||`
  chain (Ch.5's `is_vowel`) or with `strchr` against a literal string —
  both are valid, and a real codebase may use both in different places.
- A `switch` doesn't have to be the entire decision — it can exist purely
  to special-case a few exceptions, each returning early, while every
  unlisted case falls through to one shared statement written once after
  the `switch` block closes.
- Relying on a buffer having already been zero-initialized, instead of
  explicitly writing a terminator after every `strncpy`, is a real,
  identifiable gap in defensive consistency — worth being able to name
  precisely rather than assuming every line in a large codebase follows
  its own best practices uniformly.

## Search YouTube for

- "separation of concerns software design principle"
- "strchr function in C explained"
- "switch statement fallthrough and default explained"
- "zero initialization guarantees in C structs and arrays"

## Coming up in Chapter 15

`gloss.c` is the second-largest table-driven file in the project (776
lines, with a 200+ entry verb-gloss table). Chapter 15 covers how it
reuses Chapter 7's lookup-table techniques one more time, at a larger
scale, plus the specific string-formatting work needed to produce
Leipzig-style four-line interlinear glosses — aligned columns of text,
built entirely with fixed-width `printf` formatting, no terminal UI
library involved.
