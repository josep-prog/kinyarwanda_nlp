# Chapter 10 — The Marks Between Words: Punctuation Placement Rules

## How this chapter works

Same rules as Chapters 1 through 9. Every code example was actually
compiled with `gcc -std=c99 -Wall -Wextra` and actually executed — the
output shown in a fenced block prefixed with `$` is the real terminal
output of that exact program.

Chapter 7 sampled `kin_check_syntax`'s twelve numbered rules and found
several of them were about punctuation placement — but `kin_check_syntax`
only *contains* a few of those rules secondhand. The dedicated engine
is `src/punctuation.c`: 796 lines, sixteen numbered rules, and (a first
for this book) real corpus-derived percentages cited rule by rule, not
just once as supporting evidence. It is also the first chapter whose
rules are written entirely in terms of *part-of-speech tags and clause
structure* rather than raw characters — which makes the two real bugs
this chapter finds a different flavor from anything Chapters 6, 8, or
9 turned up.

---

# Part 1 — Language and Code, Side by Side

## 1.1 A comma rule backed by an actual corpus measurement

`punctuation.c`'s own header comment states rule P1 with a number this
book hasn't seen attached to a grammar rule before:

> P1 – Comma before adversative/concessive conjunctions: 'ariko',
> 'naho', 'cyakora', 'icyakora', 'ahubwo', 'nyamara' in
> non-sentence-initial position require a comma immediately before
> them. ... Corpus: nyamara precedes comma in 83% of occurrences
> (Bibiliya Yera).

This isn't a rule someone wrote down because it sounded right — it's
a rule that was checked against 83% of real occurrences of "nyamara"
in a 30,984-sentence corpus (the same "Bibiliya Yera" text this book
first cited back in Chapter 5 for the locative-connector rules) before
being added to this engine. Run the rule's own two contrasting
examples through the real library:

```c
#include "kinyarwanda.h"
#include <stdio.h>

static void show(const char *s) {
    SentenceAnalysis sa = kin_analyze(s);
    kin_check_punctuation(&sa);
    printf("\"%s\"\n", s);
    printf("  errors: %d\n", sa.error_count);
    for (int i = 0; i < sa.error_count; i++)
        printf("    %s\n", sa.errors[i].message);
}

int main(void) {
    show("Yagiye, ariko aragaruka.");
    show("Yagiye ariko aragaruka.");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p1_comma.c -L . -lkinyarwanda -o p1_comma
$ LD_LIBRARY_PATH=. ./p1_comma
"Yagiye, ariko aragaruka."
  errors: 0
"Yagiye ariko aragaruka."
  errors: 1
    Icyungo 'ariko' gikeneye koma imbere yacyo. / Conjunction 'ariko' requires a comma before it.
```

Notice the function call shape: `kin_analyze` runs first, and
`kin_check_punctuation` is a *second*, separate call against the same
`SentenceAnalysis`. Chapter 7's diagram of `kin_analyze`'s nine
internal steps never included punctuation checking — this chapter's
entire engine lives outside that pipeline, called explicitly by
`validator.c` afterward. Section 5.2 explains why.

## 1.2 Build it: a checker that only knows one rule, and what it misses

```c
/* p1_toy_one_rule.c -- only checks for missing terminal punctuation */
#include "kinyarwanda.h"
#include <stdio.h>
#include <string.h>

static void show(const char *s) {
    size_t len = strlen(s);
    char last = len ? s[len - 1] : '\0';
    bool ok = (last == '.' || last == '?' || last == '!');
    printf("\"%s\" -> %s\n", s, ok ? "looks fine" : "MISSING terminal punctuation");
}

int main(void) {
    show("Yagiye ariko aragaruka.");
    show("Yagiye ariko aragaruka");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra p1_toy_one_rule.c -o p1_toy_one_rule
$ ./p1_toy_one_rule
"Yagiye ariko aragaruka." -> looks fine
"Yagiye ariko aragaruka" -> MISSING terminal punctuation
```

The toy correctly catches the one thing it was written to catch, and
gives a clean bill of health to the first sentence — exactly the
sentence Section 1.1's real engine flags for a missing comma. A
checker that only knows about the period at the end of a sentence has
no way to see the conjunction in the middle of it; `punctuation.c`'s
sixteen rules each look at a different, specific place in the
sentence, and Part 2 explains the one piece of shared machinery that
lets all sixteen ask "what real word is near this punctuation mark"
without each reinventing the answer.

