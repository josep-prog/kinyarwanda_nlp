# Chapter 15 — When the Guard Rail Has a Gap of Its Own

## How this chapter works

Same rules as every chapter before it. Every fenced `$` block is real
`gcc -std=c99 -Wall -Wextra` output from a program actually compiled
and run against this project's real, built library.

Chapter 14 ended with a real, unexplained result: `yandikijwe`, a
correctly-spelled Kinyarwanda word, wasn't recognized by the pipeline
at all. This chapter goes back into `verb_match_inner` — the
1,100-line function Chapter 12 named as the largest in the project and
left mostly unread — to find out why, and ends up finding something
more specific and more interesting than a missing pattern: a real word
gets rejected by a safety check that is *correctly* doing its job, while
a different, almost-coincidental string sails past that exact same
check with a fully fabricated analysis.

---

# Part 1 — Language and Code, Side by Side

## 1.1 One elided vowel, traced all the way through

Kinyarwanda's vowel-contact rule (Chapters 1, 9, and 12) says two
vowels can't sit next to each other across a morpheme boundary. The
subject prefix `ya` (3rd person singular / class 1 past) ends in `a`;
the verb root `andik` (write) begins with `a`. Spoken and written
together, the two `a`s merge into one: `ya` + `andik` surfaces as
`yandik`, not `yaandik`. Every chapter since Chapter 1 has used this
rule to *explain* forms like `yandika`. This chapter asks a narrower
question: when the matching engine sees the surface form `yandik...`,
does it know to put the missing `a` back before checking whether
`andik` is a real root?

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
    printf("%-12s ok=%d stem=%-8s tense=%-28s known_stem=%d\n",
           w, ok, ok?stem:"", kin_verb_tense_name(tense), ok && kin_is_known_verb_stem(stem));
}

int main(void) {
    show("yanditse");  /* active past-perfect: ya + andik + tse */
    show("yandike");   /* subjunctive: ya + andik + e */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p1_elision.c -L . -lkinyarwanda -o p1_elision
$ LD_LIBRARY_PATH=. ./p1_elision
yanditse     ok=1 stem=andik      tense=Impitakare (Impitagihe – recent past) known_stem=1
yandike      ok=1 stem=ndik       tense=Ikigombero (Subjunctive: SP+stem+e)  known_stem=0
```

`yanditse` correctly recovers the real root `andik` — the `a` was put
back. `yandike`, same subject prefix, same root, same elided vowel,
recovers `ndik` instead — one letter short, and not a real root at
all. Both calls reached `kin_is_verb_conjugated`, in the same file,
moments apart in this test. Part 4 traces exactly where they diverge.

## 1.2 Build it: a stripper with and without vowel restoration

```c
/* p1_toy_restore.c -- strip SP "ya", with and without recovering the
   elided root-initial vowel */
#include <stdio.h>
#include <string.h>

static void naive(const char *word) {
    /* word starts with "ya"; just remove the 2 literal characters */
    printf("naive:    %s -> %s\n", word, word + 2);
}
static void restored(const char *word) {
    /* word starts with "ya"; SP's final 'a' may have absorbed the
       root's initial 'a' -- put it back before reporting the root */
    char buf[32];
    snprintf(buf, sizeof(buf), "a%s", word + 2);
    printf("restored: %s -> %s\n", word, buf);
}

int main(void) {
    naive("yandike");
    restored("yandike");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra p1_toy_restore.c -o p1_toy_restore
$ ./p1_toy_restore
naive:    yandike -> ndike
restored: yandike -> andike
```

`naive` is exactly what Section 4.1's broken branch does. `restored`
is exactly what a different branch in the same function — quoted in
full in Section 6.2 — already does, for a different tense, using this
exact word as its own worked example.

## 1.3 Checkpoint

1. `ya` + `andik` → `yandik` loses one `a`. If the matcher instead saw
   the subject prefix `a` (1 character, not 2) in front of `andik`,
   would the same naive-stripping bug occur? Why or why not?
2. `kin_is_known_verb_stem("ndik")` returned `0`. What would have to
   be true of the lexicon for that check to accidentally return `1`
   for some *other* wrongly-stripped root — and why would that be
   worse than returning `0`?

---

# Part 2 — One Matcher, One Guard, Two Files

## 2.1 `verb_match_inner` is permissive on purpose; `pos_tagger.c` is supposed to clean up after it

```
   morphology.c: verb_match_inner()         pos_tagger.c: Step 8 guard
   ──────────────────────────────            ──────────────────────────
   "does SP + remainder + FV                 "is the remainder a REAL
    fit a known TENSE SHAPE                    known verb root, or did
    at all?" -- structural,                    the matcher above just
    not lexical                                find a shape that fits?"
        │                                            │
        ▼                                            ▼
   accepts ANY remainder for                 rejects unknown stems for
   SUBJUNCTIVE / PRESENT_NORA                 SUBJUNCTIVE / PRESENT_NORA
   shapes, known or not                       ... except when the SP is
                                               cy/by/ry/zy (Section 4.2)
```

`pos_tagger.c` says this about its own guard, directly:

```c
/* Quality guard: kin_is_verb_conjugated() always succeeds for any word
 * whose length and ending vowel permit a structural SP+stem+FV parse —
 * including words with single-char or completely unknown stems (fallback
 * path in verb_match_inner).  Without a guard, proper names like "Ada"
 * at sentence start ... would be labelled as conjugated verbs with
 * fabricated roots ("kuda"). */
```

This is the project's own engineers, in their own words, naming the
exact failure mode this chapter is about — "fabricated roots" is their
phrase, not this book's. The guard exists precisely because the
matcher underneath it is deliberately loose.

## 2.2 Why build it this way instead of validating inside the matcher

`verb_match_inner` doesn't have access to the full sentence context,
doesn't know if the word is at the start of a sentence (where capitals
mean nothing grammatically), and is already the most complex single
function in the project (Chapter 12). Pushing every tense branch to
individually verify `kin_is_known_verb_stem` would mean repeating that
check in dozens of places — and Chapter 12 already found that this
project doesn't always notice when the same check needs repeating
consistently (`PAST_SP_ELOC`/`PAST_SP_LIST`/`PAST_SPS`). Centralizing
the "is this stem real" decision in one place, downstream, in
`pos_tagger.c`, is the same kind of separation-of-concerns choice
Chapter 13 found between detection and verification — *when the
downstream check actually covers everything the matcher is willing to
produce.* Part 4 finds where it doesn't.

---

# Part 3 — Making It Interactive

```c
/* p3_repl.c -- type a word, see whether the pipeline accepts it as a
   verb and what root it found */
#include "kinyarwanda.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    char line[128];
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, "q") == 0) break;
        SentenceAnalysis sa = kin_analyze(line);
        Token *t = &sa.tokens[0];
        printf("  pos=%s stem=%s\n", kin_pos_name(t->pos), t->stem);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p3_repl.c -L . -lkinyarwanda -o p3_repl
