# Chapter 20 — The Test Suite: Macros, `do { } while(0)`, and Testing a Rule-Based Engine

## A hand-rolled framework, on purpose

`tests/test_framework.h` is barely 50 lines, has zero external
dependencies, and gives this project everything a much larger testing
library would — assertions, pass/fail counting, a per-suite report. This
chapter explains the one C idiom that makes its macros safe to use
anywhere, then looks at what running 871 lines of test code across four
files actually checks for a rule-based linguistic engine like this one.

## 20.1 The `do { } while(0)` trick, explained precisely

```c
#define ASSERT(r, cond, label) do {                                     \
    if (cond) {                                                         \
        (r)->pass++;                                                    \
    } else {                                                            \
        fprintf(stderr, "  FAIL  %-55s  [%s:%d]\n", (label), __FILE__, __LINE__); \
        (r)->fail++;                                                    \
    }                                                                   \
} while (0)
```

This macro's *body* is genuinely several statements: an `if`/`else`, two
increments, an `fprintf` call. The problem: a macro that expands to
multiple bare statements can silently break code that calls it expecting
a single statement. Imagine, hypothetically, a version of `ASSERT`
*without* the `do { } while(0)` wrapper, used like this:

```c
if (some_condition)
    ASSERT(r, x == 5, "x should be 5");
else
    do_something_else();
```

If `ASSERT` expanded to a bare `if (cond) { ... } else { ... }`, the
`else` immediately following the macro's own internal `else` would bind
to the **macro's** `if`, not the programmer's outer one — producing a
dangling-else bug that's nearly invisible at the call site, because the
call site just looks like an ordinary function call.

`do { ... } while (0)` solves this completely, by exploiting a property
of the `do`/`while` loop: its body runs exactly once (the condition `0`
is always false, so it never loops a second time), but syntactically, a
`do { ... } while(0);` **is a single statement** — to anything outside
it, regardless of how many actual statements live inside the braces, it's
indistinguishable from a single function call followed by a semicolon.
That means `ASSERT(r, cond, label);` can be safely used absolutely
anywhere a single statement is expected — inside an `if` with no braces,
as a loop body, anywhere — and it will always behave correctly, with no
risk of an outer `if`/`else` accidentally binding to something inside the
macro. This is one of the most famous idioms in C, specifically because
it solves a real, easy-to-trigger bug class (multi-statement macros used
without braces) with a trick that costs nothing at runtime — the
`while(0)` is a compile-time-constant condition that any decent compiler
eliminates entirely, leaving just the body's actual instructions.

## 20.2 `__FILE__` and `__LINE__`: the preprocessor fills these in for you

```c
fprintf(stderr, "  FAIL  %-55s  [%s:%d]\n", (label), __FILE__, __LINE__);
```

`__FILE__` and `__LINE__` are special, predefined preprocessor macros —
not variables, not function calls. `__FILE__` expands to the name of the
source file currently being compiled, as a string literal; `__LINE__`
expands to the current line number, as an integer literal. Critically,
because `ASSERT` is a macro (not a real function), `__FILE__` and
`__LINE__` are expanded at **the exact line where `ASSERT(...)` is
written in `test_morphology.c` or `test_ortho.c`** — not at the line
inside `test_framework.h` where `ASSERT` itself is defined. This is
precisely why every failed assertion reports the right file and line
number for the *specific test case that failed*, without the test
author ever having to type a filename or line number by hand — the
preprocessor inserts the correct, current location automatically, fresh,
at every single call site, because that's where the macro's text actually
gets substituted in before compilation.

## 20.3 `run_tests.c`: a compound literal, and accumulating pass/fail flags

```c
int main(void) {
    int any_fail = 0;
    TestResult r;

    r = (TestResult){0, 0};
    run_morphology_tests(&r);
    any_fail |= suite_report(&r, "morphology");

    r = (TestResult){0, 0};
    run_lexicon_tests(&r);
    any_fail |= suite_report(&r, "lexicon");
    /* ... two more suites, identical shape ... */

    return any_fail ? 1 : 0;
}
```

`r = (TestResult){0, 0};` uses a **compound literal** — `(Type){...}` is
a C99 expression that constructs a fresh, temporary value of `Type` on
the spot, usable anywhere an expression is allowed. This is different
from `TestResult r = {0, 0};` (which only works as part of a
*declaration*, the moment a variable is introduced) — here, `r` already
exists from the line above, and this is a plain *assignment*, reusing
struct-literal-style syntax as a general expression to reset an
already-declared variable back to a fresh `{pass: 0, fail: 0}` state
before each suite runs.

`any_fail |= suite_report(&r, "morphology");` uses bitwise OR-assignment
to accumulate a yes/no signal across multiple calls. Recall from this
book's earlier read of `test_framework.h` that `suite_report()` returns
`1` if that suite had any failures, `0` if it was clean. `|=` is exactly
the right tool for "remember if *any* of several checks ever came back
true": once `any_fail` becomes `1` from an earlier suite, OR-ing in a
later suite's `0` result (`1 | 0 == 1`) can never accidentally reset it
back to `0` — only a fresh `1` can ever turn it on, and nothing can turn
it back off once set. The final `return any_fail ? 1 : 0;` feeds this
accumulated signal directly into the program's **exit status** — the
standard Unix convention where `0` means success and any nonzero value
means failure, which is exactly what lets a shell script, a CI pipeline,
or `make test`'s own success/failure reporting (Chapter 19) work
correctly off of nothing more than this one returned integer.

