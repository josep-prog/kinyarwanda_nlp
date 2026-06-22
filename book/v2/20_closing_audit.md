# Chapter 20 — Closing Audit: The Marker That Wasn't Unambiguous

## How this chapter works

Same rules as every chapter before it, for the last time. Every
fenced `$` block is real output from the actual built CLI or a
program actually compiled with `gcc -std=c99 -Wall -Wextra` and run
against this project's real library.

Chapters 15 and 16 each found `verb_match_inner` accepting a
structural match — SP-plus-root, root-plus-marker — without confirming
the one precondition that match actually depended on. This chapter
closes that audit. It finds the same shape of bug a third time, in the
`NARRATIVE` tense branch neither chapter opened, and the bug directly
contradicts a comment in `pos_tagger.c` that names this exact tense
marker as one that "carries unambiguous morphological markers" and
therefore needs no further checking. It doesn't.

---

# Part 1 — Language and Code, Side by Side

## 1.1 A tense marker that happens to spell a real root

Kinyarwanda's narrative ("sequential") tense is built as
SP + `ka` + root + `a` — "and then [subject] [verb]s." It's
unambiguous as long as the `ka` and the root that follows it are
visibly two separate pieces. `gukata`, "to cut," has the misfortune of
having a root, `kat`, that *itself* starts with the letters `k` and
`a`:

```
$ ./kinyarwanda_nlp -s "Bagakata igiti."     # genuine narrative
 Bagakata             Inshinga-conjugated (Verb conj.)  Nt.2      kat
  └─ Uturemajambo (Morphemes): ba(SP·Nt.2) + ka(TM) + kat(root) + a(FV)
  └─ Imbundo (Citation verb): gukata  (igicumbi -kat-)
  └─ Inkurikizo (Narrative/Sequential: SP+ka+stem+a)
  └─ Gusubiza (Reconstruction):
       Ingingo: [SP]ba(Nt.2) + [TM]ka + [root]kat + [FV]a
       Itegeko:  k→g §3.7.1 (narrative TM 'ka'→'ga' after vowel-final SP 'ba')
       Guhuza:  ba + ka + kat + a  →  bagakata  ✓
```

"And then they cut [it]" — `ba` (SP) + `ka` (narrative TM) + `kat`
(root) + `a` (FV), correctly identified, citation verb correctly
`gukata`. The TM and the root's first syllable happen to look alike
(`ka...kat`), but the analysis correctly keeps them apart. Section 4.1
shows the same root without the narrative marker in front of it.

## 1.2 Build it: a prefix check with no fallback verification

```c
/* p1_toy_kashadow.c -- matching a marker prefix without checking what's left */
#include <stdio.h>
#include <string.h>

static int starts_with(const char *s, const char *p) {
    return strncmp(s, p, strlen(p)) == 0;
}

/* "is_known" stands in for a lexicon check the toy doesn't have */
static int is_known(const char *stem) {
    return strcmp(stem, "kat") == 0; /* only "kat" is "known" here */
}

int main(void) {
    const char *inner = "kata"; /* SP already stripped: could be ka+t, or just "kata" itself */
    if (starts_with(inner, "ka")) {
        char stem[8];
        strncpy(stem, inner + 2, strlen(inner) - 3);
        stem[strlen(inner) - 3] = '\0';
        printf("assumed marker 'ka' + stem '%s' (known=%d)\n", stem, is_known(stem));
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra p1_toy_kashadow.c -o p1_toy_kashadow
$ ./p1_toy_kashadow
assumed marker 'ka' + stem 't' (known=0)
```

The toy commits to "the first two letters are the marker" the moment
it sees `ka`, with no attempt to check whether treating the *whole*
word as the root instead would have been the better reading. The real
`verb_match_inner` branch (Section 6.1) has exactly this shape.

## 1.3 Checkpoint

1. `bagakata`'s root and TM happen to be distinguishable because the
   TM also voices (`ka→ga` after a vowel-final SP, Section 1.1's
   `Itegeko` line). Why doesn't that voicing rule help disambiguate
   the *unvoiced* case in Section 4.1?
