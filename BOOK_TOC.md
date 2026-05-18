# Kinyarwanda NLP — Building a Native C Library
### Architecture, Data Structures, Morphological Algorithms, and Language Integration

---

## TABLE OF CONTENTS

---

## FOREWORD

- The Problem This Book Solves
- Who This Book Is For
- What You Will Be Able to Do After Reading It
- Prerequisites
- How This Book Is Organized
- Conventions, Notation, and Code Formatting
- A Note on Bilingual Terminology
- Source Code and Repository

---

## PART I — FOUNDATIONS: LANGUAGE AND ENGINEERING CONTEXT

### Chapter 1 — Kinyarwanda and the Challenge of Building NLP for It
- 1.1 A Language Spoken by Over 12 Million People
- 1.2 Why Kinyarwanda Is Hard for Software
- 1.3 The Gap in NLP Resources
- 1.4 What This Engine Does
- 1.5 The Broader Platform This Engine Feeds Into

### Chapter 2 — Linguistic Foundations Every Developer Needs
- 2.1 Bantu Language Family: Where Kinyarwanda Fits
- 2.2 Morphology: How Words Are Built
- 2.3 The Five Word-Type Trees
- 2.4 Morpheme Terminology — The Building Blocks
- 2.5 Phonological Rules — The Core Obstacle
- 2.6 The 16 Noun Classes — A Complete Reference
- 2.7 The RALC 2017 Orthographic Standard

---

## PART II — WHY C, AND WHAT WE ARE BUILDING

### Chapter 3 — Why C Was the Right Choice
- 3.1 What "Native C Library" Means
- 3.2 The Six Engineering Reasons C Was Chosen
  - 3.2.1 Zero Runtime Dependencies
  - 3.2.2 Predictable Latency
  - 3.2.3 Deterministic Memory: No malloc in the Hot Path
  - 3.2.4 Universal FFI: Every Language Can Call C Directly
  - 3.2.5 WebAssembly Compilation via Emscripten
  - 3.2.6 Small Binary Size
- 3.3 What We Are NOT Building and Why
- 3.4 Exact Terminology Used in This Book

### Chapter 4 — Project Architecture: Overview
- 4.1 The Two Things This Project Produces
- 4.2 The Analysis Pipeline: Seven Steps from Text to Result
- 4.3 How the Source Files Divide Responsibility
- 4.4 The Three Public Header Files
- 4.5 What the Makefile Does
- 4.6 Current Integration Use Case: ASR Fine-Tuning

### Chapter 5 — Project File Structure: Every File Explained
- 5.1 Directory Map with Purpose of Each File
- 5.2 How the Files Depend on Each Other
- 5.3 What Goes into the Library vs. the CLI Binary
- 5.4 Include Guards and `extern "C"` for C++ Callers

---

## PART III — DATA STRUCTURES AND ALGORITHMS

### Chapter 6 — Data Structures: Design Decisions Explained
- 6.1 The Design Philosophy: No Heap Allocation in the Core
- 6.2 Enums: The Foundation of Type Safety
- 6.3 The `NounClass` Struct
- 6.4 The `KinMorpheme` Struct
- 6.5 The `MorphBreakdown` Struct
- 6.6 The `Token` Struct: The Central Working Unit
- 6.7 The `SentenceAnalysis` Struct
- 6.8 The `Error` Struct
- 6.9 The `NounPluralPair` Struct
- 6.10 The `KinPhonemeSeq` Struct
- 6.11 Static Lookup Tables: The Linguistic Database in Memory

### Chapter 7 — Algorithms: Every Non-Trivial Logic Decision
- 7.1 Algorithm 1 — Longest Prefix Matching
- 7.2 Algorithm 2 — Greedy Longest Match
- 7.3 Algorithm 3 — Sequential Suffix Stripping
- 7.4 Algorithm 4 — Multi-Pass Rule Application
- 7.5 Algorithm 5 — Linear Priority Dispatch Chain
- 7.6 Algorithm 6 — Backward Context Scan
- 7.7 Algorithm 7 — Prefix Substitution for Correction
- 7.8 Algorithm 8 — Finite-State-Machine Pattern for Verb Forms
- 7.9 Summary Table: Algorithm by File, Function, Complexity, and Reason

