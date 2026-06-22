# Chapter 18 — The Display Layer: When the Explanation Contradicts the Line Above It

## How this chapter works

Same rules as every chapter before it. Every fenced `$` block is real
output from the actual built CLI or from a program actually compiled
with `gcc -std=c99 -Wall -Wextra` and run against this project's real
library.

Every chapter in this book has quoted the CLI's `Uturemajambo` /
`Gusubiza` / `✓` blocks dozens of times without ever reading the code
that produces them. This chapter opens `analysis.c` — specifically the
568-line `kin_print_analysis` function, the single largest function in
the file — and finds a real, self-contradictory explanation: two
lines of the *same* analysis block disagreeing about which subject
prefix the word being analyzed actually has.

---

# Part 1 — Language and Code, Side by Side

## 1.1 A consonant that fortifies after a nasal, and the note that explains it

Kinyarwanda's first-person-singular subject prefix is the nasal `n`.
When `n` sits directly before the root `h` (from `guha`, "to give"),
two phonological rules fire in sequence: `n` assimilates to `m` before
`h` (the same nasal-place rule from Chapters 9 and 13), and then `h`
itself fortifies to `p` after that labial nasal. The result: 1sg
`guhesha` ("to cause to give," causative of `guha`) surfaces not as
`*nheshejwe` but as `mpeshejwe`. This project's CLI carries a
hand-written note explaining exactly this, and shows it correctly for
the word it was written for:

```
$ ./kinyarwanda_nlp -s "Mpeshejwe neza."
 Mpeshejwe            Inshinga-conjugated (Verb conj.)
  └─ Uturemajambo (Morphemes): n(SP) + ∅(TM) + h(root) + esh(CAUS) + ejw(EXT) + e(FV)
  └─ Imbundo (Citation verb): guhesha  (igicumbi -hesh-)
  └─ Icyitonderwa (Inkomoko/Derivation): guha [to give] + -esh- (causative) =
       guhesha [to make/cause to give]; ... (1) n+h→mh: SP ya 1sg 'n' igenwa
       mbere ya 'h' (n→m /_h); (2) mh→mp: 'h' ihinduka 'p' inyuma ya m
       bilabiale (h→p /_m) ...
```

The morpheme line says `n(SP)`. The note says the rule fires because
the SP is `n`. They agree — for this word. Section 4.1 shows a
different word where they don't.

## 1.2 Build it: a note keyed by stem, not by context

```c
/* p1_toy_citation.c -- a citation note table keyed only by stem string */
#include <stdio.h>
#include <string.h>

typedef struct { const char *stem; const char *note; } Citation;

static const Citation TABLE[] = {
    { "pesh", "this form arises because SP 'n' assimilates before 'h'" },
    { NULL, NULL }
};

static void show_note(const char *stem, const char *actual_sp) {
    for (int i = 0; TABLE[i].stem; i++) {
        if (strcmp(stem, TABLE[i].stem) == 0) {
            printf("stem=%-6s actual_sp=%-4s note: %s\n", stem, actual_sp, TABLE[i].note);
            return;
        }
    }
}

int main(void) {
    show_note("pesh", "n");    /* the case the note was written for */
    show_note("pesh", "ya");   /* a different subject prefix entirely */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra p1_toy_citation.c -o p1_toy_citation
$ ./p1_toy_citation
stem=pesh  actual_sp=n    note: this form arises because SP 'n' assimilates before 'h'
stem=pesh  actual_sp=ya   note: this form arises because SP 'n' assimilates before 'h'
```

The toy prints the identical, SP-specific claim regardless of what the
actual subject prefix is, because the lookup key is the stem alone.
The real `SUPPLETIVE_CITATIONS[]` table (Section 6.1) has exactly this
shape.

## 1.3 Checkpoint

1. Why does this risk only apply to a citation note that makes a claim
   *about* the subject prefix, and not to Section 6.3's `"sig"`
   homograph note (also keyed by stem alone)?
2. If `analysis.c` checked `t->verb_tense` or the stored `SP` morpheme
   before printing Section 1.1's note, would that fully close the gap
   — or would it just move the same risk to a different field?