$ printf "yandike\nbyandike\nq\n" | LD_LIBRARY_PATH=. ./p3_repl
  pos=Ijambo ry'amahanga (Foreign/Unknown) stem=
  pos=Inshinga-conjugated (Verb conj.) stem=ndik
```

`yandike` — a real word, correctly spelled — gets nothing. `byandike`
— a string built from the exact same (wrong) root `ndik` that
`yandike` was correctly rejected for — gets accepted as a conjugated
verb. Part 4 explains precisely why `by` is treated differently from
`ya`, and why that difference has nothing to do with which one is
actually a real word.

---

# Part 4 — Capstone: A Real Word Rejected, a Wrong One Accepted

## 4.1 The unconditional branch, and the word it was never going to get right

`verb_match_inner`'s subjunctive handling is four lines:

```c
/* SUBJUNCTIVE: ends in e */
if (ilen >= 2 && inner[ilen-1]=='e') {
    size_t sl = ilen - 1;
    if (stem_buf) { strncpy(stem_buf, inner, sl); stem_buf[sl]='\0'; }
    if (subj_class) *subj_class = SP[i].cls;
    if (tense_out)  *tense_out  = TENSE_SUBJUNCTIVE;
    return true;
}
```

No `kin_is_known_verb_stem` check, no vowel-restoration step — `inner`
is whatever's left after the literal subject-prefix string was
subtracted from `word`, full stop. For `yandike`, `inner` is
`"ndike"` (the SP `"ya"` removed two literal characters, including
the root's true initial `a`, which should have been restored, not
discarded). The branch reports stem `"ndik"` and returns `true`
immediately — structurally valid, lexically meaningless.

## 4.2 The guard's bypass, and the word it lets through anyway

`pos_tagger.c`'s Step 8 guard requires a known stem for
`TENSE_SUBJUNCTIVE` — *except* when the word starts with one of four
specific two-character strings:

```c
bool is_bare_phon_sp = (
    (w[0]=='c' && w[1]=='y') ||   /* cy = ki + vowel-initial stem */
    (w[0]=='b' && w[1]=='y') ||   /* by = bi + vowel-initial stem */
    (w[0]=='r' && w[1]=='y') ||   /* ry = ri + vowel-initial stem */
    (w[0]=='z' && w[1]=='y')      /* zy = zi + vowel-initial stem */
);
...
&& (v_tense != TENSE_SUBJUNCTIVE
    || (kin_is_known_verb_stem(v_stem) && !(v_obj > 0 && v_obj == cls))
    || is_bare_phon_sp)