## 20.4 Internal wiring without a shared header

```c
/* in run_tests.c, with no dedicated header file for these */
void run_morphology_tests(TestResult *r);
void run_lexicon_tests(TestResult *r);
void run_ortho_tests(TestResult *r);
void run_pipeline_tests(TestResult *r);
```

Chapter 1 made a point of "the header is the contract" for this
project's *public* API. Here, for four small, test-internal functions
that nothing outside this five-file test suite will ever call, the
project simply restates their declarations directly inside
`run_tests.c`, rather than creating a dedicated header just to hold four
function prototypes. This is a fair, proportionate choice: a formal
shared header earns its cost when multiple files need the same
declarations *and* the API is meaningful to outside callers (exactly
`kinyarwanda.h`'s situation); for four functions used by exactly one
caller, in a private test harness, a header would be pure ceremony with
no real benefit. Recognizing when the "proper" pattern (Chapter 1's
header-as-contract) is worth its overhead, and when a smaller, more
direct approach is the more honest engineering choice, is itself part of
what separates a junior read of "always do X" from actually
understanding *why* X is usually the right call.

## 20.5 What these tests actually check — and how that differs from Chapter 11's `verified` flag

```c
kin_ortho_gen("ku|eza", false, buf, sizeof(buf));
ASSERT_STR(r, buf, "gweza", "ku|eza → gweza (u→w then k→g §3.7)");
```

This is worth contrasting carefully against Chapter 11's `verified`
field. The `verified` flag checks **internal self-consistency**: does
regenerating a surface word from this *specific analysis run's own*
reverse-engineered morphemes reproduce the *same* word that was just
analyzed. It can tell you the analysis was internally coherent — it
cannot tell you the analysis matches what RALC 2017 actually says is
correct Kinyarwanda, because it never consults any source of truth
outside the program itself.

The test suite checks something different: **external correctness**.
Every `ASSERT_STR` here compares `kin_ortho_gen`'s output against a
specific string a human (working from the actual RALC rulebook) decided
is the linguistically correct result — `"ku|eza"` really should become
`"gweza"`, confirmed by an authority outside the code. These are two
genuinely different kinds of "is this right," and a defensible system
needs both: the `verified` flag catches a program that contradicts
*itself*; the test suite catches a program that's internally consistent
but simply *wrong* about the language. If you're asked "how do you know
your analysis is correct," the strongest answer names both checks
explicitly and explains what each one can and cannot catch.

## 20.6 Try it yourself: prove the `do`/`while(0)` wrapper is necessary

```c
#include <stdio.h>

/* The UNSAFE version: no do/while(0) wrapper */
#define BAD_CHECK(cond) if (cond) printf("ok\n"); else printf("fail\n");

/* The SAFE version: wrapped */
#define GOOD_CHECK(cond) do { if (cond) printf("ok\n"); else printf("fail\n"); } while(0)

int main(void) {
    int x = 5;

    if (x == 5)
        BAD_CHECK(x == 5);     /* compiles, but watch what the 'else' below binds to */
    else
        printf("outer else\n");

    if (x == 5)
        GOOD_CHECK(x == 5);    /* safe: behaves as one statement no matter what */
    else
        printf("outer else (safe version)\n");

    return 0;
}
```

Compile this and trace through it by hand (or step through it in a
debugger): the `BAD_CHECK` call's own internal `else` silently consumes
the outer `else printf("outer else\n");`, attaching it to the macro's
`if` instead of the programmer's — exactly the dangling-else bug Section
20.1 described, now visible with your own eyes instead of taken on
faith.

## Key takeaways

- `do { ... } while(0)` makes a multi-statement macro behave as exactly
  one statement everywhere, preventing a dangling-`else` bug when the
  macro is used without braces — the single most important idiom for
  writing safe multi-line C macros.
- `__FILE__` and `__LINE__` are preprocessor macros expanded at the
  call site, not the definition site — which is exactly why a shared
  `ASSERT` macro can report the correct file and line for every
  individual test case that uses it.
- A compound literal, `(Type){...}`, constructs a fresh value of a
  struct type as a plain expression, usable in an assignment to an
  already-declared variable — distinct from initializing a variable at
  the point it's declared.
- `|=` is the right tool for accumulating a "has anything failed yet"
  flag across multiple 0/1 results, feeding directly into the Unix
  exit-status convention that `make test` and CI tooling rely on.
- Not every shared declaration needs a dedicated header — a header earns
  its overhead when an API is genuinely public and used by several
  callers; small, internal-only wiring can reasonably skip that ceremony.
- A self-consistency check (Ch.11's `verified` flag) and an external
  ground-truth test suite check two different things — internal
  coherence versus actual correctness against a real authority — and a
  defensible system benefits from having both, not just one.

## Search YouTube for

- "do while(0) macro trick in C explained"
- "dangling else problem in C"
- "__FILE__ __LINE__ preprocessor macros explained"
- "compound literals in C99"
- "Unix exit status codes explained"

## Coming up in Chapter 21

Chapter 21 steps back from any single file to name, directly and in one
place, the defensive-programming habits you've now seen scattered across
every chapter: `snprintf` over `sprintf`, the complete absence of
`malloc`/`free`, bounds checks before every buffer write, and the
specific reasoning for why each one matters — a consolidated reference
you can use to answer "what makes this code defensively written" without
having to reassemble the answer from fifteen different files.
