# Chapter 5 — Amagambo Adahinduka: The Words That Never Change

## How this chapter works

Same rules as Chapters 1 through 4. Every code example was actually
compiled with `gcc -std=c99 -Wall -Wextra` and actually executed — the
output shown in a fenced block prefixed with `$` is the real terminal
output of that exact program.

This chapter is different from the start. Chapters 1 through 4 each
opened by quoting a formula — `D + RT + C`, `RS + C`, `SP + TM + C +
FV`, a connector fused with a person suffix — because each category's
entire grammatical story was "here is how this word's shape changes."
*Amagambo adahinduka* ("invariable words") have no formula to quote,
because nothing about them changes. That absence is the whole point of
this chapter, not a gap in it.

---

# Part 1 — Language and Code, Side by Side

## 1.1 What doesn't change, and why that's a real category

Every tree this book has built so far marks agreement: a noun's class
prefix, an adjective's concordance marker, a verb's subject and object
prefixes, a pronoun's connector. Locatives, conjunctions, adverbs,
interjections, and a handful of other small word classes do none of
that — the same spelling appears no matter what noun, class, or tense
surrounds it. This project's own source code states the category
directly, in the comment sitting above its lookup table (there is no
dedicated formula section for this category in the grammar reference
the way there was for nouns, adjectives, verbs, and pronouns — these
words are usually just listed, in any grammar, not derived):

> **TREE 5 — AMAGAMBO ADAHINDUKA (Invariable Words)**
> These words never change form regardless of context.
> Checked at POS priority Step 1 — BEFORE all morphological analysis.
>
> Sub-categories: **Indangahantu** (locative markers: *ku, mu, i, kuri,
> muri, kwa...*), **Umugereka** (true prepositions: *nka, bwa,
> nyiri*), **Icyungo** (conjunctions: *na, kandi, ariko, rero...*),
> **Akamamo** (adverbs: *cyane, neza, gato, kenshi...*),
> **Irangamutima** (interjections: *yee, ahaa, wee...*),
> **Ikegeranshinga** (verb particles: *ngo, ko, dore...*), and a small
> set of frozen/suppletive verb forms (*ni, si, ndi, ati...*).

Seven sub-categories, one shared property: whatever box a word lands
in here, it stays spelled exactly that way in every sentence it ever
appears in. That property is also why this tree is checked **first**,
before any noun, adjective, verb, or pronoun detector runs at all — if
a word matches one of these fixed spellings, there is nothing left to
derive, so there is no reason to try.

## 1.2 Build it: a toy invariable-word lookup

```c
/* p1_toy_inv.c -- a first, tiny invariable-word lookup */
#include <stdio.h>
#include <string.h>

typedef enum { T_LOC, T_PREP, T_CONJ, T_ADV, T_INTERJ, T_UNKNOWN } ToyPOS;
typedef struct { const char *word; ToyPOS pos; } ToyEntry;

ToyPOS toy_lookup(const char *word) {
    static const ToyEntry table[] = {
        { "ku",     T_LOC },
        { "na",     T_CONJ },
        { "cyane",  T_ADV },
        { "yee",    T_INTERJ },
        { "nka",    T_PREP },
        { NULL, T_UNKNOWN }
    };
    for (int i = 0; table[i].word; i++)
        if (strcmp(word, table[i].word) == 0) return table[i].pos;
    return T_UNKNOWN;
}

int main(void) {
    const char *tests[] = { "ku", "na", "cyane", "ikigo", NULL };
    for (int i = 0; tests[i]; i++)
        printf("%-6s -> %d\n", tests[i], toy_lookup(tests[i]));
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p1_toy_inv p1_toy_inv.c
$ ./p1_toy_inv
ku     -> 0
na     -> 2
cyane  -> 3
ikigo  -> 5
```

`ikigo` ("institution," a real noun) correctly falls through to
`T_UNKNOWN` — there's no prefix to strip, no rule to apply, just a
flat scan that either finds a literal match or doesn't. This is
already the entire algorithm. Every previous chapter's first toy
detector needed a prefix check, a length guard, at least one `if` that
inspected part of the word's *shape*. This one only ever compares
whole words. There is genuinely nothing else to build here — which is
itself worth sitting with, after four chapters of progressively
bigger formulas.

## 1.3 The seven sub-categories, with one real word each

