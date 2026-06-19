# Chapter 2 — Enums and C's Type System

## Why this comes right after headers

Chapter 1 established that `kinyarwanda.h` is the contract. Now we look at
the single most-used *kind* of declaration in that contract: `enum`. Open
the header and count — `POS`, `PunctType`, `VerbTense`, `GramRole`,
`VerbExtension`, `PronounType`, `OrthoViolationType`, `ErrorType`. Eight
enums, one of them (`VerbTense`) with over twenty members. Before any
struct can make sense (Chapter 3), you need to understand exactly what an
`enum` is in C — and, just as importantly, what it is *not*, because C's
enum is much weaker than the enums you may have heard about in Java,
C#, Rust, or TypeScript, and several design decisions in this project only
make sense once you see where C's enum falls short and how the code
compensates.

## 2.1 An enum is just named integers — nothing more

```c
typedef enum {
    POS_UNKNOWN        = 0,
    POS_NOUN,
    POS_ADJECTIVE,
    POS_RELATIVE_NOUN,
    POS_COMPOUND_ADJ,
    POS_VERB_INF,
    POS_VERB_CONJ,
    POS_PRONOUN,
    POS_PREPOSITION,
    POS_CONJUNCTION,
    POS_INTERJECTION,
    POS_ADVERB,
    POS_ADVERB_TIME,
    POS_LOCATIVE,
    POS_VERB_PARTICLE,
    POS_NUMBER,
    POS_FOREIGN,
    POS_PUNCTUATION,
} POS;
```

At compile time, this is entirely equivalent to writing:

```c
#define POS_UNKNOWN 0
#define POS_NOUN 1
#define POS_ADJECTIVE 2
/* ... and so on, +1 each time ... */
```

`POS_UNKNOWN` is pinned to `0` explicitly. Every member after it with no
explicit value gets the *previous value plus one*, automatically — that's
why only the first line has `= 0` and the rest just list a name. If you
inserted a new tag in the middle of the list, every tag after it would
silently shift to a new number. That's fine as long as nothing outside
this file ever assumes a *specific* numeric value (and nothing in this
project does — everything compares by name, e.g. `tok->pos == POS_NOUN`,
never `tok->pos == 1`). It would only become dangerous if you serialized
these numbers to a file or another process and then changed the enum —
something this project never does, since `Token` structs never leave the
process.

`typedef enum { ... } POS;` does two things in one statement: it defines
the enum's members, and it gives the whole thing the alias `POS`, so you
can write `POS pos;` instead of the more verbose `enum POS pos;`
everywhere else in the codebase. This `typedef enum { ... } Name;` shape
is the standard C idiom — you'll see it on every enum and every struct in
this project (Chapter 3 covers the struct half).

**The crucial limitation**: in C, a variable of type `POS` is, under the
hood, just an `int` (or sometimes a smaller integer type the compiler
chooses). Nothing stops you from writing:

```c
POS p = 9999;       /* compiles fine — no enum value range-checking */
tok->pos = (POS)(-1);
```

C will not stop you. There is no runtime check that a `POS` variable
actually holds one of the listed values. This is fundamentally different
from enums in languages like Java or Rust, which are real, separate types
that the compiler enforces — you literally cannot assign an out-of-range
value to them. C's enum is a *naming convenience over int*, not a new
type the compiler protects. Keep this in mind; it explains a few
defensive `default:` cases you'll see in Section 2.5.

## 2.2 Names as hand-rolled namespacing

Notice every single member is prefixed: `POS_NOUN`, `TENSE_PRESENT`,
`VEXT_PASSIVE`, `PRON_DEMONSTRATIVE`, `GRAM_ROLE_MAIN_VERB`,
`ORTHO_VV_HIATUS`, `ERR_ADJ_AGREEMENT`. This isn't decoration — it's
working around a real gap in the language. C has no namespaces (no
`POS::NOUN` like C++, no module system like Python). Every enum member
name lives in the same single, global naming pool as every other
identifier in the program — functions, variables, everything. If
`VerbTense` and `VerbExtension` both had a member called `NONE`, that
would be a flat redefinition error, because both are competing for the
exact same global name.

