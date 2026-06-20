# Kinyarwanda NLP in C — A Build-Along Book for Defending This Project

This book teaches C **and** Kinyarwanda grammar **together, through this
project**, in the order the project actually executes. Each chapter
introduces only the C concepts the next file needed — the same order you'd
have learned them if you'd built this project yourself, one file at a
time — and, wherever the code encodes a piece of Kinyarwanda grammar or
orthography, the chapter stops and explains that piece of grammar on its
own terms first, in plain English, before showing how it became C code.

You do **not** need to already speak Kinyarwanda to read this book. Every
linguistic concept — noun classes, verb conjugation, the RALC 2017 spelling
rules, phonological sound changes — is introduced from first principles,
with a visual diagram, a glossary of the Kinyarwanda grammatical terms used,
and at least one fully worked case study (a real word or sentence, traced
step by step, with an English gloss) before any C code is shown. If you DO
already speak Kinyarwanda, those sections double as a precise, formal
restatement of grammar you know informally — useful for defending *why* a
rule is implemented the way it is, not just that it works.

Chapters that are pure engineering (the Makefile, the test macros, FFI
mechanics) stay focused on C/build concerns, since there is no grammar to
explain there.

## Part I — Foundations laid before the pipeline runs
1. Project Anatomy & The Header File as a Contract
2. Enums and C's Type System
3. Structs: Composing the Data the Whole Project Runs On

## Part II — Walking the pipeline in execution order
4. `tokenizer.c` — Strings, Char Arrays, and Splitting Text
5. `morphology.c` (I) — Pointers as Multiple Return Values
6. `morphology.c` (II) — Phonological Detection & the Longest-Match Algorithm
7. `lexicon.c` — Static Lookup Tables as a Compiled-In Database
8. `pos_tagger.c` — Priority Dispatch Without OOP Polymorphism
9. `ortho.c` (I) — Mutable Buffers, `memmove`, and Bounds-Checked Editing
10. `ortho.c` (II) — Multi-Pass Rule Engines & Fixed-Point Iteration
11. `morph_dispatch.c` — Switch-Based Dispatch and Safe String Building
12. `syntax.c` — Cross-Token Consistency Checks Over Arrays of Structs
13. `punctuation.c` — Clause Boundaries and Corpus-Driven Heuristics
14. `corrector.c` — Generating Candidate Corrections
15. `gloss.c` — Large Tables, Round Two, and Output Formatting
16. `analysis.c` — The Orchestrator, and Returning Structs by Value
17. `g2p.c` — Another Full Table-Driven Conversion Pass
18. `api.c` — Designing a Thin Convenience Layer
19. `main.c` — `argv`/`argc`, File I/O, and the REPL Loop
20. `validator.c` — Bilingual Reporting and the Presentation Layer

## Part III — Build, ship, verify
21. The Makefile Deep Dive — Objects, `.d` Files, Static vs. Shared Libraries
22. The Test Suite — Macros, `do { } while(0)`, and Testing a Rule-Based Engine
23. Defensive Programming — `snprintf`, No `malloc`, and Why
24. Cross-Language Integration — Why C, `extern "C"`, and What FFI Means

## Part IV — Defense prep
25. Full Trace: One Sentence Through Every File, Concept by Concept
26. Likely Defense Questions, Answered with Line References

---

Status: Chapter 1 in progress. Each chapter lives in its own file:
`book/01_...md`, `book/02_...md`, etc.