| Sub-category | Kinyarwanda | One real example | Meaning |
|---|---|---|---|
| Locative | Indangahantu | `kuri` | to / towards |
| Preposition | Umugereka | `nyiri` | owner of |
| Conjunction | Icyungo | `ariko` | but / however |
| Adverb | Akamamo | `cyane` | very / a lot |
| Time adverb | Akamamo k'igihe | `none` | now / currently |
| Interjection | Irangamutima | `yee` | yes! |
| Verb particle | Ikegeranshinga | `ngo` | that / reportedly |

Two categories deserve a second look before moving on. **Verb
particles** look like ordinary conjunctions but specifically introduce
reported or quoted speech (`ngo`, "[he said] that..."); they get their
own POS rather than being folded into `POS_CONJUNCTION` because later
sentence-level analysis (outside this chapter's scope) needs to find
them specifically to recognize quotation structure. And a handful of
**frozen verb forms** — `ni` ("is"), `si` ("is not"), `ndi` ("I am"),
and the irregular *kumenya* ("to know") paradigm (`nzi`, `uzi`, `azi`,
"I/you/he know") — live in this same table even though they're
semantically verbs, because their conjugation is suppletive (REB
grammar's own term: *inshinga nkene*, "defective verb") and doesn't
follow Chapter 3's SP+TM+C+FV formula at all. Rather than teach
Chapter 3's conjugation engine a one-off exception for each of these,
the project simply lists their fixed surface forms directly — the
same "when no rule generalizes, just name the exception" choice
Chapter 4's `PRIMARY_NOUN_STEMS` made for noun/verb collisions.

## 1.4 Checkpoint: test yourself before continuing

1. Why does this tree get checked *first*, before nouns, adjectives,
   verbs, or pronouns, rather than last (as a catch-all for whatever
   nothing else matched)?
2. `toy_lookup` (Section 1.2) would need a sixth entry to recognize
   `ni` ("is") at all. What `ToyPOS` would you assign it, given that
   it's a verb semantically but never conjugates?
3. Name one way a flat table like this could go subtly wrong as it
   grows — not crash, just quietly give the wrong answer for some
   word. (Part 4's capstone finds three real examples in the actual
   project.)

---

# Part 2 — The Simplest Interface in This Book

## 2.1 One out-parameter, not two, not five

```c
bool kin_is_invariable(const char *word, POS *pos_out);
```

Chapter 1's noun detector reported two things (stem and class).
Chapter 3's verb detector reported five. This one reports exactly one:
a part-of-speech tag, and nothing else — no stem (there's no stem to
strip), no class (these words don't agree with anything), no tense.
The out-parameter pattern from Chapter 1, Section 2.1 hasn't gone
anywhere; it's just been handed the smallest possible job.

## 2.2 Why this chapter has no new way to crash

Every previous chapter found a new flavor of undefined behavior to
demonstrate, because every previous chapter's detector did real work
on a buffer: stripping a prefix, copying a stem, recursing on a
shortened string. `kin_is_invariable`'s entire body is one loop and
one `strcmp` (Part 6 quotes it in full) — there is no buffer to
overflow, no recursion to runaway, no out-parameter left stale on
failure because there's only one out-parameter and it's only ever
read after checking the return value the way it should be. The
honest lesson here is structural, not a bug demo: **the safest
function in this book is also the simplest one**, and that's not a
coincidence. Every defensive-programming lesson in Chapters 1 through
4 was really a lesson about the *risk a function takes on* by doing
more — copying bytes, recursing, tracking five independent pieces of
state. A function that does less has correspondingly less to get
wrong. That isn't an argument for never writing complex functions —
nouns, verbs, and possessives all genuinely need the complexity
Chapters 1, 3, and 4 found in them — but it is a real, observable
correlation worth carrying forward: complexity and risk scale
together, in this codebase and in general.

---

# Part 3 — Making It Interactive

## 3.1 Build it: a word-category lookup you can talk to

```c
/* p3_repl.c -- type a word, find out if it's invariable and what kind */
#include "kinyarwanda.h"
#include <stdio.h>
#include <string.h>

static const char *posname(POS p) {
    switch (p) {
        case POS_LOCATIVE: return "locative (indangahantu)";
        case POS_PREPOSITION: return "preposition (umugereka)";
        case POS_CONJUNCTION: return "conjunction (icyungo)";
        case POS_ADVERB: return "adverb (akamamo)";
        case POS_ADVERB_TIME: return "time adverb (akamamo k'igihe)";
        case POS_INTERJECTION: return "interjection (irangamutima)";
        case POS_VERB_PARTICLE: return "verb particle (ikegeranshinga)";
        case POS_VERB_CONJ: return "frozen/copula verb form";
        default: return "(other)";
    }
}

int main(void) {
    char line[64];
    printf("Type a word, or 'q' to quit.\n");
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == 'q' && line[1] == '\0') break;
        POS p;
        bool ok = kin_is_invariable(line, &p);
        if (!ok) { printf("  \"%s\" is not in the invariable-words table.\n", line); continue; }
        printf("  \"%s\" -> %s\n", line, posname(p));
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p3_repl.c -L . -lkinyarwanda -o p3_repl
$ printf "kandi\nnka\nikigo\nq\n" | ./p3_repl
Type a word, or 'q' to quit.
  "kandi" -> conjunction (icyungo)
  "nka" -> preposition (umugereka)
  "ikigo" is not in the invariable-words table.
```

`nka` came back as a preposition. Hold onto that; Part 4 finds out
it's only half the real story.

---

# Part 4 — Capstone: Auditing a Table That Grew by Hand

## 4.1 A flat table has a hazard none of the previous trees had

Every lookup table in this book so far was small enough, and curated
carefully enough, to be free of an obvious hazard: **what happens when
the same key gets added twice, with two different answers, by two
different rounds of edits?** `kin_is_invariable` resolves duplicate
keys by silently keeping whichever one appears *first* — there is no
warning, no error, nothing — so a duplicate entered later in the file
simply becomes permanently unreachable, dead code that compiles
cleanly and never runs. Searching the real `INVARIABLES[]` table's
source for words that appear more than once finds **sixteen** of them.
Testing every one against the real, compiled library shows exactly
which entries actually won:

```c
/* p4_capstone.c */
#include "kinyarwanda.h"
#include <stdio.h>

static const char *posname(POS p) {
    switch (p) {
        case POS_VERB_CONJ: return "VERB_CONJ";
        case POS_PREPOSITION: return "PREPOSITION";
        case POS_CONJUNCTION: return "CONJUNCTION";
        case POS_INTERJECTION: return "INTERJECTION";
        case POS_ADVERB: return "ADVERB";
        case POS_ADVERB_TIME: return "ADVERB_TIME";
        case POS_LOCATIVE: return "LOCATIVE";
        case POS_VERB_PARTICLE: return "VERB_PARTICLE";
        default: return "OTHER";
    }
}

int main(void) {
    const char *words[] = {
        "ariko","cyane","hafi","hanyuma","kenshi","naho","nanone","ngo",
        "nka","none","nta","ntaho","nzi","oya","yego","yewe", NULL
    };
    for (int i = 0; words[i]; i++) {
        POS p;
        int ok = kin_is_invariable(words[i], &p);
        printf("%-9s -> %s\n", words[i], ok ? posname(p) : "(not found)");
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_capstone.c -L . -lkinyarwanda -o p4_capstone
$ LD_LIBRARY_PATH=. ./p4_capstone
ariko     -> CONJUNCTION
cyane     -> ADVERB
hafi      -> ADVERB
hanyuma   -> ADVERB_TIME
kenshi    -> ADVERB
naho      -> CONJUNCTION
nanone    -> CONJUNCTION
ngo       -> VERB_PARTICLE
nka       -> PREPOSITION
none      -> CONJUNCTION
nta       -> ADVERB
ntaho     -> ADVERB
nzi       -> VERB_CONJ
oya       -> ADVERB
yego      -> ADVERB
yewe      -> INTERJECTION
```

Cross-referencing each result against both of its source-file entries
splits these sixteen into two genuinely different categories:

**Harmless duplicates (7)** — `ariko`, `cyane`, `kenshi`, `nta`, `nzi`,
`oya`, `yego` are each listed twice with the *same* POS both times
(one comment for `nzi` even says outright, "kumenya short
suppletive" — a second author re-documenting a fact the table already
had). Redundant, but never wrong: whichever entry wins, the answer is
identical. This is the same harmless-redundancy shape Chapter 2 found
in `kin_vv_join`/`build_adj` (Section 6.5 there) — code that grew in
more than one pass sometimes restates itself without anyone noticing,
and it costs nothing when it does.

**Real shadowing (9)** — `hafi`, `hanyuma`, `naho`, `nanone`, `ngo`,
`nka`, `none`, `ntaho`, and `yewe` are each listed twice with **two
different** POS values, and only the first one is reachable.
`nka`'s second entry, tagged `POS_CONJUNCTION`, has been completely
unreachable since the moment it was added — every call to
`kin_is_invariable("nka", ...)` has always returned, and always will
return, `POS_PREPOSITION`, because that's the entry the linear scan
finds first. `ngo`'s case is the most linguistically real: the first
entry (`POS_VERB_PARTICLE`, "that / in order to," reported speech) and
the shadowed second entry (`POS_ADVERB`, "they say / apparently,"
hearsay) are two genuinely distinct, both common, uses of the same
word — and the table can currently only ever report one of them, no
matter which sense actually fits the sentence in front of it.

`nka`'s case, traced through the actual linear scan, looks like this:

```
   INVARIABLES[] in source-file order:
     ...
     { "nka", POS_PREPOSITION }    <- line 803, the EARLIER entry
     ...
     { "nka", POS_CONJUNCTION }    <- line 810, UNREACHABLE
     ...

   kin_is_invariable("nka", &p) walks the array from index 0:
     ... every earlier entry: strcmp fails, keep going ...
     line 803's entry: strcmp("nka","nka") == 0  -> MATCH, return true
                        (line 810's entry is never even reached)
```

There is no error, no warning, nothing in `gcc -Wall -Wextra`'s output
that would ever point at line 810 — the array compiles exactly as
written, and the function runs exactly as written. The bug, if it's
fair to call a silently-unreachable grammatical sense a bug, is
entirely in what the *table as a whole* claims to do versus what one
specific linear scan can ever actually deliver.

This is a real, previously undocumented finding about the project,
discovered the same way every finding in this book has been: by
testing the compiled library against its own source, not by reading
the source and guessing what it does.

## 4.2 Why this hazard is specific to flat, hand-grown tables

Nothing about this is a flaw in the flat-table *design itself* —
Chapter 4's Part 5 already established that a flat lookup is the
right choice when the underlying set of words is small, closed, and
known in advance. The hazard is specific to how a flat table built by
hand, across many editing sessions and corpus passes, can silently
accumulate exactly the kind of duplicate-key conflict Section 4.1
found, with nothing in the language (C doesn't warn about a repeated
struct literal in an array) and nothing in the lookup function (a
linear scan has no way to *know* a key is meant to be unique) to catch
it. The fix isn't a cleverer data structure — it's the same fix every
real engineering team uses for exactly this hazard: a small script,
run once, that scans the table's own source for repeated keys and
prints every one, exactly the way Section 4.1's analysis was done by
hand for this chapter. That script doesn't exist yet in this project.
Section 8's practice exercises ask you to write it.

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 Completing the spectrum

Four chapters have now shown four different points on the same
spectrum — how much a category's surface form needs to be *derived*,
versus simply *known*:

| Tree | Stem set | Agreement | Design |
|---|---|---|---|
| Noun (Ch. 1) | open | class prefix (RT) | lookup + rule fallback |
| Adjective (Ch. 2) | closed (~40) | class prefix (RS) | closed-set check + rule |
| Verb (Ch. 3) | large lookup + shape fallback | subject + object (SP, OM) | hybrid lookup + heavy rules |
| Pronoun (Ch. 4) | closed (~80 whole words) | class-fused into the word | flat lookup, no rule |
| Invariable (Ch. 5) | closed (333 entries) | **none** | flat lookup, no agreement at all |

This chapter's category sits at the true floor of that spectrum: not
just "no rule needed to derive the word" (Chapter 4 already reached
that floor), but "no agreement to encode in the first place." A
locative, a conjunction, an interjection doesn't just happen to be
listed rather than derived — there is nothing about a noun's class,
a verb's tense, or anyone's person that this word could ever need to
reflect. That is the actual definition of *amagambo adahinduka*,
restated as an engineering fact rather than a grammatical label: a
word belongs in this tree exactly when its correctness never depends
on anything outside itself.

## 5.2 Why this tree has to run first

Section 1.1 stated this tree is checked before any other detector
runs. Now that Part 4 has shown what a flat table can hide, the
ordering matters for a second reason beyond efficiency: every other
detector in this project (Chapters 1, 2, 3's `kin_strip_noun_prefix`,
`kin_strip_adj_prefix`, `kin_is_verb_conjugated`) is a *heuristic* —
it can be fooled by a word that happens to resemble its pattern by
coincidence, the way Chapter 4's `PRIMARY_NOUN_STEMS` had to guard
`umugabo` against being mistaken for a deverbative noun. An exact
match against a known-fixed, closed list is never a coincidence in
that sense — if `kandi` is in the table at all, it really is the
conjunction "and also," full stop, with no shape-based ambiguity for
a later detector to second-guess. Running the safest, most certain
check first means every word that *can* be resolved with total
confidence is resolved that way, before any heuristic gets a chance
to guess wrong about it.

```
   Step 1 ── Tree 5 (Amagambo adahinduka): exact match, no ambiguity
                │
                ▼  (no match)
   Step 2+ ── Tree 1 (Noun), Tree 2 (Adjective), Tree 3 (Verb),
               Tree 4 (Pronoun): each a heuristic, each can be
               fooled by a word that merely resembles its pattern
                │
                ▼
            POS assigned
```

---

# Part 6 — Reading the Real Production Code

## 6.1 `kin_is_invariable`, in full

```c
bool kin_is_invariable(const char *word, POS *pos_out) {
    for (int i = 0; INVARIABLES[i].word; i++) {
        if (strcmp(word, INVARIABLES[i].word) == 0) {
            if (pos_out) *pos_out = INVARIABLES[i].pos;
            return true;
        }
    }
    return false;
}
```

The entire function — structurally identical to Chapter 4's
`kin_is_pronoun` (Section 6.1 there), because both functions are
solving the exact same kind of problem: a small, closed, fully-known
set of literal words, checked by exact match. The only difference
between this function and that one is which table it scans.

## 6.2 The table: 333 entries, seven sub-categories, one deliberate omission

```c
static const InvEntry INVARIABLES[] = {
    /* == Indangahantu (Locative markers) ============================= */
    { "mu",      POS_LOCATIVE },     /* in / at (Nt.18 locative)        */
    { "ku",      POS_LOCATIVE },     /* on / at / to (Nt.17 locative)   */
    /* ... */
    /* == kumenya (to know) -- irregular stative-present paradigm ===== */
    { "nzi",     POS_VERB_CONJ }, /* 1sg: I know */
    { "uzi",     POS_VERB_CONJ }, /* 2sg: you know */
    /* "muzi" (2pl: you know) OMITTED -- conflicts with umuzi (homestead) */
    { "azi",     POS_VERB_CONJ }, /* Nt.1 3sg: he/she knows */
    /* ... */
    { NULL, 0 }
};
```

333 entries across the seven sub-categories Section 1.3 listed. The
commented-out `"muzi"` line is worth contrasting directly with Part
4's findings, because it is the same *kind* of trade-off — a real
grammatical form left out of the table — made for the opposite
reason. Part 4's shadowed entries (`nka`'s conjunction sense, `ngo`'s
hearsay sense) are accidental: nobody intended for them to be
unreachable, and nothing in the file says so. `"muzi"` is the
project's author *explicitly choosing*, in a comment, not to add a
real word at all, because the cost of adding it (every future mention
of `umuzi`, "homestead," would risk being mistagged as the rare verb
form "you know") outweighs the benefit of recognizing it. One is a
documented decision; the others are silent accidents. Telling them
apart is only possible by reading the comment — which is exactly why
Section 6.3 of Chapter 4 made the same point about a different table,
and why it's worth making twice: a comment that explains a choice and
a missing feature that nobody noticed can look identical from the
outside, and the only way to know which one you're looking at is to
read what the code actually says, not just what it does.

## 6.3 Real traces, side by side

| Word | Sub-category | Resolved POS | Note |
|---|---|---|---|
| `kuri` | Indangahantu | `POS_LOCATIVE` | clean |
| `ariko` | Icyungo | `POS_CONJUNCTION` | harmless duplicate |
| `ngo` | Ikegeranshinga | `POS_VERB_PARTICLE` | shadows a real second sense (hearsay) |
| `nka` | Umugereka | `POS_PREPOSITION` | shadows its own conjunction entry |
| `nzi` | (kumenya, frozen) | `POS_VERB_CONJ` | suppletive, never conjugated by rule |
| `muzi` | — | not found | deliberately omitted, by name, in a comment |

---

# Part 7 — Why Every Agreement Checker Has to Skip These Words

## 7.1 Concordance, inverted

Parts 7 of Chapters 2, 3, and 4 each read a real agreement checker:
adjective-noun, subject-verb, possessive-noun. Every single one of
those checkers had to solve the same sub-problem first, before it
could compare a single class number: **find the actual noun an
agreement-marked word is supposed to agree with, in a sentence that
may have adjectives, adverbs, conjunctions, or locatives sitting in
between them.** This chapter's words are exactly the ones every one of
those searches has to look straight through.

## 7.2 The real evidence, already quoted in Chapter 3

Chapter 3, Section 7.2 quoted the real subject-verb agreement
checker's backward scan in full. Read it again with this chapter's
vocabulary in hand:

```c
for (int j = vi - 1; j >= 0; j--) {
    const Token *t = &sa->tokens[j];
    if (t->pos == POS_PUNCTUATION) {
        if (t->is_sent_boundary || t->is_clause_boundary) break;
        continue;
    }
    if (t->pos == POS_NOUN && t->noun_class > 0) { noun_idx = j; break; }
    if (t->pos == POS_ADJECTIVE  || t->pos == POS_ADVERB    ||
        t->pos == POS_CONJUNCTION || t->pos == POS_LOCATIVE  ||
        t->pos == POS_PRONOUN)
        continue;            /* skip these, keep looking further back */
    break;                   /* anything else: stop, can't safely attribute */
}
```

`POS_ADVERB`, `POS_CONJUNCTION`, and `POS_LOCATIVE` — three of this
chapter's own seven sub-categories — are named *explicitly*, by POS
constant, as words this scan must pass straight through without
stopping. This is not a coincidence of naming; it's the practical
payoff of Section 5.1's definition. An invariable word never carries
class agreement, so it can never *be* the noun an agreement check is
searching for, and a correct search has to know that and look past it
rather than mistake it for the end of the search. The same is true,
unread in this book but true in the project, of Chapter 4's
possessive-agreement checker and Chapter 2's adjective-agreement
checker — every agreement search in this project is, underneath, a
search for the nearest *real* noun, with an explicit list of which
POS values are safe to walk through and which ones mean "stop, this
isn't safely attributable anymore."

## 7.3 What happens if you don't skip them

```c
/* p_no_skip.c -- a toy agreement search WITHOUT the skip list */
#include <stdio.h>
#include <string.h>

typedef struct { const char *word; const char *pos; int noun_class; } Tok;

int find_subject_naive(Tok *toks, int verb_idx) {
    for (int j = verb_idx - 1; j >= 0; j--) {
        if (strcmp(toks[j].pos, "NOUN") == 0) return j;
        return -1;   /* BUG: stops at the first non-noun, full stop */
    }
    return -1;
}

int main(void) {
    Tok sent[] = {
        { "Umugabo", "NOUN", 1 },
        { "rwose",   "ADVERB", 0 },     /* "indeed" -- invariable */
        { "aragenda","VERB", 1 },
    };
    int idx = find_subject_naive(sent, 2);
    printf("subject found at index: %d (%s)\n", idx, idx >= 0 ? sent[idx].word : "NONE");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p_no_skip p_no_skip.c
$ ./p_no_skip
subject found at index: -1 (NONE)
```

`Umugabo rwose aragenda` ("The man indeed is going") has a perfectly
ordinary subject, one invariable adverb away from its verb — and a
search that stops at the first non-noun token loses it completely,
reporting no subject found at all. Multiply this by how often real
Kinyarwanda sentences place an adverb, a conjunction, or a locative
between a subject and its verb, and a checker without Section 7.2's
explicit skip list wouldn't just miss the occasional edge case — it
would silently fail on enormously common, perfectly correct sentences.
The fix is exactly the five-way `||` condition Section 7.2 already
showed: name every POS that's safe to see through, and only stop the
search when something arrives that genuinely *could* be the answer or
genuinely blocks the question (a sentence boundary, another verb, an
unrecognized word).

## 7.4 A correctness rule about invariable words themselves

Sections 7.1–7.3 were about *other* trees' agreement checkers needing
to see through invariable words. There is also a real, implemented
check in this project about getting an invariable word **itself**
wrong — proof that "invariable" (the spelling never changes once
chosen) is not the same claim as "there's nothing to get wrong"
(which exact word to choose can still depend on what follows it).
`ERR_WRONG_LOCATIVE` governs exactly one decision: a locative must be
the right *member of its own pair* — `mu` or `muri`, `ku` or `kuri` —
for whatever comes right after it. The real check, with its own
justification quoted directly from the source:

```c
/* Source: corpus analysis of Bibiliya Yera (30,984 sentences):
 *   muri + personal pronoun: 1,156 instances (zero exceptions)
 *   mu  + common noun:       13,626 instances (zero muri+common_noun)
 *   kuri + personal pronoun: 200+ instances
 *   ku  + common noun:       6,197 instances
 *
 * 11a: mu  + PRON_PERSONAL  -> should be "muri"
 * 11b: ku  + PRON_PERSONAL  -> should be "kuri"
 * 11c: muri + common noun   -> should be "mu"           */
```

This is the same evidence-from-a-real-corpus discipline Chapter 3
used to justify its tense-marker tables, applied here to a two-word
paradigm instead of a sixteen-class one: 30,984 real sentences, zero
counter-examples to either rule, is what makes this check confident
enough to fire automatically rather than just being a style
suggestion. Four minimal-pair test sentences against the real,
compiled library show both directions of the rule and both halves of
the `mu`/`ku` pair:

```c
/* p_loc.c */
#include "kinyarwanda.h"
#include <stdio.h>

static void run(const char *text) {
    SentenceAnalysis sa = kin_analyze(text);
    kin_suggest_corrections(&sa);
    printf("%-26s errors=%d\n", text, sa.error_count);
}

int main(void) {
    run("Umugabo yagiye mu nzu.");    /* mu + common noun   -- correct */
    run("Umugabo yagiye muri nzu.");  /* muri + common noun -- WRONG   */
    run("Umugabo yagiye muri we.");   /* muri + pronoun     -- correct */
    run("Umugabo yagiye mu we.");     /* mu + pronoun       -- WRONG   */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p_loc.c -L . -lkinyarwanda -o p_loc
$ LD_LIBRARY_PATH=. ./p_loc
Umugabo yagiye mu nzu.     errors=0
Umugabo yagiye muri nzu.   errors=1
Umugabo yagiye muri we.    errors=0
Umugabo yagiye mu we.      errors=1
```

Same two locatives, same following-word categories (a common noun
versus a personal pronoun), and the checker tells the two wrong
combinations apart from the two right ones every time — with a
suggestion message (quoted in full from a live run) that names the
exact fix: `"Replace 'mu we' with 'muri we'."` Nothing here contradicts
this chapter's central claim that these words never change shape —
`mu` is always spelled `mu` and `muri` is always spelled `muri`. What
this check enforces is a separate, narrower kind of correctness:
*which one of two fixed, already-known spellings belongs here*, decided
by the single word immediately to its right. It is the closest thing
this tree has to Chapter 3's `kugenda`/`kujya` lexical verb-selection
check (Section 7.6 there) — not a derivation rule, just a small,
explicit, corpus-justified lookup of which fixed word is correct next
to which kind of neighbor.

---

# Part 8 — Practice

### Beginner

1. **Extend `p1_toy_inv.c`** (Section 1.2) with three more real
   invariable words of your choice from `INVARIABLES[]`, in three
   different sub-categories, and verify your additions against the
   real library.
2. **Trace `ndi`** ("I am," Section 1.3) by hand: why does this word
   live in the invariable-words table instead of being handled by
   Chapter 3's regular SP+TM+C+FV conjugation engine?

### Intermediate

3. **Write the duplicate-key audit script** Section 4.2 said doesn't
   exist yet. It doesn't need to be C — a short script in any
   language that reads `src/lexicon.c`, finds every word that appears
   more than once inside `INVARIABLES[]`, and prints each one with
   both of its POS values is enough to reproduce Section 4.1's table
   automatically instead of by hand.
4. **Confirm one more shadowed entry's real-world impact.** Construct
   a sentence where `ngo`'s shadowed `POS_ADVERB` ("hearsay") sense is
   the only correct reading, run it through `kin_analyze`, and
   describe in your own words what (if anything) goes wrong as a
   result of the table only ever returning `POS_VERB_PARTICLE`.
5. **Extend `p3_repl.c`** (Section 3.1) to print *both* POS values for
   a word, if you can find them by reading `src/lexicon.c` directly,
   whenever the word is one of Section 4.1's sixteen duplicates.

### Advanced

6. **Build the naive agreement search from Section 7.3 into something
   safer**, by adding back a skip list for `POS_ADVERB` and
   `POS_CONJUNCTION` only (not the full five-way list), and find one
   real sentence where your narrower list still fails where Chapter
   3's full list succeeds.
7. **Decide, and justify in writing, what the right fix for `nka` is**
   (Section 4.1): delete the shadowed conjunction entry as genuinely
   dead code, or keep both senses and change `kin_is_invariable`'s
   contract so it can report more than one possible POS for a single
   word. Argue for one, acknowledging what the other would cost.
8. **Find a fourth real shadowed pair** in `INVARIABLES[]` that
   Section 4.1's sixteen-word list didn't include, by reading the
   table's full source yourself rather than trusting this chapter's
   list to be exhaustive.
9. **Find `ERR_MISSING_LOCATIVE`** (declared right next to
   `ERR_WRONG_LOCATIVE` in `kinyarwanda.h`) in `syntax.c`, work out in
   your own words what it checks that's different from Section 7.4's
   rule, and construct one real test sentence that triggers it.

## Key takeaways

- *Amagambo adahinduka* (invariable words) are defined by an absence,
  not a formula: a word belongs here exactly when its correctness
  never depends on agreeing with anything else in the sentence.
- `kin_is_invariable` is the simplest, safest function in this book —
  one loop, one `strcmp`, one out-parameter — and that safety is a
  direct consequence of doing the least work of any detector in the
  project, not a separate virtue.
- A flat, hand-grown lookup table has a real hazard none of this
  book's other designs share as sharply: duplicate keys resolve
  silently to whichever entry was written first, leaving the other
  permanently unreachable with no warning from the compiler or the
  function itself. Sixteen real duplicate keys exist in the project's
  own `INVARIABLES[]` table; nine of them genuinely shadow a different,
  real second meaning.
- A documented, deliberate omission (`"muzi"`, left out by name in a
  comment to avoid a worse conflict) and an undocumented, accidental
  shadowed entry can look identical from the outside — reading what a
  comment actually says, rather than assuming code that compiles
  cleanly is finished, is the only way to tell them apart.
- This category is checked before every other detector in the project
  for a reason beyond speed: an exact match against a known, fixed,
  closed list can never be fooled by coincidence the way every other
  chapter's shape-based heuristic can.
- Every real agreement checker in this book (Chapters 2, 3, and 4)
  has to explicitly name which invariable-word categories are safe to
  search straight through while looking for the noun a marked word
  actually agrees with — proven directly by quoting Chapter 3's own
  skip-list condition, and by showing, with a toy search that omits
  it, how often a real sentence would otherwise be misread.
- "Invariable" means a word's spelling never changes once chosen —
  it does not mean there's nothing to get wrong. `ERR_WRONG_LOCATIVE`
  is a real, corpus-justified (30,984 sentences, zero counter-examples)
  check that a locative is the right *member of its own pair* — `mu`
  vs `muri`, `ku` vs `kuri` — for the word immediately following it,
  the closest thing this tree has to Chapter 3's lexical
  `kugenda`/`kujya` verb-selection check.

## Sources quoted in this chapter

- `include/kinyarwanda.h` (`kin_is_invariable`, the `POS` enum, `Error`).
- `src/lexicon.c` (`INVARIABLES[]`, `kin_is_invariable`).
- `src/pos_tagger.c` (the Tree 1–5 comment block; the backward
  subject-search skip list, quoted again from Chapter 3).
- `src/syntax.c` (re-quoted from Chapter 3, Section 7.2, for Part 7's
  argument; `ERR_WRONG_LOCATIVE`'s real corpus-justified rule, Section
  7.4).
- Every `pN_*.c`/`p_*.c` program and every real-library run in this
  chapter was actually compiled with `gcc -std=c99 -Wall -Wextra` and
  actually executed to produce the exact output quoted above.

## Coming up in Chapter 6

Five chapters have now built or read all five trees this project's own
`pos_tagger.c` names. But every example in every one of those chapters
quietly assumed something this book has never actually examined: that
a raw line of Kinyarwanda text has already been split into the
`Token` array every `kin_analyze` example printed from. Chapter 6 goes
one layer earlier than any tree — into `tokenizer.c` — to find out how
a sentence becomes a list of words in the first place, and what
happens at the boundaries no tree-level rule ever has to think about:
punctuation, apostrophes, capitalization, and the handful of decisions
that have to be made correctly before any of the last five chapters'
detectors ever get a chance to run at all.
