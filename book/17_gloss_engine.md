# Chapter 17 — The Glossing Engine: Where the English Goes Missing

## How this chapter works

Same rules as every chapter before it. Every fenced `$` block is real
`gcc -std=c99 -Wall -Wextra` output from a program actually compiled
and run against this project's real, built library.

This chapter opens a file no previous chapter has read: `gloss.c`
(776 lines). It's the engine behind the `--gloss` flag (Chapter 11)
and the interlinear gloss block that has appeared in this book's CLI
output since Chapter 14 — but its own source, and the lookup tables
it's built on, have never been examined directly. Two real,
independently verified findings come out of finally opening it.

---

# Part 1 — Language and Code, Side by Side

## 1.1 A word, its morphemes, and the English meant to go with each one

`gloss.c`'s job is to take a morpheme breakdown this book has displayed
since Chapter 1 — `D`, `RT`, `C` for nouns; `SP`, `TM`, root, `FV` for
verbs — and attach an English word or abbreviation to each slot. Run
it on a word this book has used as a clean example before:

```c
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    SentenceAnalysis sa = kin_analyze("uburyo");  /* "strategy/way/method" */
    Token *t = &sa.tokens[0];
    for (int i = 0; i < t->morph.n; i++)
        printf("%-4s form=%-6s gloss=%s\n",
               t->morph.m[i].label, t->morph.m[i].form, t->morph.m[i].english_gloss);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p1_uburyo.c -L . -lkinyarwanda -o p1_uburyo
$ LD_LIBRARY_PATH=. ./p1_uburyo
D    form=u      gloss=CL14.ABSTR
RT   form=bu     gloss=Nt.14
C    form=ryo    gloss=river
```

`uburyo` means "strategy," "way," or "method" — confirmed directly by
this project's own `lexicon.c`, which comments the noun-stem entry
`"ryo"` (under class 14) as exactly that: *"strategy / way / method
(ubu+ryo)."* The gloss engine says `river`. Section 4.1 finds out why.

## 1.2 Build it: a lookup table with one key listed twice

```c
/* p1_toy_dup.c -- a linear-scan gloss table with a duplicated key */
#include <stdio.h>
#include <string.h>

typedef struct { const char *key; const char *gloss; } Entry;

static const Entry TABLE[] = {
    { "ryo", "river" },          /* listed first */
    { "ntu", "person" },
    { "ryo", "it (Nt.5)" },      /* listed again, later */
    { NULL,  NULL }
};

static const char *lookup(const char *key) {
    for (int i = 0; TABLE[i].key; i++)
        if (strcmp(key, TABLE[i].key) == 0) return TABLE[i].gloss;
    return "(not found)";
}

int main(void) {
    printf("ryo -> %s\n", lookup("ryo"));
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra p1_toy_dup.c -o p1_toy_dup
$ ./p1_toy_dup
ryo -> river
```

A duplicated key in a linear-scan table isn't a compile error, isn't a
runtime crash, and produces no warning of any kind — it just silently
picks whichever entry was written first, and the second is dead code
forever. The real `NOUN_GLOSS_TABLE` has exactly this shape, quoted in
full in Section 6.1.

## 1.3 Checkpoint

1. If the two `"ryo"` entries in Section 1.2's toy were swapped
   (second one written first), what would `lookup("ryo")` return
   instead? Would that be more correct, less correct, or just
   differently wrong?
2. What would have to change about the table's *type* (not its
   contents) to make a duplicate key a compile-time error instead of a
   silent shadow?

---

# Part 2 — One Set of Morphemes, Two Different Consumers

## 2.1 Filling every slot vs. building one sentence

```
   kin_fill_morpheme_glosses(tok)         build_rough_translation(sa, ...)
   ───────────────────────────             ─────────────────────────────
   runs once per TOKEN                     runs once per SENTENCE
   fills tok->morph.m[i].english_gloss     reads each token's morph glosses
   for every morpheme slot                 (root only, for verbs) and
                                            stitches one English sentence
        │                                       │
        ▼                                       ▼
   leaves "NEG" morpheme's                 has no idea negation exists —
   gloss field EMPTY (no case               doesn't check tok->is_negative,
   for label=="NEG")                        doesn't look for a NEG morpheme
                                             at all
```

