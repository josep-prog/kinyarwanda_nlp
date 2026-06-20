# Chapter 13 — `punctuation.c`: Clause Boundaries and Corpus-Driven Heuristics

## Where we are

Chapter 12 checked agreement *within* a clause: does this adjective's
prefix match that noun's class, does this possessive connector match its
noun. `punctuation.c` (796 lines, sixteen named rules, `P1`–`P16`) checks a
different, higher-level kind of correctness: where does one clause end and
the next begin, and does the punctuation on the page actually mark those
boundaries the way RALC 2017 and standard Kinyarwanda prose require. It
reuses Chapter 12's exact `Error`-array plumbing (`add_error`, the same
capacity+count idiom) — what's new here is *what* gets checked, and one
genuinely new engineering idea: deciding which words belong in a detection
list using **measured corpus statistics**, not intuition alone.

## 13.0 What a "clause boundary" rule actually checks, in plain English

Every other chapter's grammar rules looked at agreement between two
*words*. This file looks at the relationship between **punctuation marks**
and the **clause structure** around them. Two examples from the file's own
header comment make the shape of the problem concrete:

```
   CORRECT:    Yagiye, ariko aragaruka.   "He left, but he is coming back."
   INCORRECT:  Yagiye ariko aragaruka.    (missing comma before "ariko")

   CORRECT:    Sara arapfa; Aburahamu agura ubuvumo.
               "Sara dies; Abraham buys the tomb."  (two complete clauses)
   INCORRECT:  Sara arapfa; ariko          (";" followed by a bare fragment)
```

`ariko` ("but/however") is an **adversative conjunction** — a word that
joins two clauses in contrast — and Kinyarwanda convention requires a
comma immediately before it whenever it isn't the very first word of the
sentence. A semicolon, separately, is reserved for joining two *complete*
clauses (each with its own predicate) — using it before a fragment, or
before a word like `ariko` that wants a comma instead, is a punctuation
error a fluent reader notices immediately, the same way an English reader
notices a comma splice. `kin_check_punctuation()` is the function that
catches exactly this class of error, mechanically, across sixteen
distinct rules.

## 13.1 Reusing Chapter 12's plumbing exactly

```c
static void add_error(SentenceAnalysis *sa, ErrorType type, int idx,
                      const char *msg, const char *sug) {
    if (sa->error_count >= KIN_MAX_ERRORS) return;
    Error *e = &sa->errors[sa->error_count++];
    /* ... identical to syntax.c's add_error, Section 12.1 ... */
}
```

This is, byte for byte, the same function Chapter 12 introduced — same
bounds check, same append-via-`sa->error_count++`, same
`sa->tokens[idx].error_count++` side effect reaching back into the
`Token` the error is about. `punctuation.c` doesn't share a header
declaring this helper with `syntax.c` (it's `static`, private to each
file) — each file simply re-implements the identical four lines rather
than factoring them into a shared internal header. This is a real,
visible instance of the kind of judgment call Chapter 22's test-suite
chapter will name directly: a function this short, used internally by
exactly one file at a time, isn't obviously worth the ceremony of a shared
header just to avoid four lines of duplication — a defensible choice, even
though it does mean two files maintain their own copies of the same logic.

## 13.2 `prev_content`/`next_content`: skipping punctuation to find the real neighbor

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