---

# Part 2 — One Citation Table, Three Places Verb Display Logic Lives

## 2.1 Where this code actually sits

```
   analysis.c's verb display logic, by function:

   print_verb_morphemes()        126 lines   (line 546)
   print_verb_reconstruction()   205 lines   (line 717)
   kin_print_analysis()          568 lines   (line 1406 — the file's
                                               largest single function)
                                               │
                                               └─ SUPPLETIVE_CITATIONS[]
                                                  and HOMOGRAPH_ROOTS[]
                                                  live INLINE here, not
                                                  in either dedicated
                                                  verb-printing helper
```

Two functions exist with "verb" in their purpose and "morphemes" or
"reconstruction" in their name. The citation-note logic this chapter
is about isn't in either of them — it's written directly inside the
top-level dispatcher that calls both, alongside the rest of
`kin_print_analysis`'s per-token branching.

## 2.2 Why that placement made the gap easier to introduce

A lookup table inside a small, single-purpose helper invites the
question "does this generalize correctly for every input the helper
receives?" A lookup table embedded in a 568-line dispatcher, sitting
among dozens of other special-case `if` blocks for different
irregularities, invites a narrower question: "does this look right for
the one word I tested it against?" `SUPPLETIVE_CITATIONS[]`'s `"pesh"`
entry reads as carefully reasoned (Section 6.1 quotes it in full,
complete with the two-step phonological derivation) — it just was
apparently checked against `mpeshejwe` alone, not against whatever
else this project's own verb matcher is willing to call `"pesh"`.

---

# Part 3 — Making It Interactive

```c
/* p3_repl.c -- type a verb, see its stem and SP exactly as detected */
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
$ printf "mpeshejwe\nyapeshejwe\nyaheshejwe\nq\n" | LD_LIBRARY_PATH=. ./p3_repl
  stem=pesh     known=1
  stem=pesh     known=1
  stem=hesh     known=1
```

Two different subject prefixes, `n` and `ya`, both produce the
identical stem `pesh`. Part 4 traces what that means for the printed
explanation.

---

# Part 4 — Capstone: A Real, Self-Contradictory Explanation

## 4.1 Three words, one stem, one note that's only right for one of them

```
$ ./kinyarwanda_nlp -s "Yaheshejwe neza."     # correct 3rd-person form
 Yaheshejwe           Inshinga-conjugated (Verb conj.)  Nt.6      hesh
  └─ Uturemajambo (Morphemes): ya(SP·Nt.6) + ∅(TM) + hesh(root) + ejw(EXT) + e(FV)
  └─ Imbundo (Citation verb): guhesha  (igicumbi -hesh-)
```

`yaheshejwe` — subject prefix `ya`, root surfaces plainly as `hesh`,
no nasal anywhere in sight — gets no special note at all, because its
stem is `hesh`, which isn't in `SUPPLETIVE_CITATIONS[]`. That's the
linguistically unremarkable, correct case: `ya` isn't nasal, so the
`n→m→p` rule never has a reason to fire, and it doesn't.

```
$ ./kinyarwanda_nlp -s "Yapeshejwe neza."     # same SP, mutated stem anyway
 Yapeshejwe           Inshinga-conjugated (Verb conj.)  Nt.6      pesh
  └─ Uturemajambo (Morphemes): ya(SP·Nt.6) + ∅(TM) + h(root) + esh(CAUS) + ejw(EXT) + e(FV)
  └─ Imbundo (Citation verb): guhesha  (igicumbi -hesh-)
  └─ Icyitonderwa (Inkomoko/Derivation): guha [to give] + -esh- (causative) =
       guhesha [to make/cause to give]; ... SP ya 1sg 'n' igenwa mbere ya 'h'
       (n→m /_h) ...
```