Both functions are real, both are called on the same analyzed
sentence, and they disagree about how much information survives —
because they were built to answer two different questions
(Section 4.2 traces the consequence precisely).

## 2.2 A printing function's cosmetic fallback can hide a real gap

`kin_print_interlinear` — the function that actually prints the
morpheme-and-gloss block this book has quoted since Chapter 14 — has
its own fallback for an empty gloss: it prints the morpheme's *label*
instead.

```c
const char *gl = t->morph.m[m].english_gloss[0]
                 ? t->morph.m[m].english_gloss
                 : t->morph.m[m].label;
```

That's why a negative sentence's interlinear line shows `NEG` under
the `nt` morpheme and looks complete — `NEG` is the *label*, printed
because the *gloss* is blank, not because anyone wrote English for it.
Section 4.2 shows what happens one line later, in the rough
translation, which has no equivalent fallback.

---

# Part 3 — Making It Interactive

```c
/* p3_repl.c -- type a sentence, see the full interlinear gloss */
#include "kinyarwanda.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    char line[256];
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, "q") == 0) break;
        SentenceAnalysis sa = kin_analyze(line);
        kin_print_interlinear(&sa);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p3_repl.c -L . -lkinyarwanda -o p3_repl
$ printf "Umugabo akora.\nUmugabo ntakora.\nq\n" | LD_LIBRARY_PATH=. ./p3_repl
  ...
  ─── Translation hint: man/husband work/do.
  ...
  ─── Translation hint: man/husband work/do.
```

"The man works" and "The man does not work" — opposite meanings —
produce the identical translation hint. Part 4 traces exactly where
the negation gets lost.

---

# Part 4 — Capstone: Two Real, Independently Verified Findings

## 4.1 A duplicate table key silently shadows the correct meaning of a real word

`NOUN_GLOSS_TABLE` lists `"ryo"` twice:

```c
{ "ryo",       "river"             },   /* line 211, listed first  */
...
{ "ryo",       "it (Nt.5)"         },   /* line 220, listed again */
```