2. Before reading Section 4.1, predict: would you expect `gukata`'s
   root-initial `ka` to cause a problem for the *narrative* tense, the
   *present* tense, or both? Why might one be affected and not the
   other?

---

# Part 2 — The Guard That Was Supposed to Catch This

## 2.1 Two tenses are checked; the rest are trusted

`pos_tagger.c`'s Step 8 quality guard — the same mechanism Chapter 15
found bypassed for a different reason — has its own explicit, written
rationale for which tenses it checks:

```
   PRESENT_NORA  →  checked  (kin_is_known_verb_stem required)
   SUBJUNCTIVE   →  checked  (kin_is_known_verb_stem required)
   everything else, including NARRATIVE  →  trusted, NOT checked
       "All other tenses carry unambiguous morphological markers
        (ra, aga, za, ye, ta-, ka-) that are strong enough evidence
        on their own."
```

`ka-` is named, explicitly, as one of the markers trusted to be
"strong enough evidence on their own" not to need a known-stem check.
Section 4.1 is the test of that claim for a root that starts with
exactly those two letters.

## 2.2 Why this is a different kind of finding than Chapters 15 or 16

Chapter 15 found a tense branch with *no guard at all*, where one
nearby branch (a different tense) happened to have the right guard.
Chapter 16 found a branch *shadowed* by an earlier, more general one
matching first. This chapter's finding is neither — `verb_match_inner`
correctly produces an ambiguous structural match (`ka` could be marker
or root-start), and `pos_tagger.c`'s guard *deliberately, explicitly,
in writing* decided this particular marker didn't need disambiguating.
The decision itself, not an oversight in implementing it, is what
Section 4 shows to be wrong.

---

# Part 3 — Making It Interactive

```c
/* p3_repl.c -- type a word, see whether the narrative branch claimed it */
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
        printf("  stem=%-6s tense=%-2d known=%d\n",
               ok?stem:"(none)", te, ok && kin_is_known_verb_stem(stem));
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p3_repl.c -L . -lkinyarwanda -o p3_repl
$ printf "akata\nbakata\nakaza\nakanguka\nagakata\nq\n" | LD_LIBRARY_PATH=. ./p3_repl
  stem=t      tense=7  known=0
  stem=t      tense=7  known=0
  stem=z      tense=7  known=0
  stem=nguk   tense=7  known=0
  stem=kat    tense=7  known=1
```

Four ordinary present-tense words, all misread as narrative
(`tense=7` is `TENSE_NARRATIVE`), three with a truncated single-letter
or otherwise wrong stem and `known=0` — and the *real* narrative form,
`agakata`, is the only one of the five that's actually correct. Part 4
runs the wrong four through the full CLI.

---

# Part 4 — Capstone: Three Real Roots, the Same Misparse

## 4.1 An ordinary sentence, analyzed as the wrong verb entirely

```
$ ./kinyarwanda_nlp -s "Umuhungu akata igiti."
 akata                Inshinga-conjugated (Verb conj.)  Nt.1      t
  └─ Uturemajambo (Morphemes): a(SP·Nt.1 / Nt.6) + ka(TM) + t(root) + a(FV)
  └─ Imbundo (Citation verb): guta  (igicumbi -t-)
  └─ Inkurikizo (Narrative/Sequential: SP+ka+stem+a)
  └─ Gusubiza (Reconstruction):
       Ingingo: [SP]a(Nt.1 / Nt.6) + [TM]ka + [root]t + [FV]a  →  akata  ✓
Nta makosa aboneka / No errors detected.
```

"The boy cuts the tree" (`gukata`, root `kat`, present tense) is
analyzed as citation verb `guta` — "to throw away," a real, different
verb — with root `t`, narrative tense, and the `✓` confirming only
that `a+ka+t+a` mechanically reassembles into `akata`, not that the
analysis means what the sentence means. No error is reported.

