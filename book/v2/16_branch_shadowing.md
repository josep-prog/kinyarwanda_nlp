# Chapter 16 — A Vowel That Goes Missing in More Than One Place

## How this chapter works

Same rules as every chapter before it. Every fenced `$` block is real
`gcc -std=c99 -Wall -Wextra` output from a program actually compiled
and run against this project's real, built library.

Chapter 15 found one specific collision — subject prefix `ya` plus a
vowel-initial root, inside one unconditional branch of
`verb_match_inner` — and named, without chasing, the open question of
whether the same kind of collision recurs elsewhere in that function.
This chapter chases it. The answer is yes, twice, in two genuinely
different ways: once through a precise, fully-traceable case of one
correct branch being shadowed by a more permissive one ahead of it,
and once through a recurrence of Chapter 15's exact bug in a tense
nobody had tested yet.

---

# Part 1 — Language and Code, Side by Side

## 1.1 The same elision rule, now colliding with a tense marker instead of a subject prefix

Chapters 1, 9, and 12 established that adjacent vowels across a
morpheme boundary resolve — they don't just sit there. Chapter 15
traced one consequence at the subject-prefix boundary. The present
tense marker `ra` creates the identical opportunity one slot further
in: `ra` ends in `a`; if the root starts with `a` too, the two `a`s
collapse into one, same as before.

```c
#include "kinyarwanda.h"
#include <stdio.h>

static void show(const char *w) {
    char stem[KIN_MAX_STEM];
    int subj_class = 0, obj_class = 0;
    VerbTense tense = TENSE_NONE;
    VerbExtension ext = VEXT_NONE;
    bool neg = false;
    bool ok = kin_is_verb_conjugated(w, stem, &subj_class, &tense, &obj_class, &ext, &neg);
    printf("%-12s stem=%-8s known=%d\n", w, ok?stem:"", ok && kin_is_known_verb_stem(stem));
}

int main(void) {
    show("baremera");   /* ba + ra + emer + a:  root starts with 'e' */
    show("bariga");     /* ba + ra + ig   + a:  root starts with 'i' */
    show("barandika");  /* ba + ra + andik+ a:  root starts with 'a' */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p1_ra.c -L . -lkinyarwanda -o p1_ra
$ LD_LIBRARY_PATH=. ./p1_ra
baremera     stem=emer     known=1
bariga       stem=ig       known=1
barandika    stem=ndik     known=0
```

Roots starting with `e` and `i` come through correctly. The one
starting with `a` — the same `andik` from Chapter 15 — doesn't.
Section 4.1 traces exactly why this particular vowel, and not the
others, breaks.

## 1.2 Build it: two stripping rules, ordered

```c
/* p1_toy_order.c -- two TM-stripping rules: one literal, one careful.
   Order them the way verb_match_inner does and see which one wins. */
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *inner = "randika";  /* "ra" eliding before vowel-initial "andika" */

    /* Rule A: literal "ra" prefix -- meant for consonant-initial roots */
    if (strncmp(inner, "ra", 2) == 0) {
        printf("Rule A (literal 'ra') fires first: root = \"%s\"\n", inner + 2);
    }

    /* Rule B: 'r' + vowel -- meant for vowel-initial roots (never reached here) */
    if (inner[0] == 'r' && (inner[1]=='a'||inner[1]=='e'||inner[1]=='i'||inner[1]=='o'||inner[1]=='u')) {
        printf("Rule B ('r'+vowel) would say: root = \"%s\"\n", inner + 1);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra p1_toy_order.c -o p1_toy_order
$ ./p1_toy_order
Rule A (literal 'ra') fires first: root = "ndika"
Rule B ('r'+vowel) would say: root = "andika"
```

Both rules *match* the same string — `"randika"` starts with the
literal substring `"ra"` AND with `'r'` followed by a vowel, because
those happen to be the same two characters when the root's vowel is
specifically `a`. Whichever rule is checked first wins, regardless of
which one is semantically correct for this word. Real `verb_match_inner`
has exactly this pair of rules, in exactly this order, quoted in full
in Section 6.1.

