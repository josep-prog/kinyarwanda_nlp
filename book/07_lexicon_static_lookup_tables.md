# Chapter 7 — `lexicon.c`: Static Lookup Tables as a Compiled-In Database

## The scale of this file

`lexicon.c` is 2,672 lines, and almost none of it is "logic" in the
control-flow sense — it's *data*. Counting the real entries by hand
against the source:

| Table              | Entry shape                              | Approx. size |
|---------------------|-------------------------------------------|--------------|
| `NOUN_CLASSES`       | `NounClass` struct (Ch.3)                  | 16 (exact)   |
| `ADJ_STEMS`          | `const char *`, `NULL`-terminated          | 42 (exact)   |
| `PRONOUNS`           | `PronounEntry` struct                      | ~445         |
| `INVARIABLES`        | `InvEntry` struct                          | ~330         |
| `VERB_STEMS`         | `const char *`, `NULL`-terminated          | ~300+        |
| `KNOWN_WORDS`        | `KnownWord` struct                         | ~305         |
| `NOUN_PLURAL_PAIRS`  | `NounPluralPair` struct (Ch.3)              | ~125         |

This chapter is about the two *shapes* every one of these tables takes,
and what kind of C code reads each shape. Once you've internalized the
two shapes, you've understood the entire file — the rest is just more
rows of the same patterns repeated for different words.

## 7.1 Shape 1: a flat list, used as a membership test

```c
static const char *ADJ_STEMS[] = {
    "nini",   /* 1.  large / adult */
    "inshi",  /* 2.  many          */
    "bi",     /* 3.  bad           */
    /* ... 39 more entries ... */
    "meze",   /* 25. resembling / having the condition of */
    NULL
};

bool kin_is_adj_stem(const char *stem) {
    for (int i = 0; ADJ_STEMS[i]; i++)
        if (strcmp(stem, ADJ_STEMS[i]) == 0) return true;
    return false;
}
```

This is Chapter 4's `NULL`-terminated-array idiom again, now holding 42
adjective stems instead of 13 `-fite` forms — same shape, different
domain. The function that reads it, `kin_is_adj_stem`, is a **linear
search**: walk every entry, `strcmp` against the target, return `true` on
the first exact match, `false` if the sentinel is reached with no match.
`VERB_STEMS` and its reader `kin_is_known_verb_stem` are the *exact* same
two-line shape, just for a different word class — once you've read one,
you've read the structural skeleton of both:

```c
bool kin_is_known_verb_stem(const char *stem) {
    for (int i = 0; VERB_STEMS[i]; i++)
        if (strcmp(stem, VERB_STEMS[i]) == 0) return true;
    return false;
}
```

Note this uses `strcmp` (exact, whole-string equality), not
`kin_starts_with` (prefix match) from Chapters 5–6. That's deliberate:
by the time code calls `kin_is_adj_stem()` or `kin_is_known_verb_stem()`,
all the prefix-stripping has already happened elsewhere (Chapter 6) — what
remains is a bare stem that should match one of these table entries
*exactly*, or not at all. Choosing the right comparison function for the
right stage of processing is itself a small design decision worth being
able to name.

**Why linear search, not something faster?** With roughly 300 entries,
worst case, `kin_is_known_verb_stem` does 300 calls to `strcmp`. On any
modern CPU that's a negligible fraction of a millisecond — there is no
real-world performance problem here to solve. A hash table would make
each lookup faster in the abstract, but would add real cost: a hashing
function to write and trust, dynamic bucket sizing (in tension with
Chapter 1's no-`malloc` rule, though a *fixed-size* hash table is
possible), and — critically — code that's harder to read and verify by
hand against the RALC rulebook you're defending against. For a table this
size, looked up at most a few hundred times per sentence analyzed, linear
search is not a missed optimization; it's the right tool, and you should
be ready to defend that judgment directly rather than treating "use a
faster data structure" as automatically correct.

## 7.2 Shape 2: a table with multiple columns, as an array of structs

```c
typedef struct { const char *word; PronounType type; int class; } PronounEntry;
static const PronounEntry PRONOUNS[] = {
    { "nge",  PRON_PERSONAL, 0 },
    /* ... roughly 445 rows total ... */
};
```

```c
typedef struct { const char *word; POS pos; } InvEntry;
static const InvEntry INVARIABLES[] = { /* ~330 rows */ };

typedef struct { const char *stem; int class; } NounStem;
static const NounStem NOUN_STEMS[] = { /* ... */ };

typedef struct { const char *word; int class; char stem[KIN_MAX_STEM]; } KnownWord;
static const KnownWord KNOWN_WORDS[] = {
    { "ijuru",   5, "juru"  },  /* sky / heaven    */
    { "isi",     9, "si"    },  /* earth / world   */
    /* ... ~305 rows ... */
};
```

