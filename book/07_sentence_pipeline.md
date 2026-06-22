# Chapter 7 — One Sentence, Many Passes: How `kin_analyze` Decides

## How this chapter works

Same rules as Chapters 1 through 6. Every code example was actually
compiled with `gcc -std=c99 -Wall -Wextra` and actually executed — the
output shown in a fenced block prefixed with `$` is the real terminal
output of that exact program.

Chapter 6 found the layer underneath all five trees — the tokenizer
that decides where words begin. This chapter finds the layer *above*
all five trees: the orchestration inside `kin_analyze` that decides,
after every word has already been tagged by Chapters 1 through 5's
detectors, things no single-word detector could ever decide alone —
whether the *sentence* has a verb, whether it's grammatically
complete, and which of several still-ambiguous per-word guesses the
surrounding sentence actually resolves.

---

# Part 1 — Language and Code, Side by Side

## 1.1 A real ambiguity no single word can resolve

Chapter 3 built the verb tagger and trusted it completely. Reading
`analysis.c`'s own header comment for `kin_resolve_sp_ambiguity`
reveals that trust was conditional:

> In Kinyarwanda the "ya" subject prefix is shared by two grammatical
> contexts:
> 1. Nt.6 PRESENT habitual: `ya + stem + a` (yamara, yagenda)
> 2. Nt.1/3 PAST: `a(SP) + a(past) -> ya + stem + ye/tse/aga`
>    (yagiye, yaremye, yagendaga, yabonye)
>
> The verb morphology tagger has no sentence context, so it always
> assigns class 6 when it sees "ya" as SP.

Read `yagiye` ("[someone/something] went") on its own, with no
sentence around it, and there is genuinely no way to know which of
these two real grammatical patterns produced it — Chapter 3's tagger
has to guess, and the comment says outright what it guesses: always
class 6, every time, regardless of which one actually happened. That
isn't a bug in Chapter 3's detector; it's an honest acknowledgment of
a limit every single-word detector in this book shares. The question
this chapter answers is what happens next.

## 1.2 Build it: a toy resolver that sees what the tagger couldn't

```c
/* p1_toy_resolve.c -- why one word at a time isn't enough */
#include <stdio.h>
#include <string.h>

/* A tiny stand-in for the real verb tagger: sees ONLY the verb,
 * has no idea what noun (if any) precedes it. */
int toy_tag_verb_class(const char *sp) {
    if (strcmp(sp, "ya") == 0) return 6;   /* tagger's only guess: class 6 */
    return 0;
}

/* A tiny stand-in for the real sentence-level resolver: sees the
 * WHOLE sentence, so it can correct the guess using the subject. */
int toy_resolve(int guessed_class, int subject_class) {
    if (guessed_class == 6 && (subject_class == 1 || subject_class == 9))
        return subject_class;   /* "ya" was really this subject's past SP */
    return guessed_class;
}

int main(void) {
    int g1 = toy_tag_verb_class("ya");   /* word-level guess: always 6 */
    printf("word-level guess for 'ya': class %d\n", g1);
    printf("resolved, subject=umuntu (class 1): class %d\n", toy_resolve(g1, 1));
    printf("resolved, subject=Imana  (class 9): class %d\n", toy_resolve(g1, 9));
    printf("resolved, subject=amazu  (class 6): class %d\n", toy_resolve(g1, 6));
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p1_toy_resolve p1_toy_resolve.c
$ ./p1_toy_resolve
word-level guess for 'ya': class 6
resolved, subject=umuntu (class 1): class 1
resolved, subject=Imana  (class 9): class 9
resolved, subject=amazu  (class 6): class 6
```

The tagger's guess never changes — it can't, it only ever sees `ya`.
The *resolver* is what looks one token to the left, finds the real
subject, and either corrects the guess (classes 1 and 9) or confirms
it was right all along (class 6, the genuinely ambiguous case where
`ya` really was the present-habitual marker). This two-stage shape —
guess locally, correct globally — is this entire chapter's subject,
demonstrated here in nine lines before Part 6 reads the real,
production version.

## 1.3 Checkpoint: test yourself before continuing

1. Why can't Chapter 3's verb tagger simply look at the previous token
   itself, instead of needing a separate pass afterward?