Same subject prefix `ya` — confirmed by the `Uturemajambo` line two
rows above — and yet `kin_is_verb_conjugated` (Section 6.2's upstream
cause) still reports the mutated stem `pesh`, exactly as if the
subject prefix had been the nasal `n`. The `Icyitonderwa` note that
follows says, in the same paragraph, *"SP ya 1sg 'n'"* — describing
this specific token's subject prefix as the very nasal the
`Uturemajambo` line directly above it says it is not.

## 4.2 Where the contradiction is textually located

Both lines come from the same call to `kin_print_analysis`, for the
same token, within a few lines of printed output:

```
  └─ Uturemajambo (Morphemes): ya(SP·Nt.6) + ...      ← says SP = "ya"
  └─ Imbundo (Citation verb): guhesha ...
  └─ Icyitonderwa (Inkomoko/Derivation): ... SP ya 1sg 'n' ...   ← says SP = "n"
```

There is no intervening recomputation, no different code path for the
two lines — `kin_print_analysis` prints the morpheme chain from
`t->morph.m[]` (which correctly has `SP` form `"ya"`), then, a few
statements later in the same function, prints a *fixed string* from
`SUPPLETIVE_CITATIONS[]` that was written under the assumption that
any token reaching this branch must have subject prefix `n`. Nothing
re-checks that assumption against the very `Token` the note is being
printed for.

## 4.3 The upstream cause: the matcher doesn't require the nasal that triggers its own rule

```c
#include "kinyarwanda.h"
#include <stdio.h>

static void show(const char *w) {
    char stem[KIN_MAX_STEM]; int sc=0, oc=0;
    VerbTense te=TENSE_NONE; VerbExtension ex=VEXT_NONE; bool neg=false;
    bool ok = kin_is_verb_conjugated(w, stem, &sc, &te, &oc, &ex, &neg);
    printf("%-14s stem=%-6s\n", w, ok?stem:"(none)");
}

int main(void) {
    show("mpeshejwe");   /* SP "n" -- the nasal the h->p rule requires */
    show("yapeshejwe");  /* SP "ya" -- not nasal, rule should not apply */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_pesh.c -L . -lkinyarwanda -o p4_pesh
$ LD_LIBRARY_PATH=. ./p4_pesh
mpeshejwe      stem=pesh
yapeshejwe     stem=pesh
```

`verb_match_inner` (Chapter 12's territory) recognizes `pesh` as a
valid stem-shape for `"hesh"`'s `h→p` mutation without checking that
the subject prefix triggering it is actually the nasal `n` the rule
depends on. `analysis.c`'s citation note inherits that over-acceptance
one layer downstream, and states its own assumption as fact rather
than checking it.

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 A stem-only key is the right design for a context-free fact

`SUPPLETIVE_CITATIONS[]`'s other entry, `"giy"` → *"suppletive past
stem of kugenda,"* and `HOMOGRAPH_ROOTS[]`'s `"sig"` entry (Section
6.3) are both safe with a stem-only key precisely because neither note
makes a claim about anything other than the stem itself — "this root
is a suppletive form" and "this root is ambiguous between two verbs"
are true for every token with that stem, regardless of tense, subject,
or anything else. The `"pesh"` entry is different in kind: its note
asserts a fact about the *subject prefix*, which is exactly the one
piece of information a stem-only key can't see.

## 5.2 The fix belongs at the boundary that already failed once

Chapter 15 and 16 both found `verb_match_inner` accepting a structural
match without confirming the specific phonological precondition that
match depends on. This is the same shape of gap a third time, in code
neither of those chapters opened: the `h→p` mutation's precondition —
*the subject prefix must be the nasal `n`* — is never checked before
`pesh` is accepted as a valid stem. Fixing that one check would
silently fix both layers of this chapter's finding at once: an
unconfirmed `yapeshejwe` would stop matching as `pesh` at all (closing
the upstream gap), and the citation note's contradiction would never
have a token to misfire on (closing the downstream one) without
`analysis.c` itself needing to change.

---

# Part 6 — Reading the Real Production Code

## 6.1 `SUPPLETIVE_CITATIONS[]`, the `"pesh"` entry, in full

