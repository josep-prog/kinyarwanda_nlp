# Chapter 16 — `analysis.c`: The Orchestrator, and Returning Structs by Value

## The capstone function

Every chapter from 4 through 14 covered one stage of a pipeline. This
chapter covers the one function that actually *runs* that pipeline, in
order: `kin_analyze()`. Reading it now, with everything else already in
your head, should feel like watching a single sentence of credits roll
past — and that's deliberate; this chapter is partly a review, walking
through `kin_analyze()` line by line and naming exactly which earlier
chapter explains each call.

## 16.1 `kin_analyze()`: one function, one line per pipeline stage

```c
SentenceAnalysis kin_analyze(const char *text) {
    SentenceAnalysis sa;
    memset(&sa, 0, sizeof(sa));

    sa.token_count = kin_tokenize(text, sa.tokens, KIN_MAX_TOKENS);   /* Ch.4  */
    kin_tag_sentence(&sa);                                            /* Ch.8  */
    kin_propagate_proper_nouns(&sa);
    kin_resolve_sp_ambiguity(&sa);                                    /* §16.4 below */

    for (int i = 0; i < sa.token_count; i++)
        kin_morpheme_analyze(&sa.tokens[i]);                          /* Ch.11 */

    for (int i = 0; i < sa.token_count; i++)
        kin_fill_morpheme_glosses(&sa.tokens[i]);                     /* Ch.15 */

    kin_tag_gram_roles(&sa);
    kin_check_syntax(&sa);                                            /* Ch.12 */
    kin_suggest_corrections(&sa);                                     /* Ch.14 */

    return sa;
}
```

Every single call here is a function whose internals you've already
studied in detail. There's no new control-flow cleverness in this
function — its entire value is *sequencing*: text becomes tokens
(Ch.4), tokens get tagged with a grammar tree (Ch.8), ambiguous subject
prefixes get resolved against context (§16.4), every token's morphemes
get broken down (Ch.11) and then glossed in English (Ch.15), grammatical
roles get assigned, agreement gets checked (Ch.12), and corrections get
suggested (Ch.14) — strictly in that order, because each stage's input is
the previous stage's output. If you remember nothing else about this
function, remember this: **it is the literal, physical evidence that
this entire project is one pipeline**, and every file you've studied is
one named stage of it, called here in the only order that makes sense.

## 16.2 `memset(&sa, 0, sizeof(sa));` — Chapter 14's open question, now settled

Back in Chapter 14, `corrector.c` was found relying on an *assumption*:
that `err->suggestion` already started as all-zero bytes before its
`strncpy` call, even without an explicit terminator written afterward.
Here is the proof that assumption holds, for every `SentenceAnalysis`
this project ever produces: the very first thing `kin_analyze()` does,
before tokenizing a single character of input, is zero the **entire**
struct — every byte of every `Token`, every `Error`, every nested
`MorphBreakdown`, all 597,516 bytes of it (Chapter 3's measured size),
in one `memset` call. This means `err->suggestion[255]` genuinely is
guaranteed `'\0'` the moment `corrector.c` reaches it, for every
`SentenceAnalysis` that flows through this single, central entry point.

This is worth stating precisely, because the *honest* lesson isn't "so
Chapter 14's concern was wrong" — it's: **the safety Chapter 14 flagged
as missing locally does, in fact, hold globally, but only because of a
single `memset` call in a completely different file, which
`corrector.c` itself neither performs nor checks for.** That's a real
characteristic of this codebase worth being able to articulate exactly:
correctness here depends on `kin_analyze()` always being the entry
point that constructs every `SentenceAnalysis`. If any future code ever
constructed or reused a `SentenceAnalysis` *without* going through this
`memset`, `corrector.c`'s reliance on pre-zeroed memory would silently
stop holding. Being able to name *exactly which line, in which file,*
your code's safety actually depends on — rather than vaguely gesturing at
"it's probably fine" — is precisely the level of precision a defense
should reward.

## 16.3 Returning a 583-kilobyte struct by value

```c
SentenceAnalysis kin_analyze(const char *text) {
    SentenceAnalysis sa;        /* ~583.5 KB, on THIS function's stack frame */
    /* ... */
    return sa;
}
```

Every other multi-output function in this entire project (Chapters
5–14) used the out-parameter pattern: take a pointer, write the answer
through it, return a `bool`/`int` status. `kin_analyze()` does something
none of them do — it declares its result as a perfectly ordinary local
variable and returns it **by value**, with the function's return *type*
being the full `SentenceAnalysis` struct itself, not a pointer to one.

Two things are worth understanding precisely about what this actually
costs.

**First, conceptually, in the C language itself**: returning a struct by
value means the caller receives a complete, independent copy of that
struct — exactly Chapter 3's struct-copy semantics, just at the scale of
a return statement instead of an assignment. The language gives no
special treatment to `return sa;` versus `SentenceAnalysis other = sa;`
— both are, conceptually, "copy this struct's bytes to wherever the
destination is."

**Second, in practice, on real compilers and real calling conventions**:
the C language standard does not itself dictate *how* a function
physically hands a large struct back to its caller — that's defined by
the platform's **ABI** (Application Binary Interface), not by the C
standard. On the calling convention this project's `gcc`-compiled build
actually uses on Linux/x86-64 (the System V AMD64 ABI), a struct too
large to fit in a couple of registers is returned via a **hidden
pointer**: the *caller* allocates space for the result and silently
passes its address into the function, which then writes `sa` directly
into that destination as it builds it, instead of building a separate
`sa` and then performing one additional, wasteful 583 KB copy at the
`return` statement. This is an ABI-level optimization, not a C-language
guarantee — it's worth stating it exactly this way if asked, rather than
either overclaiming "C guarantees no copy happens" (it doesn't; that's a
calling-convention detail) or underclaiming "this obviously copies 583 KB
every call" (in practice, on the platform this almost certainly runs on,
it typically does not).