---

## PART IV — DEEP CODE WALKTHROUGH: EVERY SOURCE FILE

### Chapter 8 — `kinyarwanda.h`: The Complete Type System
- 8.1 Why the Header IS the API
- 8.2 Constants and Limits
- 8.3 All 18 POS Tags
- 8.4 All 18 `VerbTense` Values
- 8.5 All 14 `VerbExtension` Values
- 8.6 All 9 `PronounType` Values
- 8.7 All 7 `GramRole` Values
- 8.8 The `NounClass` Struct: Field-by-Field
- 8.9 The `KinMorpheme` Struct: Field-by-Field
- 8.10 The `MorphBreakdown` Struct: Field-by-Field
- 8.11 The `Token` Struct: All 30+ Fields
- 8.12 The `SentenceAnalysis` Struct
- 8.13 Every Public Function Signature

### Chapter 9 — `tokenizer.c`: Splitting Text into Tokens
- 9.1 The Tokenization Challenge in Kinyarwanda
- 9.2 `kin_tokenize()`: Full Code Walk-Through
- 9.3 How Tokens Flow into the `Token[]` Array
- 9.4 Examples: 6 Sentences Before and After Tokenization

### Chapter 10 — `morphology.c`: Detection Functions for All Word Types
- 10.1 File Structure: Four Sections
- 10.2 Utility Functions
- 10.3 Phonological Rule Functions
- 10.4 Tree 1 — Noun Detection
- 10.5 Tree 2 — Adjective Detection
- 10.6 Tree 3a — Verb Infinitive Detection
- 10.7 Tree 3b — Conjugated Verb Detection

### Chapter 11 — `lexicon.c`: The Linguistic Database
- 11.1 The Role of the Lexicon in a Rule-Based Engine
- 11.2 `NOUN_CLASSES[16]`
- 11.3 `ADJ_STEMS[27]`
- 11.4 `PRONOUNS[]`
- 11.5 `INVARIABLES[]`
- 11.6 `NounPluralPair[]`
- 11.7 Known Verb Stems and Noun Stems
- 11.8 Numerical Value Lookup
- 11.9 Name-to-String Helpers

### Chapter 12 — `pos_tagger.c`: Part-of-Speech Assignment
- 12.1 The Tagging Problem: One Word, Multiple Possible Types
- 12.2 The 9-Step Priority System
- 12.3 `kin_tag_token()`: Complete Annotated Code
- 12.4 `check_deverbative()`: Detecting Verb-Derived Nouns
- 12.5 `kin_tag_sentence()`: Sentence-Level Context Passes
- 12.6 Conditional Clause Detection
- 12.7 `kin_tag_gram_roles()`: Assigning Sentence Roles
- 12.8 SP Ambiguity Resolution
- 12.9 Worked Example: Full Trace of "Imana yaremye ijuru n'isi"

### Chapter 13 — `morph_dispatch.c`: Filling the Morpheme Breakdown
- 13.1 Role: Bridge Between Word Type and Word Parts
- 13.2 The Dispatch Pattern: Switch on `tok->pos`
- 13.3 `analyse_noun()`: D + RT + C for All 16 Classes
- 13.4 `analyse_adj()`: RS + C
- 13.5 `analyse_vinf()`: PREF + root + FV
- 13.6 `analyse_vconj()`: SP + TM + OM + root + EXT + FV
- 13.7 `kin_morpheme_analyze()`: The Top-Level Dispatcher
- 13.8 The `verified` Flag
- 13.9 Examples: 8 Words Fully Traced Through `analyse_vconj()`

### Chapter 14 — `analysis.c`: The Full Pipeline Orchestrator
- 14.1 `kin_analyze()`: One Function That Runs the Whole Engine
- 14.2 `kin_resolve_sp_ambiguity()`: Post-Processing Pass
- 14.3 `kin_print_analysis()`: Human-Readable Output Format
- 14.4 `analyse_text()`: Splitting Multi-Sentence Input
- 14.5 `analyse_stream()`: Processing a File Line by Line
- 14.6 10 Fully Traced Pipeline Examples