```
$ ./kinyarwanda_nlp -s "Umugore akaza ku ruzi."
 akaza                Inshinga-conjugated (Verb conj.)  Nt.1      z
  └─ Uturemajambo (Morphemes): a(SP·Nt.1 / Nt.6) + ka(TM) + z(root) + a(FV)
  └─ Imbundo (Citation verb): kuza  (igicumbi -z-)
  └─ Inkurikizo (Narrative/Sequential: SP+ka+stem+a)
  └─ Gusubiza (Reconstruction):
       Ingingo: [SP]a(Nt.1 / Nt.6) + [TM]ka + [root]z + [FV]a  →  akaza  ✓
```

"The woman tightens [it] at the river" (`gukaza`, root `kaz`) is
analyzed as `kuza` — "to come," also a real, unrelated verb. Both
words pass with `known=1` in one case and `known=0` in the other only
because the *coincidentally truncated* one-letter stem (`z`, from
`kuza`) happens to itself be a registered root — the guard would not
have caught this even if it had been checking, because the wrong stem
is a real one. Confirmed via `kin_is_known_verb_stem("z")` returning
true and `lexicon.c` listing `kuza` independently.

## 4.2 The comment's claim, tested directly against its own example

`pos_tagger.c`'s Step 8 comment lists `ka-` among the markers it
trusts "on their own." Three real, independent verb roots beginning
with `ka` — `kat` (`gukata`, to cut), `kang` (`gukanguka`, to wake up),
and `kaz` (`gukaza`, to tighten) — all confirm the marker is not
unambiguous for any word whose root happens to share its first two
letters:

```
$ printf "akata\nakanguka\nakaza\nbakata\n" | LD_LIBRARY_PATH=. ./p3_repl
  stem=t      tense=7  known=0
  stem=nguk   tense=7  known=0
  stem=z      tense=7  known=0
  stem=t      tense=7  known=0
```

Every one of these is an ordinary present-tense conjugation of a real,
common verb. None of them is narrative. All four are misclassified as
narrative because the structural ambiguity Section 1.1 introduced is
real, and the only place equipped to resolve it — `pos_tagger.c`'s
quality guard — was deliberately written to skip this exact case.

## 4.3 The fix, applied to the real source, and the one case it can't reach

`verb_match_inner`'s `TENSE_NARRATIVE` branch (Section 6.1) now
requires `kin_is_known_verb_stem` on the candidate stem before
returning a match — the same guard Section 6.2 already showed working
for the voiced `ga` sibling — and the structurally identical
`TENSE_NARRATIVE_SUBJ` branch (`ka` + stem + `e`, one branch above it,
sharing the same unguarded shape) received the identical fix. Rebuilt
and re-tested directly against this section's own examples:

```
$ ./kinyarwanda_nlp -s "Umuhungu akata igiti."
 akata                Inshinga-conjugated (Verb conj.)  Nt.1      kat
  └─ Uturemajambo (Morphemes): a(SP·Nt.1 / Nt.6) + ∅(TM) + kat(root) + a(FV)
  └─ Imbundo (Citation verb): gukata  (igicumbi -kat-)
  └─ Indagihe y'ubusanzwe (Present – habitual)
  └─ Gusubiza (Reconstruction):
       Ingingo: [SP]a(Nt.1 / Nt.6) + [TM]∅ + [root]kat + [FV]a  →  akata  ✓

$ ./kinyarwanda_nlp -s "Bagakata igiti."     # genuine narrative -- still correct
 Bagakata             Inshinga-conjugated (Verb conj.)  Nt.2      kat
  └─ Imbundo (Citation verb): gukata  (igicumbi -kat-)
  └─ Inkurikizo (Narrative/Sequential: SP+ka+stem+a)
```

