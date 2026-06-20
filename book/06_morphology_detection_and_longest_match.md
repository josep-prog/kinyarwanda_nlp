# Chapter 6 — `morphology.c` (II): Detection & the Longest-Match Algorithm

## Where we are

Chapter 5 covered the small, general-purpose utilities at the top of
`morphology.c`. This chapter covers the actual reason the file exists:
`kin_detect_noun_class()`, `kin_strip_noun_prefix()`, and the verb-side
tables (`OM_TABLE`) and helpers (`om_strip()`). This is also where you'll
meet this project's one and only use of **recursion**.

## 6.0 Why noun-class detection is a string-prefix problem at all

Section 3.1.1 introduced the noun formula `D + RT + C` and the idea of 16
noun classes. Before reading the code, it's worth seeing *why* "which class
is this noun?" reduces, almost entirely, to "which prefix does this string
start with?" Each of the 16 classes has its own characteristic `D+RT`
combination, and — critically — these combinations are largely distinct
strings, which is exactly what makes prefix matching a viable strategy in
the first place:

```
 nt.1  umu-   (umuntu, "person")        nt.9   in-/i(n)-  (inka, "cow")
 nt.2  aba-   (abantu, "people")        nt.10  in-/zi(n)- (inka pl, "cows")
 nt.3  umu-   (umuti, "tree")           nt.11  uru-       (uruzi, "river")
 nt.4  imi-   (imiti, "trees")          nt.12  aka-       (akana, "small child")
 nt.5  i(ri)- (ijuru, "sky")            nt.13  utu-       (utwana, "small children")
 nt.6  ama-   (amata, "milk")           nt.14  ubu-       (ubuzima, "life")
 nt.7  iki-   (ikintu, "thing")         nt.15  uku-       (kugenda, "to walk" — infinitive!)
 nt.8  ibi-   (ibintu, "things")        nt.16  aha-       (ahantu, "place")
```

(Full reference with every concordance form is Chapter 7's table; this is
just enough to see the shape of the problem.) Two things make this *harder*
than a flat lookup table, and both are exactly what `kin_detect_noun_class`
spends its 100+ lines handling:

1. **Classes 1 and 3 share the identical prefix `umu-`.** There is no
   string-level way to tell "person" (class 1) apart from "tree" (class 3)
   by prefix alone — that ambiguity is real, not a flaw in the code, and
   the disambiguation has to come from elsewhere (often the verb agreement
   prefix the noun later triggers, or simply the dictionary identity of the
   stem — `-ntu` is class 1, `-ti` is class 3, looked up in Chapter 7's
   stem tables).
2. **The same surface bytes can belong to a noun *or* something else
   entirely**, once you account for casual speech, poetry, or fast typing
   dropping the leading `D` vowel. `"bantu"` missing its `a-` could still
   plausibly be intended as `"abantu"`. This is exactly the kind of
   ambiguity Sections 6.1–6.2 below show the code resolving with ordered
   checks and guard conditions — not because the programmer wanted extra
   complexity, but because the language itself is genuinely ambiguous at
   the surface-string level, and a rule-based system has to make an
   explicit, defensible choice every time that happens.

## 6.1 The longest-match algorithm: order is the entire algorithm

```c
int kin_detect_noun_class(const char *w) {
    size_t wlen = strlen(w);
    if (wlen < 3) return 0;

    /* Ordered by prefix length (longest first) to avoid partial matches */
    if (kin_starts_with(w, "umu") && wlen > 4) return 1;  /* Nt.1/3 */
    if (kin_starts_with(w, "aba") && wlen > 4) return 2;  /* Nt.2   */
    if (kin_starts_with(w, "imi") && wlen > 4) return 4;  /* Nt.4   */
    /* ... 19 more full-prefix checks ... */
```

This function is, structurally, just a long chain of `if` statements,
each one tested in sequence, the **first match wins**. There's no
sophisticated data structure here — and that's the point worth
understanding. The comment "ordered by prefix length (longest first)"
names the entire algorithm: a word like `"umuti"` could, in principle,
match several candidate noun-class prefixes if you only looked at single
characters — but it should match the *longest* one that actually fits,
because the longer prefix is the more specific, more certain
identification. By placing every 3-character prefix check (`"umu"`,
`"aba"`, `"imi"`, ...) **before** any 2-character or 1-character fallback
check further down the function, a word that matches a long prefix gets
claimed by that check and `return`s immediately — the shorter, vaguer
checks later in the function never even run for that word, because C's
`if` chain stops at the first `return`.