The lookup function (Section 6.1) scans front to back and returns on
first match — `"river"` always wins; `"it (Nt.5)"` can never be
reached through this table by any input, ever. Neither entry is
actually correct for the word this chapter opened with:
`lexicon.c`'s own comment on the noun stem `"ryo"` (class 14, as in
`uburyo`/`buryo`) says *"strategy / way / method."* Confirmed three
ways — by the gloss engine's output (Section 1.1), by reading the
duplicate table entries directly, and by checking the project's own
lexicon comment against both. `kin_get_verb_gloss` and `get_noun_gloss`
(Section 6.1) are the same eleven-line shape, copy-pasted; a full,
programmatic scan of both tables for repeated keys finds **7 more**
duplicates in `VERB_GLOSS_TABLE` (`nywer`, `komez`, `jy`, `hagarar`,
`fung`, `er`, `cecek`) and one more in `NOUN_GLOSS_TABLE` (`byo`,
listed twice with the identical gloss — harmless, but still dead code
the moment it's written).

## 4.2 Negation disappears between the morpheme line and the translation line

`build_rough_translation`'s handling of `POS_VERB_CONJ` tokens reads
exactly two things: the `"root"` morpheme's gloss, and a `tense_str`
derived from a `switch` on `t->verb_tense` (Section 6.2, in full).
Neither of those touches `t->is_negative`, and neither looks for a
morpheme labeled `"NEG"`. Trace why that's not just an oversight in
one function but two compounding gaps:

```c
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    SentenceAnalysis sa = kin_analyze("Umugabo ntakora.");
    Token *t = &sa.tokens[1];   /* the verb token */
    printf("surface=%s is_negative=%d\n", t->surface, t->is_negative);
    for (int i = 0; i < t->morph.n; i++)
        printf("  [%d] label=%-6s form=%-6s gloss=\"%s\"\n",
               i, t->morph.m[i].label, t->morph.m[i].form, t->morph.m[i].english_gloss);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_neg.c -L . -lkinyarwanda -o p4_neg
$ LD_LIBRARY_PATH=. ./p4_neg
surface=ntakora is_negative=1
  [0] label=NEG    form=nti   gloss=""
  [1] label=SP     form=a     gloss="3SG.HUM"
  [2] label=TM     form=∅     gloss="∅"
  [3] label=root   form=kor   gloss="work/do"
  [4] label=FV     form=a     gloss="IND"
```

`tok->is_negative` is correctly `1`. The `NEG` morpheme exists, with
form `"nti"` — and its `english_gloss` field is the empty string.
`kin_fill_morpheme_glosses` (Section 6.3) has a branch for `SP`, `TM`,
`OM`, `root`, `EXT`, `FV`, `PREF`, `D`, `RT`, `C`, and `RS` — eleven
labels, none of them `"NEG"`. The interlinear printer's label-fallback
(Section 2.2) papers over this one morpheme's missing gloss by
displaying `NEG` (the label) where a gloss should be — but
`build_rough_translation` has no such fallback, never reads the label
either, and never asks `t->is_negative` directly. The information is
present in the `Token` the entire time; nothing between it and the
final English sentence ever looks at it.

## 4.3 The tense marker that vanishes is the rule, not the exception

`build_rough_translation`'s tense-marking `switch` (quoted in full,
Section 6.2) has four cases — `TENSE_PAST_PERF`/`TENSE_PAST_PERF_LOC`
("past"), `TENSE_PAST_IMPF` ("was"), `TENSE_FUTURE` ("will"),
`TENSE_SUBJUNCTIVE` ("should") — and a `default` of empty string for
everything else. `TENSE_PRESENT` and `TENSE_PRESENT_NORA` correctly
fall into that default — present tense is supposed to be unmarked in
English. But so does every other tense this project defines:
`TENSE_NARRATIVE`, `TENSE_CONDITIONAL`, `TENSE_COPULA_PRES`,
`TENSE_COPULA_PAST`, `TENSE_OPTATIVE`, `TENSE_IMPERATIVE`,
`TENSE_NEG_RELATIVE`, and others — all silently unmarked, the same way
negation is. Four tenses get a deliberate English marker; the rest get
nothing, by the same default branch that correctly handles present
tense and silently mishandles everything else this `switch` doesn't
explicitly name.

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 Why two separate functions read the same morphemes for two different jobs

`kin_fill_morpheme_glosses` answers "what does this *piece* mean,"
slot by slot, intended for the interlinear display where every
morpheme gets its own column. `build_rough_translation` answers "what
does this *sentence* mean," in one line, intended as a fast,
approximate reading aid — its own header comment says so directly:
*"This is an approximation useful for quick reference — not a true MT
output."* Building one sentence-level string from eleven
independently-filled per-morpheme glosses is fundamentally a different
operation from filling each slot — there's no obvious single function
that should do both, and splitting them is a reasonable design.

## 5.2 The cost: a gap in the careful one is invisible until you stop trusting the cosmetic one

Section 2.2's label-fallback (`gloss ? gloss : label`) is a small,
sensible display choice on its own — better to show `NEG` than a
blank cell. But it means the *interlinear* output, the more detailed
and more carefully built of `gloss.c`'s two outputs, never visibly
reveals that the `NEG` morpheme's gloss was never filled in — the
fallback makes the gap look like a feature. The *rough translation*,
built without that same forgiving fallback, is what actually exposes
it. This is the same shape of lesson as Chapter 13's `verified` flag:
a check (or, here, a display) that looks complete is not the same as
one that's been confirmed complete, and the surest way to tell the
difference is to build a second, independent consumer of the same
data and see if it agrees.

---

# Part 6 — Reading the Real Production Code

## 6.1 The lookup functions, and the duplicate key, in full

```c
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

static bool get_noun_gloss(const char *igicumbi, char *out, size_t outsize) {
    if (!igicumbi || !out || outsize == 0) return false;
    out[0] = '\0';
    for (int i = 0; NOUN_GLOSS_TABLE[i].igicumbi; i++) {
        if (strcmp(igicumbi, NOUN_GLOSS_TABLE[i].igicumbi) == 0) {
            strncpy(out, NOUN_GLOSS_TABLE[i].gloss, outsize - 1);
            out[outsize - 1] = '\0';
            return true;
        }
    }
    return false;
}
```