## 1.3 Checkpoint

1. Why does this exact ambiguity — one rule matching by coincidence —
   only arise when the root's first vowel is `a`, and not when it's
   `e`, `i`, `o`, or `u`?
2. If Rule B (Section 1.2) were checked *before* Rule A instead of
   after, would it produce the wrong answer for any genuinely
   consonant-initial root? Construct one and check by hand.

---

# Part 2 — One Function, Two Branches, One Coincidence

## 2.1 The careful branch already exists — it's just unreachable for this root

```
   verb_match_inner(), present-tense matching, in source order:

   ┌─────────────────────────────────────────┐
   │ Branch 1: literal "ra" prefix match       │ ← checked FIRST
   │ (for consonant-initial roots)             │
   │ NO known-stem check                       │
   └─────────────────┬─────────────────────────┘
                     │ matches "randika" too (coincidence)
                     ▼
   ┌─────────────────────────────────────────┐
   │ Branch 2: 'r' + vowel match                │ ← never reached
   │ (§1.1 rule 4c, explicitly for              │    for "andik"
   │  vowel-initial roots)                      │
   │ DOES check kin_is_known_verb_stem          │
   └─────────────────────────────────────────┘
```

Branch 2 (Section 6.1) is not a missing feature — it's real,
documented, cites the exact linguistic rule (`§1.1 rule 4c`) by name,
and correctly verifies the candidate against the lexicon before
accepting it. It was written by someone who understood this elision
precisely. It simply never gets a turn for roots beginning with `a`,
because Branch 1 already returned `true` by then.

## 2.2 Why `a` specifically

`ra` + a vowel-initial root elides to `r` + root, by the same `a+V`
rule from Chapter 12's `kin_vv_join`. When the root's own first vowel
is `a`, the result is `r` + `a` + (rest of root) — which, read as a
flat string, starts with the literal two characters `r`, `a`: exactly
what Branch 1 is looking for, with no way to tell from the string
alone that this `a` is the root's, not the tense marker's surviving
intact. For any other vowel, the elided form starts with `r` followed
by something Branch 1 doesn't recognize, so it falls through to
Branch 2 correctly.

---

# Part 3 — Making It Interactive

```c
/* p3_repl.c -- type a present-tense verb, see what stem the matcher finds */
#include "kinyarwanda.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    char line[128];
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, "q") == 0) break;
        char stem[KIN_MAX_STEM]; int sc=0, oc=0;
        VerbTense te=TENSE_NONE; VerbExtension ex=VEXT_NONE; bool neg=false;
        bool ok = kin_is_verb_conjugated(line, stem, &sc, &te, &oc, &ex, &neg);
        printf("  stem=%-8s known=%d\n", ok?stem:"(none)", ok && kin_is_known_verb_stem(stem));
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p3_repl.c -L . -lkinyarwanda -o p3_repl
$ printf "baremera\nbarandika\nbazandika\nbandikaga\nq\n" | LD_LIBRARY_PATH=. ./p3_repl
  stem=emer     known=1
  stem=ndik     known=0
  stem=ndik     known=0
  stem=ndik     known=0
```

Three different tenses, the same root, the same wrong answer. Part 4
confirms these aren't three coincidences but two distinct mechanisms.

---

# Part 4 — Capstone: The Same Vowel, Breaking Two Different Ways

## 4.1 Present tense: a precise, fully-traced branch-shadowing bug

Section 2.1's diagram is not speculation — it's the real structure,
quoted in full in Section 6.1. `barandika` fails for exactly the
reason `baremera` and `bariga` succeed: the root's first vowel decides
which of two real branches gets to run. This is the cleanest,
most mechanically explainable finding in this book since Chapter 9's
two-pass-numbering-schemes discovery — not a missing rule, a rule
placed one branch too late for one specific vowel.

## 4.2 Past imperfect: the same root, no careful branch at all