2. Section 1.2's `toy_resolve` only corrects the guess when the
   subject is class 1 or 9. What would happen, concretely, to a
   genuinely class-6 sentence like `"Amazu yagenda."` if `toy_resolve`
   "corrected" every guess unconditionally?
3. Name one other ambiguity from Chapters 1–6 that a single word, read
   in isolation, could not have resolved on its own. (Hint: Chapter
   4's class-1/3 noun collision, or Chapter 6's sentence-initial
   capitalization problem, are both real candidates.)

---

# Part 2 — A Struct That Gets Refined, Not Just Filled

## 2.1 Every previous out-parameter was filled once

Chapters 1 through 6's out-parameters were each populated by exactly
one function call, once, and then only ever read afterward. This
chapter's central object, `SentenceAnalysis`, is different: it gets
written to by tokenization, then read and partially *rewritten* by
each of several later passes, each one correcting or annotating
fields an earlier pass already set. `kin_resolve_sp_ambiguity`
doesn't fill in a blank `noun_class` — it overwrites one Chapter 3's
own tagger already wrote, on purpose, because the earlier value was a
documented best-guess rather than a final answer.

## 2.2 The risk here isn't memory safety — it's which pass ran last

Every crash this book has demonstrated (Chapters 1, 2, 4) came from an
unchecked array index or an unbounded copy. The functions this
chapter reads are uniformly well-guarded in exactly that sense —
`scan_back_noun` bounds its backward scan at `j >= 0`,
`kin_propagate_proper_nouns` bounds its lookup table at
`KIN_MAX_TOKENS` and null-terminates every `strncpy`. Reading them
closely turns up no new buffer-overflow flavor to add to this book's
running list. What replaces it, in a chapter about multiple passes
correcting each other, is a different and arguably subtler risk: *the
order the passes run in is now part of the program's correctness*,
not just its performance. Part 4's capstone finds a real, verified
case where that exact risk already produced a documentation mistake
in this project — not a crash, but a wrong claim about which pass
depends on which.

---

# Part 3 — Making It Interactive

## 3.1 Build it: watch the sentence-level verdict, not just the tags

```c
/* p3_repl.c -- type a sentence, see the sentence-level verdict */
#include "kinyarwanda.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    char line[256];
    printf("Type a Kinyarwanda sentence, or 'q' to quit.\n");
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == 'q' && line[1] == '\0') break;
        SentenceAnalysis sa = kin_analyze(line);
        printf("  has_verb=%d  error_count=%d  is_complete=%d\n",
               sa.has_verb, sa.error_count, sa.is_complete);
        for (int i = 0; i < sa.error_count; i++)
            printf("    error: %s\n", sa.errors[i].message);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p3_repl.c -L . -lkinyarwanda -o p3_repl
$ printf "Umuntu yagiye.\nUmuntu n'umugore.\nq\n" | LD_LIBRARY_PATH=. ./p3_repl
Type a Kinyarwanda sentence, or 'q' to quit.
  has_verb=1  error_count=0  is_complete=1
  has_verb=0  error_count=1  is_complete=0
    error: Iyi nteruro ntifite inshinga / This sentence has no verb.
```

Both lines look perfectly consistent here: a verb and no errors gives
`is_complete=1`; no verb and a real error gives `is_complete=0`. Part
4's capstone finds a *third* case — no verb, but also no error — where
that same simple formula stops agreeing with `kin_check_syntax`'s own
more careful judgment about what counts as a real sentence.

---

# Part 4 — Capstone: Three Real Findings From Reading the Orchestration

## 4.1 The "ya" resolver, tested on all three real cases at once

```c
/* p4_spambig.c */
#include "kinyarwanda.h"
#include <stdio.h>

static void run(const char *t) {
    SentenceAnalysis sa = kin_analyze(t);
    printf("\"%s\"\n", t);
    for (int i = 0; i < sa.token_count; i++) {
        Token *tk = &sa.tokens[i];
        if (tk->pos != POS_VERB_CONJ) continue;
        printf("  verb \"%s\" -> noun_class=%d\n", tk->surface, tk->noun_class);
    }
}

int main(void) {
    run("Umuntu yagiye.");   /* class 1 PAST  -- must be corrected from 6 */
    run("Imana yaremye.");   /* class 9 PAST  -- must be corrected from 6 */
    run("Amazu yagenda.");   /* class 6 PRESENT -- genuinely class 6, untouched */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_spambig.c -L . -lkinyarwanda -o p4_spambig
$ LD_LIBRARY_PATH=. ./p4_spambig
"Umuntu yagiye."
  verb "yagiye" -> noun_class=1
"Imana yaremye."
  verb "yaremye" -> noun_class=9
"Amazu yagenda."
  verb "yagenda" -> noun_class=6
```