Both functions are the identical shape: linear scan, `strcmp`, return
on first hit. `NOUN_GLOSS_TABLE`'s relevant two entries, at their real
source distance apart:

```c
static const NounGloss NOUN_GLOSS_TABLE[] = {
    ...
    { "ryo",       "river"             },   /* among the common-noun entries */
    ...
    { "yo",        "it (Nt.4/6/9)"     }, { "ryo",       "it (Nt.5)"          },
    ...
};
```

## 6.2 `build_rough_translation`'s verb-conjugated branch, in full

```c
else if (t->pos == POS_VERB_CONJ) {
    const char *rg = "";
    for (int m = 0; m < t->morph.n; m++) {
        if (strcmp(t->morph.m[m].label, "root") == 0 &&
            t->morph.m[m].english_gloss[0]) {
            rg = t->morph.m[m].english_gloss; break;
        }
    }
    const char *tense_str = "";
    switch (t->verb_tense) {
        case TENSE_PAST_PERF:
        case TENSE_PAST_PERF_LOC:  tense_str = "(past) ";    break;
        case TENSE_PAST_IMPF:       tense_str = "(was) ";     break;
        case TENSE_FUTURE:          tense_str = "(will) ";    break;
        case TENSE_SUBJUNCTIVE:     tense_str = "(should) ";  break;
        default:                    tense_str = "";            break;
    }
    if (rg[0])
        snprintf(piece, sizeof(piece), "%s%s", tense_str, rg);
    else
        snprintf(piece, sizeof(piece), "%s", t->surface);
}
```

No reference to `t->is_negative`, no loop over `t->morph.m[]` looking
for a `"NEG"` label — this entire branch's only two inputs are the
root gloss and the tense.

## 6.3 `kin_fill_morpheme_glosses`'s label dispatch, in full

```c
if (strcmp(m->label, "SP") == 0) { ... }
else if (strcmp(m->label, "TM") == 0) { ... }
else if (strcmp(m->label, "OM") == 0) { ... }
else if (strcmp(m->label, "root") == 0) { ... }
else if (strcmp(m->label, "EXT") == 0) { ... }
else if (strcmp(m->label, "FV") == 0) { ... }
else if (strcmp(m->label, "PREF") == 0) { ... }
else if (strcmp(m->label, "D") == 0) { ... }
else if (strcmp(m->label, "RT") == 0) { ... }
else if (strcmp(m->label, "C") == 0) { ... }
else if (strcmp(m->label, "RS") == 0) { ... }
/* no case for "NEG" -- gloss stays "" from the initialization above */
```

Eleven labels handled; `"NEG"` (set by `morph_dispatch.c`'s
`set_morph(&mb->m[n++], "NEG", ...)`, Section 4.2) is not one of them.

---

# Part 7 — Looking Back, Looking Forward

## 7.1 Every interlinear block this book has shown was built by this exact code

Chapter 14's `--gloss` examples, every `═══ Interlinear Gloss ═══`
block since, were generated by the two functions read in this chapter.
This chapter is the first to open the file producing output this book
had already quoted four times.

## 7.2 What this chapter didn't check

This chapter found duplicate keys in two of `gloss.c`'s three lookup
tables (`VERB_GLOSS_TABLE`, `NOUN_GLOSS_TABLE`) but did not check
whether `adj_stem_gloss`'s `if`/`else if` chain (a different shape —
no table, no duplicate-key risk by construction) or `INV_GLOSS_TABLE`
(invariables) have any analogous correctness problems of their own
kind. It also did not check whether any of the 7 other duplicate verb
keys (Section 4.1) produce a *wrong* gloss the way `"ryo"` does, the
way Chapter 16 didn't trace every tense branch to the same depth as
the one it fully diagnosed — confirmed as duplicates, not individually
checked against the lexicon the way `"ryo"` was.

---

# Part 8 — Practice

### Beginner