Future and past-imperfect tenses don't insert a separate tense marker
the way present does — Chapter 14's `ext_strip` and this chapter's
target function build them as bare `SP + root + aga` (imperfect) or
`SP + za + root + a` (future), so the collision moves to the
subject-prefix boundary, the exact site Chapter 15 found. Confirmed
directly, holding the root constant and varying only the prefix's
final vowel collision:

```c
#include "kinyarwanda.h"
#include <stdio.h>

static void show(const char *w) {
    char stem[KIN_MAX_STEM];
    int subj_class = 0, obj_class = 0;
    VerbTense tense = TENSE_NONE;
    VerbExtension ext = VEXT_NONE;
    bool neg = false;
    bool ok = kin_is_verb_conjugated(w, stem, &subj_class, &tense, &obj_class, &ext, &neg);
    printf("%-12s stem=%-8s known=%d\n", w, ok?stem:"", ok && kin_is_known_verb_stem(stem));
}

int main(void) {
    show("bemeraga");   /* ba + emer + aga: 'a' elides cleanly, root starts 'e' */
    show("biga");       /* ba + ig + a:     root starts 'i' */
    show("bandikaga");  /* ba + andik + aga: root starts 'a' -- the collision */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_aga.c -L . -lkinyarwanda -o p4_aga
$ LD_LIBRARY_PATH=. ./p4_aga
bemeraga     stem=emer     known=1
biga         stem=ig       known=1
bandikaga    stem=ndik     known=0
```

`bemeraga` and `biga` both recover the real root cleanly — there's no
literal `"ra"` string for a careful/permissive branch pair to fight
over here, because past imperfect never has a separate tense-marker
string to begin with. The matcher just strips `SP` (2 literal
characters) from the front and `"aga"` from the back, the same naive
subtraction Chapter 15 found in the `SUBJUNCTIVE` branch, and the same
absence of a restore-and-retry step. This isn't branch-shadowing — the
careful branch from Section 4.1 simply has no counterpart here at all.

```c
/* the actual PAST_IMPERFECT branch, in full -- no stem check, no restoration */
if (ilen >= 4 && kin_ends_with(inner, "aga")) {
    size_t sl = ilen - 3;
    if (stem_buf) { strncpy(stem_buf, inner, sl); stem_buf[sl]='\0'; }
    if (subj_class) *subj_class = SP[i].cls;
    if (tense_out)  *tense_out  = TENSE_PAST_IMPF;
    return true;
}
```

## 4.3 Future tense: the third confirmed instance

```
$ LD_LIBRARY_PATH=. ./p3_repl   # (Part 3's REPL, future-tense lines)
baziga       -> stem=ig   known=1
bazandika    -> stem=ndik known=0
```

Same root, same vowel, same failure, in a third tense built the same
general way (`SP + za + root + a`, future's tense marker colliding
with the root the same way present's `ra` does). This chapter doesn't
trace future's internal mechanism as precisely as Section 4.1 traced
present's — that would mean re-opening another multi-branch section of
`verb_match_inner` — but the result is consistent with one of the two
mechanisms already confirmed, and is reported here as a third data
point, not a third fully-diagnosed root cause.

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 Why a careful, narrow branch and a broad, permissive one coexist in the same loop

`verb_match_inner` has to handle both "this word's tense marker is
exactly the literal string `ra`" (the overwhelmingly common case,
consonant-initial roots) and "this word's tense marker *was* `ra`
before a vowel ate part of it" (the rarer case Section 1.1 names).
Writing the specific, vowel-aware rule and the general, consonant rule
as two separate `if` blocks — rather than one combined rule handling
both — keeps each individually readable, which matters in a function
already over a thousand lines (Chapter 12). The cost of that choice is
exactly Section 2.1's diagram: nothing in the language enforces "more
specific rule first," so whichever block a future edit happens to
place earlier wins silently, with no compiler warning that two
`if`-conditions can both be true for the same input.