### Chapter 15 — `syntax.c`: Grammar Agreement Checking
- 15.1 Four Rules Implemented and Their Textbook Sources
- 15.2 Rule 1 — Noun-Adjective Agreement
- 15.3 Rule 2 — Possessive Connector Agreement
- 15.4 Rule 3 — Sentence Completeness
- 15.5 Rule 4 — Unknown Word Flagging
- 15.6 Subject-Verb Agreement Checking
- 15.7 The `kugenda` vs. `kujya` Verb Selection Rule
- 15.8 Worked Examples: Correct and Incorrect Sentences

### Chapter 16 — `corrector.c`: Generating Correction Suggestions
- 16.1 How Corrections Are Generated
- 16.2 `build_adj()`: Constructing the Correct Adjective Form
- 16.3 Possessive Correction
- 16.4 Spelling Correction: Candidate Generation
- 16.5 How `kin_correct()` Is Used for ASR Post-Processing

### Chapter 17 — `ortho.c`: The RALC 2017 Orthographic Rule Engine
- 17.1 Why This Module Exists as a Separate File
- 17.2 `kin_ortho_gen()`: Forward Generation — The 16 Passes
- 17.3 `kin_ortho_validate()`: Surface Word Validation
- 17.4 `kin_ortho_recover_verb_root()`: Reverse-Engineering the Stem
- 17.5 `kin_ortho_nt9_stem()`: Nt.9/10 Stem Extraction
- 17.6 `kin_ortho_fix()`: Iterating to Stable Corrected Form
- 17.7 35 Complete Before/After Transformation Examples

### Chapter 18 — `g2p.c`: Grapheme-to-Phoneme Conversion
- 18.1 The Phoneme Inventory: 60+ Phoneme Types
- 18.2 `kin_normalize_text()`: Preparing Text for Conversion
- 18.3 `kin_g2p_word()`: Converting a Single Word
- 18.4 `kin_g2p_sentence()`: Full Sentence with Word Boundaries
- 18.5 `KinPhonemeSeq`: Integer Sequence and ASCII String
- 18.6 Use in the ASR Pipeline
- 18.7 Use in the TTS Pipeline
- 18.8 20 Words with ASCII Token Output and IPA Equivalent

### Chapter 19 — `gloss.c`: Interlinear Morpheme Glossing
- 19.1 What a Linguistic Gloss Is and Why It Matters
- 19.2 Leipzig Glossing Rules (LGR)
- 19.3 `VERB_GLOSS_TABLE[200+]`
- 19.4 `kin_fill_morpheme_glosses()`: Filling Every Morpheme Slot
- 19.5 `kin_print_interlinear()`: Leipzig-Style 4-Line Display
- 19.6 5 Complete Interlinear Examples

### Chapter 20 — `api.c`: High-Level Convenience Wrappers
- 20.1 The Purpose of `api.c`
- 20.2 `kin_correct()`: ASR Post-Processing
- 20.3 `kin_g2p()`: Phoneme String for TTS
- 20.4 Why These Wrappers Live in a Separate File

### Chapter 21 — `main.c`: The Command-Line Tool
- 21.1 Argument Parsing: The Six Operating Modes
- 21.2 `analyse_line()`: Single-Sentence Entry Point
- 21.3 `analyse_text()`: Multi-Sentence Splitter
- 21.4 `analyse_stream()`: Line-by-Line File Processing
- 21.5 `analyse_pdf()`: PDF Extraction via `pdftotext`
- 21.6 Interactive Mode: The REPL Loop
- 21.7 `print_help()`: Bilingual Help Output

---

## PART V — THE GRAMMAR TREES IN FULL DEPTH

### Chapter 22 — Tree 1: Nouns (Izina Mbonera) — D + RT + C
- 22.1 The Formula and What Each Part Means
- 22.2 All 16 Classes with Prefix, Agreement Markers, and Examples
- 22.3 Class Transitions and Sub-Types
- 22.4 Nt.9/10 Special Rules: Nasal Prefix Transformations
- 22.5 Code Examples with Full Output

