# Kinyarwanda AI Platform — Project Vision & Technical Roadmap

**Author:** Project Lead (Native Kinyarwanda Speaker)
**Technical Partner:** Claude Code (AI Engineering)
**Date:** March 2026
**Status:** Phase 1 in progress

---

## 1. The Problem This Project Solves

### 1.1 Rwandan Film and the Global Stage

Rwandan film struggles to reach international audiences for one fundamental reason: it is produced in a language that very few people outside Rwanda speak. Unlike French or English films, which travel freely across borders with subtitles or dubbing, Kinyarwanda films are effectively invisible to the global market.

The challenge is compounded by a lack of local engineers who can build transcription tools. Without transcription, there are no subtitles. Without subtitles, there is no translation. Without translation, Rwandan stories remain confined to Rwanda.

The goal is not a workaround or a compromise. The goal is a proper, linguistically accurate engine that understands Kinyarwanda the way a native speaker does — producing subtitles, translations, and eventually dubbed audio that sounds natural, not mechanical.

### 1.2 Access to Services in Your Own Language

Rwanda has a growing ecosystem of digital services: government platforms, banking apps, health information systems, educational tools. The majority of these services operate in French or English. For many Rwandans, especially outside Kigali, navigating these platforms in a foreign language creates a barrier that should not exist.

The solution is a conversational AI assistant that can explain any service — what it does, how to navigate it, what documents are needed — in spoken Kinyarwanda. Not translated text on a screen. A voice. A voice that sounds like a person, not a machine.

This assistant does not require every service to rewrite itself in Kinyarwanda. It sits on top of whatever exists and acts as a bridge between the platform and the person.

### 1.3 Why This Has Not Been Built Before

There are very few Kinyarwanda programmers. The NLP resources that exist for languages like English, French, and Swahili simply do not exist for Kinyarwanda at the same depth. Most existing AI tools treat Kinyarwanda as an afterthought — translating it through English first, losing nuance, generating grammatical errors, and producing output that sounds foreign to native ears.

This project builds natively. Every component understands Kinyarwanda grammar directly. That is the distinction.

---

## 2. What Is Being Built

The platform consists of seven interconnected systems, each building on the one before it.

```
┌──────────────────────────────────────────────────────────────┐
│                    THE FULL PLATFORM                         │
│                                                              │
│  1. NLP Engine (C)              ← foundation, in progress    │
│     tokenize · morphology · POS · syntax · spell correct     │
│                       ↓                                      │
│  2. G2P + Text Normalizer (C)   ← feeds both ASR and TTS     │
│     grapheme-to-phoneme · numbers · abbreviations            │
│                       ↓                                      │
│  3. ASR Engine                  ← speech → text              │
│     fine-tuned Whisper · offline · whisper.cpp inference     │
│                       ↓                                      │
│  4. TTS Engine                  ← text → speech              │
│     VITS/Coqui training · ONNX inference · offline           │
│                       ↓                                      │
│  5. Translation Layer           ← Kinyarwanda ↔ English       │
│     NLLB-200 · NLP engine validates output                   │
│                       ↓                                      │
│  6. Conversational Agent        ← service assistant          │
│     llama.cpp · RAG over service docs · ASR + TTS            │
│                       ↓                                      │
│  7. Voice Cloning / Film Dubbing ← film use case             │
│     preserve speaker voice across language boundaries        │
└──────────────────────────────────────────────────────────────┘
```

---

## 3. Component Breakdown

### 3.1 NLP Engine — Status: 90% Complete

**What it does:** The linguistic brain of the entire platform. Every other system feeds through this. It handles:

- Tokenization (splitting sentences into words correctly for Kinyarwanda's agglutinative structure)
- Morphological analysis (breaking words into prefix + stem + suffix, identifying noun class, tense, subject/object markers)
- Part-of-speech tagging (noun, verb, adjective, adverb, conjunction, pronoun, etc.)
- Syntax checking (agreement validation between noun and verb, noun and adjective, possessives)
- Spell correction (suggesting the correct form when a word has the wrong agreement)

**Built in:** C (standard C99, no external dependencies, runs anywhere)

**Remaining work:** Conditional tense detection, relative clause mood validation, noun class 12/13 plural pairing.

---

### 3.2 G2P + Text Normalizer — Status: To Be Built

**What it does:** Converts written Kinyarwanda text into a sequence of phonemes (sounds) that the TTS engine can pronounce. Also handles edge cases: numbers written as digits, abbreviations, foreign words, proper nouns.

Kinyarwanda is largely phonemic — words are pronounced close to how they are spelled — which makes this significantly simpler than the equivalent task in English. The digraphs (`ny`, `sh`, `ch`, `bw`, `rw`) and vowel sequences need specific handling.

**Built in:** C (integrates directly with the existing NLP engine)

**Why it matters for ASR too:** The same phoneme rules used to teach the TTS how to speak are used to validate what the ASR engine heard. If Whisper transcribes a sound that does not correspond to any valid Kinyarwanda phoneme sequence, the NLP engine flags it.

---

### 3.3 ASR Engine — Status: Planning Phase

**What it does:** Listens to spoken Kinyarwanda and converts it to text. This is the ear of the platform.

**Why ASR comes before TTS:** ASR has an immediate standalone use. Given a Kinyarwanda film, you run ASR and get a transcript. That is a complete, demonstrable product. TTS alone — a voice with nothing to say — requires the entire rest of the stack before it becomes useful.

**The approach:**

Whisper (OpenAI, fully open source) already has weak Kinyarwanda support from its pre-training. Fine-tuning it on available Kinyarwanda audio data pushes accuracy from weak to usable far faster than training from scratch.

For deployment, `whisper.cpp` — a C/C++ reimplementation of Whisper — runs fully offline on a CPU without requiring Python or a GPU. The fine-tuned model exports to GGML format and runs directly in whisper.cpp. This meets the offline and fast requirements.

**The NLP engine as post-processor:**

```
Audio input
    ↓
Whisper  →  raw transcript (may contain morphological errors)
    ↓
NLP Engine  →  corrects wrong prefixes, wrong agreement, unknown words
    ↓
Clean, validated Kinyarwanda transcript
```

Whisper hears the right sound but may write the wrong morphological form. The NLP engine knows Kinyarwanda grammar and catches these errors. The combination is stronger than either system alone.

---

### 3.4 TTS Engine — Status: Planning Phase

**What it does:** Reads Kinyarwanda text aloud in a natural, human-sounding voice. This is the mouth of the platform.

**The approach:** VITS (Variational Inference TTS) or Coqui TTS. Training happens in Python with PyTorch. The trained model is exported to ONNX format and loaded by ONNX Runtime (which has a C++ API), making inference fully offline and fast.

**Voice options:** See Section 5 for a full breakdown of voice source decisions.

---

### 3.5 Translation Layer — Status: Planning Phase

**What it does:** Translates between Kinyarwanda and English (and potentially French).

**Existing tools that work:**

- **NLLB-200** (Meta, open source) — supports Kinyarwanda natively, part of a 200-language model
- **opus-mt-en-rw** (Helsinki NLP, open source) — dedicated English ↔ Kinyarwanda model

Both run offline via ONNX Runtime. The NLP engine acts as post-processor on translated Kinyarwanda output, correcting grammatical agreement errors that the translation model produces.

**For film specifically:** The Kinyarwanda-to-English direction (subtitling Kinyarwanda films for international audiences) is simpler and comes first. The English-to-Kinyarwanda direction (dubbing international films) is harder because Kinyarwanda has a smaller vocabulary for some concepts — the system will provide explanatory phrasing where direct translation does not exist.

---

### 3.6 Conversational Agent — Status: Planning Phase

**What it does:** Allows any Rwandan to speak to any digital service in Kinyarwanda, regardless of what language that service uses.

**How it works:**

1. User speaks in Kinyarwanda → ASR converts to text
2. NLP engine validates and normalizes the text
3. The agent searches service documentation for relevant information (RAG — Retrieval Augmented Generation)
4. A language model generates a Kinyarwanda response
5. TTS reads the response aloud

The language model runs via `llama.cpp` — a C/C++ implementation that runs on CPU, offline, without a GPU.

**What is needed:** Service documentation in Kinyarwanda or translated to Kinyarwanda for the services the assistant will cover.

---

### 3.7 Voice Cloning / Film Dubbing — Status: Research Phase

**What it does:** Takes the voice of an actor from a Kinyarwanda film and generates new speech in that voice — in a different language. The actor still sounds like themselves after translation.

**Current best tools:**
- **XTTS v2** (Coqui, open source) — voice cloning from 3–30 seconds of reference audio
- **SeamlessM4T** (Meta, open source) — speech-to-speech translation preserving some speaker characteristics

**Build this last.** It is the most technically complex component and depends on having a working TTS engine first.

---

## 4. Technology Stack

The stack is split between training (where Python is necessary) and inference (where C/C++ enables offline, fast deployment).

| Component | Training | Inference (Deployed) |
|---|---|---|
| NLP Engine | C | C |
| G2P / Text Normalizer | C | C |
| ASR | Python + PyTorch | whisper.cpp (C++) |
| TTS | Python + PyTorch | ONNX Runtime (C++) |
| Translation | Python, fine-tune existing models | ONNX Runtime (C++) |
| Voice Cloning | Python + PyTorch | ONNX Runtime (C++) |
| Conversational Agent | Python (model preparation) | llama.cpp (C++) |

**Training must happen in Python.** The PyTorch ecosystem is the standard for machine learning model training and there is no practical alternative. This is where models are taught.

**Inference runs in C/C++.** Once a model is trained, it is exported and run through optimized C/C++ runtimes (whisper.cpp, llama.cpp, ONNX Runtime). This is how the same approach used by whisper.cpp and llama.cpp achieves offline, CPU-only, fast inference. The end user never needs Python installed.

This architecture achieves the offline and speed requirements.

---

## 5. Voice Data — ASR vs TTS Requirements

ASR and TTS have opposite data requirements. Understanding this determines which data sources to use for which system.

| Requirement | ASR (speech → text) | TTS (text → speech) |
|---|---|---|
| Goal | Understand any speaker | Generate one consistent voice |
| Data diversity | Many voices, accents, styles | One clean, consistent speaker |
| Your personal voice | Useful, one of many | Only needed for your specific voice |
| Kinyarwanda Bible audio | Excellent | Good if single reader |
| Podcasts / sports / news | Excellent | Not suitable |

### 5.1 Available Data Sources Rated for ASR

| Source | Estimated Hours | ASR Value | Notes |
|---|---|---|---|
| Kinyarwanda Bible audio | 70–80 hours | Excellent | Multiple speakers, text already exists |
| Mozilla Common Voice (rw) | ~8 hours validated | Good | Community recorded, verified transcripts |
| Podcasts | Variable | Excellent | Natural speech, conversational register |
| Sports commentary | Variable | Good | Fast speech, builds robustness |
| News and public speakers | Variable | Good | Formal register |
| **Total potential** | **100+ hours** | **Strong base** | Enough for a real model |

100+ hours for a low-resource language is a strong starting position. Most published Kinyarwanda ASR research works with significantly less.

**The Bible having multiple speakers is good news for ASR.** Diversity of voices is exactly what ASR training needs. All readers can be used without selecting one.

### 5.2 Your Voice — When It Matters

| Situation | Your Voice Required? | Alternative |
|---|---|---|
| ASR that understands all speakers | No, one voice among many | Use all available sources |
| Generic neutral assistant voice (TTS) | No | Bible reader is likely cleaner |
| Assistant that sounds like you specifically | Yes, mandatory | No substitute exists |
| Dubbing a film actor's voice | Only if the actor is you | Use that actor's audio |

Your voice is not required to build a working system. It is required only if you specifically want the assistant to sound like you as a person.

If that is the goal, the recommended approach is: train the initial model on Bible audio first (which teaches the model Kinyarwanda phonetics), then fine-tune on 2–4 hours of your recordings. Transfer learning means you need far fewer hours of personal recordings than if you started from scratch.

### 5.3 Why Bible Audio as the Starting Point for TTS

A single consistent Bible reader provides:
- Controlled recording environment (no background noise)
- Deliberate, clear pronunciation (readers do not mumble)
- Formal, neutral register appropriate for a public-facing assistant
- 70–80 hours of paired audio and text without requiring any new recording

If the Bible recording has multiple readers, select the single clearest and most consistent one for TTS training. Use all readers for ASR.

---

## 6. Build Order and Phases

### Updated Sequence (ASR First)

Starting with ASR rather than TTS is the correct strategic decision for this project. ASR produces a standalone, demonstrable product immediately: give it film audio, receive a transcript. Film subtitles are a real, visible win that can be shown to others. TTS alone requires the full stack before it becomes useful as a product.

```
Phase 1  [NOW]      NLP Engine — finish remaining gaps
                    Conditional tense detection
                    Relative clause mood validation
                    Noun class 12/13 pairing

Phase 2  [Next]     G2P + Text Normalizer — build in C
                    Phoneme rules for all Kinyarwanda digraphs
                    Number and abbreviation expansion
                    Integration with existing NLP engine

Phase 3             ASR — Fine-tune Whisper
                    Training data: Bible + Mozilla Common Voice + podcasts
                    Training environment: Python + PyTorch + Hugging Face
                    Inference: whisper.cpp (C++, offline, no GPU required)

Phase 4             ASR Post-Processor
                    Connect NLP engine output to Whisper output
                    Morphological error correction on transcripts

Phase 5             First Demonstration
                    Run a Kinyarwanda film through ASR
                    Produce automatic transcript and subtitles
                    Validate with native speaker review

Phase 6             TTS
                    Training: Coqui TTS or VITS on single consistent speaker
                    Inference: ONNX Runtime (C++, offline)
                    Initial voice: Bible reader or personal recording

Phase 7             Translation Pipeline
                    Fine-tune NLLB-200 or opus-mt-en-rw
                    Post-process translations through NLP engine
                    Film subtitle translation: Kinyarwanda → English first

Phase 8             Conversational Agent
                    llama.cpp for language model
                    RAG over service documentation
                    Full loop: ASR → NLP → Agent → TTS

Phase 9             Voice Cloning for Film Dubbing
                    XTTS v2 or SeamlessM4T
                    Kinyarwanda film actor voice preservation
                    Test: Kinyarwanda → English while preserving voice
```

---

## 7. What the Project Lead Must Provide

The technical partner (Claude Code) can build all software components. The following inputs require the project lead as a native Kinyarwanda speaker and domain expert.

| Input Required | Purpose | Phase |
|---|---|---|
| Kinyarwanda Bible audio files (access/format) | Primary ASR and TTS training data | Phase 3 |
| Podcast and sports audio sources | ASR training for natural speech | Phase 3 |
| Native speaker validation at each phase | Quality assurance — no tool can replace this | Every phase |
| Personal voice recordings (if personal voice TTS) | Fine-tune TTS on your voice | Phase 6 |
| Service documentation in Kinyarwanda | Train conversational agent | Phase 8 |
| Film audio samples | Test and train voice cloning | Phase 9 |
| Confirmation of unusual vocabulary, idioms, proverbs | Ensure NLP engine coverage is complete | Ongoing |

**The project lead's role as a native speaker is not optional.** Every trained model must be validated by someone who can judge whether the output sounds natural. This judgment cannot be automated. It requires a human who grew up speaking the language.

---

## 8. What Makes This Project Different

Most Kinyarwanda AI efforts treat it as a translation problem. They convert Kinyarwanda to English first, process it in English, then convert back. This approach produces output that sounds foreign because it was never actually understood in Kinyarwanda — it was translated, processed, and translated again, with errors accumulating at each step.

This project processes Kinyarwanda natively. The NLP engine written in C understands Kinyarwanda grammar directly: noun classes, verb conjugation patterns, morphological agreement rules, the agglutinative structure of words. This engine becomes the linguistic ground truth that validates every other component in the stack.

When the ASR engine mishears a word, the NLP engine catches it because the result is not valid Kinyarwanda grammar. When the translation engine produces grammatically incorrect Kinyarwanda, the NLP engine flags the error and corrects it. When the TTS engine is given text to speak, the G2P component derived from this same engine knows exactly how each word should sound.

The NLP engine is not one component among seven. It is the foundation that makes the other six trustworthy.

---

## 9. Current State of the NLP Engine

The following has been implemented and tested:

- Tokenizer with Kinyarwanda-specific rules
- Full morphological analyzer: prefix stripping, stem lookup, suffix identification
- 16-class noun system with subject/object prefix tables
- POS tagger: 9-step priority system (invariables → pronouns → known words → infinitives → proper nouns → nouns → adjectives → conjugated verbs → foreign words)
- Adjective agreement checker with 10 noun classes
- Possessive agreement checker
- Tense detection: present, past perfect, past imperfect, future
- -ti quotative particle paradigm (all 16 noun classes: ati, iti, bati, uti, nti, yati, mwati, twati, rwati, kuti, ruti, hati, kiti, tuti, buti, ziti)
- Reduplicated adjective support (barebare, muremure, rurerure, birebire patterns)
- Object marker validation with false-positive reversal
- kugenda/kujya semantic rule
- Spell corrector
- Syntax error reporting with suggested corrections
- Verb extension analysis (causative, applicative, passive, reciprocal, stative)
- `niba` conjunction and `gutya` adverb correctly tagged

Remaining gaps: conditional tense, relative clause mood, Nt.12/13 plural pairing.

---

*This document reflects the technical planning conversations of March 2026. It will be updated as each phase is completed.*
