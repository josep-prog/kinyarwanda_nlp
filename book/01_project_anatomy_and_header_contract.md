# Chapter 1 — Project Anatomy & The Header File as a Contract

## Why we start here, before any algorithm

Before a single phonological rule or tokenizer loop matters, a C project has
to answer a much more boring question: **how do many separate `.c` files
agree on what functions and data types exist, without each of them needing
to see every other file's source code?**

That question is answered entirely by *header files* and by a discipline
called **separate compilation**. Get this wrong and nothing else in the
book works — every later chapter assumes you understand why `kinyarwanda.h`
looks the way it does. So Chapter 1 is not about clever code. It's about
the skeleton everything else hangs on.

## 1.1 The two things a C program is split into

In C, every function and every global variable has exactly two faces:

- A **declaration** — "this function exists, it's called `kin_tokenize`, it
  takes these argument types, and it returns this type." This is a promise,
  not an implementation.
- A **definition** — the actual `{ ... }` body that does the work.

A `.h` file holds declarations. A `.c` file holds definitions. Look at the
bottom of `include/kinyarwanda.h`:

```c
/* tokenizer.c */
int  kin_tokenize(const char *text, Token *out, int max_tokens);
```

That line is the *entire* content `kinyarwanda.h` has about tokenization.
No loop, no logic, nothing about how text gets split. It's a promise: "a
function with this exact name and these exact argument types exists
somewhere, and it returns an `int`." The actual 300+ lines of logic live in
`src/tokenizer.c`, which you'll meet in Chapter 4.

**Why split them at all?** Because `pos_tagger.c` needs to *call*
`kin_tokenize()`, but it doesn't need to know *how* tokenization works
internally — it just needs the promise. If `pos_tagger.c` could only call
functions whose full source it could see, every file would have to
`#include` every other file's entire source, and changing one line of
`tokenizer.c` would force you to recompile the whole project from scratch
every time. Instead, each `.c` file is compiled once into a `.o` object
file (more on this in Chapter 19), and the **linker** — a separate tool
that runs after the compiler — matches up calls to definitions across all
the `.o` files at the end. The header is the thing that lets the compiler
check your calls are *type-correct* without yet knowing where the real
code lives.

This is the single most important idea in C project structure: **the
header is the API; the `.c` files are implementation details.** Everything
in this book traces back to that sentence.

## 1.2 Project anatomy — what's in each directory

Run `ls` at the repo root and you'll see:

```
include/    — public header files (the contract — Section 1.3)
src/        — implementation files, one .c per pipeline stage / grammar tree
tests/      — a hand-rolled test suite (Chapter 20)
man/        — a Unix manual page for the CLI tool
Makefile    — build instructions (Chapter 19)
```

`include/` holds three headers:

- `kinyarwanda.h` — the main contract: every struct, every enum, every
  function the engine exposes. This is the one you'll spend most of this
  book reading.
- `g2p.h` — the grapheme-to-phoneme (text-to-sound) API, kept separate
  because it's a self-contained sub-feature with its own types
  (`KinPhonemeSeq`).
- `kinyarwanda_api.h` — a *thin convenience layer* on top of the other two,
  meant for external callers who just want "give me corrected text" or
  "give me a phoneme string" without learning the full `Token`/
  `SentenceAnalysis` model. You'll see why this separation matters in
  Chapter 17.

`src/` mirrors the pipeline. The Makefile's `LIB_SRCS` list is, in effect,
a second table of contents — it's the order the *build system* sees the
files in, which is alphabetical-ish by topic rather than execution order.
This book instead follows the order the *data* flows through them at
runtime: `tokenizer.c` first (nothing can happen before text becomes
tokens), then `morphology.c` and `lexicon.c` (classifying and looking up
each token), then `pos_tagger.c`, then the analysis/correction/output
stages.

One file conspicuously does **not** appear in `LIB_SRCS`: `main.c`. That's
deliberate, and it's a design decision worth defending on its own — see
Section 1.5.

## 1.3 Anatomy of `kinyarwanda.h`: reading a header like an engineer

Open `include/kinyarwanda.h`. Ignore the big linguistic comment block at
the top for now (that's the *content* the engine encodes — Kinyarwanda
grammar trees — not the C structure). Skip down to line 188:

```c
#ifndef KINYARWANDA_H
#define KINYARWANDA_H

#include <stdbool.h>
#include <stddef.h>
```

