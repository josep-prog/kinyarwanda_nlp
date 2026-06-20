# Chapter 24 — Cross-Language Integration: Why C, `extern "C"`, and What FFI Means

## The last design question

Every chapter so far explained a decision *inside* the C code. This
chapter explains the decision that shaped the language choice itself:
why this entire engine is written in C, when Python, Java, or
JavaScript could each plausibly have implemented the identical rule
tables and `if`-chains. The honest answer isn't "C is faster" — it's
that C is the only language whose compiled output every *other* language
already knows how to call.

## 24.1 Why C, restated precisely

Chapter 1 listed C's advantages briefly; here's why each one matters
specifically for a library meant to be embedded inside other systems:

- **Zero runtime dependencies.** No garbage collector, no virtual
  machine, no interpreter needs to be present on the target system —
  the compiled `.so`/`.a` is self-contained, runnable on anything with a
  C ABI, which is to say, almost everything.
- **Predictable latency.** No garbage-collection pause can interrupt an
  analysis call partway through, because there is no garbage collector
  (Chapter 23's no-heap-allocation discipline is what makes this true,
  not a separate guarantee bolted on afterward).
- **Deterministic memory.** Chapter 3 and Chapter 23 already covered
  this from the inside; from the outside, it means a caller embedding
  this library never has to worry about it allocating unbounded memory
  under unusual input.
- **WebAssembly compilability.** Emscripten compiles C (not C++ with
  exceptions, not a garbage-collected language) to WASM cleanly,
  letting this exact codebase run inside a browser with no server round
  trip.
- **Small binary size.** No language runtime to bundle alongside the
  actual logic.

None of these are abstract claims — every one of them is a direct
consequence of design choices you've already read the actual code for,
in Chapters 1, 3, 9, and 23.

## 24.2 What makes a compiled function "universally callable"

Every mainstream language's foreign-function mechanism — Python's
`ctypes`, Java's JNA, Go's `cgo`, C#'s `P/Invoke`, Node's N-API — works
on the same underlying principle: **load the compiled library into the
calling process's own memory, locate a function inside it by its exact
exported name, and call it directly as if it were a function pointer**,
using the platform's standard calling convention (Chapter 16 introduced
this idea when explaining how large structs get returned — the same
calling convention governs how *every* function's arguments and return
value are physically passed, in registers or on the stack, regardless of
which language originally wrote the caller).

This only works because C's compiled output has an unusually simple,
stable **ABI** (Application Binary Interface — the actual, physical
contract a compiled function honors, as opposed to the *source-level*
contract a header expresses). A C function's name in the compiled
binary's symbol table is, by default, exactly its source name —
`kin_analyze`, `kin_correct`, `kin_tokenize` — nothing more. Every FFI
tool in every language can find a function named `kin_correct` inside
`libkinyarwanda.so` because that name really is sitting there, verbatim,
in the binary's symbol table.

## 24.3 Why `extern "C"` (Chapter 1) specifically matters here

Chapter 1 explained the mechanism: `extern "C"` stops a C++ compiler from
mangling a function's name. Here's why that's not just a C++-interop
nicety but a genuine FFI concern: **every binding tool finds functions by
name**. If this library's functions were compiled with C++ name
mangling, a function declared as `kin_correct` in the source could end up
stored in the binary under something resembling
`_Z11kin_correctPKc` (a real, compiler-specific mangled form — the exact
characters vary by compiler and even compiler version, since name
mangling is not standardized across C++ implementations). A binding tool
would either need to somehow know that exact mangled string ahead of
time, or would simply fail to find the function at all. By ensuring
`kinyarwanda.h`'s declarations always produce plain, unmangled symbol
names — guaranteed by `extern "C"` for C++ callers, and true by default
for the actual C compiler that builds this project — every language's
binding tooling can rely on the function existing under exactly the name
its own header already advertises.

## 24.4 Why the public structs are deliberately "flat"

Look back at `Token` and `SentenceAnalysis` (Chapter 3) with this
chapter's question in mind: every field is a fixed-size array, a plain
`int`, an `enum`, or a `bool` — never a pointer to something the struct
itself doesn't own, never a C++-only construct like a `std::vector` or a
virtual method table. This is not incidental. For a calling language's
FFI layer to read a `Token` directly out of memory — which is exactly
what a Python `ctypes.Structure` or a Java JNA `Structure` subclass does
— that calling language needs to replicate the **exact byte layout** of
the real C struct: the same field order, the same sizes, the same
compiler-inserted padding (Chapter 3's alignment discussion, now directly
relevant from the outside, not just inside C). A struct built from
C++-only types has no such guaranteed layout a foreign language could
hope to replicate; a "flat," POD (Plain Old Data) struct, built only
from primitive types and fixed arrays, has a layout that's
straightforward — if still occasionally fiddly — to mirror precisely.
This is the structural reason `kinyarwanda.h` exposes `Token` and
`SentenceAnalysis` the way Chapter 3 described them, rather than as
anything more "modern" — flatness isn't a missed opportunity for
abstraction, it's a requirement for the struct to be legible across a
language boundary at all.