Every one of these is the array-of-structs pattern from Chapter 3, now
put to its real use: each row is a complete record with several
"columns" (the word's surface form, plus whatever tag or extra data is
needed to use it). This is *why* Chapter 3 introduced array-of-structs
over struct-of-arrays — imagine the alternative: a `PRONOUNS_words[445]`
array, a separate parallel `PRONOUNS_types[445]` array, and a separate
`PRONOUNS_classes[445]` array, where you'd have to trust that index `i`
means the *same* pronoun in all three arrays, forever, by convention
alone. One struct per row, with a name for each field, makes that
correspondence the compiler's problem instead of yours — `PRONOUNS[i].word`,
`PRONOUNS[i].type`, and `PRONOUNS[i].class` can never accidentally drift
out of sync, because they live in the same memory as one unit.

One small inconsistency worth noticing, since being able to spot this
kind of thing is itself a sign of careful reading: `KnownWord.word` is a
`const char *` pointer, but `KnownWord.stem` is a fixed `char[KIN_MAX_STEM]`
array, even though both fields hold a compile-time string literal that's
never mutated at runtime in this table. Chapter 3's rule ("use a fixed
array when the data needs to be owned/copied, a pointer when it's a
permanent literal") would suggest both fields *could* have been declared
the same way. This isn't a bug — both choices compile and work correctly
— but it's a legitimate, honest observation that not every field in a
hand-written 2,672-line file follows its own stated convention with
perfect uniformity. Being able to point this out yourself, calmly, is a
better defense posture than hoping nobody asks.

## 7.3 Pattern matching by length arithmetic — `kin_is_adj_reduplicated`

```c
bool kin_is_adj_reduplicated(const char *sfx, const char *pfx, char *stem_out) {
    size_t pfxlen = strlen(pfx);
    for (int i = 0; ADJ_STEMS[i]; i++) {
        size_t slen = strlen(ADJ_STEMS[i]);
        if (strlen(sfx) != slen + pfxlen + slen) continue;
        if (strncmp(sfx,                 ADJ_STEMS[i], slen)   == 0 &&
            strncmp(sfx + slen,          pfx,          pfxlen) == 0 &&
            strncmp(sfx + slen + pfxlen, ADJ_STEMS[i], slen)   == 0) {
            if (stem_out) strncpy(stem_out, ADJ_STEMS[i], KIN_MAX_STEM - 1);
            return true;
        }
    }
    return false;
}
```

This checks whether `sfx` has the exact shape `stem + pfx + stem`
(reduplication: `muremure` = `mu` + `re` + `mu` + `re`, stripped down to
checking `"rebare"`-style remainders against `"re" + "ba" + "re"`). There
is no regular-expression engine anywhere in this project — C has none
built in, and pulling one in would violate Chapter 1's zero-dependency
goal. Instead, the check is done with plain arithmetic on lengths and
three separate `strncmp` calls at calculated offsets:

1. **Length check first, as a cheap filter**: `strlen(sfx) != slen + pfxlen
   + slen` — if the total length doesn't even add up, skip this candidate
   stem immediately (`continue`) without doing any character comparison
   at all. This mirrors Chapter 5's discipline of putting the cheapest
   check first.
2. **Three positional comparisons**: `sfx` itself, `sfx + slen` (the
   pointer moved forward past where the first stem copy should end), and
   `sfx + slen + pfxlen` (moved forward past the stem and the
   concordance prefix) are each compared against the expected piece. This
   is the exact same "pointer offset into the middle of a string" trick
   `kin_ends_with` used in Chapter 5 — just applied three times in a row
   to validate a more complex structural pattern instead of a single
   suffix.

This is a useful general lesson: you don't need a pattern-matching
library to check structural patterns in C — fixed-length arithmetic plus
a handful of `strncmp` calls at the right offsets covers a surprising
amount of ground, as long as the pattern's *shape* (here, "two copies of
the same substring with something in between") is known ahead of time.

## 7.4 A self-contained macro: `_NV_EW` inside `kin_numerical_value`

```c
int kin_numerical_value(const char *lower) {
    /* ... exact-match checks for icumi, ijana, etc. ... */
    size_t n = strlen(lower);
#define _NV_EW(s) (n >= sizeof(s)-1 && strcmp(lower + n - (sizeof(s)-1), (s)) == 0)
    if (_NV_EW("mwe"))     return 1;
    if (_NV_EW("biri"))    return 2;
    if (_NV_EW("tandatu")) return 6;   /* checked before "tatu" — see source comment */
    if (_NV_EW("tatu"))    return 3;
    if (_NV_EW("tanu"))    return 5;
    /* ... */
#undef _NV_EW
    return 0;
}
```

This is your first look at a **function-like macro** defined and removed
inside a single function's body. `#define _NV_EW(s) ...` looks like a
function call syntactically (`_NV_EW("biri")`), but the preprocessor
handles it before compilation even starts, by pure text substitution —
`_NV_EW("biri")` is replaced, character for character, with
`(n >= sizeof("biri")-1 && strcmp(lower + n - (sizeof("biri")-1), ("biri")) == 0)`,
and *that* expanded text is what the compiler actually sees. Two things
make this worth knowing:

- **`sizeof("biri")` is a compile-time constant**, because `"biri"` is a
  string literal whose length the compiler already knows while compiling
  — `sizeof` of a string literal includes its `'\0'`, so `sizeof("biri")
  - 1` gives exactly 4, the character count, computed with zero runtime
  cost (compare this to calling `strlen("biri")`, which — unless the
  compiler is smart enough to fold it away — would actually walk the
  string at runtime every time). Using `sizeof` on a literal instead of
  `strlen` is a small, real performance habit.
- **`#undef _NV_EW` immediately after** removes the macro definition once
  this function no longer needs it, so the name `_NV_EW` doesn't leak out
  and accidentally collide with anything else later in this very long
  file. This is the macro version of Chapter 4's `static` keyword: both
  are about deliberately limiting how far a name's visibility reaches,
  just at two different levels (macros are preprocessor-level, `static`
  is linker-level).

The ordering comment (`"checked before 'tatu'"`) is the same lesson as
Chapter 6's longest-match discipline, applied to suffixes instead of
prefixes: when one numeral's ending could be confused with another's,
the more specific/longer check has to run first, or the shorter, more
general check would claim the word incorrectly before the right one ever
gets a chance to run.

## 7.5 The `kin_*_name()` functions, revisited: switch vs. array-indexed lookup

Chapter 2 showed `kin_pos_name()` using a `switch` to map an enum value to
a string. Now that you've seen this file's tables, you can see the
alternative it *didn't* choose, and why that's a defensible choice in
both directions:

```c
const char *kin_class_name(int c) {
    if (c < 1 || c > NOUN_CLASS_COUNT) return "N/A";
    return NOUN_CLASSES[c - 1].description;
}
```

`kin_class_name` *does* use direct array indexing — because noun classes
are already stored as full structs in `NOUN_CLASSES[]` with a
`description` field sitting right there; indexing straight into the
existing table is simpler than writing a 16-case `switch` that just
repeats data already on hand. `kin_pos_name()`, by contrast, indexes into
*nothing* — `POS` values don't have a backing table elsewhere, so a
`switch` is the only sensible option. The lesson: reach for a `switch`
when there is no existing table to index into; reach for a direct array
or struct-field lookup when the data is already sitting in a table and
the enum's integer value lines up with a valid index into it (always
bounds-checked first, as `kin_class_name` does with its `if` guard).

## 7.6 Try it yourself

Reproduce one of the table-size counts directly. Since `ADJ_STEMS` is
`static` (private to `lexicon.c`, Chapter 4's lesson on internal linkage),
you can't reach it from outside that file — but you can verify the
counting technique on a table of your own:

```c
#include <stdio.h>
static const char *demo[] = { "umu", "aba", "imi", "ama", NULL };
int main(void) {
    int count = 0;
    for (int i = 0; demo[i]; i++) count++;
    printf("entries: %d\n", count);   /* prints 4 */
    return 0;
}
```

This is exactly the loop shape `kin_is_adj_stem` and
`kin_is_known_verb_stem` both use to walk their tables — only here it
counts instead of comparing.

## Key takeaways

- A "database" in a zero-dependency C project is just `static const`
  arrays — no parsing, no file I/O, no runtime construction; the data is
  already in memory the instant the program starts, baked into the
  binary at compile time.
- Flat `NULL`-terminated `const char *` arrays, searched linearly with
  `strcmp`, are the right tool for simple membership tests against a few
  hundred entries — no faster data structure is justified at this scale.
- Array-of-structs (Chapter 3) is what makes a genuine multi-column table
  possible in C; it keeps each row's fields correctly associated without
  relying on several parallel arrays staying in sync by convention alone.
- Structural pattern matching (like detecting reduplication) can be done
  with plain length arithmetic and offset `strncmp` calls — no regex
  engine required, as long as the pattern's shape is known in advance.
- A function-like macro (`#define NAME(args) ...`) is pure text
  substitution, resolved before compilation; `sizeof` on a string literal
  gives its length at compile time with no runtime cost, unlike `strlen`.
  `#undef` right after use scopes the macro's visibility the way `static`
  scopes a function's.
- Choose a `switch`-based name lookup when there's no existing table to
  index into; choose direct array/struct-field indexing when the data is
  already sitting in a table and the enum value is a valid, bounds-
  checked index into it.

## Search YouTube for

- "C preprocessor macros explained — #define and #undef"
- "sizeof string literal vs strlen in C"
- "linear search vs hash table tradeoffs"
- "array of structs C tutorial"
- "C string pattern matching without regex"

## Coming up in Chapter 8

With the morphology detectors (Ch.5–6) and the lexicon's lookup tables
(Ch.7) both in place, Chapter 8 covers `pos_tagger.c` — the file that
takes a single `Token` and decides, once and for all, which of the five
grammar trees it belongs to. You'll see a 9-step priority chain that
ties everything from this chapter and the last two together, and the
specific C reason this project uses a strict, ordered sequence of checks
instead of anything resembling object-oriented polymorphism.