The prefix convention (`TENSE_` / `VEXT_` / `PRON_` / `ERR_` / `ORTHO_`)
is the project's manual substitute for namespacing. It costs you some
typing; it buys you the ability to read `TENSE_NEG_ANTERIOR` in isolation,
anywhere in a 3,500-line file, and immediately know which enum family it
belongs to without scrolling up to check.

## 2.3 Zero as the deliberate "nothing happened yet" value

Look at the first member of every enum in this header:

```c
POS_UNKNOWN        = 0,
PUNCT_NONE         = 0,
TENSE_NONE         = 0,
GRAM_ROLE_NONE     = 0,
VEXT_NONE          = 0,
PRON_NONE          = 0,
ORTHO_OK           = 0,
ERR_NONE           = 0,
```

Every single one of these enums reserves `0` for "nothing" — unknown,
none, not-applicable, or (for `ORTHO_OK`) "no violation found." This is
not a coincidence; it's a pattern you should be able to name and defend.

Here's why it matters: in C, any variable with **static storage
duration**, and any array or struct that is **partially initialized**,
has its remaining bytes set to zero automatically by the compiler/runtime
— this is a guarantee from the C standard, not a convention. Consider
`SentenceAnalysis`, which contains `Token tokens[KIN_MAX_TOKENS]` (256
slots) — most sentences use far fewer than 256 tokens, so most of that
array is left untouched after the real tokens are written in. Because the
enum's zero value means "unknown/none," an untouched `Token` slot's
`pos` field reads as `POS_UNKNOWN` — a meaningful, correct value — purely
because zero-initialization happened to line up with "no information,"
not because anyone wrote code to explicitly set it. If `POS_NOUN` had been
given the value `0` instead of `POS_UNKNOWN`, every unused slot in that
256-element array would silently *look like a noun* until something
explicitly overwrote it. That single design choice — reserving zero for
"nothing" — is what makes the fixed-size-array design from Chapter 1
(Section "limits as `#define` constants") safe to use without manually
zeroing or tracking how many slots are "real."

## 2.4 The enum as a struct "tag" — C's substitute for tagged unions

This is the pattern you will see used constantly from Chapter 3 onward, so
it's worth naming precisely here. Look at how `Token` is shaped
(full detail in Chapter 3, but the relevant excerpt):

```c
typedef struct {
    char surface[KIN_MAX_WORD];
    POS  pos;                       /* the tag */
    PronounType   pron_type;        /* only meaningful if pos == POS_PRONOUN   */
    VerbTense     verb_tense;       /* only meaningful if pos == POS_VERB_CONJ */
    VerbExtension verb_ext;         /* only meaningful if pos == POS_VERB_CONJ */
    int  noun_class;                /* only meaningful if pos == POS_NOUN, etc.*/
    /* ... more fields ... */
} Token;
```

`pos` is the **tag**: a single field that tells you which of several
possible "shapes" of meaning the rest of the struct currently has. This
is the same idea as a *tagged union* (also called a "sum type" or
"discriminated union") in languages like Rust's `enum` or TypeScript's
discriminated unions — except C has a real `union` keyword that could do
this more memory-efficiently, and this project **deliberately doesn't use
it**. That's worth being able to defend on its own.

A C `union` overlaps its members in the *same* memory — if you wrote:

```c
union Payload { PronounType pron_type; VerbTense verb_tense; };
```

`pron_type` and `verb_tense` would share the same bytes; setting one
would corrupt the other unless you always remembered which one was
"active" via the tag. Reading the *wrong* union member for the current
tag is undefined behavior in C (not just "wrong," but a thing the
compiler is permitted to mishandle in unpredictable ways). A `union` would
make `Token` smaller, since `pron_type` and `verb_tense` would no longer
need separate storage — but in exchange you get a strictly more dangerous
type, harder to inspect in a debugger (a debugger showing a union just
shows whichever member you ask for, with no way to know which one is
"correct" without separately checking the tag), and harder to
`printf`/log for debugging output.