Scroll down in the real file and you'll find exactly this ordering
principle being followed deliberately, with the *shorter* "dropped
D-vowel" prefixes (where the leading vowel has been elided in casual
speech or fast writing) checked only *after* all the full, canonical
prefixes:

```c
/* MUST come AFTER "umu" check to avoid override of full forms. */
if (kin_starts_with(w, "mu") && wlen > 4 && !is_vowel(w[2])) return 1;
```

The comment is explicit about *why* the order matters: if this 2-letter
check ran *before* the 3-letter `"umu"` check, a word like `"umuti"` would
incorrectly match here first (since `"umuti"` does technically start with
`"mu"` once you're inside the string — no, wait, it starts with `"umu"`,
not `"mu"` — but a word that starts with `"mu"` after the D-vowel was
*already* dropped, like `"musozi"`, needs this check to ever fire at
all). The deeper lesson: **in a priority chain like this, sequence is not
a cosmetic detail — it is the disambiguation logic itself.** Reordering
two `if` statements here can silently change which class a word is
assigned, which is exactly the kind of subtle bug you should be ready to
explain if asked "what would happen if you swapped lines X and Y here?"

## 6.2 Guard conditions and short-circuit evaluation

Look at how almost every check pairs the prefix test with extra
conditions:

```c
if (kin_starts_with(w, "ba") && wlen > 5 && !is_vowel(w[2])
    && w[wlen-1] != 'a' && w[wlen-1] != 'e') return 2;
```

C's `&&` is **short-circuiting**: it evaluates left to right and stops the
instant one operand is false, never evaluating the rest. This matters
here for two reasons. First, *safety*: `w[2]` is only looked at after
`kin_starts_with(w, "ba")` has already confirmed the string is at least 2
characters long, and `wlen > 5` confirms there's a character at index 2
to look at at all — if `kin_starts_with` were false, `&&` would stop right
there and `w[2]` would never be touched, avoiding a potential out-of-
bounds read on a very short string. Second, *cost*: the cheapest, most
selective check is placed first, so for most words (which don't start
with `"ba"` at all) the rest of the expensive-looking condition never even
runs.

But the real story here is *why* these extra guards exist at all. The
comment above this exact line explains it:

```
* Pattern: agent/plural nouns (abahinzi, abarimu, abapfumu) appear as
* "bahinzi", "barimu", "bapfumu" with 'a' dropped.
* Guard: NOT ending in 'a' (would conflict with verbs bakora/bagenda)
```

A word starting with `"ba"` could be a noun with its leading vowel dropped
(`bahinzi` = `abahinzi`, "farmers") **or** a conjugated verb whose subject
prefix happens to also be `"ba"` (`bakora` = "they work", `ba-` + `-kor-`
+ `-a`). Both are real, valid words. The guard `w[wlen-1] != 'a' &&
w[wlen-1] != 'e'` is a **heuristic**: most present/habitual verb forms end
in `-a`, most subjunctive forms end in `-e`, so excluding words ending in
those letters filters out *most* verb collisions, at the cost of
occasionally being wrong (there are real nouns ending in `-a`/`-e` that
this rule will misclassify). This is worth being honest about in your
defense: this is not a perfect, mathematically complete parser — it is a
rule-based heuristic system, and several of its rules are explicitly
documented trade-offs between precision and coverage, not absolute
guarantees. That honesty is more defensible than pretending otherwise.

## 6.3 Recursion: the one place this project calls itself

```c
if (kin_starts_with(w, "nka") && wlen > 6) {
    const char *sub = w + 3;
    size_t slen = wlen - 3;
    int sub_cls = kin_detect_noun_class(sub);   /* ← calling itself */
    if (sub_cls > 0) return sub_cls;
    /* ... relaxed fallback checks on sub ... */
}
```