### The include guard

`#ifndef KINYARWANDA_H` / `#define KINYARWANDA_H` / `#endif` (at the very
last line, 767) is called an **include guard**. Here's the failure mode it
prevents: suppose `analysis.c` includes both `kinyarwanda.h` directly *and*
some other header that *also* includes `kinyarwanda.h`. Without a guard,
the preprocessor would paste the entire contents of `kinyarwanda.h` into
`analysis.c` **twice**. Every struct and enum would be defined twice in the
same translation unit, and the compiler would reject it with redefinition
errors.

The guard works because the preprocessor literally executes top to bottom,
textually, before compilation even starts:

1. First time `kinyarwanda.h` is pulled in: `KINYARWANDA_H` is not yet
   defined, so `#ifndef KINYARWANDA_H` is true. The preprocessor defines
   `KINYARWANDA_H` and processes everything down to `#endif`.
2. Second time the *same file* is pulled in (directly or transitively):
   `KINYARWANDA_H` is now defined, so `#ifndef KINYARWANDA_H` is false, and
   the preprocessor skips straight to `#endif` — the body is never pasted
   in again.

This is pure text substitution, not a runtime check. It costs nothing at
runtime; it's resolved entirely before your program is even compiled.

### Why `<stdbool.h>` needs an explicit include

C, unlike C++, has no native boolean type in early standards. `bool`,
`true`, and `false` only exist because `<stdbool.h>` defines them (as
macros for `int`, historically). This project requires C99 or later
specifically so `<stdbool.h>` is guaranteed available — check the Makefile:
`CFLAGS = -std=c99 ...`. Functions like

```c
bool kin_strip_noun_prefix(const char *word, char *stem_out, int *class_out);
```

read naturally as "returns whether this succeeded" only because that
header was included. Without it, you'd be returning `int` and relying on
the convention that 0 = false, 1 = true — which still works in C, but
reads worse and type-checks less strictly.

### Limits as `#define` constants

```c
#define KIN_MAX_WORD      128
#define KIN_MAX_STEM       96
#define KIN_MAX_PREFIX     32
#define KIN_MAX_TOKENS    256
#define KIN_MAX_ERRORS     64
#define KIN_MAX_MSG       256
```

These are **preprocessor macros**, not variables. `KIN_MAX_WORD` is not a
memory location holding the value 128 — it is literally replaced by the
text `128` everywhere it appears, before compilation. This matters because
these constants size arrays inside structs (you'll see this fully in
Chapter 3), and C requires array sizes inside a struct definition to be
known at *compile time*, not computed at runtime. A `#define` (or, in
modern C, a `const int` used in a context that doesn't require it to be a
true compile-time constant — `#define` is the traditional, portable
choice) satisfies that requirement.

Why centralize them at the top of the header instead of hardcoding `128`
directly in five different structs? Because if you ever need to raise the
maximum word length, you change one line and recompile — every struct and
every bounds check that referenced `KIN_MAX_WORD` updates automatically.
Hardcoded magic numbers scattered through the codebase would require you
to hunt down every occurrence and hope you didn't miss one. This is the
**single source of truth** principle, and in C, where there's no compiler
support for renaming a "constant" safely across files, the discipline of
defining it once in the header and never re-typing the literal is the only
thing standing between you and an inconsistent build.

## 1.4 `extern "C"` — when C code has to talk to C++ callers

Open `include/kinyarwanda_api.h` and look at lines 25 and 93:

```c
#ifdef __cplusplus
extern "C" {
#endif

/* ... function declarations ... */

#ifdef __cplusplus
}  /* extern "C" */
#endif
```

This isn't for C. It's a courtesy to anyone compiling this header with a
**C++** compiler. Here's the underlying problem: C++ supports function
*overloading* — two functions can share a name as long as their parameter
types differ. To make that possible, a C++ compiler doesn't store function
names in the compiled binary as plain text; it **mangles** them, encoding
the parameter types into a longer, compiler-specific symbol name. A C
compiler never does this — `kin_tokenize` is stored as the literal string
`kin_tokenize` in the object file's symbol table, because C has no
overloading and doesn't need disambiguation.

