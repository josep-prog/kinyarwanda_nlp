# Chapter 19 — Sentence-Level Disambiguation: Fixed for One Tense, Broken for Three

## How this chapter works

Same rules as every chapter before it. Every fenced `$` block is real
output from the actual built CLI or a program actually compiled with
`gcc -std=c99 -Wall -Wextra` and run against this project's real
library.

Since Chapter 1, this book has mentioned in passing that the verb
prefix `"ya"` is ambiguous between class-6 present and class-1/4/9
past, and that the project resolves it "using sentence context" —
without ever opening the function that does it. This chapter opens
`kin_resolve_sp_ambiguity`, `kin_tag_gram_roles`, and
`kin_propagate_proper_nouns` (`analysis.c`, lines 51–261) and finds
that the single-letter version of this same ambiguity — the `"y"`
glide before a vowel-initial root — is correctly resolved for exactly
one tense and silently left unresolved for three others, visibly
breaking the CLI's own output for ordinary, real sentences.

---

# Part 1 — Language and Code, Side by Side

## 1.1 One subject prefix, two unrelated grammatical contexts

Kinyarwanda's class-1 ("human") subject prefix is `a` in the present
and `ya` in the past. Class-6 ("mass/plural") present is also `ya`.
The verb-detection layer (Chapters 12–16's territory) has no sentence
context, so on seeing `ya` it always guesses class 6. `analysis.c`'s
header comment names the fix:

```
 * In Kinyarwanda the "ya" subject prefix is shared by two grammatical contexts:
 *   1. Nt.6 PRESENT habitual:  ya + stem + a     (yamara, yagenda)
 *   2. Nt.1/3 PAST:            a(SP) + a(past) → ya + stem + ye/tse/aga
 *                               (yagiye, yaremye, yagendaga, yabonye)
 * ... After syntax checking we know whether the subject noun is Nt.1/3
 * ... or Nt.6 ... This pass corrects the verb's stored noun_class ...
```

A vowel-initial root (like `ig`, "study," from `kwiga`) produces a
*shorter* version of the same problem: SP `a` plus root `ig` glides to
a single `y`, with no separate vowel left to distinguish anything.
Run it in present tense, with a class-1 noun directly in front of it:

```
$ ./kinyarwanda_nlp -s "Umwana yiga."
 Umwana               Izina mbonera (Noun)            Nt.1      ana
 yiga                 Inshinga-conjugated (Verb conj.)  Nt.1      ig
  └─ Uturemajambo (Morphemes): a(SP·Nt.1 / Nt.6) + ∅(TM) + ig(root) + a(FV)
  └─ Gusubiza (Reconstruction):
       Ingingo: [SP]a(Nt.1 / Nt.6) + [TM]∅ + [root]ig + [FV]a
       Itegeko:  a→y §1.1 (SP 'a' word-initial + 'i'-initial root → 'y': semivocalisation)
       Guhuza:  a + ∅ + ig + a  →  yiga  ✓
```

The class column correctly resolves to `Nt.1` — `Umwana` (a class-1
noun) is right there to disambiguate it, exactly as the header comment
promises. Section 4.1 runs the identical sentence in a different
tense.

## 1.2 Build it: resolving an ambiguous tag by looking left

```c
/* p1_toy_resolve.c -- the smallest version of "scan left for the antecedent" */
#include <stdio.h>
#include <string.h>

typedef struct { const char *word; int noun_class; int is_noun; } Tok;

static int resolve(Tok *toks, int n, int verb_idx, const int *candidates, int ncand) {
    for (int j = verb_idx - 1; j >= 0; j--) {
        if (!toks[j].is_noun) continue;
        for (int k = 0; k < ncand; k++)
            if (toks[j].noun_class == candidates[k]) return toks[j].noun_class;
    }
    return 0; /* unresolved */
}

int main(void) {
    Tok sent[] = { {"umwana", 1, 1}, {"yiga", 0, 0} };
    int cand[] = {1, 6};
    printf("resolved class = %d\n", resolve(sent, 2, 1, cand, 2));
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra p1_toy_resolve.c -o p1_toy_resolve
$ ./p1_toy_resolve
resolved class = 1
```

The real `scan_back_noun` (Section 6.1) is this exact idea, with one
more layer: it's only ever *called* for specific combinations of
stored class and tense. Section 4.1 shows what happens for a
combination it's never called for.

## 1.3 Checkpoint

1. Section 1.1's sentence resolved correctly in present tense. Before
   reading further, predict: would you expect a past-tense version of
   the same sentence (`"Umwana yize."`, "the child studied") to be
   *easier*, *equally easy*, or *harder* to resolve the same way? Why?
2. `scan_back_noun` (Section 1.2's toy) stops at the *first* matching
   noun scanning backward. What real-sentence construction would make
   that the wrong noun to stop at?

---

# Part 2 — Three Passes, Run in a Fixed Order

## 2.1 What each pass actually fixes

```
   kin_tag_sentence()  →  kin_morpheme_analyze()  →  these three passes:

   1. kin_resolve_sp_ambiguity()      fixes Token.noun_class on VERBS,
                                       using nearby NOUNS as evidence
   2. kin_tag_gram_roles()            fixes Token.gram_role on VERBS,
                                       using nearby PARTICLES as evidence
   3. kin_propagate_proper_nouns()    fixes Token.is_proper_noun on NOUNS,
                                       using OTHER OCCURRENCES of the
                                       same word elsewhere in the text
```

All three exist for the same underlying reason: the morphology layer
analyzes one word at a time and genuinely cannot know some things
(which noun a "ya" actually agrees with; whether a capitalized,
sentence-initial word is a name or just a normal word that happens to
start a sentence) until the rest of the sentence — or the rest of the
text — is visible.

## 2.2 The shape this chapter is built around

Pass 1's fix for the two-letter `"ya"` ambiguity (Section 1.1's quoted
comment) explicitly lists four candidate classes (`{1, 4, 6, 9}`) and
applies to four named past tenses. Its fix for the one-letter `"y"`
ambiguity is gated by a *tense* condition instead of just a class-list
— and that condition, read closely in Part 4, lists fewer tenses than
the ambiguity can actually occur in.

---

# Part 3 — Making It Interactive

```c
/* p3_repl.c -- type a two-word "noun verb." sentence, see the resolved class */
#include "kinyarwanda.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    char line[256];
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, "q") == 0) break;
        SentenceAnalysis sa = kin_analyze(line);
        for (int i = 0; i < sa.token_count; i++) {
            Token *t = &sa.tokens[i];
            if (t->pos != POS_VERB_CONJ) continue;
            printf("  %-10s resolved_class=%d  tense=%d\n",
                   t->surface, t->noun_class, t->verb_tense);
        }
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p3_repl.c -L . -lkinyarwanda -o p3_repl
$ printf "Umwana yiga.\nUmwana yize.\nUmwana yigaga.\nq\n" | LD_LIBRARY_PATH=. ./p3_repl
  yiga       resolved_class=1  tense=2
  yize       resolved_class=0  tense=3
  yigaga     resolved_class=0  tense=4
```

Three tenses of the same root, the same unambiguous preceding noun
every time — and only the first one resolves. Part 4 traces exactly
why.

---

# Part 4 — Capstone: Two Real, Verified Findings

## 4.1 The "y"-glide fix only checks for present tense

```
$ ./kinyarwanda_nlp -s "Umwana yige."
 Umwana               Izina mbonera (Noun)            Nt.1      ana
 yige                 Inshinga-conjugated (Verb conj.)            ig
  └─ Uturemajambo (Morphemes): ?(SP) + ∅(TM) + ig(root) + e(FV)
  └─ Ikigombero (Subjunctive: SP+stem+e)
  └─ Gusubiza (Reconstruction):
       Ingingo: [SP]?(Pers.) + [TM]∅ + [root]ig + [FV]e  →  ?ige
```

`"Umwana yige."` — "let the child study," subjunctive, the same
`Umwana` antecedent as Section 1.1 — leaves the verb's class column
completely blank, displays its subject prefix as a literal `?(SP)`,
and reconstructs the word as `?ige`, which isn't even the actual
surface form `yige`. Past tenses fail the same way, with a less
alarming but equally wrong symptom — the SP displays as the real
string `ya` but the noun class stays unresolved:

```
$ ./kinyarwanda_nlp -s "Umwana yize."
 yize                 Inshinga-conjugated (Verb conj.)            ig
  └─ Gusubiza (Reconstruction):
       Ingingo: [SP]ya(Pers.) + [root]ig + [FV]ye
       Guhuza:  y + ig + e  →  yize  ✓

$ ./kinyarwanda_nlp -s "Umwana yigaga."
 yigaga               Inshinga-conjugated (Verb conj.)            ig
  └─ Gusubiza (Reconstruction):
       Ingingo: [SP]ya(Pers.) + [root]ig + [FV]aga
       Guhuza:  y + ig + aga  →  yigaga  ✓
```

`yize` and `yigaga` at least reconstruct correctly (the `✓` is
genuine), but their class column is blank, same as `yige`'s. All three
words sit directly after `Umwana` — the exact configuration
`kin_resolve_sp_ambiguity`'s `"y"` branch exists to resolve (Section
6.1, in full) — and the branch's own condition is:

```c
} else if (subj->noun_class == 1 &&
           (verb->verb_tense == TENSE_PRESENT_NORA ||
            verb->verb_tense == TENSE_PRESENT)) {
    verb->noun_class = 1;
}
```

Two tenses named; `TENSE_SUBJUNCTIVE`, `TENSE_PAST_PERF`, and
`TENSE_PAST_IMPF` all reach this same branch (confirmed directly,
Section 4's REPL output: `tense=6`, `tense=3`, `tense=4` respectively,
none of them `2` or its `_NORA` counterpart) and all three fall
through the `else if` doing nothing, leaving `noun_class` at its
original, ambiguous `0`.

## 4.2 A name used only at sentence-initial position is never recognized

```c
#include "kinyarwanda.h"
#include <stdio.h>

static void show(const char *sent) {
    SentenceAnalysis sa = kin_analyze(sent);
    for (int i = 0; i < sa.token_count; i++) {
        Token *t = &sa.tokens[i];
        if (t->pos != POS_NOUN) continue;
        printf("  %-10s is_proper=%d class=%d\n", t->surface, t->is_proper_noun, t->noun_class);
    }
}

int main(void) {
    show("Kayini yishe Abeli. Kayini yahunze.");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_propnoun.c -L . -lkinyarwanda -o p4_propnoun
$ LD_LIBRARY_PATH=. ./p4_propnoun
  Kayini     is_proper=0 class=12
  Abeli      is_proper=1 class=0
  Kayini     is_proper=0 class=12
```