What *is* guaranteed, regardless of any ABI-level copy elision: the
local variable `sa` itself needs 583.5 KB of stack space to exist at all,
for the duration of this one function call. That's a real, fixed,
unavoidable cost of this design, paid every single time `kin_analyze()`
is called — and it's the same number Chapter 3 calculated, now seen
attached to a concrete, callable function rather than an abstract
`sizeof()` result.

## 16.4 `scan_back_noun`: the backward-search pattern, generalized into a reusable helper

Chapter 12 showed `syntax.c`'s subject-verb agreement check performing a
hand-written backward scan, inline, specific to that one check.
`analysis.c` needs the *same kind* of backward search — for a different
reason (resolving which noun class an ambiguous verb subject prefix
actually refers to) — and this time, the search itself is factored out
into its own small, reusable function:

```c
static const Token *scan_back_noun(const SentenceAnalysis *sa, int from,
                                   const int *cls, int n) {
    for (int j = from; j >= 0; j--) {
        const Token *t = &sa->tokens[j];
        if (t->pos != POS_NOUN || t->noun_class == 0) continue;
        for (int k = 0; k < n; k++)
            if (t->noun_class == cls[k]) return t;
    }
    return NULL;
}
```

Rather than hardcoding *which* noun classes count as a valid match
(Chapter 12's version checked specific class numbers directly inline),
this version takes the acceptable classes as a parameter: `const int
*cls, int n` — an array and its length, passed in by the caller. This
makes the exact same backward-walking loop reusable for several
genuinely different ambiguity cases in `kin_resolve_sp_ambiguity`:

```c
static const int ya_cls[] = {1, 4, 6, 9};
const Token *subj = scan_back_noun(sa, i - 1, ya_cls, 4);

/* ... later, a different call site, different acceptable classes ... */
static const int i_cls[] = {4, 9};
const Token *subj = scan_back_noun(sa, i - 1, i_cls, 2);
```

This is the DRY principle from Chapter 11 applied to an *algorithm*
rather than a simple copy-and-terminate operation: the same backward-walk
logic, parameterized over what counts as success, written once and
called three separate times with three different class sets in this one
function. It's worth being able to compare this directly against
Chapter 12's version and explain, honestly, why `syntax.c`'s check wasn't
*also* rewritten to call this shared helper: the two checks have
sufficiently different stopping conditions (Chapter 12's scan also has to
recognize sentence/clause boundaries and a wider set of "skip past"
token types) that unifying them wasn't a clean fit — a real, defensible
software-engineering judgment call, not an oversight you need to defend
as a mistake. Recognizing *when* two similar-looking pieces of code are
similar enough to deserve a shared abstraction, and when they're better
left separate because their actual requirements diverge, is itself part
of what you're being asked to demonstrate understanding of.

### 16.4.1 Why "SP ambiguity" is a real linguistic fact, not a parsing bug

`ya_cls[] = {1, 4, 6, 9}` looks arbitrary until you connect it back to
Section 7.1.1's noun-class table: classes 1, 4, 6, and 9 are exactly the
classes whose verb subject-prefix surfaces as `ya`-shaped in the past
tense, and classes 4 and 9 separately share the surface form `i`. A
conjugated verb beginning with `ya-` is genuinely, irreducibly ambiguous
about which of four noun classes its subject belongs to *until* you look
backward in the sentence and find which class the actual subject noun
carries — `kin_resolve_sp_ambiguity` exists because this ambiguity is real
in the language itself, not an artifact of imperfect detection; resolving
it is a textbook case of needing sentence-level context (`scan_back_noun`)
to settle something the verb's own letters genuinely cannot.

## Key takeaways

- `kin_analyze()` is the single function that proves this project is a
  pipeline — every other file's main entry point is called here, exactly
  once, in the only order that produces correct results.
- A `memset` on the very first line of the orchestrator is what makes
  every later "assume the buffer started zeroed" assumption (Ch.14) true
  in practice — but that safety depends specifically on every
  `SentenceAnalysis` being constructed through this one function.
- Returning a struct by value is, conceptually, a full copy in the C
  language; in practice, on common ABIs (like System V x86-64), a struct
  too large for registers is returned via a hidden pointer the caller
  supplies, typically avoiding a second bulk copy — an ABI detail, not a
  language guarantee, and worth describing with that exact precision.
- Regardless of ABI-level copy elision, the local variable itself still
  requires its full size in stack space for the duration of the call —
  583.5 KB here, every single time.
- The same backward-search *shape* can be hand-written inline for one
  specific check (Ch.12) or factored into a small, parameterized, reusable
  helper when multiple call sites need the same walk with different match
  criteria (this chapter) — which approach fits depends on how much the
  individual checks' requirements actually overlap.

## Search YouTube for

- "C struct return by value vs pointer"
- "System V ABI x86-64 struct return calling convention"
- "stack frame size and function calls explained"
- "DRY principle applied to algorithms not just code"

## Coming up in Chapter 17

`g2p.c` (626 lines) converts written Kinyarwanda into a phoneme sequence
for text-to-speech and speech-recognition use. Chapter 17 covers its
`PHONEME_TABLE` and the `sizeof(arr)/sizeof(arr[0])` length-counting
idiom you first glimpsed back in Chapter 7, now seen in its actual,
complete call sites — plus how this file packages its output as both a
raw integer sequence and a printable ASCII string, two representations of
the same underlying data for two different downstream consumers.