This project chose the safer, more debuggable option: a plain `struct`
with every possible field laid out side by side, always fully valid
memory, at the cost of a few unused bytes per `Token` when, say, a noun
token carries an always-irrelevant `verb_tense` field. Given `Token` is a
fixed, modest-size struct and there are at most 256 of them on the stack
at once (Chapter 1's `KIN_MAX_TOKENS`), that extra memory is a few
kilobytes, at most — a trivial cost next to the debugging and safety
benefit. If you're asked "why not use a `union` here to save memory," that
trade-off — safety and inspectability vs. a small, bounded memory saving —
is the honest answer.

## 2.5 `switch` on an enum: dispatch, and the `default:` trade-off

Enums exist to be `switch`ed on. Here's the dispatcher in
`morph_dispatch.c` that decides how to break a word into morphemes based
on its tag:

```c
switch (tok->pos) {
    case POS_NOUN:
    case POS_RELATIVE_NOUN:
    case POS_COMPOUND_ADJ:
        analyse_noun(tok);
        break;

    case POS_ADJECTIVE:
        analyse_adj(tok);
        break;

    case POS_VERB_CONJ:
        analyse_vconj(tok);
        break;

    case POS_VERB_INF:
        analyse_vinf(tok);
        break;

    default:
        /* Invariables, pronouns, foreign words: no morpheme breakdown */
        break;
}
```

Two C-specific things to notice:

1. **Fallthrough grouping**: `case POS_NOUN:` / `case POS_RELATIVE_NOUN:`
   / `case POS_COMPOUND_ADJ:` stacked with no `break` between them means
   all three tags run the *same* code (`analyse_noun(tok)`). In C, a
   `case` label without a `break` falls through to the next one — this is
   usually a bug magnet (forgetting a `break` is one of C's most common
   mistakes), but here it's used *deliberately and correctly*: these three
   POS tags really should be handled identically, because all three are,
   structurally, a noun-shaped word (D+RT+C — see your linguistics
   knowledge here, not the C side).
2. **The `default:` case**: this is what makes the `switch` exhaustive
   from the compiler's point of view — every possible `int` value
   (remember, a `POS` *is* just an int) is handled by *something*, even
   values not listed by name.

Now, the trade-off: this project compiles with `-Wall -Wextra -Wpedantic`
(see the Makefile). One of the warnings that combination enables is
`-Wswitch`, which specifically checks: *if you `switch` on an enum type
and do not include a `default:` case, did you handle every single named
member of that enum?* If a new `POS_*` value were added later and a
`switch` like this one forgot to handle it, `-Wswitch` would flag it at
compile time — **but only if there is no `default:` case**. The moment you
add `default:`, the compiler considers the switch fully handled no matter
what, and that exhaustiveness check goes silent.

So why does this codebase add `default:` everywhere, giving up that
safety net? Because in this specific switch, the *correct* behavior for
every POS tag not explicitly listed (pronouns, prepositions, conjunctions,
punctuation, foreign words...) really is "do nothing" — there's no
morpheme breakdown to compute for an invariable word. Silently doing
nothing is the right behavior for an open-ended set of "everything else"
tags, so `default:` is the honest, correct choice here, not a missed
safety opportunity. The trade-off to be aware of: if a *new noun-like* POS
tag were ever added and the author forgot to add it to the first group of
cases, it would silently fall into `default:` and get no morpheme
analysis — a real bug that `-Wswitch` would no longer catch. That's a
genuine, defensible limitation you can point to if asked "is this code
perfect?" — the honest answer is "no, and here's the specific trade-off."

## 2.6 Enums have no built-in string form — hence the `_name()` functions

Unlike some higher-level languages, C gives you no way to ask an enum
value to print its own name. `printf("%d", tok->pos)` will print `6` (or
whatever the underlying integer is) — not `"POS_VERB_CONJ"`. To produce a
human-readable label, every enum in this header is paired with a small
function whose entire job is mapping value → string:

```c
const char *kin_pos_name(POS pos) {
    switch (pos) {
        case POS_NOUN:         return "Izina mbonera (Noun)";
        case POS_ADJECTIVE:    return "Ntera (Adjective)";
        /* ... one case per POS value ... */
        default:               return "Ntizwi (Unknown)";
    }
}
```

This is the manual implementation of what a `toString()` method gives you
for free in an object-oriented language. C makes you write it yourself,
once per enum: `kin_pos_name()`, `kin_verb_tense_name()`,
`kin_verb_ext_name()`, `kin_pron_type_name()`, `kin_gram_role_name()`,
`kin_ortho_rule_name()`. Every one of these lives in `lexicon.c` (or
`ortho.c` for the orthography one) and follows the identical
switch-returning-a-string-literal shape. Notice the string literals
returned are `const char *` pointing at *string constants* — these live
in the read-only data section of the compiled binary (commonly called
`.rodata`), not on the stack or heap, so there's no lifetime issue
returning a pointer to one: it's valid for the entire life of the program.
You'll see this exact pattern reused for table lookups in Chapter 7 (some
of these could alternately be implemented as an array indexed directly by
the enum's integer value, which works precisely *because* enum values are
small, dense, sequential integers starting near zero — Chapter 7 will
contrast the switch approach against that array-indexing approach and
when each is preferable).