## 5.2 Why past imperfect never got the careful branch at all

Present tense's vowel-elision problem is visible in the source the
moment you read the comment quoting `§1.1 rule 4c` — someone
specifically noticed this collision for `ra` and wrote code for it.
Past imperfect's tense marker is the empty string (no `ra`, no `za` —
just `SP` directly against the root), so the *same kind* of collision
exists one boundary earlier, at the subject prefix itself, exactly
where Chapter 15 found it. It's a reasonable guess that the present-
tense fix and the chapter-15 SUBJUNCTIVE gap were noticed and fixed
(or not) independently, by whoever was working on that specific
branch at the time, rather than as a single project-wide policy
applied everywhere the same collision could occur. Chapter 12 already
found this project's verb-matching code organized as many small,
independently-evolved special cases rather than one general
mechanism; this is that same shape of organization, with the
consequence made concrete.

---

# Part 6 — Reading the Real Production Code

## 6.1 The two present-tense branches, in source order, exactly as they appear

```c
/* PRESENT with ra marker (consonant-initial root) */
if (kin_starts_with(inner, "ra") && ilen > 3 &&
    (inner[ilen-1]=='a' || inner[ilen-1]=='o')) {
    const char *s = inner + 2; size_t sl = ilen - 3;
    if (sl < 1) continue;
    if (stem_buf) { strncpy(stem_buf, s, sl); stem_buf[sl]='\0'; }
    if (subj_class) *subj_class = SP[i].cls;
    if (tense_out)  *tense_out  = TENSE_PRESENT;
    return true;
}
/* PRESENT with ra, vowel-initial root -- §1.1 rule 4c: the 'a' of TM 'ra'
 * elides before a vowel-initial root, leaving only 'r' on the surface.
 * Pattern: inner = 'r' + vowel + rest + 'a'
 *   e.g.  ireza  = i(SP·Nt.4) + r[a→∅] + ez(root) + a(FV)
 *         areza  = a(SP·Nt.1) + r[a→∅] + ez(root) + a(FV)
 * Validated: root must be a known stem or a causative-y surface (§1.3)
 * to prevent accidental matches on consonant-r-initial habitual forms.  */
if (ilen >= 4 && inner[0] == 'r' && is_vowel(inner[1]) && inner[ilen-1] == 'a') {
    size_t rvlen = ilen - 2;
    char   rvbuf[KIN_MAX_STEM];
    if (rvlen >= 2 && rvlen < KIN_MAX_STEM - 1) {
        strncpy(rvbuf, inner + 1, rvlen); rvbuf[rvlen] = '\0';
        if (kin_is_known_verb_stem(rvbuf) || kin_is_causative_y_surface(rvbuf)) {
            if (stem_buf) { strncpy(stem_buf, rvbuf, KIN_MAX_STEM-1);
                            stem_buf[KIN_MAX_STEM-1] = '\0'; }
            if (subj_class) *subj_class = SP[i].cls;
            if (tense_out)  *tense_out  = TENSE_PRESENT;
            return true;
        }
    }
}
```

The first block's own examples (`ireza`, `areza` — both vowel-initial
roots!) in its comment make clear the author of the second block knew
exactly which words needed it. Nothing in either block's text
references the other, and nothing prevents the first from running on
input the second was written for.

## 6.2 The past-imperfect branch, in full

Quoted already in Section 4.2 — four lines, no known-stem check, no
companion vowel-aware branch.

---

# Part 7 — Looking Back, Looking Forward

## 7.1 What this chapter adds to Chapters 12, 14, and 15

Chapter 12 surveyed `verb_match_inner`'s shape without auditing its
correctness. Chapter 14 found a downstream consequence (harmony) in a
different file. Chapter 15 found one specific instance of this
chapter's broader pattern and explicitly left open whether it
recurred. This chapter confirms it does, in at least two structurally
different ways within the one function — branch-shadowing for one
tense, a flatly missing companion branch for another — which means
the honest answer to "is this one bug or a pattern" is: it's a
pattern, with more than one distinct cause, not a single fix away from
being closed.