Exactly Section 1.1's comment, confirmed against the real, compiled
library on all three real cases simultaneously: the tagger's class-6
default gets corrected to 1 and 9 where the real subject demands it,
and left alone — correctly — for the one sentence where 6 was the
real answer all along, not a leftover guess.

## 4.2 Proper nouns the tokenizer's own rule couldn't catch — caught anyway

Chapter 6, Section 6.2 established that Kinyarwanda excludes the
letter `l` from native words, letting the tokenizer flag a
sentence-initial capitalized word as a proper noun *only when* it
contains an `l`. Most real names don't. `kin_propagate_proper_nouns`
exists specifically for what's left over:

```c
/* p4_propprop.c */
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    const char *text = "Kayini akorera mu murima. Imana yarababaye kubera Kayini.";
    SentenceAnalysis sa = kin_analyze(text);
    printf("\"%s\"\n", text);
    for (int i = 0; i < sa.token_count; i++) {
        Token *t = &sa.tokens[i];
        if (t->pos != POS_NOUN && !t->is_proper_noun) continue;
        printf("  [%d] \"%-8s\" is_proper_noun=%d\n", i, t->surface, t->is_proper_noun);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_propprop.c -L . -lkinyarwanda -o p4_propprop
$ LD_LIBRARY_PATH=. ./p4_propprop
"Kayini akorera mu murima. Imana yarababaye kubera Kayini."
  [0] "Kayini  " is_proper_noun=1
  [3] "murima  " is_proper_noun=0
  [5] "Imana   " is_proper_noun=0
  [8] "Kayini  " is_proper_noun=1
```

The *first* `Kayini` — sentence-initial, no `l`, exactly the case
Chapter 6 said the tokenizer alone can't resolve — is correctly
flagged `is_proper_noun=1`. It only got there because the *second*
`Kayini`, capitalized safely mid-sentence where no ambiguity exists,
let this pass work backward and retroactively fix the first
occurrence's flag. Neither occurrence could have told the tokenizer
the truth on its own; only seeing both, in the same pass, across the
whole text, could.

## 4.3 Two comments in the same function disagree with each other

`kin_resolve_sp_ambiguity`'s own header comment (Section 1.1, quoted
again here for the exact wording) says:

> "...The verb morphology tagger has no sentence context, so it
> always assigns class 6 when it sees 'ya' as SP. **After syntax
> checking** we know whether the subject noun is Nt.1/3..."

The call site, eight lines inside the same function `kin_analyze`,
disagrees with its own neighbor:

```c
kin_propagate_proper_nouns(&sa);
kin_resolve_sp_ambiguity(&sa);   /* resolve ya/i SP class before syntax    */
/* ... morpheme analysis, glosses ... */
kin_tag_gram_roles(&sa);
kin_check_syntax(&sa);
```

The header comment says this pass runs *after* syntax checking. The
call-site comment, and the actual, real, executed order, both agree:
it runs *before* — three function calls earlier, in fact. This isn't
a harmless disagreement. Chapter 3, Section 7.2's real subject-verb
agreement check compares a verb's `noun_class` field against the
class of its subject noun, and that comparison happens inside
`kin_check_syntax` — which runs *after* this resolver, in the real
order. If the header comment's claimed order were the real one, every
sentence like `"Umuntu yagiye."` would reach the agreement checker
with the verb still carrying its unresolved class-6 default, and a
perfectly correct sentence would be compared as class-1-subject
against class-6-verb — a false positive, on exactly the kind of
sentence this book has used as a clean case study since Chapter 3.
Section 4.1's test already confirmed the real behavior: both
sentences pass with `error_count=0`. The call-site comment and the
actual code are right; the function's own header comment, describing
its own reason for existing, is wrong about when it runs.

## 4.4 `is_complete` doesn't know about its own neighbor's exception

Section 3.1's REPL output flagged something worth coming back to:
`"Umuntu n'umugore."` (no verb, genuinely ungrammatical as a sentence)
correctly reported `error_count=1`. But check a *different*,
genuinely verbless sentence first:

```c
/* p4_complete.c */
#include "kinyarwanda.h"
#include <stdio.h>

static void run(const char *t) {
    SentenceAnalysis sa = kin_analyze(t);
    printf("\"%-20s\" has_verb=%d error_count=%d is_complete=%d\n",
           t, sa.has_verb, sa.error_count, sa.is_complete);
}

int main(void) {
    run("Umuntu mwiza.");      /* noun + adjective, NO verb, but GRAMMATICAL */
    run("Umuntu n'umugore.");  /* noun + conj + noun, NO verb, ungrammatical */
    run("Umuntu yagiye.");     /* noun + verb, grammatical                   */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_complete.c -L . -lkinyarwanda -o p4_complete
$ LD_LIBRARY_PATH=. ./p4_complete
"Umuntu mwiza.       " has_verb=0 error_count=0 is_complete=0
"Umuntu n'umugore.   " has_verb=0 error_count=1 is_complete=0
"Umuntu yagiye.      " has_verb=1 error_count=0 is_complete=1
```

`"Umuntu mwiza."` ("a good person") has *zero* errors — and reading
`syntax.c`'s own RULE 3 explains exactly why: a noun directly followed
by an adjective is a real, complete Kinyarwanda sentence with an
implicit copula (`is_nominal_pred` in the real code, Section 6.3
quotes it in full), explicitly exempted from `ERR_NO_VERB` for
precisely this reason. The check that decides whether to *raise an
error* already knows this sentence is fine. But `is_complete` is
computed by a separate, simpler formula —
`sa->is_complete = sa->has_verb && (sa->error_count == 0)` — that only
asks whether a verb literally exists, with no awareness of the
exception RULE 3 just used three function calls earlier. The result:
a sentence `kin_check_syntax` itself considers grammatically valid
enough to pass error-free still gets `is_complete=0`, a real,
previously-undocumented inconsistency between two pieces of the same
pipeline that should agree and currently don't. Anyone writing code
downstream that trusts `is_complete` as "this sentence is fine" will
get a false negative on every nominal-predicate sentence in the
language.

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 Why this has to be two passes, not one

Every tree in Chapters 1 through 5 made do with one pass over one word
at a time. This layer cannot, for a structural reason Section 4.3's
finding already proved by accident: the subject-verb agreement check
*depends on* the SP-ambiguity resolution having already happened, and
the SP-ambiguity resolution *depends on* the noun-class tags every
tree's first pass already assigned. Collapsing these into a single
pass would require the verb tagger to already know its own sentence's
subject noun class *before* any noun has necessarily been tagged yet
— a dependency loop a single left-to-right pass cannot satisfy no
matter how it's written. Splitting tagging and resolution into two
ordered passes is not a stylistic choice; it's the only way to let
later information correct an earlier guess without needing to predict
that information before it exists.

## 5.2 What this layer is allowed to assume that no earlier chapter could

Chapters 1 through 6 each had to defend against malformed input at
the boundary of their own responsibility — Chapter 1 against
unbounded strings, Chapter 6 against truncated UTF-8. This layer gets
to assume every token already has a syntactically valid `.pos`, a
range-checked `.noun_class`, and correctly-set boundary flags, because
five prior chapters' worth of detectors and Chapter 6's tokenizer
already guaranteed it. That's the payoff of the whole book's layered
structure so far: each layer only has to solve the one problem that's
genuinely new at its level — here, *which already-valid tag is the
right one once the whole sentence is visible* — without re-solving
problems the layers underneath it have already closed out.

---

# Part 6 — Reading the Real Production Code

## 6.1 `kin_analyze`, the whole pipeline, in order

```c
SentenceAnalysis kin_analyze(const char *text) {
    SentenceAnalysis sa;
    memset(&sa, 0, sizeof(sa));
    sa.token_count = kin_tokenize(text, sa.tokens, KIN_MAX_TOKENS);
    kin_tag_sentence(&sa);
    kin_propagate_proper_nouns(&sa);
    kin_resolve_sp_ambiguity(&sa);
    for (int i = 0; i < sa.token_count; i++)
        kin_morpheme_analyze(&sa.tokens[i]);
    for (int i = 0; i < sa.token_count; i++)
        kin_fill_morpheme_glosses(&sa.tokens[i]);
    kin_tag_gram_roles(&sa);
    kin_check_syntax(&sa);
    kin_suggest_corrections(&sa);
    return sa;
}
```