`"nka"` ("like"/"as") sometimes appears fused directly onto the noun that
follows it in real text (`"nkabantu"` = `"nka"` + `"abantu"`, "like
people"). Rather than writing a second, near-duplicate copy of the entire
noun-detection logic to handle "noun, but preceded by `nka`," this
function does something much simpler: it strips off the `"nka"` prefix
(`sub = w + 3` — a pointer three bytes further into the same string, no
copying) and calls **itself** on what's left.

This is **recursion**: a function invoking itself to solve a smaller
version of the same problem. Two things must always be true for recursion
to be safe, and both hold here:

1. **A base case that doesn't recurse.** `if (wlen < 3) return 0;` at the
   very top of the function. Every recursive call must eventually reach
   this, or the function would call itself forever (in practice,
   crashing with a stack overflow once it exhausts the call stack).
2. **Guaranteed progress toward the base case.** Every recursive call
   here is made on `sub = w + 3` — a string that is **always exactly 3
   bytes shorter** than the one passed in. Since the string can only ever
   get shorter, and the base case triggers once it's shorter than 3
   bytes, the recursion is mathematically guaranteed to terminate. There
   is no input for which this could loop forever.

If you're asked in your defense "does this project use recursion, and is
it safe?" — this is your complete, precise answer: yes, exactly once, and
it's provably safe because each call strictly shrinks its input toward a
guaranteed base case.

`kin_strip_noun_prefix()` (the function that, beyond just detecting the
class number, also extracts the bare stem) repeats this exact recursive
"nka" pattern, calling **itself**:

```c
char sub_stem[KIN_MAX_STEM];
int  sub_cls = 0;
if (kin_strip_noun_prefix(sub, sub_stem, &sub_cls) && sub_cls > 0) {
    if (class_out) *class_out = sub_cls;
    strncpy(stem_out, sub_stem, KIN_MAX_STEM - 1);
    stem_out[KIN_MAX_STEM - 1] = '\0';
    return true;
}
```

## 6.4 `strncpy`'s missing-null-terminator trap

You'll see this exact two-line shape dozens of times across this project:

```c
strncpy(stem_out, sub_stem, KIN_MAX_STEM - 1);
stem_out[KIN_MAX_STEM - 1] = '\0';
```

`strncpy(dst, src, n)` looks like a "safe" version of `strcpy` because it
takes a maximum length — but it has a sharp, frequently-misunderstood
edge case: **if `src` is `n` characters long or longer, `strncpy` does
NOT null-terminate `dst` at all.** It copies exactly `n` bytes and stops,
full stop — no terminator is appended for you, unlike every other
"copy a string" function you might expect. If the code stopped after the
`strncpy` call, a `src` exactly as long as the limit would leave `dst` as
a non-terminated, dangerous buffer — every later `strlen`/`strcmp`/`printf
("%s")` call on it would read past the end into whatever memory happens
to follow. That's exactly why the second line is never omitted: writing
`'\0'` explicitly at `dst[n]` (here, `stem_out[KIN_MAX_STEM - 1]`)
guarantees termination regardless of how long `src` was. This is one of
C's most commonly cited "gotchas" in interviews and code review — you
should be able to explain it without hesitation.

## 6.5 `kin_strip_noun_prefix`: `switch` on the tag, `if`/`else if` on the variant

Once the recursive "nka" special case is out of the way, the function
calls `kin_detect_noun_class()` to get a class number, then needs to
figure out exactly *how many bytes* of surface prefix to skip to reach
the bare stem — and that differs depending on which of several surface
spelling variants matched:

```c
switch (cls) {
    case 1: case 3:
        if (kin_starts_with(word, "umw"))      stem_start = word + 3;
        else if (kin_starts_with(word, "umu")) stem_start = word + 3;
        else if (kin_starts_with(word, "mw"))  stem_start = word + 2;
        else if (kin_starts_with(word, "wu"))  stem_start = word + 1;
        else if (kin_starts_with(word, "mu"))  stem_start = word + 2;
        break;
    case 2:
        if (kin_starts_with(word,"aba"))       stem_start = word + 3;
        else if (kin_starts_with(word,"ab") && is_vowel(word[2]))
                                               stem_start = word + 1;
        else if (kin_starts_with(word,"ba"))   stem_start = word + 2;
        break;
    /* ... one case block per noun class ... */
}
```

This nests two different dispatch styles you've now learned, one inside
the other: the **outer** `switch (cls)` dispatches on the *tag* (which
noun class — Chapter 2's enum-and-tag pattern, here using a plain `int`
instead of an enum, since class numbers 1–16 are looked up dynamically
rather than named individually). The **inner** `if`/`else if` chain
within each case then re-derives *which exact surface spelling* of that
class's prefix is present, because — as Chapter 1's linguistic notes
describe — the same class can surface several different ways depending on
which phonological rule fired (`"umu"` vs. the contracted `"mu"`, etc.).
`stem_start` ends up as a pointer somewhere inside the original `word`
string — no copying happens here either; it's just a pointer moved
forward past however many prefix bytes were identified.

### 6.5.1 What an "object marker" actually is, before the table

`OM_TABLE` stands for **object-marker table**, and it's worth pausing to
say what that means in plain terms, since the rest of this section uses
the abbreviation constantly. Kinyarwanda can express a verb's *object*
(the thing the action is done to) two ways: as a separate noun after the
verb (*Yabonye umuntu* — "He/she saw a person"), or **fused directly inside
the verb itself**, as an infixed pronoun-like marker sitting between the
tense marker and the stem:

```
   Yarabonye        "He/she saw [something]"          (no object marker)
   Ya-mu-bonye      "He/she saw him/her"               mu = OM, class 1
   Ya-bi-bonye      "He/she saw it/them"               bi = OM, class 8
```

This `mu`/`bi`/`ki`/... infix is exactly the `OM` slot from Section
3.3.1's ten-symbol vocabulary (`SP+TM+OM+root+EXT+FV`) — and you can see
immediately why it's ambiguous with a *noun-class prefix*: the object
marker for class 1 (`mu`) is spelled identically to class 1's noun prefix
fragment. `OM_TABLE` exists specifically to let the code recognize these
infixed object markers inside a conjugated verb form, using the same
"prefix-matching against a small table" strategy as noun-class detection
— a second application of the same underlying idea, applied to a different
slot in a different word-type's formula.

## 6.6 `OM_TABLE`: a third sentinel-terminated-list idiom

```c
typedef struct { const char *om; const char *om_v; int cls; } OmEntry;

static const OmEntry OM_TABLE[] = {
    { "mu",  "mw",  1  },
    { "ki",  "cy",  7  },
    { "bi",  "by",  8  },
    /* ... */
    { "ha",  "ha",  16 },
    { NULL, NULL, 0 }
};
```

Chapter 4 showed a `NULL`-terminated array of plain string pointers
(`FITE_FORMS`). This is the same trick, one level deeper: an array of
**structs**, where the *last row* is a sentinel whose `om` field is
`NULL` — a value no real entry would ever have. Walking the table doesn't
need a separate count anywhere:

```c
for (int i = 0; OM_TABLE[i].om; i++) {
    /* OM_TABLE[i].om is non-NULL (truthy) for every real row */
}
```

This is the *third* distinct "list of unknown length" idiom you've now
seen in this project (capacity+count from Chapter 3; `NULL`-terminated
pointer array from Chapter 4; now a `NULL`-terminated array of structs).
All three solve the identical underlying problem; which one gets used
depends on what's most natural for the element type at hand — a struct
field can serve as its own sentinel just as well as a bare pointer can.

## 6.7 `om_strip`: table lookup, plus a second-layer disambiguation guard

```c
static bool om_strip(const char *stem, int *om_cls_out,
                     char *bare_out, size_t bare_sz) {
    size_t slen = strlen(stem);
    for (int i = 0; OM_TABLE[i].om; i++) {
        size_t olen = strlen(OM_TABLE[i].om);
        if (slen > olen && kin_starts_with(stem, OM_TABLE[i].om)) {
            if (om_cls_out) *om_cls_out = OM_TABLE[i].cls;
            if (bare_out) { strncpy(bare_out, stem + olen, bare_sz - 1);
                            bare_out[bare_sz - 1] = '\0'; }
            return true;
        }
        size_t ovlen = strlen(OM_TABLE[i].om_v);
        if (slen > ovlen && kin_starts_with(stem, OM_TABLE[i].om_v)) {
            if (ovlen == 1 && !is_vowel(stem[1])) continue;
            /* ... accept this match ... */
            return true;
        }
    }
    return false;
}
```

This is a **linear search**: walk the table from the start, test each
row, stop at the first match (`return true` inside the loop). With only
~15 rows, a linear scan is plenty fast — there's no need for anything
fancier like a hash table or binary search for a table this small (you'll
see this exact judgment call again in Chapter 7 with larger tables, where
it's worth asking explicitly "why not something faster?").

The `continue` inside the second `if` block is worth pausing on: even
after `kin_starts_with(stem, OM_TABLE[i].om_v)` has confirmed a textual
match, the code can still decide "no, this match doesn't count" and move
on to the *next* table row, rather than accepting the match. `continue`
in a `for` loop skips straight to the next iteration — it's the tool for
exactly this situation: "this candidate looked right at first glance, but
failed a deeper check, so don't accept it, but don't give up on the whole
loop either." This is a second illustration of something Chapter 6.2
already showed with the noun-class guards: matching a prefix textually is
necessary but not always sufficient — extra, hand-coded disambiguation
logic is frequently layered on top of a simple lookup.

## 6.8 Case study: why `"bagenda"` and `"bahinzi"` must be told apart

Section 6.2 quoted the guard against verb collisions in the abstract; here
is the full disambiguation worked end to end on two real words that begin
with the identical two bytes, `ba`:

```
   "bagenda"  →  intended reading: VERB, not noun
                 ba-gend-a  =  SP(class 2, "they") + root "-gend-" + FV "-a"
                 English: "they go/walk"
                 Last letter is 'a'  →  guard `w[wlen-1] != 'a'` is FALSE
                 → kin_detect_noun_class() correctly REFUSES to claim this
                   as a class-2 noun with dropped 'a-'; it falls through.

   "bahinzi"  →  intended reading: NOUN, class 2, dropped D-vowel
                 (a)-ba-hinzi  =  D(elided "a") + RT "ba" + C "hinzi"
                 English: "farmers"
                 Last letter is 'i'  →  guard `w[wlen-1] != 'a' && != 'e'`
                 is TRUE for both checks → kin_detect_noun_class() accepts
                 this as class 2, returns 2.
```

Both words are real, valid Kinyarwanda; both share the exact same first two
letters; the *only* signal `kin_detect_noun_class()` has to tell them apart
— since it only ever looks at one bare word string, with no surrounding
sentence context available at this stage of the pipeline — is the final
letter. That single-character heuristic is genuinely fallible (a noun
ending in `-a` would be wrongly rejected here), and the project accepts
that cost deliberately rather than building a far more expensive
context-aware disambiguator for what is, in practice, a small minority of
cases. If asked to defend this exact trade-off, this worked pair is the
concrete evidence: it shows the heuristic working correctly on two genuine
near-collisions, and names exactly the shape of input that would break it.

## Key takeaways

- A long, manually-ordered `if`/`return` chain *is* a longest-match
  algorithm when longer, more specific patterns are deliberately checked
  before shorter, vaguer ones — order is the algorithm, not an
  implementation detail.
- `&&` short-circuits left to right; put cheap, safety-establishing
  checks (like a length check) before any check that indexes into the
  string, so out-of-bounds reads can never happen.
- Guard conditions appended to a prefix check are usually disambiguation
  heuristics for genuine ambiguity in the language (the same prefix bytes
  can start either a noun or a verb) — a documented trade-off, not a bug.
- Recursion needs a base case and a guarantee that every recursive call
  strictly shrinks the problem; this project's only recursive functions
  (`kin_detect_noun_class`, `kin_strip_noun_prefix`, both for the `"nka"`
  prefix) satisfy both by always recursing on a string exactly 3 bytes
  shorter.
- `strncpy` does **not** null-terminate its destination if the source is
  exactly as long as (or longer than) the given limit — always follow it
  with an explicit `dst[n] = '\0';`.
- A `NULL`-terminated array of *structs* (one designated sentinel field)
  is a third variant of the "list without a separate length" idiom,
  alongside capacity+count and a `NULL`-terminated array of plain
  pointers.
- `continue` inside a loop lets you reject a candidate that matched on
  the surface but fails a deeper check, without abandoning the search
  entirely.

## Search YouTube for

- "recursion in C explained with base case"
- "strncpy gotchas and null termination"
- "linear search algorithm C"
- "short circuit evaluation && || explained"
- "continue vs break in C loops"

## Coming up in Chapter 7

`OM_TABLE` was a small taste of a static lookup table. Chapter 7 covers
`lexicon.c` — the file that holds the project's *real* compiled-in
database: `NOUN_CLASSES[16]`, `ADJ_STEMS[27]`, `PRONOUNS[]`,
`INVARIABLES[]`, and a 200+ entry verb-stem table. You'll see how C
represents "a database" with nothing but `static const` arrays, how
`sizeof(arr)/sizeof(arr[0])` computes a table's length without hardcoding
it, and when a linear search across a few dozen rows is genuinely fine
versus when it would start to matter.