```

The reasoning behind `is_bare_phon_sp` is sound on its own terms: a
word starting with literal `cy`/`by`/`ry`/`zy` (no vowel before it)
*cannot* be a noun in disguise — only a mutated verb subject prefix
produces that exact shape. But the bypass doesn't just rule out "this
might secretly be a noun" — it skips the known-stem check entirely,
for any root, real or fabricated. `byandike` starts with `by`. Run it
next to `yandike`:

```c
#include "kinyarwanda.h"
#include <stdio.h>

static void show(const char *w) {
    SentenceAnalysis sa = kin_analyze(w);
    Token *t = &sa.tokens[0];
    printf("%-12s pos=%-22s stem=%-8s\n", w, kin_pos_name(t->pos), t->stem);
}

int main(void) {
    show("yandike");
    show("byandike");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_gap.c -L . -lkinyarwanda -o p4_gap
$ LD_LIBRARY_PATH=. ./p4_gap
yandike      pos=Ijambo ry'amahanga (Foreign/Unknown) stem=
byandike     pos=Inshinga-conjugated (Verb conj.)      stem=ndik
```

Same wrong stem, `"ndik"`, from the same unconditional branch in
`verb_match_inner` — `pos_tagger.c`'s guard correctly distrusts it
when the subject prefix is `ya`, and waives that distrust entirely
when the subject prefix happens to be `by`. The CLI confirms this
isn't a quiet failure either — it produces a complete, confident,
fully-labeled morpheme breakdown:

```
$ ./kinyarwanda_nlp -s "byandike"
 byandike             Inshinga-conjugated (Verb conj.)  Nt.8
  └─ Uturemajambo (Morphemes): bya(SP·Nt.8) + ∅(TM) + nd(root) + ik(EXT) + e(FV)
  └─ Imbundo (Citation verb): kundika  (igicumbi -ndik-)
  └─ Ikigombero (Subjunctive: SP+stem+e)
  └─ Gusubiza (Reconstruction):
       Itegeko:  Ngirika (Stative/Potential: -ik-/-ek-)
       Guhuza:  bya + ∅ + nd + ik + e  →  byandike  ✓
```

`ext_strip` (Chapter 14) ran on the fabricated stem `"ndik"`, found
that it ends in `-ik`, and peeled that off as a stative extension too
— so the reported root is actually `"nd"`, not even `"ndik"`. Neither
`"nd"` nor `"ndik"` is a Kinyarwanda verb. `kundika` does not appear
anywhere in `lexicon.c`. `kin_is_known_verb_stem("nd")` returns `0`,
confirmed directly. The pipeline invented a root, then invented an
extension on top of the invented root, and reported the whole
invention with a
checkmark.

## 4.3 The net effect: correctness and confidence pointing in opposite directions

`yandike` — real, correctly-spelled, semantically meaningful Kinyarwanda
— gets nothing: no analysis, no error message, just "Foreign/Unknown."
`byandike` — built from the identical defect, an SP+root split that
loses real Kinyarwanda's only restorable vowel — gets a full,
confident, checkmarked analysis built on a verb that doesn't exist.
The system is, in the precise sense this book has used throughout,
*more wrong when it looks more confident.* That is the more serious
half of this chapter's finding — worse than simply failing to
recognize a word, which Chapter 14 already showed isn't even rare in
this codebase (`ukuri`, also left open).

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 The guard was built to solve a real, different problem, correctly

The `is_bare_phon_sp` bypass exists because of a genuine ambiguity:
`byiza` could be `bi`(adjective concordance)+`iza` (an adjective,
"beautiful") or `bi`(subject prefix)+`izer`+`a`-shaped (a verb). Both
readings start with literal `by`. Section 4.2's comment in
`pos_tagger.c` explains this is resolved correctly elsewhere (adjective
checked first, Section 4.2's quoted block). The bypass's *stated*
purpose — "trust the verb analysis even... with an unknown stem — the
phonological pattern alone is strong evidence" — is true exactly as
far as "this is a verb, not a noun." It was never meant to certify
"this root exists," and nothing in the bypass's own reasoning claims
it does. The gap is a scope mismatch, not faulty reasoning: a fix
built for one question (verb vs. noun) got reused as if it answered a
different one (is this root real).

## 5.2 The missing half already exists, ninety lines away, citing this exact word

Section 1.2's "restored" toy isn't hypothetical — `morphology.c`
already contains the real version, in the branch gated by
`PAST_SP_TSE` (Section 6.2), and its own comment uses `yandik`-type
forms as the explicit reason it's there: *"Recover 'a' prefix for
vowel-initial roots (ya+andik: a+a→a elides root's 'a')."* The
developers who wrote that line had already identified, by name, the
exact problem Section 4.1 found — just for one tense branch among the
dozen-plus this function implements, not for the unconditional
subjunctive catch-all that runs into the identical SP+root collision
with no defense at all.

---

# Part 6 — Reading the Real Production Code

## 6.1 The unconditional SUBJUNCTIVE branch

Quoted in full in Section 4.1 — four lines, no stem check, no vowel
restoration.

## 6.2 The vowel-restoration logic that's missing from it, quoted from where it does exist

```c
if (sp_is_past_k && ilen >= 4 && kin_ends_with(inner, "tse")) {
    size_t sl = ilen - 3;   /* strip "tse" */
    char cand_k[KIN_MAX_STEM];
    strncpy(cand_k, inner, sl); cand_k[sl] = 'k'; cand_k[sl+1] = '\0';
    if (kin_is_known_verb_stem(cand_k)) {
        ... return true;   /* direct match, no elision at this boundary */
    }
    /* Recover 'a' prefix for vowel-initial roots (ya+andik: a+a→a elides root's 'a') */
    char cand_ak[KIN_MAX_STEM];
    cand_ak[0] = 'a';
    strncpy(cand_ak + 1, inner, sl);
    cand_ak[sl+1] = 'k'; cand_ak[sl+2] = '\0';
    if (kin_is_known_verb_stem(cand_ak)) {
        ... return true;   /* "andik" recovered */
    }
}
```

This is the `TENSE_PAST_PERF` branch gated by the `PAST_SP_TSE` list
(Chapter 12's territory) — it tries the literal stripped candidate
first, and if that's not a known stem, explicitly prepends `'a'` and
tries again. `yanditse` reaches this branch, fails the first try
(`"ndik"`+`'k'`-shaped candidate isn't real), succeeds on the second
(`"andik"` is). The unconditional subjunctive branch in Section 4.1
has no second try.

## 6.3 `pos_tagger.c`'s guard, the relevant slice

```c
bool is_bare_phon_sp = (
    (w[0]=='c' && w[1]=='y') || (w[0]=='b' && w[1]=='y') ||
    (w[0]=='r' && w[1]=='y') || (w[0]=='z' && w[1]=='y')
);
...
if (kin_is_verb_conjugated(w, v_stem, &v_cls, &v_tense, &v_obj, &v_ext, &v_neg)
    && (v_tense != TENSE_SUBJUNCTIVE
        || (kin_is_known_verb_stem(v_stem) && !(v_obj > 0 && v_obj == cls))
        || is_bare_phon_sp)
    && ... ) {
    tok->pos = POS_VERB_CONJ;
    ...
}
```

Quoted and tested in Section 4.2. The same `is_bare_phon_sp` term
appears in three places in this one guard (lines 514, 534, 560 of
`pos_tagger.c`) — once for `PRESENT_NORA`, once for `SUBJUNCTIVE`,
once in the long tail of "other tenses." All three carry the same
scope mismatch, for the same reason.

---

# Part 7 — Looking Back, Looking Forward

## 7.1 This chapter's finding required three chapters' worth of groundwork to even ask the right question

Chapter 12 first read `verb_match_inner`'s structure and its fallback
mechanism in passing. Chapter 13 established that `verified` checks
internal consistency, not correctness — without that distinction,
Section 4.2's `byandike` result would have looked like a `verified`
problem instead of a `pos_tagger.c` problem. Chapter 14 noticed
`yandikijwe` failing and explicitly declined to chase it, naming it as
this chapter's starting point. None of those chapters' findings were
wasted groundwork; each was a precondition for being able to state
this chapter's finding precisely instead of vaguely.

## 7.2 What's still not chased

This chapter traced exactly one collision (`ya` + vowel-initial root,
unconditional `SUBJUNCTIVE` branch) to its root cause across two
files. It did not check whether the same naive-stripping pattern
recurs in any of the dozen other tense branches in `verb_match_inner`
that don't cite a `PAST_SP_TSE`-style restoration comment, and it did
not re-derive why `ext_strip` (Chapter 14) reports `"ndits"` rather
than `"ndik"` for the passive-perfect forms specifically (the passive
`-w-` and the `ts`-fusion both layer on top of the same underlying
elision). Both are real, answerable questions this chapter's method
would answer the same way it answered this one — left here, honestly,
rather than chased further.

---

# Part 8 — Practice

### Beginner

1. Predict, then verify with `kin_is_verb_conjugated`, what stem
   `"yemeza"` (subjunctive-shaped, SP `"y"`, root `"emez"`) produces,
   and explain why this one isn't affected by Section 4.1's bug even
   though its root is also vowel-initial.
2. Find one more real Kinyarwanda verb root that begins with a vowel
   (check `lexicon.c`'s `VERB_STEMS[]`) and reproduce Section 1.1's
   `yanditse`/`yandike` contrast with it.

### Intermediate

3. Section 4.2 found `is_bare_phon_sp` appears at three separate
   points in `pos_tagger.c`'s Step 8 guard. Read all three and
   determine whether fixing only the `SUBJUNCTIVE` one (line 534)
   would also close the gap for `PRESENT_NORA` (line 514) and the
   long tail (line 560), or whether each needs its own fix.
4. Construct a word starting with `ry` (one of the four
   `is_bare_phon_sp` strings not tested in this chapter) using a
   vowel-initial root, and confirm the same fabricated-stem pattern
   reproduces.

### Advanced

5. Propose the smallest fix to the unconditional `SUBJUNCTIVE` branch
   (Section 6.1) that would add the same restore-and-retry logic
   Section 6.2 already implements for `PAST_SP_TSE`, without changing
   that branch's behavior for SPs that aren't `"ya"`/`"a"`-shaped.
6. Section 5.1 argued the `is_bare_phon_sp` bypass solves a real,
   separate problem (noun/verb ambiguity) correctly. Propose a
   narrower bypass condition that preserves that fix while no longer
   waiving the known-stem check — i.e. one that answers "is this a
   verb" without also implying "is this root real."
7. Section 7.2 named two specific unchased questions. Pick one, apply
   this book's standard method — read the comment, write a real test,
   compile it, run it, compare against the claim — and report what you
   find.

---

## Key takeaways

- Kinyarwanda's vowel-contact rule means `ya` (SP) + `andik` (root)
  surfaces as `yandik`, one `a` short of simple concatenation — and
  recovering the real root requires explicitly restoring that `a`
  before checking it against the lexicon.
- `verb_match_inner`'s unconditional `SUBJUNCTIVE: ends in e` branch
  does plain prefix subtraction with no such restoration, confirmed
  directly: `yandike` produces the non-existent stem `"ndik"` instead
  of the real root `"andik"`.
- `pos_tagger.c`'s own header comment already names this exact failure
  mode — "fabricated roots" — and implements a guard requiring a known
  stem before accepting a `SUBJUNCTIVE`-tense verb reading.
- That guard has an explicit bypass, `is_bare_phon_sp`, built to solve
  a real, separate ambiguity (a word starting with literal
  `cy`/`by`/`ry`/`zy` can't be a noun in disguise) — and the bypass
  waives the known-stem check entirely, not just the noun-ambiguity
  check it was built for.
- Real, verified consequence: `yandike`, a correct word, is rejected
  outright (`Foreign/Unknown`); `byandike`, built from the identical
  wrongly-stripped root, is accepted with a complete, checkmarked
  analysis citing a fabricated verb (`kundika`) that does not exist
  in this project's lexicon.
- The fix for the root cause already exists in the same file, for a
  different tense branch, and cites this exact word (`andik`) as its
  own justification — confirmed by reading and quoting it directly.

## Sources quoted in this chapter

- `src/morphology.c`'s unconditional `SUBJUNCTIVE` branch and the
  `PAST_SP_TSE`-gated vowel-restoration branch inside
  `verb_match_inner`, first surveyed in Chapter 12.
- `src/pos_tagger.c`'s Step 8 conjugated-verb quality guard and its
  `is_bare_phon_sp` flag.
- `src/lexicon.c`'s `VERB_STEMS[]`, used to confirm `"ndik"` is not a
  known root and `"andik"` is.
- Every `pN_*.c` program in this chapter was actually compiled with
  `gcc -std=c99 -Wall -Wextra` and actually executed; the real CLI
  output in Section 4.2 was reproduced against the actual built binary.