## 24.5 Why `api.c` (Chapter 18) is also an FFI design decision

Chapter 18 explained `kin_correct()` and `kin_g2p()` as a convenience
layer for C callers who don't want to learn the full `Token` model. From
this chapter's vantage point, there's a second, equally real reason
they exist: **a function that takes a `const char *` and returns a
`const char *` is close to the simplest possible thing for *any*
language's FFI tooling to call.** Marshalling a single string argument
and a single string return value requires almost no setup in `ctypes`,
JNA, or N-API — often a few lines of boilerplate. Marshalling the full
`SentenceAnalysis` struct across a language boundary is still entirely
possible (`kinyarwanda_api.h`'s own comment documents the "quick-access
fields" a caller could read directly), but it requires the calling
language to correctly mirror Chapter 3's exact struct layout, field by
field, padding included — real, fiddly work that a binding author has
to get exactly right or risk silently reading the wrong bytes. By
offering the string-in, string-out convenience functions *as well as*
the full struct-based API, this project gives every calling language a
choice: the simplest possible integration for common cases (ASR
post-processing, TTS phoneme generation), and the complete, detailed
model for callers willing to do the extra work of mirroring the real C
layout.

## 24.6 A brief, concrete illustration

To make Section 24.2's abstract description tangible, here is the
entire mechanism, in Python, using nothing but the standard library's
`ctypes` module — no extra packages, no code generation step:

```python
import ctypes

lib = ctypes.CDLL("./libkinyarwanda.so")
lib.kin_correct.restype = ctypes.c_char_p
lib.kin_correct.argtypes = [ctypes.c_char_p]

result = lib.kin_correct(b"Umuntu mugni aragenda")
print(result.decode("utf-8"))
```

Walk through this against Section 24.2's description: `ctypes.CDLL(...)`
performs step 1 (load the compiled library into this Python process).
Setting `.argtypes`/`.restype` performs step 3 (tell Python's FFI layer
the function's real signature, since C's compiled binary carries no
type information of its own for `ctypes` to discover automatically).
Calling `lib.kin_correct(...)` performs steps 2 and 4 together — Python
finds the symbol `kin_correct` in the loaded library and calls it
directly, passing the byte string through to the real C function
exactly as Chapter 18 wrote it, with no translation layer in between
beyond what `ctypes` itself needs to convert a Python `bytes` object into
a raw C string pointer. Every other language's FFI tool — Java's JNA,
Go's `cgo`, Node's N-API — performs the conceptually identical four
steps; only the exact syntax for declaring the signature changes.

## Key takeaways

- C's main advantage for this project isn't raw speed — it's that every
  other mainstream language already knows how to call a compiled C
  function, because C's ABI is simple and stable in a way other
  languages' compiled output generally is not.
- `extern "C"` matters for FFI specifically because every binding tool
  finds functions by their exact exported symbol name — name mangling
  would make that name unpredictable and compiler-version-dependent.
- Public structs meant to cross a language boundary need to be "flat"
  (primitive types, fixed arrays, no language-specific constructs),
  because the calling language has to replicate the exact byte layout,
  padding included, to read them correctly.
- A simple string-in/string-out convenience function (Ch.18) is not just
  easier for a C caller — it's close to the lowest-friction possible
  surface for any language's FFI tooling, sidestepping the
  struct-layout-mirroring problem entirely.
- Every FFI mechanism in every language performs the same four steps:
  load the library, find the function by name, know its signature well
  enough to follow the platform's calling convention, and call it
  directly — only the syntax for expressing this differs per language.

## Search YouTube for

- "what is FFI foreign function interface explained"
- "C ABI application binary interface explained"
- "Python ctypes tutorial calling C library"
- "name mangling C++ vs C explained"
- "WebAssembly Emscripten compiling C to WASM"

## Coming up in Chapter 25

Part IV begins here: Chapter 25 traces one real sentence — start to
finish — through every single file this book has covered, naming the
exact function, the exact algorithm, and the exact chapter for each
step along the way. It's the rehearsal run for your actual defense.