1. Using Section 1.2's method, predict what `kin_get_verb_gloss`
   returns for `"komez"` (one of Section 4.1's seven verb duplicates),
   then verify directly and identify which of its two glosses is
   reachable.
2. Construct a present-tense and a future-tense sentence with the same
   verb and subject, and confirm the rough translation correctly marks
   one with `(will)` and not the other.

### Intermediate

3. Section 4.3 named several tenses that fall into `build_rough_translation`'s
   silent default case. Pick `TENSE_NARRATIVE` specifically, construct
   a real narrative-tense sentence, and check whether its rough
   translation reads as ambiguous with the same verb's present-tense
   form.
4. `lexicon.c` was the independent source that confirmed `"ryo"`'s
   correct gloss in Section 4.1. Pick one of the 6 *other* duplicate
   `VERB_GLOSS_TABLE` keys and check `lexicon.c`'s own comments (where
   present) for a similar independent confirmation of which entry, if
   either, is actually correct.

### Advanced

5. Propose the smallest fix to `build_rough_translation`'s
   `POS_VERB_CONJ` branch that would correctly prefix negated verbs
   with "not," using the same `t->is_negative` field Section 4.2
   confirmed is already populated correctly.
6. Section 5.2 argued the interlinear display's label-fallback hid the
   `NEG` gloss gap from view. Find one more morpheme label (besides
   `NEG`) that `kin_fill_morpheme_glosses` doesn't handle, and confirm
   whether the same fallback is hiding the same kind of gap for it.
7. Propose a structural fix (not a one-line patch) to
   `NOUN_GLOSS_TABLE`/`VERB_GLOSS_TABLE` that would make a duplicate
   key like `"ryo"` impossible to introduce silently in the future —
   consider what would need to run, and when, to catch it before the
   table reaches production.

---

## Key takeaways

- `gloss.c` (776 lines, opened in this chapter for the first time)
  fills `KinMorpheme.english_gloss` per morpheme
  (`kin_fill_morpheme_glosses`) and separately stitches a whole-sentence
  approximate English translation (`build_rough_translation`) — two
  different consumers of the same morpheme data, for two different
  purposes.
- Real, verified finding: `NOUN_GLOSS_TABLE` lists the key `"ryo"`
  twice with different glosses (`"river"` and `"it (Nt.5)"`); the
  linear-scan lookup always returns the first, so the real word
  `uburyo` ("strategy/way/method," confirmed via `lexicon.c`'s own
  comment) glosses incorrectly as `"river"` — and a full scan finds 7
  more duplicate keys in `VERB_GLOSS_TABLE` and one more (harmless) in
  `NOUN_GLOSS_TABLE`.
- Real, verified finding: `build_rough_translation` never inspects
  `t->is_negative` or looks for a `"NEG"`-labeled morpheme, so
  negated and affirmative sentences with the same verb produce the
  identical rough translation (`"Umugabo akora."` and
  `"Umugabo ntakora."` both give `"man/husband work/do."`).
- That gap compounds with a second one: `kin_fill_morpheme_glosses`
  has no case for the `"NEG"` label at all, so its `english_gloss`
  field is always empty — invisible in the interlinear display only
  because `kin_print_interlinear` falls back to printing the
  morpheme's *label* when its gloss is blank, a cosmetic choice that
  happens to look like a real translation.
- `build_rough_translation`'s tense-marking `switch` explicitly
  handles 4 of this project's many `VerbTense` values; everything else
  — correctly for present tense, silently for narrative, conditional,
  copula, optative, imperative, and more — falls through to the same
  empty-string default.

## Sources quoted in this chapter

- `src/gloss.c` (`VERB_GLOSS_TABLE`, `NOUN_GLOSS_TABLE`,
  `kin_get_verb_gloss`, `get_noun_gloss`, `kin_fill_morpheme_glosses`,
  `build_rough_translation`, `kin_print_interlinear`), opened for the
  first time in this chapter.
- `src/lexicon.c`'s comment on the noun stem `"ryo"`, used as
  independent confirmation against `gloss.c`'s output.
- `src/morph_dispatch.c`'s `set_morph(&mb->m[n++], "NEG", ...)` call,
  confirming the morpheme label `gloss.c` doesn't handle.
- Every `pN_*.c` program in this chapter was actually compiled with
  `gcc -std=c99 -Wall -Wextra` and actually executed.