```c
static const struct {
    const char *stem;
    const char *canonical;
    const char *stem_note;
    const char *note;
} SUPPLETIVE_CITATIONS[] = {
    { "giy", "kugenda", "giy",
      "umuzi w'indangika (suppletive past stem) wa kugenda" },
    { "pesh", "guhesha", "hesh",
      "guha [to give] + -esh- (indangika/causative: 'gutera umuntu guha') "
      "= guhesha [to make/cause to give]; "
      "Uturemajambo rw'ibanze: n(SP) + h(root) + esh(CAUS) + ejw(PASS) + e(FV); "
      "Itegeko ryigenamajwi rigize inzira ebyiri: "
      "(1) n+h\xe2\x86\x92mh: SP ya 1sg 'n' igenwa mbere ya 'h' "
      "(n\xe2\x86\x92m /_h: itegeko ry'aho ijwi rivuye); "
      "(2) mh\xe2\x86\x92mp: 'h' ihinduka 'p' inyuma ya m bilabiale "
      "(h\xe2\x86\x92p /_m: h fortifies to bilabial stop after labial nasal); "
      "-ejw- ni indangika igenamajwi y'imbundo -w- nyuma ya -esh- "
      "(-esh- + -w- \xe2\x86\x92 -eshejw-: epenthetic -ej- breaks -shw- cluster)" },
    { NULL, NULL, NULL, NULL }
};
bool supp_found = false;
for (int si = 0; SUPPLETIVE_CITATIONS[si].stem; si++) {
    if (strcmp(t->stem, SUPPLETIVE_CITATIONS[si].stem) == 0) {
        printf("  \342\224\224\342\224\200 Imbundo (Citation verb): %s  (igicumbi -%s-)\n",
               SUPPLETIVE_CITATIONS[si].canonical, SUPPLETIVE_CITATIONS[si].stem_note);
        printf("  \342\224\224\342\224\200 Icyitonderwa (Inkomoko/Derivation): %s\n",
               SUPPLETIVE_CITATIONS[si].note);
        supp_found = true;
        break;
    }
}
```

The match condition is `strcmp(t->stem, SUPPLETIVE_CITATIONS[si].stem) == 0`
— stem only. Nothing in this loop reads `t->morph.m[]`, `t->verb_tense`,
or any subject-prefix field before printing a note whose own text
makes a claim about exactly that.

## 6.2 The matching logic this note's assumption depends on

Quoted and tested in Section 4.3 — `verb_match_inner`'s recognition of
`pesh` as a stem-shape, reached the same way regardless of which
subject prefix preceded it.

## 6.3 The safe comparison: `HOMOGRAPH_ROOTS["sig"]`

```c
static const struct { const char *root; const char *note; } HOMOGRAPH_ROOTS[] = {
    { "sig",
      "gusiga (to leave/to remain, short vowel) | gusiiga (to paint/anoint, "
      "long vowel) -- distinguished by vowel length in speech only" },
    { NULL, NULL }
};
```

This note is true for every token whose stem is `"sig"`, in every
tense, with every subject prefix — there is no context it could ever
contradict, which is exactly why a stem-only key is the right choice
here and the wrong one for `"pesh"`.

---

# Part 7 — Looking Back, Looking Forward

## 7.1 Every citation line this book has shown silently trusted this code

Every `Imbundo (Citation verb)` and `Icyitonderwa` line quoted in any
previous chapter came from exactly the lookup read in this chapter.
None of those earlier quotations happened to trigger the `"pesh"`
entry — this chapter is the first to test it deliberately, by
constructing the comparison `yaheshejwe` / `yapeshejwe` / `mpeshejwe`
specifically to probe whether the note's assumption holds.

## 7.2 What this chapter didn't check

This chapter checked one entry in one citation table. It did not
re-run `SUPPLETIVE_CITATIONS[]`'s only other entry (`"giy"`) or
`HOMOGRAPH_ROOTS[]`'s only entry (`"sig"`) against constructed
counter-examples the way `"pesh"` was checked here — Section 5.1's
reasoning about why they're safe is an argument from reading their
text, not from testing every input that could reach them. It also did
not check whether `print_verb_morphemes` or `print_verb_reconstruction`
— the two dedicated helpers named in Part 2 but not opened in detail
in this chapter — contain any citation-style logic of their own with
the same risk.