### Chapter 23 — Tree 2: Adjectives (Ntera) — RS + C
- 23.1 The Formula and the Concordance System
- 23.2 The 27 Adjective Stems: Complete Table
- 23.3 Reduplication: `muremure`, `barebare`, `torotore`
- 23.4 Izina ntera vs. Ntera: A Critical Distinction
- 23.5 Agreement Correction: How the Engine Fixes Class Mismatches

### Chapter 24 — Tree 3: Verbs (Inshinga)
- 24.1 Part A: Infinitive — PREF + root + FV
- 24.2 Part B: Conjugated — SP + TM + OM + root + EXT + FV
- 24.3 All 6 Verb Modes
- 24.4 All 18 Tenses and Modes in the `VerbTense` Enum
- 24.5 All 14 Derivational Extensions — Complete Reference
- 24.6 The Copula `kuba`: Suppletive `-ri-` Paradigm
- 24.7 Negative Forms: `nt-`, `si-`, `ta-`, `da-`, `nti-`
- 24.8 The `kugenda` vs. `kujya` Selection Rule

### Chapter 25 — Tree 4: Pronouns (Ikinyazina)
- 25.1 Nine Sub-Types with Full 16-Class Paradigm Tables
- 25.2 How Pronouns Drive Agreement in the Sentence
- 25.3 Numerical Pronouns 1–7
- 25.4 Vocative Pronoun

### Chapter 26 — Tree 5: Invariable Words (Amagambo Adahinduka)
- 26.1 Prepositions
- 26.2 Conjunctions
- 26.3 Adverbs
- 26.4 Locatives
- 26.5 Interjections
- 26.6 Verb Particles
- 26.7 The `-ti` Quotative Paradigm: Full 16-Class Table

---

## PART VI — THE COMPLETE PUBLIC API REFERENCE

### Chapter 27 — API Reference: `kinyarwanda.h`
- 27.1 Core Pipeline Functions
- 27.2 Morphology Detection Functions
- 27.3 Lexicon Lookup Functions
- 27.4 Orthographic Rule Functions
- 27.5 String Helpers
- 27.6 Display Helpers

### Chapter 28 — API Reference: `g2p.h`
- 28.1 Text Normalization
- 28.2 G2P Conversion
- 28.3 Phoneme Utilities

### Chapter 29 — API Reference: `kinyarwanda_api.h` (Convenience Layer)
- 29.1 `kin_correct()` — ASR Post-Processor
- 29.2 `kin_g2p()` — TTS Phoneme String
- 29.3 Thread Safety Notes
- 29.4 Return Value Lifetimes

### Chapter 30 — CLI Tool: Complete Usage Guide
- 30.1 Installation
- 30.2 All Flags and Modes
- 30.3 Output Format: Every Column Explained
- 30.4 Interactive Mode
- 30.5 Batch Processing with `batch_tag`
- 30.6 Unix Manual Page
- 30.7 The 36 Built-in Test Cases

---

## PART VII — INTEGRATING THE NATIVE LIBRARY IN YOUR PROJECT

### Chapter 31 — Before You Integrate: Understanding What You Are Linking
- 31.1 Static vs. Dynamic Linking
- 31.2 The Three Header Files You Must Include
- 31.3 Memory Ownership Rules
- 31.4 The Current Primary Integration: ASR Fine-Tuning with Whisper

### Chapter 32 — C and C++: Direct Integration
- 32.1 Building and Installing the Library
- 32.2 Compiler and Linker Flags
- 32.3 C++ Callers: `extern "C"` Is Already Handled
- 32.4 Complete C Example: Full Sentence Analysis
- 32.5 Complete C Example: ASR Post-Processing with `kin_correct()`
- 32.6 Complete C Example: TTS Phoneme Pipeline with `kin_g2p()`
- 32.7 Complete C Example: Morpheme-Level Analysis
- 32.8 Complete C++ Example: Wrapping the API in a Class
- 32.9 Complete C Example: Batch POS Tagging a Text File