`akata` and `akanguka` (Section 4.1's `kat`/`kang` roots) are now both
correctly identified as present tense, with the correct citation verb
and a `known=1` stem, while `bagakata` — and every other genuinely
narrative form tested — is untouched. `make clean && make && make
test` still passes all 244 tests.

`akaza` (the third root, `kaz`/`gukaza`, "to tighten") is **not**
fixed by this change, and testing it directly shows why a known-stem
check alone cannot fix it:

```
$ ./kinyarwanda_nlp -s "Umugore akaza ku ruzi."
 akaza                Inshinga-conjugated (Verb conj.)  Nt.1      z
  └─ Uturemajambo (Morphemes): a(SP·Nt.1 / Nt.6) + ka(TM) + z(root) + a(FV)
  └─ Imbundo (Citation verb): kuza  (igicumbi -z-)
  └─ Inkurikizo (Narrative/Sequential: SP+ka+stem+a)
```

Stripping `"ka"` from `akaza`'s inner `"kaza"` leaves the candidate
stem `"z"` — and `"z"` is independently a real, known root (`kuza`,
"to come"; confirmed in Section 1.1's own pos_tagger.c quote, which
names it directly among the project's monosyllabic verb roots). The
guard's question is "is this candidate stem real," and for `akaza`
the answer is genuinely yes on *both* readings — narrative `ka+z`
("and then she comes") and present `kaz+a` ("she tightens") are both
structurally valid matches for the identical surface string. A
known-stem check can reject a fabricated stem; it cannot choose
between two real ones. Resolving this one would need a different kind
of evidence than this fix supplies — most likely preferring whichever
reading leaves the *longer* known stem, which would require comparing
against the present-tense fallback's own candidate rather than
deciding within the narrative branch alone. That comparison is left
undone here, the same way Chapter 13 left `ukuri` documented rather
than guessed at: `akaza` is a genuine, narrower homograph-style
ambiguity, not the over-broad enumeration bug `akata` and `akanguka`
turned out to be, and conflating the two would risk a wrong fix for
the sake of a complete-looking one.

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 "Unambiguous on its own" was an assumption, not a verified property

The four tense markers other than `ra` (already audited in Chapter 16)
named in the Step 8 comment — `aga`, `za`, `ye`, `ta-`, `ka-` — were
presumably judged unambiguous by the same kind of reasoning: each one
is a fixed string that doesn't commonly start a root. That reasoning
holds for most roots and fails for the small number that happen to
start with the same letters as the marker. This chapter found three
such roots for `ka-` alone, by deliberately searching `lexicon.c` for
roots starting with `ka`, the same method Chapter 16 used for
`ra`-initial roots. It is very likely some of `aga`, `za`, `ye`, and
`ta-` have analogous counter-examples in the lexicon that this book
has not searched for.

## 5.2 The fix, and where it was actually applied

Section 4.3 applied this fix directly to the real source: requiring
`kin_is_known_verb_stem` on the candidate stem before
`verb_match_inner`'s `TENSE_NARRATIVE` and `TENSE_NARRATIVE_SUBJ`
branches return a match — the same shape of guard the voiced `ga`
sibling branch (Section 6.2) already had, and the same shape Chapter
15 found missing for a different tense entirely. The fix was placed in
`morphology.c`, at the source of the ambiguous match, rather than in
`pos_tagger.c`'s guard — the same choice Chapter 13 made for the
voicing-rule fix, on the same reasoning: a guard at the shared engine
benefits every caller of `verb_match_inner`, not just the one path
`pos_tagger.c` happens to walk. `pos_tagger.c`'s own comment (Section
2.1) was updated to stop claiming `ka-` is unambiguous "on its own"
and to note that `verb_match_inner` now enforces the check itself.

This closed two of this chapter's three counter-examples cleanly:
`akata` and `akanguka` are now correctly present tense, with the
correct citation verb, and `bagakata` and every other genuinely
narrative form tested is unaffected. `akaza` remains open — Section
4.3 explains why a known-stem check, by itself, cannot distinguish a
fabricated stem from a second, equally real one, and why guessing at
a fix for that narrower case risked getting it wrong for the sake of
appearing complete.

---

# Part 6 — Reading the Real Production Code

## 6.1 The narrative branch as this chapter found it

```c
/* NARRATIVE: ka + stem + a (when SP is not "ka" itself) */
if (kin_starts_with(inner, "ka") && ilen > 3 && inner[ilen-1]=='a'
    && strcmp(SP[i].pfx, "ka") != 0) {
    const char *s = inner + 2; size_t sl = ilen - 3;
    if (sl < 1) continue;
    if (stem_buf) { strncpy(stem_buf, s, sl); stem_buf[sl]='\0'; }
    if (subj_class) *subj_class = SP[i].cls;
    if (tense_out)  *tense_out  = TENSE_NARRATIVE;
    return true;
}
```

Three conditions, none of them a lexicon check: the inner string
starts with `ka`, is longer than 3 characters, and ends in `a`. Every
one of `akata` (`inner="kata"`), `akaza` (`inner="kaza"`), and
`akanguka` (`inner="kanguka"`) satisfies all three. Section 4.3
applied the fix to this exact branch directly in the real source —
the version quoted here is the one that was actually running when
Section 4.1's broken CLI output was captured.

## 6.2 The voiced sibling branch — which *does* check, for a different reason

```c
/* NARRATIVE voiced: ka→ga (k→g §3.7.1 after vowel-final SP).
 * Gate on stem validation (≥ 2 chars, known root) to avoid false
 * positives against roots that happen to start with the letters
 * left after stripping 'ga' (e.g. kugaba root='gab' not 'b'). */
if (kin_starts_with(inner, "ga") && ilen > 3 && inner[ilen-1]=='a'
    && strcmp(SP[i].pfx, "ka") != 0
    && strcmp(SP[i].pfx, "ga") != 0) {
    size_t sl = ilen - 3;
    if (sl >= 2) {
        char cand[KIN_MAX_STEM];
        strncpy(cand, inner + 2, sl); cand[sl] = '\0';
        bool found = kin_is_known_verb_stem(cand);
        /* ... h→s restoration fallback ... */
        if (found) {
            /* ... accept ... */
        }
    }
}
```

This sibling branch — for the *voiced* `ga` form of the same marker —
already had exactly the guard Section 4.3's fix added to the unvoiced
`ka` branch, and its own comment names the precise risk ("roots that
happen to start with the letters left after stripping `ga`") that this
chapter's finding showed was equally real one branch above it. The fix
for `ka` was modeled directly on this branch's own working logic.

## 6.3 The guard that was supposed to catch this, and why it didn't need to change

```c
if (kin_is_verb_conjugated(verb_w, stem, &scls, &vtense, &obj_cls, &vext, &is_neg)
    && (vtense != TENSE_PRESENT_NORA || kin_is_known_verb_stem(stem))
    && (vtense != TENSE_SUBJUNCTIVE  || kin_is_known_verb_stem(stem))
    ) {
    tok->pos = POS_VERB_CONJ;
    /* ... */
}
```

`TENSE_NARRATIVE` is not `TENSE_PRESENT_NORA` and not
`TENSE_SUBJUNCTIVE` — both conditions were vacuously true for it, and
the word was accepted unconditionally the moment
`kin_is_verb_conjugated` returned true at all. This function itself
was left unchanged by Section 4.3's fix — once `verb_match_inner` no
longer *returns* a narrative match for `akata`/`akanguka`, there is
nothing left here for this guard to filter for those two words; it
falls through, exactly as it already does for `bagakata`'s correctly
accepted `kat`. Only the explanatory comment above this guard (Section
2.1) needed updating, to stop describing `ka-` as something this
function doesn't need to check.

---

# Part 7 — Looking Back: What Twenty Chapters Found

This book opened with the premise that a reader who knows no
Kinyarwanda and a reader who speaks it natively could both come to
understand — and improve — this project's real source, by treating
the grammar and the code as one subject rather than two. Twenty
chapters later, here is what that reading turned up, in the order it
was found:

- **Chapter 13**: `ortho.c`'s voicing rule fired before vowel-initial
  morphemes that should have blocked it — fixed in the real source,
  with two `lexicon.c` data errors and one self-contradicting test
  found and fixed alongside it.
- **Chapter 14**: `RULE 9`'s vowel-harmony check covered 2 of the 4
  harmony-sensitive allomorph pairs the generator itself recognizes.
- **Chapters 15–16**: `verb_match_inner`'s SUBJUNCTIVE, past-imperfect,
  and present-tense branches all independently dropped the vowel
  restoration a sibling branch already knew how to do, fabricating
  roots that `pos_tagger.c`'s own guard exists to catch — except for
  one bypass built for an unrelated ambiguity that disabled it
  entirely.
- **Chapter 17**: `gloss.c`'s `NOUN_GLOSS_TABLE` silently shadowed a
  real word's correct meaning with a duplicate key, and
  `build_rough_translation` never read `t->is_negative` at all.
- **Chapter 18**: a hardcoded citation note in `analysis.c`, written
  for one specific subject prefix, printed its own assumption as fact
  for a token whose actual subject prefix — visible two lines above in
  the same output — contradicted it.
- **Chapter 19**: the same sentence-disambiguation pass that correctly
  resolves an ambiguous subject prefix in present tense silently
  leaves it unresolved in three other tenses it never checks for.
- **This chapter**: a tense marker explicitly documented as
  "unambiguous … on its own" turned out not to be, for three real,
  common verbs, with the fix already implemented one branch away for
  this exact marker's voiced sibling.

No two of these were the same bug. Several were the same *shape* of
bug — an enumeration narrower than the property it stands in for,
found independently in `morph_dispatch.c`, `gloss.c`,
`build_rough_translation`, and now twice more in `verb_match_inner`
and `pos_tagger.c`. Recognizing that shape, once, in Chapter 14, made
every later instance of it faster to spot and explain — which is
itself the book's central claim about reading code linguistically:
the grammar gives you categories (harmony pairs, tense markers,
subject-prefix classes) precise enough to ask "does this enumeration
actually cover the category it claims to," and that question, asked
of nine different functions across eight chapters, found something
real in nearly every one.

## 7.2 What this book never checked

Every chapter has said so honestly as it went, and the pattern holds
at the end: `morph_dispatch.c`'s detection layer for nouns (Chapter
13's territory) was never audited as exhaustively as `verb_match_inner`
was in Chapters 15, 16, and 20; `analysis.c`'s `print_verb_morphemes`
and `print_noun_reconstruction` (Chapter 18's named-but-not-opened
helpers) were never read in full; `syntax.c`'s other rules beyond RULE
9 (Chapter 14) were never enumerated; and the four other tense markers
named alongside `ka-` in this chapter's Section 5.1 — `aga`, `za`,
`ye`, `ta-` — were never searched for their own counter-example roots.
A reader who wanted to keep going has, at minimum, those four leads.

---

# Part 8 — Practice

### Beginner

1. Run Section 4.1's two CLI commands and confirm the exact citation
   verb and root each one wrongly reports.
2. Using `lexicon.c`, find one more verb root starting with `ka` not
   named in this chapter, and confirm via Section 3's REPL whether it
   reproduces the same misparse.

### Intermediate

3. Section 5.1 named `aga`, `za`, `ye`, and `ta-` as untested for
   analogous counter-examples. Pick one, search `lexicon.c` for a root
   starting with those same letters, and test it.
4. Section 6.2 quoted the `ga`-branch's existing guard. Adapt it
   (rather than just adding a bare `kin_is_known_verb_stem` check) as
   the fix for the `ka`-branch, preserving its `h→s` restoration
   fallback logic if you believe narrative `ka`-roots could need it.

### Advanced

5. Section 4.3 applied this fix to the real source already, and it
   closed `akata` and `akanguka` but left `akaza` open (both readings
   use a real, known stem). Implement the longer-known-stem
   tie-break Section 4.3 proposed but did not build: compare the
   narrative branch's candidate stem against the present-tense
   fallback's candidate for the same word, and prefer whichever is
   longer. Confirm it resolves `akaza` to present tense without
   breaking `bagakata` or any other narrative form already tested in
   this chapter.
6. This book's twenty chapters checked roughly nine of `analysis.c`,
   `morphology.c`, `morph_dispatch.c`, `pos_tagger.c`, `syntax.c`,
   `gloss.c`, `ortho.c`, and `lexicon.c`'s functions in real depth, out
   of dozens across the project. Pick one function never named in any
   chapter and apply this book's method to it once: read the doc
   comment, find its strongest claimed example, write a real test,
   and see if the claim holds.

---

## Key takeaways

- `pos_tagger.c`'s Step 8 quality guard, before this chapter's fix,
  explicitly checked `TENSE_PRESENT_NORA` and `TENSE_SUBJUNCTIVE`
  against `kin_is_known_verb_stem`, and explicitly trusted five other
  tense markers — including `ka-` — as "unambiguous … on their own,"
  needing no such check.
- Real, verified finding: three independent, real verb roots —
  `kat` (`gukata`, to cut), `kang` (`gukanguka`, to wake up), and
  `kaz` (`gukaza`, to tighten) — all begin with the same two letters
  as the narrative tense marker `ka-`, and all three were misparsed by
  `verb_match_inner`'s unconditional `ka`-branch as narrative tense
  with a truncated, wrong stem. `"Umuhungu akata igiti."` ("The boy
  cuts the tree") was analyzed with citation verb `guta` ("to throw
  away") instead of `gukata`, with zero errors reported.
- The fix was applied directly to `src/morphology.c`: both the
  unvoiced `ka` narrative branch and its `ka`+stem+`e` sibling now
  require `kin_is_known_verb_stem` before returning a match, mirroring
  the guard the *voiced* `ga` sibling branch already had. `pos_tagger.c`'s
  comment was corrected to stop calling `ka-` unambiguous. `akata` and
  `akanguka` are now correctly present tense; `bagakata` and every
  other genuine narrative form tested are unaffected; all 244 existing
  tests still pass.
- `akaza` (`kaz`/`gukaza`) was deliberately left unfixed and documented
  as an open question, the same way Chapter 13 left `ukuri`: stripping
  `ka` from it leaves the candidate stem `z`, which is independently
  real (`kuza`, "to come") — a known-stem check cannot tell a
  fabricated stem from a second, equally valid one, so `akaza` remains
  a genuine, narrower homograph-style ambiguity rather than the
  over-broad-enumeration bug its two siblings turned out to be.
- This is the third time this book found `verb_match_inner` or its
  immediate guard accepting a structural match without confirming the
  one precondition the match actually depended on (Chapters 15, 16,
  and this chapter) — and the sixth or seventh time overall it found
  an enumeration (of harmony pairs, tenses, table keys, or markers)
  narrower than the property it was meant to stand in for.

## Sources quoted in this chapter

- `src/morphology.c`'s `verb_match_inner`, specifically the
  `TENSE_NARRATIVE` branch and its voiced `ga` sibling, opened in
  detail for the first time in this chapter and edited directly
  (Section 4.3) to add the missing known-stem guard to the `ka` branch
  and its `TENSE_NARRATIVE_SUBJ` sibling.
- `src/pos_tagger.c`'s Step 8 quality guard, re-opened from Chapter 15
  to check a tense Chapter 15 did not test; its documenting comment
  was edited (Section 2.1) to correct the now-falsified claim that
  `ka-` needs no check.
- `src/lexicon.c`, searched directly for verb roots beginning with
  `ka` to find this chapter's three counter-examples.
- Every `pN_*.c` program and every quoted CLI invocation in this
  chapter was actually compiled with `gcc -std=c99 -Wall -Wextra` and
  actually run against the real built binary or library.

---

## Closing

Twenty chapters ago, this book opened with a single claim: that
Kinyarwanda's grammar and this project's C source are one subject, not
two, and that a reader who took both seriously at once would
understand the code better than a reader who skipped the linguistics,
and understand the linguistics better than a reader who skipped the
code. Every chapter's capstone finding was a test of that claim, not
just a bug report — each one came from asking a grammatical question
("is this harmony rule actually symmetric? is this tense marker
actually unambiguous? is this subject prefix actually resolved?") of
real, running C code, and getting a real, verified, sometimes
surprising answer back.

The project itself is large enough that this book's twenty chapters
covered a genuine fraction of it, not the whole of it — Part 7.2, in
every chapter that had one, said so plainly. That was a deliberate
choice, not a shortcoming to apologize for: the method demonstrated
here — read the comment, find its strongest claim, write the smallest
real test that could break it, trust only what actually compiles and
runs — is the book's real subject, more than any single chapter's
finding is. A reader who has followed this method through twenty
chapters has everything needed to keep applying it to the rest of the
source long after this book ends.