Almost every rule in this file needs to answer "what's the nearest *real
word* before/after this punctuation mark" — not simply "the token at
index `i-1`," because that adjacent slot might itself be another
punctuation mark. These two functions are a bounded linear search in
exactly the spirit of Chapter 12's `scan_back_noun`, but simpler: there's
no class-matching condition to satisfy, just "skip anything tagged
`POS_PUNCTUATION` and return the first thing that isn't." Notice the two
different sentinel values on failure: `prev_content` returns `-1` (no
real index is ever negative — Chapter 9's "impossible value as failure
signal" idiom), while `next_content` returns `sa->token_count` (one past
the last valid index — also never a real index, just a different
impossible value chosen because it reads naturally in callers like
`if (nci < sa->token_count)`, which both states "a content token was
found" and is immediately ready to use as a loop bound). Same underlying
idea, two different sentinel values, each chosen to make its own most
common caller read cleanly.

## 13.3 Corpus percentages as a documented engineering decision

This is the genuinely new idea in this file, and it's worth reading the
header comments closely, because they document something no earlier
chapter's tables did: **why each specific word made it into a detection
list, with a measured number attached.**

```
 * Corpus: nyamara precedes comma in 83% of occurrences (Bibiliya Yera).
 * ...
 * EXCLUDED: 'dore' (behold/here is) — corpus ratio only 2.6x; it heads
 * many declarative sentences ("Dore ibyanditswe...") with no "!" needed.
```

Every other lexicon table in this project (Chapter 7's `ADJ_STEMS`,
`PRONOUNS`) is a **closed, deterministic set** — a word either is or isn't
a known adjective stem, with no ambiguity once you've matched it. The
words in `is_adversative()`, `is_exclamative()`, and friends are
different: they're words that *usually* signal one particular punctuation
requirement, but not with 100% certainty, the same way Chapter 6's
"verb-or-noun" guard conditions were honest, fallible heuristics rather
than perfect rules. What's new here is the *discipline* applied to
deciding the cutoff: rather than guessing which words are reliable enough
to include, the file's author measured how often each candidate word
actually co-occurred with the punctuation mark in question, across a real
corpus (the Bibiliya Yera — Kinyarwanda Bible — text), and used that
percentage as the actual inclusion criterion. `nyamara` clears the bar at
83%; `dore` is explicitly named and excluded at 2.6%, with the specific
counter-example sentence quoted directly in the comment. This is a higher
standard of honesty than "this list felt about right" — every inclusion
*and* every notable exclusion is backed by a number, and that number is
written down for a reader to check.

If you're asked in your defense *"how do you know these are the right
trigger words?"* — this is the precise, strong answer: not by intuition,
but by frequency analysis against a real text corpus, with the threshold
and the specific evidence documented inline, including the cases that
were deliberately left out and why.

## 13.4 Counting to check balance: a new use for a running difference

Chapters 9–10 used a `bool any` flag to detect "did anything change this
pass." This file introduces a different counting idiom, for a different
kind of question — "are these two kinds of marker present in equal
numbers":

```c
int open_quotes = 0, close_quotes = 0;
/* ... single forward scan over every token ... */
if (t->punct_type == PUNCT_QUOTE_OPEN)  open_quotes++;
if (t->punct_type == PUNCT_QUOTE_CLOSE) close_quotes++;
/* ... after the scan ... */
if (open_quotes != close_quotes) { /* report which side is missing */ }
```

One forward pass tallies two independent counters; only *after* the scan
finishes does the code compare them. This is the right tool specifically
because "is this sentence's quoting balanced" doesn't depend on *where*
the mismatch is — it depends only on the *final totals*. Contrast this
with `kin_check_punctuation`'s very next balance check, parentheses, which
needs something stronger: not just "are the totals equal," but "at every
point in the sentence, has a `'('` already been seen for every `')'`
encountered so far" (P14's depth tracking, Section 13.5) — a clear
illustration that "count and compare totals at the end" and "track a
running depth as you go" are two different tools for two superficially
similar-looking problems, and choosing between them depends on whether
*position*, not just *quantity*, matters.

The sign of the difference, not just its presence, is also read directly:
`bool missing_close = (open_quotes > close_quotes);` distinguishes "you
opened a quote and never closed it" from "you closed a quote you never
opened" with one comparison, letting the function report the *specific*
problem rather than a generic "quotes don't match."

## 13.5 P14: a depth counter, for a question balance counts alone can't answer

Parentheses being numerically balanced overall doesn't yet tell you
whether a *specific* terminal punctuation mark sitting between an open and
a close paren is itself wrong. P14 needs a second, nested scan with its
own local depth counter to answer a more specific question — "does the
sentence keep going after the parenthetical this `.`/`?`/`!` is trapped
inside":

```c
if ((t->punct_type == PUNCT_PERIOD || ... ) && open_parens > close_parens) {
    bool content_after_close = false;
    int depth = open_parens - close_parens;
    for (int j = i + 1; j < sa->token_count; j++) {
        if (sa->tokens[j].punct_type == PUNCT_PAREN_CLOSE) {
            depth--;
            if (depth == 0) {
                int nci = next_content(sa, j);
                content_after_close = (nci < sa->token_count);
                break;
            }
        }
        if (sa->tokens[j].punct_type == PUNCT_PAREN_OPEN) depth++;
    }
    if (content_after_close) { /* this terminal mark is wrong, flag it */ }
}
```

`open_parens > close_parens`, checked at the *outer* scan's current
position, is exactly how the function recognizes "we are currently inside
an unclosed parenthetical" without needing a separate stack data
structure — the same two running totals from Section 13.4, reused for a
second purpose. The inner loop's own `depth` counter is needed because
parentheses can nest: simply looking for the *next* `')'` wouldn't
correctly handle `"(outer (inner) still outer)"` — the depth counter
specifically tracks when execution has returned to the *original*
nesting level (`depth == 0`) before asking whether real content follows.
This is the simplest possible **stack-counting** technique — incrementing
on open, decrementing on close, watching for a return to zero — solving
the nesting problem without ever allocating an actual stack data
structure, because the only thing that matters here is the *depth
number*, not the identity of each unmatched opener.

## 13.6 Accumulating sentence-wide properties from token-by-token signals

P4 and P7 need a *sentence-level* fact — "does this sentence contain any
interrogative word at all" — built up from individual token checks during
the same single forward scan that handles everything else:

```c
bool has_interrogative = false;
bool has_exclamative   = is_ko_exclamative_initial(sa);   /* checked once, up front */
for (int i = 0; i < sa->token_count; i++) {
    /* ... */
    if (is_interrogative(t->lower)) has_interrogative = true;
    if (is_exclamative(t->lower))   has_exclamative   = true;
    /* ... */
}
/* ... after the loop: act on the accumulated flags ... */
if (has_interrogative) { /* check the sentence ends in '?' */ }
```

Notice `has_interrogative`/`has_exclamative` are only ever set to `true`,
never reset to `false` once raised — the same one-way-latch property
you'll meet again in Chapter 22's test runner (`any_fail |=`): once *any*
token in the sentence trips the flag,
the sentence-wide fact is settled, and nothing later in the scan can
un-flag it. This is the natural way to compute a sentence-wide "does
*any* token have property X" fact in a single pass, without a second,
separate scan just to check the accumulated result — the check against
the final punctuation mark only happens once, after the loop, using
whatever the flag settled on.

## 13.7 An un-extracted repetition, named honestly

Three separate blocks near the end of `kin_check_punctuation` — the
unbalanced-quote report, the unbalanced-paren report, and the
unbalanced-dash report — each need to find "the right token to attach
this sentence-level error to," and each does it with the identical
hand-written loop:

```c
int attach = sa->token_count - 1;
while (attach > 0 && sa->tokens[attach].pos == POS_PUNCTUATION &&
       sa->tokens[attach].is_sent_boundary)
    attach--;
```

This walks backward from the very last token, skipping past trailing
sentence-boundary punctuation, to find the last *meaningful* token to
attach a whole-sentence error to (Chapter 3's `Error.token_index`,
Section 3.5 again — an error about the sentence as a whole still has to
point at *some* token index). It appears three times, identically, in
this one function. Naming this plainly is more useful than pretending
otherwise: this is a clear candidate for a small shared helper (a
`last_meaningful_token(sa)` function, alongside `prev_content`/
`next_content`), and the fact that it wasn't extracted is a small,
genuine gap in this file's own internal consistency — the same kind of
honest observation Chapters 7, 13, and 21 each made about a different
file. Three repetitions of four lines is a minor cost, not a real bug,
but recognizing the missed DRY opportunity yourself is worth more in a
defense than having it pointed out to you.

## 13.8 Case study: tracing three rules over one sentence

Take the sentence `"Yagiye ariko aragaruka kuko yibagiwe ikofi."` ("He
left, but he is coming back because he forgot his coffee.") with its
commas omitted, and walk it through `kin_check_punctuation()`:

```
   Yagiye   ariko   aragaruka   kuko   yibagiwe   ikofi   .
     ↑        ↑                  ↑
   verb   ADVERSATIVE          CAUSAL
          (needs comma         (needs comma after
           before it,           the preceding verb
           P1)                  clause, P9)
```

- **P1 fires on `ariko`**: it's an adversative conjunction (Section 13.3's
  list), it isn't sentence-initial (`i > 0`), and `preceded_by_comma`
  scanning backward from its position finds the verb `Yagiye` directly
  before it with no comma in between → `ERR_MISSING_COMMA` is recorded at
  `ariko`'s token index, with the suggestion "insert a comma before
  `ariko`."
- **P9 fires on `kuko`**: `prev_content` finds `aragaruka`, tagged
  `POS_VERB_CONJ` — exactly the condition `is.*VERB_CONJ` check requires
  — and there's no comma immediately before `kuko` either → a second
  `ERR_MISSING_COMMA`, this time citing the causal meaning ("because") in
  its bilingual message.
- **P5 fires once, at the very end**: the sentence does end with `.`, and
  neither `has_interrogative` nor `has_exclamative` was ever raised by any
  token in the scan, so the terminal-punctuation check finds a real
  sentence-boundary mark present and *correctly does not* fire — a clean
  illustration that P5 only fires on a sentence with **no** terminal mark
  at all, which this one (despite its other errors) actually has.

Three independent rules, one shared scan, two real errors recorded and one
correctly suppressed — exactly the kind of multi-rule, single-pass
analysis this file performs on every sentence handed to it.

## Key takeaways

- `punctuation.c` reuses Chapter 12's `Error`-array plumbing exactly
  (duplicated, not shared via a header — a small, honest inconsistency),
  and applies it one level up: clause-to-clause structure, not
  word-to-word agreement.
- `prev_content`/`next_content` are a simpler cousin of Chapter 12's
  `scan_back_noun` — a bounded linear search with no matching condition
  beyond "isn't punctuation" — returning two different sentinel values
  (`-1` and `token_count`) chosen so each one's typical caller reads
  naturally.
- Several detection lists in this file (`is_adversative`,
  `is_exclamative`) are deliberately *not* closed, perfect sets — they're
  documented, corpus-measured heuristics, with both inclusion and
  exclusion decisions backed by a specific percentage from a real text
  corpus, not intuition.
- "Count totals, compare at the end" (quote/paren balance) and "track a
  running depth as you scan" (P14's nested-parenthetical check) are two
  different tools: use totals when only the final count matters, use a
  depth counter when *position relative to nesting* matters.
- Boolean flags can accumulate a sentence-wide fact across a single
  forward scan as a one-way latch (set to `true`, never reset) — the same
  underlying idea as `any_fail |=` in this project's test runner.
- A short block of logic repeated identically at multiple call sites
  within the same function, without being extracted into a shared helper,
  is a minor, real gap worth naming directly rather than glossing over.

## Search YouTube for

- "comma splice and conjunction punctuation rules explained"
- "corpus linguistics frequency analysis explained"
- "balanced parentheses algorithm using a counter"
- "stack depth counting without an explicit stack data structure"
- "sentinel return values in C explained"

## Coming up in Chapter 14

`corrector.c` is the shortest substantive file in the project — only 107
lines. Chapter 14 covers how it reuses almost everything from Chapter 12
(the `adj_concordance` table, `kin_vv_join`, the morpheme-label search) to
actually *generate* the corrected text a caller should use instead of the
original, completing the loop from "detect a problem" — which both
`syntax.c` and this chapter's `punctuation.c` do — to "propose a fix."