### Chapter 33 — Python: Three Ways to Call the Native Library
- 33.1 What "Python Binding" Means Here
- 33.2 Approach 1: `ctypes` (Built-In, No Dependencies)
- 33.3 Approach 2: `cffi` (Cleaner Type Safety)
- 33.4 Approach 3: Python C Extension Module
- 33.5 Complete Example (ctypes): Sentence Analyzer
- 33.6 Complete Example (cffi): Batch POS Tagger
- 33.7 Complete Example: ASR Post-Processing Script
- 33.8 Complete Example: Phoneme Generation for TTS
- 33.9 Notes on Windows (DLL) vs. Linux (.so) Paths

### Chapter 34 — JavaScript and Node.js: Three Ways to Call the Native Library
- 34.1 What "JavaScript Binding" Means Here
- 34.2 Approach 1: `ffi-napi` (Node.js, Runtime FFI)
- 34.3 Approach 2: WebAssembly via Emscripten
- 34.4 Approach 3: Native Addon (`node-addon-api` / N-API)
- 34.5 Complete Node.js Example: REST API with Express.js
- 34.6 Complete Browser Example: Real-Time Grammar Checker
- 34.7 Complete Node.js Example: Batch Processing a Corpus

### Chapter 35 — Java: JNI and JNA
- 35.1 What "Java Binding" Means Here
- 35.2 Approach 1: JNI (Java Native Interface)
- 35.3 Approach 2: JNA (Java Native Access)
- 35.4 Complete Java Example (JNA): Sentence Analyzer
- 35.5 Complete Android Example: On-Device NLP

### Chapter 36 — Other Languages: The General Pattern
- 36.1 What Every Language Binding Needs
- 36.2 Rust — `bindgen` + FFI
- 36.3 Go — `cgo`
- 36.4 Ruby — `ffi` gem
- 36.5 PHP — FFI Extension (PHP 7.4+)
- 36.6 .NET / C# — P/Invoke with `DllImport`
- 36.7 The "Wrapper Package" Pattern
- 36.8 C-to-Language Type Mapping Reference Table

---

## PART VIII — THE BROADER PLATFORM AND FUTURE ROADMAP

### Chapter 37 — The Seven-Layer AI Platform
- 37.1 Architecture Overview
- 37.2 Layer 1 → 2: NLP Engine Feeds G2P
- 37.3 Layer 2 → 3: G2P Feeds ASR Fine-Tuning (Whisper)
- 37.4 Layer 2 → 4: G2P Feeds TTS Acoustic Model Training
- 37.5 Layer 1 → 5: NLP Validates NLLB-200 Translation Output
- 37.6 Layer 3+4+5 → 6: Conversational Voice Agent with RAG
- 37.7 Layer 6 → 7: Voice Cloning for Film Dubbing

### Chapter 38 — Planned Features and Known Gaps
- 38.1 Compound Nouns
- 38.2 Noun Class Shifts
- 38.3 Igisantera (Compound Adjective)
- 38.4 Full Punctuation Restoration for ASR Output
- 38.5 Demonstrative Proximity Forms 3–6
- 38.6 Numerals Beyond 7
- 38.7 REST HTTP Wrapper
- 38.8 Python pip Package Distribution
- 38.9 WebAssembly Browser Bundle

---

## APPENDICES

- **Appendix A** — The 16 Noun Classes: Complete Reference Table
- **Appendix B** — All 27 Adjective Stems: Table with Meaning and Examples
- **Appendix C** — SP_TABLE: Subject Prefix for All Classes and Persons
- **Appendix D** — OM_TABLE: Object Marker for All 16 Classes
- **Appendix E** — VerbTense Quick Reference
- **Appendix F** — VerbExtension Quick Reference
- **Appendix G** — RALC 2017 Orthographic Rules: Complete Reference
- **Appendix H** — Phoneme Inventory
- **Appendix I** — Glossary of Kinyarwanda Grammatical Terms
- **Appendix J** — Glossary of NLP and C Programming Terms
- **Appendix K** — Data Structure Quick Reference
- **Appendix L** — Algorithm Quick Reference
- **Appendix M** — Bibliography and Source Authority

---

## INDEX