## 2.7 Try it yourself

Confirm for yourself that an enum is "just an int" with a tiny experiment.
Create `/tmp/enum_test.c`:

```c
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    POS p = POS_VERB_CONJ;
    printf("As a name, the tag is POS_VERB_CONJ.\n");
    printf("As a number, it's: %d\n", p);          /* an int, not a struct */
    printf("sizeof(POS) = %zu bytes\n", sizeof(POS)); /* usually 4, same as int */

    p = (POS)999;                                   /* compiles with NO error */
    printf("Out-of-range POS still compiles: %d\n", p);
    return 0;
}
```

Compile it: `gcc -Iinclude -Wall -Wextra -c /tmp/enum_test.c -o /tmp/enum_test.o`
— notice `-Wall -Wextra`, the same flags this project's Makefile uses,
produce **no warning at all** about assigning `999` to a `POS`. That's the
concrete, hands-on proof of Section 2.1's claim: C's enum type does not
range-check assignments. The safety this project has comes entirely from
*disciplined usage* (only ever assigning named constants) — never from
the compiler enforcing it.

## Key takeaways

- A C `enum` is just a set of named `int` constants with auto-incrementing
  values; the compiler does **not** enforce that a variable of that enum
  type only ever holds one of the listed values.
- Every enum in this project prefixes its members (`POS_`, `TENSE_`,
  `VEXT_`, ...) as a hand-rolled substitute for the namespacing C lacks.
- Every enum reserves `0` for "none/unknown/ok" — this lines up with C's
  guarantee that statically-sized arrays and partially-initialized structs
  are zero-filled, so an unused slot automatically reads as "nothing,"
  with no extra code required.
- A struct with an enum "tag" field plus several payload fields side by
  side is a safer, more debuggable (but slightly larger) alternative to a
  C `union` — this project chose safety and inspectability over the small
  memory saving a `union` would offer.
- `switch` on an enum with no `default:` lets `-Wswitch` (enabled via
  `-Wextra`) warn you at compile time if you forget a case when the enum
  grows; adding `default:` silences that check but is the right call when
  "everything else" genuinely should be handled identically.
- C gives enums no built-in string conversion; this project pairs every
  enum with a hand-written `kin_*_name()` function that does the value →
  string mapping via `switch`.

## Search YouTube for

- "C enum tutorial — what enums really are"
- "C union vs struct explained"
- "tagged union discriminated union explained"
- "gcc -Wswitch exhaustiveness warning enum"
- "C string literals and the rodata section"

## Coming up in Chapter 3

Now that you understand the *tag* half of the tagged-struct pattern
(`POS pos`), Chapter 3 covers the *payload* half: the structs themselves
— `NounClass`, `KinMorpheme`, `MorphBreakdown`, `Token`, and
`SentenceAnalysis`. You'll see how C composes data from smaller pieces,
why arrays of structs (not structs of arrays) was the chosen layout, and
how `sizeof` and struct nesting work together to make the fixed-size,
no-`malloc` design from Chapter 1 actually hold together.
