# Kinyarwanda NLP in C — A Build-Along Book for Defending This Project

This book teaches C **through** this project, in the order the project actually
executes. Each chapter introduces only the C concepts the next file needed —
the same order you'd have learned them if you'd built this project yourself,
one file at a time.

Kinyarwanda grammar and orthography are **not** covered here — you already
know that. Every chapter is about the C language, the data structures, and
the engineering decisions.

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
13. `corrector.c` — Generating Candidate Corrections
14. `gloss.c` — Large Tables, Round Two, and Output Formatting
15. `analysis.c` — The Orchestrator, and Returning Structs by Value
16. `g2p.c` — Another Full Table-Driven Conversion Pass
17. `api.c` — Designing a Thin Convenience Layer
18. `main.c` — `argv`/`argc`, File I/O, and the REPL Loop

## Part III — Build, ship, verify
19. The Makefile Deep Dive — Objects, `.d` Files, Static vs. Shared Libraries
20. The Test Suite — Macros, `do { } while(0)`, and Testing a Rule-Based Engine
21. Defensive Programming — `snprintf`, No `malloc`, and Why
22. Cross-Language Integration — Why C, `extern "C"`, and What FFI Means

## Part IV — Defense prep
23. Full Trace: One Sentence Through Every File, Concept by Concept
24. Likely Defense Questions, Answered with Line References

---

Status: Chapter 1 in progress. Each chapter lives in its own file:
`book/01_...md`, `book/02_...md`, etc.