If a C++ program included this header *without* `extern "C"`, the C++
compiler would assume these are C++ functions and mangle the names when
generating calls to them. But the actual compiled library (built by `gcc`,
a C compiler) stored the **unmangled** C names. At link time, the linker
would look for the mangled name, fail to find it in the library, and you'd
get a "undefined reference" error — even though the function obviously
exists in the `.so`/`.a` file.

`extern "C" { ... }` tells the C++ compiler: "treat everything inside these
braces using C linkage rules — don't mangle these names." The `#ifdef
__cplusplus` guard around it means a plain C compiler (which has never
heard of `extern "C"` and would error on the unfamiliar syntax) skips it
entirely, because `__cplusplus` is a macro that's only ever defined when
you're compiling with a C++ compiler.

This is why `kinyarwanda_api.h` has it but you'll notice the comment in the
file calling this out explicitly — it's a deliberate "we know C++ users
will want to link against this" decision, tied directly to Chapter 22's
topic (cross-language integration) but introduced here because it's a
header-level concept.

## 1.5 Why is `main.c` excluded from `LIB_SRCS`?

Look again at the Makefile:

```make
LIB_SRCS = src/tokenizer.c \
           src/morphology.c \
           ... \
           src/validator.c

SRCS     = src/main.c $(LIB_SRCS)
```

`main.c` is added to `SRCS` (used for the CLI binary) but deliberately left
out of `LIB_SRCS` (used for `libkinyarwanda.a` and `libkinyarwanda.so`).

The reason: a **library** is meant to be linked into *someone else's*
program. If `main.c` — which defines the `main()` entry point for this
project's own CLI tool — were compiled into the library, then any external
program linking against `libkinyarwanda.a` would get a second, conflicting
`main()` function fighting with their own. A library should expose
*functions to be called*, never an entry point of its own. This is why
Chapter 17 (`api.c`) and Chapter 18 (`main.c`) are different chapters about
different concerns: `api.c` is part of the library (logic other programs
can call); `main.c` is the one file that exists purely to let *this*
project's own command-line tool exist, and it is explicitly excluded from
what gets shipped as a reusable library.

## 1.6 Try it yourself

Before moving to Chapter 2, prove the contract to yourself. Create a file
`/tmp/hello_kin.c`:

```c
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    SentenceAnalysis sa = kin_analyze("Umuntu munini aragenda buhoro");
    printf("Tokens found: %d\n", sa.token_count);
    return 0;
}
```

Compile and link it against the static library you already have built in
this repo:

```sh
gcc -Iinclude /tmp/hello_kin.c libkinyarwanda.a -o /tmp/hello_kin
/tmp/hello_kin
```

Notice what just happened: your `.c` file never saw a single line of
`analysis.c`'s implementation. It only `#include`d the header (the
contract), and the **linker** resolved the actual call to `kin_analyze()`
against the compiled code already sitting inside `libkinyarwanda.a`. If
this runs and prints a token count, you've just personally exercised
everything in Section 1.1 — declaration vs. definition, and separate
compilation — with your own hands.

## Key takeaways

- A header (`.h`) contains **declarations** (promises); a source file
  (`.c`) contains **definitions** (implementations). This split is what
  lets large C projects compile incrementally and lets external code link
  against a library without seeing its source.
- Include guards (`#ifndef`/`#define`/`#endif`) prevent duplicate
  definitions when a header is pulled in more than once — a pure
  preprocessor-level, compile-time mechanism.
- `#define` constants centralize array sizes and limits so they're never
  duplicated as magic numbers across files.
- `extern "C"` disables C++ name mangling for a block of declarations so a
  C++ program can link against a library compiled by a C compiler.
- A reusable library must never contain a `main()` — that's why `main.c` is
  excluded from `LIB_SRCS` even though it's part of `SRCS`.

## Search YouTube for

- "C header files and include guards explained"
- "declaration vs definition in C"
- "C separate compilation and linking explained"
- "extern C C++ name mangling explained"
- "static library vs shared library C tutorial"

## Coming up in Chapter 2

`kinyarwanda.h` declares more than a dozen `enum` types — `POS`,
`VerbTense`, `VerbExtension`, `PronounType`, and more. Chapter 2 covers why
C's `enum` is the backbone of this entire project's type safety, how it
gets used as a "tag" for dispatch (a pattern you'll see repeated in nearly
every file), and what its limitations are compared to enums in other
languages — limitations that explain some defensive checks you'll see
later in the book.