`Kayini` ("Cain") appears twice across two sentences, sentence-initial
both times, and is misclassified as an ordinary class-12 common noun
both times — `kin_propagate_proper_nouns` (Section 6.2) only fixes a
sentence-initial capital if the *same lowercased word* was confirmed
as a proper noun somewhere *mid-sentence* elsewhere in the text, and
`Kayini` never occurs mid-sentence anywhere in this short text.
Confirmed this is exactly the mechanism, not a special case for this
one name, by adding a single mid-sentence occurrence:

```
$ ./kinyarwanda_nlp -s "Kayini yahunze. Imana yabwiye Kayini."
```
```c
show("Kayini yahunze. Imana yabwiye Kayini.");
```
```
  Kayini     is_proper=1 class=0
  Imana      is_proper=0 class=9
  Kayini     is_proper=1 class=0
```

With one confirmed mid-sentence `Kayini` (in `"... yabwiye Kayini"`,
not sentence-initial) present anywhere in the text, *both*
occurrences — including the sentence-initial one in the first
sentence — are correctly fixed. This is a real, working mechanism with
a narrow and specific blind spot: a name that is, throughout an entire
text, only ever the first word of the sentences it appears in.

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 The bug's shape, and where this book has seen it before

Section 4.1's condition lists exactly two tenses out of (at minimum)
four it needs to cover. Chapter 14 found `RULE 9`'s harmony check
hardcoding 2 of 4 allomorph pairs; Chapter 17 found
`build_rough_translation`'s tense-marking `switch` naming 4 of many
`VerbTense` values and silently defaulting for the rest. This is the
same shape a third time: a finite, explicit enumeration standing in
for what should be a property check (*"does this verb's SP touch a
vowel-initial root with no separating tense marker"*) that holds for
more cases than the enumeration lists. The actual property doesn't
care whether the tense is present, subjunctive, or past — the glide
happens identically in all of them, which is exactly why `yize` and
`yigaga` reconstruct correctly (the *glide itself* isn't tense-gated
anywhere) while only the *class resolution* is.

## 5.2 Why the proper-noun gap is a different kind of finding

Section 4.1 is an enumeration that's narrower than it should be —
fixable by adding the missing tenses. Section 4.2 isn't a narrow
enumeration; it's a structural limit of "confirm from elsewhere in the
text," which has no fix that doesn't change the strategy itself
(e.g., a small list of known Biblical/common given names, or a
capitalization-pattern heuristic). Worth telling apart: one of this
chapter's two findings is a small, mechanical gap in an otherwise
correct mechanism; the other is a correctly-implemented mechanism
whose entire approach has an inherent boundary.

---

# Part 6 — Reading the Real Production Code

## 6.1 The full "y"-glide resolution branch

```c
/* "y" SP (stored cls 0, ambiguous) arises from i→y glide before a
 * vowel-initial root (§1.1: i+V → y+V). The stored class is 0 because
 * the "y" prefix is shared by Nt.1/6/9 present and Nt.1 past.
 * Scan back for nearest Nt.9 or Nt.1 noun and resolve:
 *   Nt.9 subject → SP was "i" → class 9  (e.g. Imana yita)
 *   Nt.1 subject + present → SP was "a" → class 1  (e.g. umuntu yiga)
 * Guard: only apply when the surface word actually starts with "y". */
if (vc == 0 && kin_starts_with(verb->lower, "y")) {
    static const int y_cls[] = {9, 1};
    const Token *subj = scan_back_noun(sa, i - 1, y_cls, 2);
    if (subj) {
        if (subj->noun_class == 9) {
            verb->noun_class = 9;
        } else if (subj->noun_class == 1 &&
                   (verb->verb_tense == TENSE_PRESENT_NORA ||
                    verb->verb_tense == TENSE_PRESENT)) {
            verb->noun_class = 1;
        }
    }
}
```

Note the asymmetry already visible in the source: the `subj->noun_class == 9`
branch has *no* tense condition at all — a class-9 antecedent resolves
the verb regardless of tense. Only the class-1 branch is tense-gated.
That asymmetry is itself evidence the gate was written with one
example in mind (present-tense `yiga`/`yita`) rather than derived from
the actual linguistic precondition.

## 6.2 `kin_propagate_proper_nouns`, in full

```c
static void kin_propagate_proper_nouns(SentenceAnalysis *sa) {
    char confirmed[KIN_MAX_TOKENS][KIN_MAX_WORD];
    int  n_confirmed = 0;

    for (int i = 0; i < sa->token_count; i++) {
        const Token *t = &sa->tokens[i];
        if (!t->is_proper_noun) continue;
        bool dup = false;
        for (int j = 0; j < n_confirmed; j++)
            if (strcmp(confirmed[j], t->lower) == 0) { dup = true; break; }
        if (!dup && n_confirmed < KIN_MAX_TOKENS) {
            strncpy(confirmed[n_confirmed], t->lower, KIN_MAX_WORD - 1);
            confirmed[n_confirmed][KIN_MAX_WORD - 1] = '\0';
            n_confirmed++;
        }
    }
    if (n_confirmed == 0) return;

    for (int i = 0; i < sa->token_count; i++) {
        Token *t = &sa->tokens[i];
        if (t->is_proper_noun) continue;
        if (t->pos != POS_NOUN)  continue;
        for (int j = 0; j < n_confirmed; j++) {
            if (strcmp(t->lower, confirmed[j]) == 0) {
                t->is_proper_noun = true;
                t->noun_class     = 0;
                break;
            }
        }
    }
}
```

The first loop collects every word already confirmed proper
(necessarily mid-sentence, since that's the only way the tokenizer can
confirm one). The second loop only ever consults that collected list —
there is no other source of evidence anywhere in this function. A name
absent from the list because it never occurred mid-sentence simply
isn't fixed; nothing in this function's design suggests it would be.

## 6.3 `kin_tag_gram_roles`, confirmed correct against its own rules

```c
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    SentenceAnalysis sa = kin_analyze("Yagize ngo agiye kure.");
    for (int i = 0; i < sa.token_count; i++) {
        Token *t = &sa.tokens[i];
        if (t->pos == POS_VERB_CONJ)
            printf("%-10s role=%d\n", t->surface, t->gram_role);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p6_roles.c -L . -lkinyarwanda -o p6_roles
$ LD_LIBRARY_PATH=. ./p6_roles
Yagize     role=0
agiye      role=2
```

`GRAM_ROLE_MAIN_VERB` (0) for `Yagize`, `GRAM_ROLE_COMPLEMENT` (2) for
`agiye` after the complement particle `ngo` — exactly as documented.
A parallel test substituting the quotative particle `ati` for `ngo`
(Part 4 of the earlier interactive testing for this chapter) correctly
returns `MAIN_VERB` for both verbs, confirming the function's explicit
quotative exclusion (Section 6's source comment: *"Quotative particles
... must NOT cause the following verb to be marked as complement"*)
works as written. No bug found in this function during this chapter's
research.

---

# Part 7 — Looking Back, Looking Forward

## 7.1 The ambiguity this book named in passing, finally opened

Earlier chapters referenced the `"ya"` class-1/class-6 ambiguity as
background context for other findings without ever reading the
resolution code itself. This chapter is the first to open it, and the
first to test it against more than the one tense (present) every prior
mention happened to use as its example.

## 7.2 What this chapter didn't check

This chapter found the class-1 branch of the `"y"`-glide fix
incomplete; it did not exhaustively check the class-9 branch (Section
6.1's asymmetry) against every tense, nor the top-of-function `"ya"`
two-letter resolution (Section 1.1, lines 58–75) for an analogous gap
in its own four-tense list (`PAST_PERF`, `PAST_PERF_LOC`, `PAST_IMPF`,
`COPULA_PAST`) — whether, for instance, a class-1/4/6/9 noun followed
by a `"ya"`-prefixed verb in some *other* past-flavored tense this
project defines (negative past, relative past) hits the same kind of
gap. It also did not check whether the `"u"`/`"i"` branches (lines 77–84,
109–121) have comparable blind spots of their own.

---

# Part 8 — Practice

### Beginner

1. Run Section 4.1's three CLI commands yourself and confirm the exact
   difference in the class column and the SP display for `yiga` vs.
   `yige` vs. `yize`.
2. Using Section 6.2's function, predict what happens to a text where
   a proper noun occurs mid-sentence *first*, then sentence-initial
   later — does propagation still work, or does order in the text
   matter? Test it.

### Intermediate

3. Propose the smallest change to Section 6.1's `else if` condition
   that would also resolve `TENSE_SUBJUNCTIVE`, `TENSE_PAST_PERF`, and
   `TENSE_PAST_IMPF`, without touching the `subj->noun_class == 9`
   branch's already-unconditional behavior.
4. Section 7.2 named the top-of-function `"ya"` two-letter resolution
   as untested for an analogous gap. Pick one Kinyarwanda tense not in
   its four-tense list and construct a real test sentence to check.

### Advanced

5. Section 4.2 argued the proper-noun gap has no fix that doesn't
   change the underlying strategy. Sketch (in comments or pseudocode,
   not necessarily working C) what a capitalization-pattern or small
   known-name-list heuristic would look like, and what new failure
   mode it would introduce in exchange for closing this one.
6. `kin_resolve_sp_ambiguity` runs once, in a fixed order, after
   `kin_tag_gram_roles` is documented to require running after it (Part
   2's pipeline order). Confirm via `kin_analyze`'s actual body whether
   gram-role tagging could ever read a not-yet-corrected `noun_class`
   for a verb this chapter's bug affects, and whether that has any
   observable consequence.

---

## Key takeaways

- `kin_resolve_sp_ambiguity`, `kin_tag_gram_roles`, and
  `kin_propagate_proper_nouns` (`analysis.c`, lines 51–261) are three
  sentence-level (or whole-text-level) passes that exist because the
  morphology layer analyzes one word at a time and cannot see context
  it would need to disambiguate certain forms.
- Real, verified finding: the single-letter `"y"`-glide branch of
  `kin_resolve_sp_ambiguity` only resolves a class-1 antecedent when
  the verb's tense is `TENSE_PRESENT` or `TENSE_PRESENT_NORA`.
  `TENSE_SUBJUNCTIVE`, `TENSE_PAST_PERF`, and `TENSE_PAST_IMPF` all
  reach the same branch and fall through unresolved — confirmed with
  `"Umwana yiga."` (resolves to Nt.1) against `"Umwana yige."` (blank
  class, literal `?` subject-prefix display, broken `?ige`
  reconstruction) and `"Umwana yize."`/`"Umwana yigaga."` (blank class,
  correct reconstruction).
- The same branch's class-9 case has no tense condition at all — an
  asymmetry that is itself evidence the gate was derived from one
  worked example rather than from the actual linguistic precondition,
  the same enumeration-narrower-than-reality shape found in Chapters
  14 and 17.
- Real, verified, second finding: `kin_propagate_proper_nouns` only
  fixes a sentence-initial proper noun if the same word is confirmed
  proper somewhere mid-sentence elsewhere in the text. A name used
  only sentence-initially throughout an entire text (`Kayini` in a
  two-sentence test, sentence-initial both times) is never recognized
  — confirmed working correctly the moment a single mid-sentence
  occurrence of the same name is added anywhere in the text.
- `kin_tag_gram_roles` was checked against four real sentences
  covering `AUXILIARY`, `COMPLEMENT` (and its documented quotative
  exclusion), and `SEQUENTIAL` roles, and matched its own documentation
  in every case — no bug found in this function during this chapter.

## Sources quoted in this chapter

- `src/analysis.c`'s `kin_resolve_sp_ambiguity`, `scan_back_noun`,
  `kin_tag_gram_roles`, and `kin_propagate_proper_nouns` (lines
  51–261), opened in full for the first time in this chapter.
- Every `pN_*.c` program and every quoted CLI invocation in this
  chapter was actually compiled with `gcc -std=c99 -Wall -Wextra` and
  actually run against the real built binary or library.