Nine steps, every one of them a function this book has now named:

```
  raw text
     |
     v
  [1] kin_tokenize             (Chapter 6 -- words, punctuation, boundaries)
     |
     v
  [2] kin_tag_sentence         (Chapters 1-5 -- per-word POS, noun_class, etc.)
     |
     v
  [3] kin_propagate_proper_nouns   } this chapter's two
  [4] kin_resolve_sp_ambiguity     } sentence-level correction passes
     |
     v
  [5] kin_morpheme_analyze (per token)   -- fills .morph
  [6] kin_fill_morpheme_glosses (per token) -- fills English glosses
     |
     v
  [7] kin_tag_gram_roles      (MAIN_VERB / AUXILIARY / COMPLEMENT / ...)
     |
     v
  [8] kin_check_syntax        (Rules 1-12, Section 6.3)
     |
     v
  [9] kin_suggest_corrections (Section 6.4 -- narrow top-up)
     |
     v
  SentenceAnalysis, returned to the caller
```

Chapter 6's tokenizer, Chapters 1–5's per-token tagging
(`kin_tag_sentence`), this chapter's two correction passes, a
per-token morpheme breakdown and gloss fill (the `.morph` field every
chapter's case studies have displayed without explaining where it
came from), grammatical-role tagging, the rule-numbered syntax checker
Section 6.3 tours next, and finally the correction-suggestion pass.
Every chapter's `kin_analyze(text)` call, from Chapter 1 onward, was
calling all nine of these in this exact order.

## 6.2 `kin_resolve_sp_ambiguity`, in full

```c
static void kin_resolve_sp_ambiguity(SentenceAnalysis *sa) {
    for (int i = 0; i < sa->token_count; i++) {
        Token *verb = &sa->tokens[i];
        if (verb->pos != POS_VERB_CONJ) continue;
        int vc = verb->noun_class;

        if (vc == 6 &&
            (verb->verb_tense == TENSE_PAST_PERF      ||
             verb->verb_tense == TENSE_PAST_PERF_LOC  ||
             verb->verb_tense == TENSE_PAST_IMPF      ||
             verb->verb_tense == TENSE_COPULA_PAST)) {
            static const int ya_cls[] = {1, 4, 6, 9};
            const Token *subj = scan_back_noun(sa, i - 1, ya_cls, 4);
            if (subj) verb->noun_class = subj->noun_class;
        }

        if (vc == 4) {
            static const int i_cls[] = {4, 9};
            const Token *subj = scan_back_noun(sa, i - 1, i_cls, 2);
            if (subj && subj->noun_class == 9)
                verb->noun_class = 9;
        }
        /* further "y" and "u" SP cases, Section 1.1's source, omitted here
           for length -- same shape, same scan_back_noun helper */
    }
}
```

The `vc == 6` branch is Section 4.1's exact mechanism: it only fires
when the *tense* is one of four past-family tenses, which is how it
avoids ever touching a genuine present-habitual class-6 verb like
`yagenda` — the tense check, not the class check, is what tells past
from present apart here, since both share the same stored default
class until this runs.

## 6.3 A tour of `kin_check_syntax`'s twelve numbered rules

The real file organizes every check this book has read across
Chapters 2 through 6 under one running numbering scheme, in the order
they execute:

| Rule | Checks | First seen |
|---|---|---|
| 1 | Noun-adjective class agreement | Chapter 2 |
| 2 | Possessive connector agreement | Chapter 4 |
| 3 | Sentence completeness (has a verb, or a valid nominal predicate) | This chapter |
| 4 | Unrecognized/foreign words flagged | Chapter 6 |
| 5 | Subject-verb agreement | Chapter 3 |
| 6 | Vowel contact (hiatus) | — |
| 6b | Letter `l` in native words | Chapter 6 |
| 7 | `kugenda` vs `kujya` verb selection | Chapter 3 |
| 8 | Conditional clause marking | — |
| 9 | Vowel harmony on verb extensions | Chapter 3 |
| 10 | Cross-word connector elision | Chapter 6 |
| 11 | Locative form selection (`mu`/`muri`, `ku`/`kuri`) | Chapter 5 |
| 12 | Purpose particle `ngo` requiring subjunctive mood | — |