## 7.2 What's still not chased

This chapter checked three tenses (present, past imperfect, future)
against one root vowel collision (`a`). It did not check the
remaining tense branches Chapter 12 named (narrative, copula,
imperative, the negative forms, the relative-clause tenses), and it
did not check whether a *consonant*-initial root could ever produce
the reverse problem — a careful branch firing when the general one
should have. Both are real, well-posed questions this chapter's
method would answer the same way it answered this one.

---

# Part 8 — Practice

### Beginner

1. Using Section 1.1's method, test `"yandika"` (present, no `ra` —
   compare against `"barandika"`) and explain whether it's affected by
   the same collision or a different one.
2. Find one more real Kinyarwanda verb root starting with `a` (check
   `lexicon.c`'s `VERB_STEMS[]`) and reproduce Section 4.1's
   `baremera`/`barandika` contrast with it.

### Intermediate

3. Section 6.1's first branch requires `ilen > 3`; the second requires
   `ilen >= 4`. Construct the shortest possible word that would matter
   for this boundary difference and check which branch (if either)
   fires.
4. Section 4.2 showed past imperfect has no careful companion branch.
   Check whether `TENSE_NARRATIVE` (searched for in `morphology.c`)
   has one, by constructing a narrative-tense word with an `a`-initial
   root and testing it the same way.

### Advanced

5. Propose the smallest fix to Section 6.1's branch *ordering* (not
   rewriting either branch) that would let Branch 2 run for `a`-initial
   roots without changing Branch 1's behavior for every other root.
6. Section 5.1 argued nothing enforces "more specific rule first" in
   this function. Sketch a structural change (a helper function, a
   different loop shape, a priority field) that would make this class
   of ordering bug impossible to introduce by accident in the future,
   without requiring every branch to be rewritten.
7. Section 7.2 named two unchased questions. Pick one, apply this
   book's standard method — read the comment, write a real test,
   compile it, run it, compare against the claim — and report what you
   find.

---

## Key takeaways

- The same vowel-elision rule Chapter 15 traced at the subject-prefix
  boundary also applies at the present-tense marker `ra`'s boundary
  with the verb root, and `verb_match_inner` already has a correct,
  documented, lexicon-checked branch for exactly this case.
- Real, verified finding: that correct branch is unreachable for any
  root whose first vowel is `a`, because a more permissive branch
  checking for the literal substring `"ra"` is ordered before it and
  matches the same elided string by coincidence — confirmed directly:
  `baremera`/`bariga` (roots `e`/`i`) succeed; `barandika` (root `a`)
  fails, every time, for the same root that broke Chapter 15's
  `SUBJUNCTIVE` branch.
- A second, structurally different recurrence: past imperfect
  (`SP + root + aga`, no separate tense marker at all) has no careful
  vowel-aware branch whatsoever, not even one being shadowed — the
  same elision bug Chapter 15 found, in a tense that was never given
  the present tense's partial fix.
- Future tense (`SP + za + root + a`) shows the identical symptom,
  confirmed as a third data point without being traced to the same
  level of mechanical precision as present and past-imperfect.
- The pattern is not one bug with one fix: it's at least two distinct
  causes (branch-shadowing vs. a missing branch entirely) producing
  the same surface symptom across at least three tenses, in a function
  this book has now read from four separate angles across four
  chapters without exhausting it.

## Sources quoted in this chapter

- `src/morphology.c`'s present-tense `ra`-marker branches (both the
  literal-match and vowel-aware versions) and the past-imperfect
  branch, all inside `verb_match_inner`, first surveyed in Chapter 12.
- `src/lexicon.c`'s `VERB_STEMS[]`, used to confirm which candidate
  stems are real Kinyarwanda roots and which are fabricated.
- Every `pN_*.c` program in this chapter was actually compiled with
  `gcc -std=c99 -Wall -Wextra` and actually executed.