## 1.3 Checkpoint: test yourself before continuing

1. `is_adversative()` recognizes `ariko`, `naho`, `cyakora`,
   `icyakora`, `ahubwo`, `nyamara`. Which one of these does the header
   comment cite a specific corpus percentage for, and which others
   does it not?
2. The toy in 1.2 would also give "looks fine" to `"Yagiye; ariko
   aragaruka."` — a sentence with a real punctuation problem. What's
   wrong with that sentence, and which of P1 through P16 (skim the
   header comment in `punctuation.c` if needed) catches it?

---

# Part 2 — Sixteen Rules, One Shared Way of Asking "What's the Nearest Real Word?"

## 2.1 This is the first chapter whose rules are about tags, not characters

Chapter 6's tokenizer worked directly on raw bytes. Chapter 8's G2P
worked on raw letters. Chapter 9's orthography engine worked on raw
morpheme strings. `punctuation.c` works on neither bytes nor letters —
every one of its sixteen rules is stated in terms of `POS_VERB_CONJ`,
`POS_NOUN`, `POS_ADJECTIVE`, `punct_type`, fields no earlier chapter
in this book could have populated before `kin_analyze` ran. This
chapter only exists because Chapters 1 through 7 already did their
jobs — it is the first module in the whole project whose own rules
*require* that everything upstream has already succeeded:

```
  raw text
     |
     v
  kin_tokenize()        (Ch.6 -- bytes become Token[], no tags yet)
     |
     v
  kin_analyze()          (Ch.1-7 -- every Token now has a POS tag,
     |                     a noun_class, a gram_role, a morph...)
     v
  ┌───────────────────────────────────┐
  │ kin_check_punctuation()  (Ch.10)   │  every rule here reads tags
  │ reads POS_VERB_CONJ, POS_NOUN,     │  this function never sets --
  │ punct_type, is_sent_boundary ...   │  it can only run AFTER
  └─────────────────┬───────────────────┘  kin_analyze has finished
     |
     v
  sa->errors[] (same Error struct kin_check_syntax already used)
```

## 2.2 `prev_content` and `next_content`: skip the punctuation, find the word

```c
static int prev_content(const SentenceAnalysis *sa, int i) {
    for (int j = i - 1; j >= 0; j--)
        if (sa->tokens[j].pos != POS_PUNCTUATION) return j;
    return -1;
}

static int next_content(const SentenceAnalysis *sa, int i) {
    for (int j = i + 1; j < sa->token_count; j++)
        if (sa->tokens[j].pos != POS_PUNCTUATION) return j;
    return sa->token_count;
}
```

Rule P6 ("no comma between a noun and its adjective") needs to know
whether the token *immediately to either side of a comma* is a noun
and an adjective — but "immediately to either side" in token-index
terms could land on another punctuation mark first if the input has
unusual spacing or stray marks. Every rule in this file that needs to
ask "what's the nearest actual word here" goes through these two
functions instead of writing `tokens[i-1]`/`tokens[i+1]` directly —
one shared, correct definition of "nearest content token," reused by
P2, P3, P6, P8, P9, P14, P15, and P16, rather than eight slightly
different ad-hoc versions of the same idea.

---

# Part 3 — Making It Interactive

## 3.1 Build it: run both calls, the way `validator.c` actually does

```c
/* p3_repl.c -- kin_analyze, then kin_check_punctuation, exactly in
 * the order validator.c's own header comment documents */
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
        kin_check_punctuation(&sa);
        printf("  error_count: %d\n", sa.error_count);
        for (int i = 0; i < sa.error_count; i++)
            printf("    %s\n", sa.errors[i].message);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p3_repl.c -L . -lkinyarwanda -o p3_repl
$ printf "Yagiye ariko aragaruka.\nUmuntu munini.\nq\n" | LD_LIBRARY_PATH=. ./p3_repl
Type a Kinyarwanda sentence, or 'q' to quit.
  error_count: 1
    Icyungo 'ariko' gikeneye koma imbere yacyo. / Conjunction 'ariko' requires a comma before it.
  error_count: 0
```

The second line — a clean noun phrase with no internal punctuation at
all — correctly produces zero errors. Part 4 feeds this exact REPL a
sentence using one of Kinyarwanda's most common words, and the result
is far less reassuring.

---

# Part 4 — Capstone: Two Real Findings From Reading the Production Code

