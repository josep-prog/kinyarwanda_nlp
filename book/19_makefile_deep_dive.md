# Chapter 19 — The Makefile Deep Dive

## Opening Part III

Chapters 4–18 covered what the code *does*. This chapter covers how it
*becomes a program at all* — the Makefile you glanced at in Chapter 1,
now read line by line. Everything here is `make` and `gcc` knowledge, not
C-language knowledge, but it's exactly as load-bearing: get the build
wrong, and none of the C you've learned matters.

## 19.1 Deriving one list from another: `$(SRCS:.c=.o)`

```make
LIB_SRCS = src/tokenizer.c \
           src/morphology.c \
           src/ortho.c \
           src/lexicon.c \
           src/pos_tagger.c \
           src/morph_dispatch.c \
           src/syntax.c \
           src/corrector.c \
           src/analysis.c \
           src/g2p.c \
           src/gloss.c \
           src/api.c \
           src/punctuation.c \
           src/validator.c

SRCS     = src/main.c $(LIB_SRCS)
OBJS     = $(SRCS:.c=.o)
LIB_OBJS = $(LIB_SRCS:.c=.o)
```

`$(SRCS:.c=.o)` is a GNU Make **substitution reference**: take every word
in the `SRCS` list, and wherever a word ends in `.c`, replace just that
suffix with `.o`. This isn't a loop you write — it's a single textual
transformation applied to the whole list at once, computed by `make`
itself. The practical effect: `OBJS` and `LIB_OBJS` never have to be
typed out by hand and kept in sync with `SRCS`/`LIB_SRCS` — add a new
`.c` file to `LIB_SRCS`, and its corresponding `.o` automatically appears
in `LIB_OBJS` the next time `make` runs, with zero additional editing
anywhere else. This is Chapter 1's "single source of truth" principle,
now enforced by the build system rather than the C compiler: one list is
authoritative (`LIB_SRCS`), and everything else that needs the same
information derives it mechanically, instead of being maintained as a
second, separately-typed copy that could silently drift out of sync.

`SRCS = src/main.c $(LIB_SRCS)` is the Makefile's own version of Chapter
1's `main.c`-exclusion rule: the *library* object list (`LIB_OBJS`) never
includes `main.o`; the *full CLI binary's* object list (`OBJS`) explicitly
prepends it. Two derived lists, from the same underlying source list,
for the same reason Chapter 1 first explained: a library must never ship
its own `main()`.

## 19.2 One pattern rule, instead of fourteen explicit ones

```make
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
```

Without this, the Makefile would need a separate, explicit rule for every
single `.c` file — `tokenizer.o: tokenizer.c` with its own recipe,
`morphology.o: morphology.c` with an identical-looking recipe, repeated
fourteen times. A **pattern rule** (`%.o: %.c`) says, once: "for *any*
target ending in `.o`, if a same-named file ending in `.c` exists, this
is how to build it." `$@` and `$<` are *automatic variables* — `$@`
expands to whatever the actual target of this particular invocation is
(e.g. `src/tokenizer.o`), and `$<` expands to the first prerequisite
(`src/tokenizer.c`). One rule, applied generically to every file that
matches the pattern. This is the exact same DRY instinct you've now seen
applied to C functions (`set_morph`, Ch.11; `scan_back_noun`, Ch.15;
`seq_append`, Ch.16) — here applied one level up, to the *build system*
itself, instead of to C source code.

## 19.3 `-MMD -MP` and `.d` files: how `make` learns about headers

```make
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -Iinclude -O2 -fPIC -MMD -MP
/* ... */
-include $(SRCS:.c=.d)
```

Here's the problem this solves. The pattern rule above tells `make` that
`tokenizer.o` depends on `tokenizer.c` — but `tokenizer.c` also
`#include`s `kinyarwanda.h` (Chapter 1). If you edit *only*
`kinyarwanda.h` and run `make` again, by the pattern rule alone, `make`
has no way to know `tokenizer.o` is now stale — its only known
prerequisite, `tokenizer.c`, hasn't changed. Without some extra
mechanism, you'd get a silently outdated build using a stale header.

`-MMD` tells `gcc`, while it's compiling `tokenizer.c` anyway, to *also*
emit a `tokenizer.d` file — a small Makefile fragment listing every
header `tokenizer.c` actually pulled in, transitively, as additional
prerequisites for `tokenizer.o`. Nobody hand-writes this list; only the
compiler, which actually parses every `#include` while compiling, knows
the true, complete answer. `-MP` adds a small safeguard alongside it
(phony targets for each header), preventing a `make` error if a header
listed in an old `.d` file is later renamed or deleted. `-include
$(SRCS:.c=.d)` then pulls every one of these generated fragments back
into the Makefile itself — `-include` (rather than plain `include`)
specifically tolerates the very first build, when no `.d` files exist
yet at all, instead of erroring out. The result: editing `kinyarwanda.h`
correctly triggers a rebuild of every `.c` file that actually includes
it, because the compiler told `make` about that dependency the *last*
time it compiled — a self-maintaining dependency graph, rebuilt fresh
every time you build.

## 19.4 The same object files, three different ways

```make
all: $(TARGET) $(STATIC_LIB) $(SHARED_LIB)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(STATIC_LIB): $(LIB_OBJS)
	ar rcs $@ $^

$(SHARED_LIB): $(LIB_OBJS)
	$(CC) -shared -o $@ $^
```

Three build targets, but notice: `LIB_OBJS` (the same fourteen object
files) feeds *both* the static and shared library rules. They are never
recompiled differently for one library versus the other — the exact same
`.o` files, produced once by the one pattern rule in Section 19.2, get
repackaged two different ways:

- **`ar rcs $@ $^`** — `ar` is the **archiver**: it bundles multiple `.o`
  files into one `.a` file (`r` = insert/replace, `c` = create the
  archive if it doesn't exist, `s` = write an index for faster linking
  later). No linking happens here at all — a `.a` file is closer to a
  labeled box of object files than a finished program; whatever
  eventually links against it is what actually resolves symbols, at
  *that* later link time (exactly what you did yourself in Chapter 1's
  exercise: `gcc ... libkinyarwanda.a -o hello_kin`).
- **`gcc -shared -o $@ $^`** — this *does* perform a real link step right
  now, producing one finished, independently loadable `.so` file that an
  operating system's dynamic linker can map into a running process later,
  without needing the original `.o` files at all anymore.

`$^` is a second automatic variable, expanding to *all* prerequisites of
the rule (every object file in `LIB_OBJS`), as opposed to `$<`'s "just
the first one."

## 19.5 `-fPIC` applied to everything, on purpose

A shared library's code can be mapped into different processes at
different memory addresses, so it cannot use absolute memory addresses
internally — it needs **Position Independent Code** (`-fPIC`), which
generates address references relative to wherever the code ends up
loaded, rather than baked-in fixed addresses. The CLI binary and the
static-library objects have no such requirement — a plain executable is
typically loaded at a fixed, known base address, so PIC buys it nothing.

Look again at `CFLAGS`: `-fPIC` is listed once, applied unconditionally,
to *every* object file the single pattern rule in Section 19.2 produces
— including the ones that end up in `kinyarwanda_nlp` (the CLI binary)
and `libkinyarwanda.a` (the static archive), which strictly don't need
it. This is a deliberate simplicity-over-micro-optimization choice,
exactly the kind you've seen repeatedly in the C source itself (Chapter
7's linear search over a hash table; Chapter 9's point-edit versus
rebuild): maintaining *two* separate compilation paths — one set of
PIC object files for the `.so`, a second non-PIC set for the binary and
`.a` — would mean twice as many `.o` files, a more complex Makefile, and
two things to keep in sync, in exchange for a typically negligible
performance/size difference on the artifacts that didn't strictly need
PIC. Compiling everything with `-fPIC` once, and reusing the identical
object files for all three build targets, is simpler, and the cost is
small enough that this project's Makefile accepts it without
hesitation.

## 19.6 `.PHONY`: targets that aren't files

```make
.PHONY: all clean test install install-lib uninstall help
```

`make` normally decides whether a target needs rebuilding by checking
whether a file with that exact name exists and is newer than its
prerequisites. `clean`, `test`, `install`, and friends aren't files at
all — they're just names for "run this recipe." `.PHONY` tells `make`
explicitly: never treat these as filenames to check the timestamp of;
always run their recipe when asked. Without it, if a stray file
literally named `clean` ever existed in the project directory, `make
clean` could get confused into thinking there's nothing to do, since a
file called `clean` would already "exist." `.PHONY` removes that
ambiguity entirely for every target that's really just a named action,
not a build artifact.

## 19.7 Installation: standard Unix conventions, briefly

```make
install-lib: $(STATIC_LIB) $(SHARED_LIB)
	install -d $(LIBDIR)
	install -m 644 $(STATIC_LIB) $(LIBDIR)/$(STATIC_LIB)
	install -m 755 $(SHARED_LIB) $(LIBDIR)/$(SHARED_LIB)
	ldconfig $(LIBDIR)
```

This is standard Unix packaging practice, not C-language content, so
only briefly: `install -d` creates a directory (with sane permissions);
`install -m NNN` copies a file *and* sets its permission bits in one
step (`644` = owner read/write, everyone else read-only — appropriate
for a static archive nobody needs to execute; `755` = also executable,
appropriate for a shared library the dynamic linker will load and
execute code from). `ldconfig` updates the system's cache of where
shared libraries live, so that a program linked against
`-lkinyarwanda` can find `libkinyarwanda.so` at runtime without you
needing to set `LD_LIBRARY_PATH` by hand every time.

## Key takeaways

- `$(LIST:.c=.o)`-style substitution references derive one list from
  another mechanically — add a source file in one place, and every
  derived list updates automatically, the build-system equivalent of
  Chapter 1's single-source-of-truth principle.
- A pattern rule (`%.o: %.c`) with automatic variables (`$@`, `$<`, `$^`)
  replaces what would otherwise be one explicit, repeated rule per
  source file — DRY, applied to the build system itself.
- `-MMD -MP` plus `-include $(SRCS:.c=.d)` lets the compiler — the only
  thing that actually knows a file's full `#include` chain — tell `make`
  about header dependencies automatically, so editing a header correctly
  triggers rebuilds of everything that includes it.
- The exact same compiled object files can be repackaged three different
  ways: linked directly into an executable, archived (not linked) into a
  static library with `ar`, or linked into a shared library with
  `gcc -shared`.
- `-fPIC` is required for shared-library code and unnecessary for a
  plain executable; this project applies it to every object file anyway,
  trading a small, usually negligible cost for a single, simpler build
  path instead of two parallel ones.
- `.PHONY` tells `make` that certain targets are actions, not files, so
  their recipe always runs regardless of what happens to exist on disk
  with the same name.

## Search YouTube for

- "GNU Make tutorial pattern rules and automatic variables"
- "Makefile dependency generation -MMD -MP explained"
- "static library vs shared library ar vs gcc -shared"
- "position independent code PIC explained"
- ".PHONY targets in Makefiles explained"

## Coming up in Chapter 20

Chapter 20 covers `tests/test_framework.h` and the four test files built
on top of it — a hand-rolled, dependency-free test harness, the
`do { ... } while(0)` macro idiom that makes its assertion macros safe to
use anywhere a single statement is expected, and what it actually means
to test a rule-based linguistic engine rather than a typical CRUD
application.