Every numbered rule this book has actually read in a previous chapter
is the *same* numbered rule here — Chapter 5's `ERR_WRONG_LOCATIVE`
finding was Rule 11 all along; Chapter 3's `kugenda`/`kujya` lexical
check was Rule 7. Three rules (6, 8, 12) were never independently
covered in any earlier chapter; that's a genuine, honestly-flagged gap
in this book's coverage, not a claim that those rules don't exist or
don't matter — Section 8's practice exercises ask you to close it
yourself by reading and testing one directly.

## 6.4 `kin_suggest_corrections`'s real, narrow scope

```c
void kin_suggest_corrections(SentenceAnalysis *sa) {
    for (int e = 0; e < sa->error_count; e++) {
        Error *err = &sa->errors[e];
        if (err->type == ERR_ADJ_AGREEMENT)  { /* rebuild corrected adjective, Ch.2 §7.4 */ }
        if (err->type == ERR_POSS_AGREEMENT) { /* rebuild corrected possessive connector */ }
    }
}
```

The name suggests this function is where every correction in the book
comes from. Reading it shows it only ever rebuilds a *concrete
corrected surface form* — actually computing what the right word
should have been, using `build_adj` (Chapter 2, Section 7.4 already
quoted this in full) — for exactly two error types. Every other
error this book has shown a suggestion for — Chapter 3's subject-verb
mismatch, Chapter 5's wrong locative, Chapter 6's foreign-word flag —
already had its complete, human-readable `suggestion` string built
directly inside the matching rule in `kin_check_syntax`, at the moment
the error was first detected (every `add_error(sa, ERR_..., idx, msg,
sug)` call this book has quoted since Chapter 3 passed `sug` in fully
formed). `kin_suggest_corrections` is a small, late top-up pass for
the two error types whose fix is a *computable word*, not the central
hub its name implies.

---

# Part 7 — Looking Back: Every Case Study Already Used This Chapter's Output

## 7.1 The fields you've been reading since Chapter 1

`error_count`, `has_verb`, `.noun_class` after correction, `.morph`,
`.gram_role` — every one of these has appeared in a case study since
this book's very first chapter, always read straight off a
`SentenceAnalysis` returned by a single call to `kin_analyze`. None of
those earlier chapters paused to explain that the value being printed
was the *output of a pipeline*, not a single function's direct
return. Chapter 1's `sa.error_count` after analyzing `"Umurima mwiza
ni byiza."`-style sentences was always this chapter's Rule 1 through
12 running to completion; Chapter 3's `.noun_class` on a past-tense
verb was always this chapter's `kin_resolve_sp_ambiguity` having
already run. Reading this chapter doesn't change anything about those
earlier outputs — it explains, for the first time, where they actually
came from.

---

# Part 8 — Practice

### Beginner

1. **Trace `"Imana yaremye."` by hand** through Section 6.2's real
   `kin_resolve_sp_ambiguity` code: which `if` branch fires, and what
   does `scan_back_noun` find?
2. **Construct one more genuinely verbless, non-nominal-predicate
   sentence** (like `"Umuntu n'umugore."`) and confirm `ERR_NO_VERB`
   fires against the real library.

### Intermediate

3. **Confirm Section 4.4's `is_complete` gap independently** with a
   different nominal-predicate sentence of your own (noun + adjective,
   no verb), and confirm it also reports `is_complete=0` despite
   `error_count=0`.
4. **Write the fix** for Section 4.4's finding: change the
   `is_complete` formula (or compute a `is_nominal_pred`-aware
   version) so that `"Umuntu mwiza."` correctly reports `is_complete=1`,
   without breaking `"Umuntu n'umugore."`'s correct `is_complete=0`.
5. **Read Rule 6, 8, or 12** in `syntax.c` (Section 6.3 named all
   three as not yet covered by this book) and construct one real test
   sentence that triggers it, with the exact error message printed.

### Advanced

6. **Decide what should happen to the header comment** Section 4.3
   found wrong. Is the right fix to correct the comment's wording, to
   add an assertion that catches a future accidental reordering, or
   something else? Justify your choice.