## 4.1 The headline bug: an extremely common word gets accused of introducing a quote it never opens

`is_speech_particle`'s own comment justifies every entry in its list
with a measured corpus rate:

> Covers the full Kinyarwanda -ti- conjugation family (SP + ti): ati
> (3sg), bati (3pl), uti (2sg), nti (1sg), iti (cl.7/9), riti (cl.5),
> muti (2pl), tuti (1pl incl.), ziti (cl.10), kiti (cl.7), hati
> (locative), kuti (cl.15/inf.), ngo (evidential). 'biti' deliberately
> excluded — homonym with igiti (tree) plural. Corpus rates (% of
> occurrences that precede a quote): all ≥89%.

Read that list again: every single entry except one is identified by
a grammatical person/class marker (3sg, 3pl, 2sg, cl.7/9, locative...)
— the shared `-ti` shape this rule is actually about. `ngo` is the one
entry tagged `(evidential)` instead, a completely different
grammatical category: a hearsay/reportative particle ("it is said
that..."), not a conjugated form of "say" at all. The comment's
closing claim — "all ≥89%" — folds `ngo` into the same blanket
percentage as the rest of the list, but never shows `ngo`'s own
measured rate the way it carefully gives one for every `-ti` form, and
never explains why a structurally different particle belongs in the
same boolean check. Test the most common real use of `ngo` against
the real engine:

```c
/* p4_ngo.c -- "ngo" used as ordinary hearsay, not a direct quote */
#include "kinyarwanda.h"
#include <stdio.h>

static void show(const char *s) {
    SentenceAnalysis sa = kin_analyze(s);
    kin_check_punctuation(&sa);
    printf("\"%s\"\n", s);
    printf("  errors: %d\n", sa.error_count);
    for (int i = 0; i < sa.error_count; i++)
        printf("    %s\n", sa.errors[i].message);
}

int main(void) {
    show("Ngo arashaka kuza.");      /* "[they say] he wants to come" */
    show("Ati: \"Murakoze.\"");      /* genuine direct speech, for contrast */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_ngo.c -L . -lkinyarwanda -o p4_ngo
$ LD_LIBRARY_PATH=. ./p4_ngo
"Ngo arashaka kuza."
  errors: 1
    'Ngo' itangira ijambo rivugwa ariko nta koma ndende (':') cyangwa nta kururika ('"') bikurikiraho. / 'Ngo' introduces direct speech but is not followed by ':' or '"'.
"Ati: "Murakoze.""
  errors: 0
```

"Ngo arashaka kuza" is a completely ordinary, grammatically correct
Kinyarwanda sentence — reported speech, the everyday "they say he
wants to come" construction, with no quotation involved at all — and
the engine flags it as a punctuation error, demanding a colon or
opening quote that nothing in the sentence actually calls for. `ngo`
is also one of the most frequent words in ordinary Kinyarwanda prose,
which makes this false positive considerably more consequential than
a rare edge case: any text using `ngo` in its ordinary hearsay sense
— almost certainly its *more* common sense than introducing a literal
quotation — gets a spurious error on every occurrence.

## 4.2 Two different definitions of "a complete clause" in the same file

Rule P11 (semicolons must separate two complete clauses) defines
"complete clause" through a dedicated helper:

```c
static bool has_predicate(const SentenceAnalysis *sa, int from, int to) {
    for (int i = from; i < to && i < sa->token_count; i++) {
        if (sa->tokens[i].pos == POS_VERB_CONJ)
            return true;
        if (strcmp(sa->tokens[i].lower, "ni") == 0 ||
            strcmp(sa->tokens[i].lower, "si") == 0)
            return true;
    }
    return false;
}
```

A clause counts as complete if it has a conjugated verb *or* the
copula `ni`/`si` — correctly recognizing that "Ni byiza" ("It is
good") is every bit as complete a predicate as "Yagiye" ("He went"),
even though `ni` itself is tagged `POS_VERB_CONJ` by the tagger but the
*adjective* that completes the copula clause is not. But rules P8 and
P9 (the comma required before sequential `maze` and causal `kuko`)
check completeness a different way — by looking only at
`prev_content`'s raw POS tag, with no copula case at all:

```c
if ((strcmp(t->lower, "maze") == 0 || strcmp(t->lower, "kuko") == 0) &&
    i > 0 && !preceded_by_comma(sa, i)) {
    int pci = prev_content(sa, i);
    if (pci >= 0 && sa->tokens[pci].pos == POS_VERB_CONJ) {
        ...
```

Test a verb-headed clause and a copula-headed clause side by side:

```c
/* p4_predicate.c -- the same connector, two different clause shapes */
#include "kinyarwanda.h"
#include <stdio.h>

static void show(const char *s) {
    SentenceAnalysis sa = kin_analyze(s);
    kin_check_punctuation(&sa);
    printf("\"%s\" -> errors: %d\n", s, sa.error_count);
}

int main(void) {
    show("Yagiye maze atuza.");     /* clause ends in a verb */
    show("Ni byiza maze atuza.");   /* clause ends in an adjective, via copula 'ni' */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_predicate.c -L . -lkinyarwanda -o p4_predicate
$ LD_LIBRARY_PATH=. ./p4_predicate
"Yagiye maze atuza." -> errors: 1
"Ni byiza maze atuza." -> errors: 0
```

The first sentence is correctly flagged: a verb clause directly
followed by `maze` with no comma. The second sentence — by P11's own
definition of "complete clause," just as complete a predicate ("Ni
byiza" = "It is good") — gets no warning at all, purely because the
word sitting immediately before `maze` is the adjective `byiza`, not
a `POS_VERB_CONJ` token, and `prev_content` only looks at the single
nearest content token, never asking whether *that token's own clause*
might still satisfy P11's more careful definition. The same file
defines "is there a complete predicate here" two different ways, and
only one of the two ever sees a copula clause as complete.

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 Why corpus-derived word lists instead of a general syntactic rule

`is_adversative`, `is_exclamative`, `is_interrogative`, and
`is_speech_particle` are all flat lists of specific words, not general
syntactic categories — there is no single part-of-speech tag this
project's tagger assigns that means "requires a comma before it" or
"signals an exclamative sentence." That's a deliberate, evidence-based
choice rather than a missed abstraction: Kinyarwanda doesn't mark
"this sentence is exclamative" with a single universal grammatical
feature the way it marks noun class with a prefix — whether a given
particle reliably signals exclamation is an empirical question about
how that *specific word* is actually used, which is exactly why the
header comments justify each list entry with a measured corpus rate
rather than a grammatical rule. A general syntactic test would need to
already know the answer this list-plus-percentage approach is
measuring directly.

## 5.2 Why this isn't `kin_analyze`'s tenth step

Chapter 7's diagram of `kin_analyze`'s nine internal steps had no
slot for punctuation checking, and that omission was real, not an
oversight in this book's diagram. `validator.c`'s own header comment
spells out the actual call sequence:

> For each sentence the full pipeline runs: `kin_analyze()` →
> `kin_check_punctuation()` → `print_validation_report()`.

Punctuation placement is a different *kind* of question from
everything `kin_analyze` resolves internally. Chapters 1 through 7's
pipeline answers "what is this word, and does the sentence hang
together grammatically" — questions about words and their
relationships. This chapter's sixteen rules answer "are the marks
between the words the ones a written Kinyarwanda sentence is supposed
to have" — a question that needs every earlier answer already settled
(you can't ask whether a comma belongs between a noun and an
adjective until you know which tokens are a noun and an adjective),
but isn't itself part of producing those answers. Keeping it a
separate, explicitly-sequenced call — rather than folding 796 more
lines into `kin_analyze`'s own body — keeps that boundary visible in
the code, not just in this book's prose.

---

# Part 6 — Reading the Real Production Code

## 6.1 The structure of `kin_check_punctuation`: one pass over tokens, then five whole-sentence checks

```c
void kin_check_punctuation(SentenceAnalysis *sa) {
    if (sa->token_count == 0) return;
    bool has_interrogative = false;
    bool has_exclamative   = is_ko_exclamative_initial(sa);
    int  open_quotes = 0, close_quotes = 0, open_parens = 0, close_parens = 0;
    int  prose_dashes = 0;

    for (int i = 0; i < sa->token_count; i++) {
        /* punctuation tokens: P12, P13, P16, P14, P6, P10, P11 */
        /* content tokens: P4/P7 tracking, P1, P2, P8/P9, P15, P3 */
    }

    /* whole-sentence checks, after the loop: */
    /* P12 quote balance, P13 paren balance, P16 dash balance,   */
    /* P4 interrogative ending, P7 exclamative ending, P5 missing terminal */
}
```

Fourteen of the sixteen rules fire *inside* the single left-to-right
scan, each rule reading only the one token currently under
examination plus whatever `prev_content`/`next_content` can see from
it. The five checks after the loop (P12, P13, P16, P4, P7, P5 — six,
not five; P4 and P7 share the interrogative-takes-priority logic)
need the *whole sentence's* tally — you can't know whether quotes are
balanced, or whether a question mark belongs at the end, until every
token has been seen. That split — "per-token rules in one pass,
whole-sentence rules after it" — is the same shape as a checksum: most
of the work streams through one token at a time, but the final verdict
needs everything.

## 6.2 `is_speech_particle`, the function behind Section 4.1's finding

```c
static bool is_speech_particle(const char *lower) {
    return (strcmp(lower, "ati")  == 0 || strcmp(lower, "bati") == 0 ||
            strcmp(lower, "uti")  == 0 || strcmp(lower, "nti")  == 0 ||
            strcmp(lower, "iti")  == 0 || strcmp(lower, "riti") == 0 ||
            strcmp(lower, "muti") == 0 || strcmp(lower, "tuti") == 0 ||
            strcmp(lower, "ziti") == 0 || strcmp(lower, "kiti") == 0 ||
            strcmp(lower, "hati") == 0 || strcmp(lower, "kuti") == 0 ||
            strcmp(lower, "ngo")  == 0);
}
```

Thirteen `strcmp` calls in one boolean expression, exactly the flat
"closed list" shape Chapter 5 established for invariable words — and,
as Chapter 5 also found in `INVARIABLES[]`, a flat list like this is
only as correct as the linguistic claim that every entry belongs to
the *same* category. Twelve of these thirteen do; `ngo`, by the
function's own comment, doesn't.

## 6.3 `has_predicate` vs. the raw `prev_content` check, the functions behind Section 4.2's finding

```c
static bool has_predicate(const SentenceAnalysis *sa, int from, int to) {
    for (int i = from; i < to && i < sa->token_count; i++) {
        if (sa->tokens[i].pos == POS_VERB_CONJ) return true;
        if (strcmp(sa->tokens[i].lower, "ni") == 0 ||
            strcmp(sa->tokens[i].lower, "si") == 0) return true;
    }
    return false;
}
```

`has_predicate` scans an entire *range* of tokens (a whole clause) and
recognizes two different ways a clause can be complete. P8/P9's check
— `sa->tokens[pci].pos == POS_VERB_CONJ` where `pci` is a single index
from `prev_content` — looks at exactly one token and recognizes only
one of those two ways. `has_predicate` already existed, in the same
file, by the time P8 and P9 were written; nothing technical stopped
either rule from calling it instead of writing its own narrower check.

---

# Part 7 — Looking Back, Looking Forward

## 7.1 The last layer that only ever adds errors, never changes a tag

Every chapter since Chapter 7 has added a *different kind* of
correction pass to this project's pipeline: Chapter 7's sentence-level
resolvers rewrote ambiguous tags; Chapter 9's orthography engine
rewrites entire surface words. This chapter's sixteen rules never
rewrite anything — they only ever append to `sa->errors[]`, using the
exact same `Error` struct `kin_check_syntax` already populated in
Chapter 7. By the time `kin_check_punctuation` runs, every tag this
book has discussed since Chapter 1 is already final; this chapter's
only job is to look at the finished picture and report what's
wrong with the marks around it.

## 7.2 Checkpoint: what you should now be able to explain

- Why this chapter's rules are the first in the book defined entirely
  in terms of POS tags and clause structure, never raw bytes.
- Why `prev_content`/`next_content` exist as shared helpers rather
  than each rule writing its own neighbor-finding logic.
- The real, verified case where a very common Kinyarwanda particle
  triggers a punctuation error it shouldn't, and exactly which
  sentence in this chapter demonstrates it.
- Why `kin_check_punctuation` is called as an explicit second step
  after `kin_analyze`, never folded into `kin_analyze` itself.

---

# Part 8 — Practice

### Beginner

1. Using the header comment's own examples, predict whether
   `"Sara arapfa; Aburahamu agura ubuvumo."` and `"Sara arapfa; ariko"`
   each produce 0 or more than 0 errors, then verify with
   `kin_check_punctuation`.
2. `preceded_by_comma` is written as a `for` loop. Trace it by hand for
   any input: does its body ever reach a second iteration? What does
   that tell you about rewriting it without a loop at all?

### Intermediate

3. Section 4.1 found `ngo` wrongly flagged in its hearsay sense.
   Construct one sentence where `ngo` *does* genuinely introduce a
   direct quotation (check `is_speech_particle`'s own comment for the
   expected following punctuation) and confirm the rule correctly
   stays silent on that one.
4. Section 4.2 found P8/P9 missing a copula-headed clause that P11's
   `has_predicate` would recognize. Construct the equivalent test for
   `kuko` instead of `maze`, and confirm the same gap reproduces.
5. P2 (`kandi`/`cyangwa`/`ndetse` joining two verb clauses) checks
   `prev_verb` using `pci >= 0 && (... POS_VERB_CONJ || ... POS_VERB_INF)`
   — the same single-token shape as P8/P9. Test whether P2 has the
   same copula blind spot Section 4.2 found, using a sentence like
   `"Ni byiza kandi arakora."`

### Advanced

6. Propose a minimal change to P8 and P9 that would let them reuse
   `has_predicate` (scanning back to the start of the clause) instead
   of checking only the single nearest content token, without
   changing `has_predicate`'s own signature.
7. Section 4.1's fix isn't simply "remove `ngo` from the list" — `ngo`
   genuinely can introduce direct speech in some constructions.
   Sketch, in words, what additional information `is_speech_particle`
   would need access to (beyond the single lowercased word) to
   distinguish hearsay `ngo` from quote-introducing `ngo`.

---

## Key takeaways

- `punctuation.c` is the first module in this book to justify its
  rules with measured corpus percentages cited rule by rule (83% for
  `nyamara` before a comma, ≥89% for every `-ti` speech particle
  before a quote), not just as one-time supporting evidence.
- It's also the first chapter whose rules are defined entirely in
  terms of POS tags and clause structure rather than raw characters —
  every rule here depends on Chapters 1 through 7 having already
  finished their work, which is also why this engine runs as an
  explicit second call after `kin_analyze`, never folded into
  `kin_analyze`'s own nine-step pipeline.
- `prev_content`/`next_content` are the one piece of shared machinery
  every neighbor-aware rule in this file reuses, rather than each of
  eight different rules writing its own slightly different "skip
  punctuation, find the nearest word" logic.
- The headline real finding: `is_speech_particle`'s own comment
  identifies `ngo` as grammatically different from the twelve `-ti`
  forms it's bundled with (an evidential/hearsay particle, not a
  conjugated speech verb), gives every other entry its own corpus
  percentage but never one for `ngo`, and the real, common, everyday
  use of `ngo` as ordinary reported speech gets a false "missing
  colon or quote" error as a result.
- A second real finding: this file defines "is there a complete
  clause here" two different ways — `has_predicate` (P11) correctly
  recognizes a copula clause like "Ni byiza" as complete, while P8 and
  P9's narrower single-token check does not, so the exact same
  missing-comma problem that's correctly caught after a verb clause
  goes uncaught after a copula clause.
- Every one of this chapter's sixteen rules only ever appends to
  `sa->errors[]` — unlike Chapter 7's tag-rewriting passes or Chapter
  9's surface-word-rewriting engine, nothing here changes a tag or a
  spelling; by the time this chapter runs, every earlier chapter's
  work is already final.

## Sources quoted in this chapter

- `src/punctuation.c` (`kin_check_punctuation` and all sixteen rules,
  `is_speech_particle`, `is_adversative`, `is_clause_coordinator`,
  `has_predicate`, `prev_content`, `next_content`, `preceded_by_comma`).
- `src/validator.c` (the documented `kin_analyze()` →
  `kin_check_punctuation()` → `print_validation_report()` call
  sequence).
- Every `pN_*.c` program in this chapter was actually compiled with
  `gcc -std=c99 -Wall -Wextra` and actually executed to produce the
  exact output quoted above.

## Coming up in Chapter 11

Ten chapters have built every individual piece of this engine — words,
sentences, sound, spelling, punctuation. None has shown how those
pieces actually become the program a user runs. Chapter 11 closes this
book by reading `src/validator.c` and `src/main.c` together: how a raw
file on disk becomes a bilingual validation report, how the `--g2p`,
`--gloss`, and `--validate` CLI flags each wire a different subset of
this book's ten chapters into one binary, and what's actually left to
build if you wanted to extend this project yourself.
