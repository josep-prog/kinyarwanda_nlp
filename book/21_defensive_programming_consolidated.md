# Chapter 21 — Defensive Programming, Consolidated

## Why this chapter has no new source code

Every defensive habit in this chapter has already appeared, scattered
across Chapters 4 through 20, attached to one specific function each
time. This chapter does something different: it names each habit once,
precisely, gathers every place you've already seen it, and states the
underlying rule in general form — so that "what makes this code
defensively written" has one clean answer instead of needing to be
reassembled from fifteen files under pressure.

## 21.1 No heap allocation, anywhere

Not one call to `malloc`, `calloc`, or `free` exists in this project's
`src/` directory (Chapter 1 confirmed this with a direct search, and
Chapters 3 and 9 built on it). Every buffer — from a single `Token`
(Chapter 3) to the entire `SentenceAnalysis` (583.5 KB, measured in
Chapter 3 and confirmed structurally in Chapter 15) to `ortho.c`'s
512-byte scratch buffer (Chapter 9) — is a fixed-size array, sized by a
named `#define` constant, with automatic or static storage duration.

**The benefit**: no allocation can ever fail at runtime (there is no
`malloc` return value to check, because there is no `malloc` call); no
memory can leak (nothing was ever separately owned, so nothing needs
separate freeing); behavior is fully deterministic and portable,
including to environments like WebAssembly where a heap allocator may
not even be assumed (Chapter 1's "why C" reasoning). **The cost**: every
buffer reserves its full worst-case capacity up front, whether or not a
given input ever needs it — the honest, measured price of this is
Chapter 3's 583.5 KB per `SentenceAnalysis`, paid on every single call,
regardless of whether the sentence being analyzed is two words or two
hundred.

## 21.2 `snprintf`, never `sprintf` or unchecked `strcat`

Every formatted string in this project — every rule explanation in
`morph_dispatch.c` (Chapter 11), every bilingual error message in
`syntax.c` and `corrector.c` (Chapters 12–13), every banner in `main.c`
and `gloss.c` (Chapters 14, 18) — is built with `snprintf`, which takes
the destination buffer's true size as an explicit argument and *will not
write past it*, no matter how long the formatted result would otherwise
be. `sprintf` has no such limit — given a long enough input, it will
write past the end of a fixed buffer with no warning, which is exactly
the shape of a classic buffer-overflow vulnerability. **The rule, stated
generally**: any time you format text into a buffer whose size you know,
pass that size to the formatting call itself — never trust that the
result "probably" fits.

## 21.3 `strncpy`, plus an explicit terminator — most of the time

You first met this in Chapter 6: `strncpy(dst, src, n)` does **not**
null-terminate `dst` if `src` is `n` characters or longer — every safe
use of it in this codebase is followed by an explicit `dst[n] = '\0';`.
Chapter 11's `set_morph()` is the clearest example of this discipline
applied consistently, on purpose, in one small reusable function used
everywhere a `KinMorpheme` needs filling.

**Stated honestly, this discipline is not applied with perfect
uniformity everywhere.** Chapter 13 found `corrector.c` skipping the
explicit terminator after `strncpy(err->suggestion, sug, KIN_MAX_MSG -
1)`. Chapter 15 then settled exactly why that omission doesn't currently
cause a real bug: `kin_analyze()` zeroes the entire `SentenceAnalysis`
with one `memset` before anything else runs, so `err->suggestion`'s
final byte is already `'\0'` from that zero-fill, regardless of whether
`strncpy` itself reaches it. That's a real, identifiable difference
between *code that defends itself unconditionally* (`set_morph`) and
*code that happens to be safe because of a guarantee established
elsewhere* (`corrector.c`, relying on `kin_analyze`'s `memset`). Being
able to name that distinction — and exactly which line elsewhere in the
codebase is silently propping up the second case — is worth more in a
defense than asserting blanket consistency that a careful reading would
disprove.

## 21.4 Never read ahead without confirming the position exists

This rule appears at every scale in this project. Chapter 4's tokenizer
checks `q + 2 < word_end` before reading `q[1]`/`q[2]` while matching a
multi-byte UTF-8 sequence. Chapter 18's argument parser checks
`i + 1 < argc` before reading `argv[i+1]`. Both are the identical
discipline, applied to two completely different kinds of array (a
`char` buffer, an array of command-line argument strings): **before you
index one position past where you currently are, confirm that position
is actually inside the array** — reading past the end of any C array,
even by one element, is undefined behavior, not a guaranteed crash you
can rely on noticing.

## 21.5 Sentinel values: one idea, expressed through four different types

You've now seen the identical underlying idea expressed through four
different C types, each time reserving one otherwise-impossible value
from that type's normal range to mean "no real answer here":

| Type | Sentinel value | Meaning | Where |
|---|---|---|---|
| pointer | `NULL` | "no string/entry found" | Ch.4 `FITE_FORMS`, Ch.6 `OM_TABLE` |
| enum | `0` (e.g. `POS_UNKNOWN`, `TENSE_NONE`) | "no tag assigned yet" | Ch.2, throughout |
| `int` | `-1` | "this operation produced no valid length" | Ch.9 `repl_at` |
| enum (trailing member) | `PH_COUNT` | "not a real phoneme — just a count" | Ch.16 |

Recognizing all four as the *same* defensive idea — rather than four
unrelated tricks to memorize separately — is a strong, compressing way to
demonstrate understanding: a sentinel value is just whichever value a
given type can hold that could never be a legitimate answer, reserved
deliberately, and checked for at every call site before the "real" result
is trusted.

## 21.6 Capacity + count instead of unbounded growth

Every "list" in this entire project — `Token tokens[256]` +
`token_count` (Ch.3, Ch.4), `Error errors[64]` + `error_count` (Ch.3,
Ch.12), `KinMorpheme m[8]` + `n` (Ch.3, Ch.11), `KinPhonemeID
phones[512]` + `count` (Ch.16) — is the same idiom: a fixed-capacity
array, paired with an integer that tracks how much of it is actually in
use. None of them ever grow. This is the direct, structural consequence
of Section 21.1: without `realloc`, a "list" cannot expand past its
initial allocation, so every one of these arrays is sized, up front, to
the largest input the project is willing to support (`KIN_MAX_TOKENS`,
`KIN_MAX_ERRORS`, `KIN_MAX_MORPHEMES`, `G2P_MAX_PHONEMES`) — and every
function that writes into one of them checks its count against that
capacity before writing (Chapter 4's `emit_punct`, Chapter 12's
`add_error`), refusing to write — not crashing, not silently corrupting
adjacent memory — once the limit is reached.

## 21.7 Bounding a loop even when you trust its real termination condition

Chapter 10's fixed-point iteration loops are the clearest example: each
one has a real, intended termination condition (`if (!any) break;` —
stop once a full pass changes nothing) *and* a hard numeric cap on the
outer iteration count (`iter < 4`, `iter < 6`), purely as a safety net
against some unforeseen rule interaction that might otherwise never
settle. This is a different defensive technique from Sections 21.1–21.6
— those all bound *memory*; this bounds *execution* — but the underlying
instinct is identical: trust your own logic, but never let a single
unverified assumption be the only thing standing between correct
behavior and the program hanging or crashing.

## 21.8 An honest tally, not a perfect one

A fair, complete defense doesn't claim this codebase is flawless — it
names the specific places where the discipline above is incomplete, and
explains precisely why each one is or isn't a real problem in practice:

- **`corrector.c`'s missing explicit `strncpy` terminator** (Ch.13) —
  currently safe, but only because of `kin_analyze`'s `memset` (Ch.15),
  not because `corrector.c` defends itself unconditionally.
- **`analyse_pdf`'s `system()` call** (Ch.18) — safe in this program's
  actual context (a local user running a CLI tool against their own
  files), but a real command-injection *pattern* that would need
  hardening before ever accepting input from an untrusted source.
- **Inconsistent idiom choice for the same check** (Ch.5's `is_vowel()`
  `||`-chain versus Ch.13's `strchr`-based vowel test) — harmless, but a
  real, visible inconsistency in a codebase this size.
- **`KnownWord`'s mixed `const char *` and `char[]` fields** (Ch.7) — both
  compile and work correctly, but don't follow the struct's own stated
  convention with perfect uniformity.

None of these are hidden — each was found, named, and explained while
walking through the file it lives in, using the same reading skills this
whole book has been building. That's the actual, defensible position:
not "this code has no flaws," but "I know exactly where the rough edges
are, why each one exists, and whether it currently matters."

## Key takeaways

- No heap allocation buys determinism and portability at the cost of
  fixed, generous worst-case memory use — a deliberate, named trade-off,
  not an oversight.
- `snprintf` (bounded) replaces `sprintf`/unchecked `strcat` (unbounded)
  everywhere formatted output is built.
- The `strncpy`-plus-explicit-terminator discipline is followed
  consistently in some files and relies on an externally-established
  guarantee in at least one other — know which is which.
- Never index one position ahead of where you are without first
  confirming that position is inside the array.
- A sentinel value — `NULL`, enum `0`, `-1`, a trailing `_COUNT` member —
  is one general idea, expressed differently per type, always meaning
  "this is not a real answer."
- Capacity+count arrays are the direct consequence of having no
  `realloc`; every list in this project is bounded and checked before
  every write.
- Bounding a loop's iteration count, even when its real termination
  condition is trusted, defends against unforeseen logic errors turning
  into a hang rather than a wrong-but-survivable answer.
- A strong defense names specific known gaps and explains their actual
  risk, rather than claiming uniform perfection a careful read would
  contradict.

## Search YouTube for

- "buffer overflow vulnerabilities explained"
- "defensive programming principles C"
- "sentinel values in programming explained"
- "fixed size vs dynamic arrays tradeoffs"

## Coming up in Chapter 22

Chapter 22 returns to a question this book has mentioned but not yet
answered in depth: why C, specifically, for this project — and what
`extern "C"` (Chapter 1) and the absence of any runtime dependency
actually buy when this same compiled library gets called from Python,
Node.js, Java, or Rust. This is the last source-level chapter before
Part IV's defense-focused review.