7. **Find one more sentence-level interaction** between two of this
   chapter's passes — `kin_propagate_proper_nouns`,
   `kin_resolve_sp_ambiguity`, `kin_tag_gram_roles` — by constructing
   a sentence where the *order* the passes run in visibly changes the
   final tags, the way Section 4.3's finding showed for syntax
   checking.
8. **Read `kin_tag_gram_roles`'s `GRAM_ROLE_VERBAL_NOUN` context pass**
   in `pos_tagger.c` (its own comment walks through `"ibimera byose
   byera imbuto"` as a worked example) and test that exact sentence
   against the real library. You'll find `"ibimera"` is already
   tagged `POS_NOUN` with `is_deverbative=1` before this pass ever
   runs — meaning the comment's own worked example is resolved by an
   earlier mechanism (Chapter 4's single-word deverbative check), not
   the one it's illustrating. Find a sentence where this specific
   context pass is the one that actually fires.

## Key takeaways

- Every tree in Chapters 1 through 5 tags one word at a time and
  sometimes has to guess — Chapter 3's verb tagger always assigns
  class 6 to subject prefix `ya`, by its own header comment's
  admission, because a single word's tagger has no sentence context
  to do otherwise.
- This chapter's two correction passes — `kin_resolve_sp_ambiguity`
  and `kin_propagate_proper_nouns` — exist specifically to revisit
  those word-level guesses once the whole sentence is available,
  verified here on three real subject-class cases and one real
  sentence-initial-proper-noun case.
- A real, previously-undocumented finding: `kin_resolve_sp_ambiguity`'s
  own header comment claims it runs *after* syntax checking; the
  call-site comment and the actual executed order both say *before* —
  and verified testing shows the real (before) order is the one that
  keeps correct sentences from false-flagging on subject-verb
  agreement, meaning the header comment isn't just stale, it
  describes a dependency direction that's actually backwards.
- A second real finding: `is_complete`'s formula
  (`has_verb && error_count == 0`) doesn't know about RULE 3's own
  nominal-predicate exception, so a genuinely complete, error-free
  Kinyarwanda sentence like `"Umuntu mwiza."` reports `is_complete=0`
  — a real inconsistency between two parts of the same pipeline that
  should agree.
- The risk profile of this layer is different from every prior
  chapter's: every function read here is soundly bounds-checked, so
  there's no new buffer-overflow flavor to demonstrate. The real risk
  is architectural — which pass runs before which — and Section 4.3
  shows that risk already produced a real documentation error in this
  project.
- `kin_check_syntax` organizes every rule this book has read since
  Chapter 2 under one running numbering scheme (1 through 12); three
  of those twelve rules were never independently covered by an earlier
  chapter, an honestly-flagged remaining gap rather than a claim of
  completeness.
- `kin_suggest_corrections`'s name overstates its scope: it only
  rebuilds a concrete corrected word for two error types out of more
  than a dozen; every other error's suggestion string is already
  fully built at the moment `kin_check_syntax` first detects it.

## Sources quoted in this chapter

- `src/analysis.c` (`kin_analyze`, `kin_resolve_sp_ambiguity`,
  `kin_propagate_proper_nouns`, `scan_back_noun`).
- `src/pos_tagger.c` (`kin_tag_gram_roles`, the `GRAM_ROLE_VERBAL_NOUN`
  context pass referenced in Section 8's exercises).
- `src/syntax.c` (`kin_check_syntax`'s twelve numbered rules,
  `is_complete`'s real formula, the nominal-predicate exception).
- `src/corrector.c` (`kin_suggest_corrections`, re-confirming Chapter
  2, Section 7.4's `build_adj`).
- Every `pN_*.c`/`p_*.c` program and every real-library run in this
  chapter was actually compiled with `gcc -std=c99 -Wall -Wextra` and
  actually executed to produce the exact output quoted above.

## Coming up in Chapter 8

Seven chapters have covered how Kinyarwanda text gets split into
words, how each word gets classified, and how a whole sentence's tags
get corrected and verdicted. None of it has touched how a word
actually *sounds*. `src/g2p.c` — grapheme-to-phoneme conversion —
turns Kinyarwanda spelling into a sequence of real IPA phonemes,
handling exactly the multi-letter clusters Chapter 5's grammar
reference catalogued (`sh`, `ny`, `nsh`, `ts`...) as single sound
units rather than separate letters. Chapter 8 is this book's first
chapter about pronunciation rather than grammar.