---

# Part 8 — Practice

### Beginner

1. Run Section 4.1's three CLI commands yourself and confirm the exact
   text difference between `yaheshejwe`'s output (no note) and
   `yapeshejwe`'s output (the contradictory note).
2. Using `lexicon.c`, confirm `"hesh"` is a directly known verb stem
   (independent of the `h→p` mutation), explaining why `yaheshejwe`
   needs no special citation at all.

### Intermediate

3. Section 6.1's match loop only checks `t->stem`. Propose the
   smallest additional condition — referencing a field already present
   on `Token` — that would make the `"pesh"` entry only fire when the
   subject prefix is actually `n`.
4. Find one more verb root in `lexicon.c` that begins with a nasal-
   sensitive consonant (`h`, `b`, or similar) and check whether
   `verb_match_inner` similarly accepts its 1sg-mutated form under a
   non-1sg subject prefix.

### Advanced

5. Implement Section 7.2's untested claim: construct a counter-example
   input for the `"giy"` entry (some subject prefix and tense
   combination) and confirm whether its note remains accurate for
   every case, or whether it has a narrower version of this chapter's
   bug.
6. Section 5.2 argued that fixing `verb_match_inner`'s acceptance of
   `pesh` without a nasal subject prefix would close both layers of
   this bug at once. Trace `verb_match_inner` (Chapter 12's territory)
   to find exactly where `pesh`-shaped stems are accepted, and confirm
   whether the fix is as simple as Section 5.2 suggests.
7. `kin_print_analysis` is 568 lines, the largest function in
   `analysis.c`, and contains at least two stem-keyed citation tables
   inline. Propose where these tables should live instead (a new file,
   a shared header, `gloss.c` from Chapter 17) and what would need to
   change about how `kin_print_analysis` calls them.

---

## Key takeaways

- `analysis.c`'s verb-display logic is split across three places —
  `print_verb_morphemes`, `print_verb_reconstruction`, and large
  inline blocks inside the 568-line `kin_print_analysis` — and the
  citation-note system this chapter is about lives in the third,
  least modular of the three.
- `SUPPLETIVE_CITATIONS[]` is a stem-keyed lookup table; for entries
  whose note makes no claim beyond "this stem is special" (`"giy"`,
  and `HOMOGRAPH_ROOTS[]`'s `"sig"`), a stem-only key is the correct,
  safe design.
- Real, verified finding: the `"pesh"` entry's note *does* make a
  claim beyond the stem — it asserts the subject prefix is the nasal
  `n` — and nothing checks that assertion against the token actually
  being printed. `yapeshejwe` (subject prefix `ya`, confirmed by the
  morpheme line directly above) triggers the identical note, which
  then states "SP ya 1sg 'n'" in direct contradiction of the line two
  rows above it in the same output block.
- Traced one layer further: the contradiction is reachable at all only
  because `verb_match_inner` accepts the stem `pesh` (the `h→p`
  mutation's result) without first confirming the subject prefix
  triggering that mutation is actually the nasal `n` it requires —
  the same category of missing-precondition-check this book found
  twice already in Chapters 15 and 16, here surfacing through the
  display layer instead of through `pos_tagger.c`'s guard.

## Sources quoted in this chapter

- `src/analysis.c`'s `kin_print_analysis` (568 lines, the file's
  largest function), specifically its inline `SUPPLETIVE_CITATIONS[]`
  and `HOMOGRAPH_ROOTS[]` tables, opened for the first time in this
  chapter.
- `src/morphology.c`'s `verb_match_inner`, confirmed (not re-opened in
  full) as the source of the unconditioned `pesh` stem match.
- `src/lexicon.c`, used to confirm `"hesh"` is directly known
  independent of the mutation.
- Every `pN_*.c` program and every quoted CLI invocation in this
  chapter was actually compiled or run against the real built binary.
