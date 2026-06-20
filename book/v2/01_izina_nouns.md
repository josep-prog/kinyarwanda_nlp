# Chapter 1 — Izina: The Kinyarwanda Noun, Built in Language and Code Together

### How this chapter — and every chapter after it — is built

A first draft of this chapter taught all of the Kinyarwanda grammar
first, in one long block, and only afterward opened any code. That
sounds reasonable, but it has a real cost: by the time you finally reach
the code, you've been holding several pages of unfamiliar grammar in
your head with nothing to anchor it to, and the concepts start to fade
exactly when you need them most. So this chapter (and every one after
it) is built differently — in **small, paired steps**:

```
   teach ONE          anchor it with ONE          teach the NEXT
   Kinyarwanda    →   small, real piece of   →    concept, and
   concept             C code (a field, a          repeat
                       table row, one line)
```

You will see code far sooner than you might expect, but only ever a
small, focused piece — just enough to make the grammar concept you
*just* read feel concrete. The big, complete functions only appear near
the end of the chapter, once every individual piece they're built from
is already familiar to you. Reading the full function at that point
should feel like recognizing old friends, not decoding something new.

By the end of this chapter you will be able to: explain what an *izina*
(noun) is and how it's assembled; recognize all 16 noun classes and what
binds each one together; and read, explain, and rebuild from scratch the
two C functions (`kin_detect_noun_class`, `kin_strip_noun_prefix`) that
turn a raw word like `"abahinzi"` into the structured fact "this is a
plural human noun, class 2, meaning farmers."

If you already speak Kinyarwanda: the grammar sections will still be
worth reading closely, because they restate what you know informally as
a precise, named, *citable* rule — which is what you need to defend a
design decision, not just feel that it's right.

---

## 1.1 What is an *izina*, and how does the engine mark one?

*Izina* (plural *amazina*) is the Kinyarwanda word for **noun** — and
also the everyday word for "name." It names a person, animal, thing,
place, or idea, exactly the role a noun plays in English.

The very first decision this project's code makes about *any* word is
binary: is it a noun, or not. That decision is recorded in exactly one
field, on exactly one struct, using a value you'll recognize if you've
read this project's type system before:

```c
typedef enum {
    POS_UNKNOWN  = 0,
    POS_NOUN,              /* Izina mbonera — this is the tag for "izina"  */
    POS_ADJECTIVE,
    /* ... */
} POS;
```

That's the entire anchor for this section: when this book says "izina,"
the code says `tok->pos == POS_NOUN`. One Kinyarwanda word, one C
constant. Keep that pairing in mind as the unit this whole chapter keeps
repeating, just for larger and larger pieces of grammar.

## 1.2 The three-part skeleton: D + RT + C

Kinyarwanda nouns are not single, unanalyzable words the way "dog" or
"table" are in English. Every simple noun is visibly assembled from
three smaller pieces:

```
        D        +        RT        +        C
   (Indomo)         (Indanganteko)      (Igicumbi)
  "pre-prefix          "class               "stem"
    vowel"              marker"
   one of: i, u, a    e.g. mu, ba,        carries the
   (sometimes           ki, bi...          actual meaning;
    silent)             determines          NEVER changes
                        agreement
```

Watch it happen on one real word:

```
        u    +    mu    +    ntu      =    umuntu
        D         RT          C              ↓
                                         "person / human being"
```

Quoting this project's own grammar-rule reference directly, compiled
from REB secondary-school textbooks:

> **Formula: D + RT + C**
> | Component | Kinyarwanda term | Description |
> |---|---|---|
> | D | Indomo | The initial vowel (prefix) of the noun. Only three vowels can be indomo: i, u, a |
> | RT | Indanganteko (indangasano) | The class marker; determines agreement with other words in the sentence |
> | C | Igicumbi | The stem; does not change; carries the core meaning |

Three rules about this formula, worth fixing in memory now:

1. **D can be missing.**
2. **RT can be missing.**
3. **C can never be missing** — without it, there's no meaning left.

## 1.3 Same skeleton, now as code: meet `KinMorpheme`

Here is the small, immediate code anchor for Section 1.2 — not an
algorithm yet, just the *shape* the engine uses to record exactly the
three pieces you just learned. Every individual morpheme (D, RT, or C)
is one of these:

```c
typedef struct {
    char label        [12];   /* "D", "RT", or "C" — literally the same
                                  three letters you just learned        */
    char form         [20];   /* the underlying piece, e.g. "mu"        */
    char surface      [20];   /* what it actually looks like on the page */
    char rule         [96];   /* which grammar rule explains it, if any */
    char english_gloss[48];   /* what this one piece means in English   */
} KinMorpheme;
```

And here is `umuntu` — the exact word from Section 1.2 — as three filled
`KinMorpheme` values, side by side with the grammar that produced them:

| You learned (1.2) | The code stores |
|---|---|
| D = `u` | `{label="D",  form="u",   surface="u",   english_gloss=""}` |
| RT = `mu` | `{label="RT", form="mu",  surface="mu",  english_gloss=""}` |
| C = `ntu` (means "person") | `{label="C",  form="ntu", surface="ntu", english_gloss="person"}` |

That's it for now — three small structs, holding exactly the three
pieces you already understand. We are not yet asking *how* the code
figures out where one piece ends and the next begins; that's Section 3's
job, once you've met every ingredient it needs.

## 1.4 Sixteen families: the noun classes (Inteko)

Here is the single most important fact in this chapter: **every
Kinyarwanda noun belongs to one of 16 numbered classes**, called
*inteko*, and the class is what the `RT` syllable encodes. Classes group
nouns that share a real semantic or grammatical thread, and most come in
singular/plural pairs:

```
                         16 NOUN CLASSES
                                |
        ┌──────────┬──────────┼──────────┬──────────┐
        │           │          │          │          │
    HUMANS      PLANTS/     ANIMALS/   SIZE-SHIFTED  STANDALONE
   (paired       THINGS      ROUND      (paired       (no plural
    1 ↔ 2)      (paired      THINGS     12 ↔ 13        partner of
                 3 ↔ 4,      (paired     diminutive)    their own)
                 7 ↔ 8)      9 ↔ 10)                    11, 14, 15, 16
                                         and 5 ↔ 6
```

The full set, with one example each:

| Class | Prefix | Example | Meaning |
|---|---|---|---|
| 1 | umu | *umuntu* | person (sg.) |
| 2 | aba | *abantu* | people (pl. of 1) |
| 3 | umu | *umuti* | tree / medicine (sg.) |
| 4 | imi | *imiti* | trees / medicines (pl. of 3) |
| 5 | i | *iryango* | door (sg.) |
| 6 | ama | *amazi* | water (pl./mass of 5) |
| 7 | iki | *ikigo* | institution (sg.) |
| 8 | ibi | *ibigo* | institutions (pl. of 7) |
| 9 | in | *inka* | cow (sg.) |
| 10 | in | *inka* | cows (pl. — identical spelling to 9!) |
| 11 | uru | *urugo* | homestead (long/round things) |
| 12 | aka | *akana* | small child (dim. sg.) |
| 13 | utu | *utwana* | small children (pl. of 12) |
| 14 | ubu | *uburezi* | education (abstract) |
| 15 | uku | *kugenda* | "to walk" — the infinitive **is** a noun class |
| 16 | aha | *ahantu* | place (locative) |

Two facts to hold onto, because they will matter the moment code enters
the picture: **classes 1 and 3 are spelled identically** (`umu-`) — there
is no way to tell "person" from "tree/medicine" by prefix alone, only by
already knowing the specific stem. **Classes 9 and 10 are also spelled
identically** — singular and plural "cow" are both `inka`; Kinyarwanda
resolves that ambiguity on the *surrounding* words, not the noun itself.

## 1.5 Same 16 classes, now as code: `NOUN_CLASSES[]`

Here is the immediate anchor. Every row in the table you just read is
one row of a single C array, `NOUN_CLASSES[]`, quoted directly from
`lexicon.c`:

```c
static const NounClass NOUN_CLASSES[] = {
    /* num  prefix   rt    adj-RS  poss-conn  subj  description */
    {  1, "umu",  "mu",  "mu",   "wa",  "a",   "Nt.1 – human singular (umuntu)"    },
    {  2, "aba",  "ba",  "ba",   "ba",  "ba",  "Nt.2 – human plural (abantu)"      },
    {  3, "umu",  "mu",  "mu",   "wa",  "u",   "Nt.3 – tree/thing singular (umuti)"},
    {  4, "imi",  "mi",  "mi",   "ya",  "i",   "Nt.4 – tree/thing plural (imiti)"  },
    {  5, "i",    "ri",  "ri",   "rya", "ri",  "Nt.5 – singular (ibuye/iryango)"   },
    {  6, "ama",  "ma",  "ma",   "ya",  "a",   "Nt.6 – plural/mass (amabuye/amazi)"},
    {  7, "iki",  "ki",  "ki",   "cya", "ki",  "Nt.7 – thing singular (ikigo)"     },
    {  8, "ibi",  "bi",  "bi",   "bya", "bi",  "Nt.8 – thing plural (ibigo)"       },
    {  9, "in",   "n",   "n",    "ya",  "i",   "Nt.9 – animal/thing sing. (inka)"  },
    { 10, "in",   "n",   "zi",   "za",  "zi",  "Nt.10 – animal/thing plur. (inka)" },
    { 11, "uru",  "ru",  "ru",   "rwa", "ru",  "Nt.11 – long/thin (urugo)"         },
    { 12, "aka",  "ka",  "ka",   "ka",  "ka",  "Nt.12 – diminutive sing. (akana)"  },
    { 13, "utu",  "tu",  "tu",   "twa", "tu",  "Nt.13 – diminutive plur. (utugabo)"},
    { 14, "ubu",  "bu",  "bu",   "bwa", "bu",  "Nt.14 – abstract (uburezi)"        },
    { 15, "uku",  "ku",  "ku",   "kwa", "ku",  "Nt.15 – infinitive/verbal noun"    },
    { 16, "aha",  "ha",  "ha",   "ha",  "ha",  "Nt.16 – locative (ahantu)"         },
};
```

Match this against Section 1.4's table column by column: `num` is the
class number; `prefix` is the exact D+RT string you just memorized.
Notice three columns you haven't been told the meaning of yet —
`adj-RS`, `poss-conn`, `subj` — they're sitting right there in the same
row, already loaded with data, waiting for Section 1.8 to explain what
they're for. You don't need to understand them yet; just notice they
exist, attached to the same 16 rows you already know.

**Why one fixed array, not 16 separate variables or a hash map?** Class
numbers are small, dense integers from 1 to 16 — exactly the case where
*direct indexing* (`NOUN_CLASSES[class_num - 1]`) gives the same O(1)
lookup a hash map would, with no hashing cost and no bucket-sizing
decision to make, because the entire key space (16 keys) is fixed at
compile time and will never grow at runtime. A hash map earns its
complexity when keys are sparse or unknown ahead of time; neither is true
here. Sixteen separate variables would work too, but then no function
could ever write a generic "for every class, do X" loop — the class
*number itself* has to be usable as an index for that to be possible.
`sizeof(NounClass)` is 48 bytes on this build; all 16 rows together cost
under a kilobyte, built once at compile time, never allocated or parsed
at runtime.

## 1.6 Singular and plural, side by side

```
   Class 1  (umu-, human sg.)  ←──pairs with──→  Class 2  (aba-, human pl.)
   Class 3  (umu-, thing sg.)  ←──pairs with──→  Class 4  (imi-, thing pl.)
   Class 5  (i-,   general sg.) ←──pairs with──→ Class 6  (ama-, general pl.)
   Class 7  (iki-, thing sg.)   ←──pairs with──→ Class 8  (ibi-, thing pl.)
   Class 9  (in-,  animal sg.)  ←──pairs with──→ Class 10 (in-,  animal pl.)
   Class 12 (aka-, small sg.)   ←──pairs with──→ Class 13 (utu-, small pl.)

   Class 11 (uru-), 14 (ubu-), 15 (uku-), 16 (aha-) — usually stand alone
```

## 1.7 Same pairs, now as code: `NOUN_PLURAL_PAIRS[]`

```c
static const NounPluralPair NOUN_PLURAL_PAIRS[] = {
    /* singular        plural             C          sg  pl */
    { "umuntu",     "abantu",          "ntu",        1,  2  },
    { "umugabo",    "abagabo",         "gabo",       1,  2  },
    { "umwana",     "abana",           "ana",        1,  2  },
    /* ... */
    { "inka",       NULL,              "ka",         9,  10 },  /* same spelling! */
};
```

Every row pairs exactly the two class numbers from Section 1.6's
diagram, plus the shared stem (`C`, Section 1.2's third piece) that both
forms have in common. Notice the `inka` row: `plural` is `NULL` — because
the spelling really is identical for singular and plural, exactly as
Section 1.4 warned. The table doesn't pretend otherwise; it records the
`NULL` and lets the singular/plural distinction be carried by the class
number alone (9 vs. 10), to be resolved by whatever *else* in the
sentence agrees with this noun — which is exactly Section 1.8's topic.

## 1.8 Why class matters beyond the noun itself: concordance

A noun's class doesn't just label the noun — it echoes onto every other
word in the sentence that relates to it:

```
   Umuntu      mwiza        aragenda.
   "person"    "good"       "is-walking"
      ↓            ↓             ↓
   class 1     mu- prefix    a- prefix
               (echoes        (echoes
                class 1)       class 1)
```

*Umuntu mwiza aragenda* — "the good person is walking." Swap in a
class-7 noun instead (*ikintu*, "thing") and both echoes change shape:
*Ikintu cyiza kiragenda* — "the good thing is moving." Nothing about
*cyiza* or *kiragenda* was chosen freely — both are mechanically
determined by the noun's class. This is **concordance** (*indangasano*).

## 1.9 Same echo, now as code: the three columns you skipped over

Go back to Section 1.5's table. Those three unexplained columns are
exactly this mechanism, stored per class:

| Column | What it produces | Class 1's value | Class 7's value |
|---|---|---|---|
| `concordance_adj` | the adjective prefix that must echo this class | `mu` (→ *mwiza*) | `ki` (→ *cyiza*) |
| `concordance_poss` | the possessive connector that must echo this class | `wa` | `cya` |
| `subj_prefix` | the verb subject prefix that must echo this class | `a` (→ *aragenda*) | `ki` (→ *kiragenda*) |

This is the entire mechanism behind Section 1.8's diagram, already
sitting in the table you met two sections ago. A future chapter on
adjectives will show the code that *compares* an adjective's actual
prefix against this column to catch a real grammar error — for this
chapter, just notice that the data needed to do that already exists,
attached to the same `NounClass` row, the moment a noun's class is known.

## 1.10 The messy reality: speech is faster than spelling

Real spoken and written Kinyarwanda regularly bends the tidy formula
above:

- **The D vowel often disappears in casual speech.** *Umuntu* → *muntu*;
  *abahinzi* ("farmers") → *bahinzi*.
- **Proper names drop their indomo entirely, as a rule.** *Umugabo* ("a
  man") vs. *Mugabo* (a name).
- **Classes 9/10 undergo real sound changes** at the class-marker/stem
  boundary, depending on the stem's first consonant.

## 1.11 Same messiness, already known to the code

This isn't a gap the code has to work around later — it's already
written directly into the comments of the function you'll read in full
in Section 3. Two real lines, quoted now just to prove the point:

```c
/* MUST come AFTER "umu" check to avoid override of full forms. */
if (kin_starts_with(w, "mu") && wlen > 4 && !is_vowel(w[2])) return 1;

/* Pattern: agent/plural nouns (abahinzi, abarimu, abapfumu) appear as
 * "bahinzi", "barimu", "bapfumu" with 'a' dropped.
 * Guard: NOT ending in 'a' (would conflict with verbs bakora/bagenda) */
```

The first comment is talking about exactly Section 1.10's first bullet —
the dropped D vowel. The second is talking about exactly the same
phenomenon, plus a real complication: a D-vowel-dropped noun can
accidentally look identical to a conjugated verb that happens to start
with the same two letters. You don't need to understand the *whole*
function yet to read these two lines — you already have everything
needed to understand exactly what problem they're solving, because you
just read it in plain English three paragraphs ago.

## 1.12 Checkpoint: test yourself before continuing

Classify these by ear/eye alone:

1. `ikiganza` — "hand"
2. `abakozi` — "workers"
3. `uburanga` — "beauty"
4. `bagabo` — a casual-speech form — what's the full form, and the class?
5. `inzu` — "house"

<details><summary>Answers</summary>

1. `i-ki-ganza` → class 7
2. `a-ba-kozi` → class 2
3. `u-bu-ranga` → class 14
4. Full form `abagabo` ("men") — class 2, D vowel dropped (Section 1.10)
5. `i-n-zu` → class 9 (or 10 — same spelling, Section 1.4)

</details>

If most of these felt natural, you're ready for the next part: instead
of meeting concepts one at a time, you're going to watch the *whole*
detection function read a real word end to end — and every single piece
of it should now be a piece you've already met.

---

## 2. From language rule to engineering contract

State the actual software problem in plain terms, now that every
ingredient is familiar: **given a string of bytes that might be a
Kinyarwanda noun, produce three answers** — is it a noun at all; which
of the 16 classes; and what is its bare stem.

```
   INPUT:   "abahinzi"
                  │
                  ▼
   ┌─────────────────────────────┐
   │  kin_detect_noun_class()      │
   │  kin_strip_noun_prefix()      │
   └─────────────────────────────┘
                  │
                  ▼
   OUTPUT:  is_noun = true,  class = 2,  stem = "hinzi"
```

**Why not just one giant dictionary, word → class?** Because the noun
stem set is open-ended — new nouns are coined and borrowed constantly; no
table, however large, lists every word real text might contain. **Why
not throw the table away and derive everything from the formula at
runtime?** Because that can't know, from spelling alone, that *umuntu*
is class 1 and not class 3 (Section 1.4) — some words need to be known,
not derived. The actual design layers both: a small, fast, exact table
for known/irregular words (you'll meet it in a later chapter), and the
general rule-based fallback this chapter is about, for everything else.

The exact contract, stated before any large code appears:

```c
int  kin_detect_noun_class(const char *w);
     /* returns 1-16 if recognized, 0 if not */

bool kin_strip_noun_prefix(const char *word, char *stem_out, int *class_out);
     /* returns true/false; writes the stem and class THROUGH pointers */
```

Notice the second function needs to hand back three pieces of
information (success, stem, class), but C only allows one `return`
value. Hold onto that puzzle — Section 3.5 resolves it.

---

## 3. The full functions, read end to end

Every piece below has already been introduced individually. This section
is the assembly, not the introduction.

## 3.1 `kin_detect_noun_class`, in full

```c
int kin_detect_noun_class(const char *w) {
    size_t wlen = strlen(w);
    if (wlen < 3) return 0;

    /* Ordered by prefix length (longest first) to avoid partial matches */
    if (kin_starts_with(w, "umu") && wlen > 4) return 1;  /* Nt.1/3      */
    if (kin_starts_with(w, "aba") && wlen > 4) return 2;  /* Nt.2        */
    if (kin_starts_with(w, "imi") && wlen > 4) return 4;  /* Nt.4        */
    if (kin_starts_with(w, "ama") && wlen > 4) return 6;  /* Nt.6        */
    /* ... every full-form prefix from Section 1.5's table, one per class ... */

    /* Dropped D-vowel variants — Section 1.10/1.11 — come AFTER every
     * full-prefix check, so a full form is never claimed by a shorter,
     * vaguer pattern that happens to also match its beginning. */
    if (kin_starts_with(w, "mu") && wlen > 4 && !is_vowel(w[2])) return 1;
    if (kin_starts_with(w, "ba") && wlen > 5 && !is_vowel(w[2])
        && w[wlen-1] != 'a' && w[wlen-1] != 'e') return 2;
    /* ... */
    return 0;
}
```

Read it against what you already know: the first block is Section 1.5's
table, checked one row at a time. The second block is Section 1.10's
messiness, handled exactly the way Section 1.11 previewed. The order
between the two blocks isn't arbitrary — it's the entire disambiguation
strategy, explained next.

### 3.2 Why this exact algorithm, and not a "cooler" one

Three real alternatives, and why each loses to a flat, ordered `if`-chain
for *this specific problem*:

**Why not a hash map from prefix to class?** Because the real question
isn't "is this exact prefix known" — it's "which is the *longest* valid
prefix this word starts with." A hash map answers exact lookups; it has
no built-in notion of preferring a longer match over a shorter one that
also fits. You would still need to check candidates longest-first
yourself, so a hash map adds overhead without removing the ordering
logic you actually need.

**Why not a trie?** A trie is the right answer once you have hundreds or
thousands of prefixes. With roughly 30 known prefixes, fixed at compile
time, a trie buys asymptotic elegance you'll never measure, at the cost
of node structures and traversal logic that don't exist anywhere else in
this zero-allocation project — and it would scatter the one-to-one
correspondence between each `if` line and one specific RALC rule across
tree nodes, making the code *harder* to audit against the textbook it
implements, not easier.

**Why not a regular expression?** C has no built-in regex engine; using
one means a third-party dependency, breaking this project's
zero-dependency design. And most regex engines don't guarantee
longest-match-first by default — you'd still need the same ordering
discipline, just spelled in a denser, harder-to-audit syntax.

**The honest conclusion**: for a small, fixed, compile-time-known
candidate set that has to stay independently verifiable against an
external rulebook, the flat `if` chain isn't a missed opportunity for a
"real" algorithm — it *is* the right algorithm for this problem's actual
shape and scale.

### 3.3 The low-level C mechanics inside each line

**`const char *w`** — read-only input; nothing here ever writes through
it, so the compiler can catch you if you accidentally tried to.

**`strlen(w)` computed once**, into `wlen`, reused by every guard below
instead of being recomputed on every single check.

**`kin_starts_with(s, prefix)`**:

```c
bool kin_starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
```

`strncmp` compares at most `strlen(prefix)` bytes — exactly enough to
test the prefix, never reading further. The wrapper exists purely so the
big function reads as "starts with," not "some strncmp call I have to
mentally translate every time."

**The `wlen > 4` guards.** They exist because a 3-letter prefix matched
against a 3-letter total string would leave zero bytes for the stem —
impossible, since Section 1.2's third rule says the stem can never be
empty. Each number is the shortest real stem that specific prefix is
ever actually paired with — not a round, arbitrary cutoff.

**`&&`, left to right, stopping at the first false.** In
`kin_starts_with(w,"ba") && wlen > 5 && !is_vowel(w[2])`, `w[2]` is only
ever read after both earlier checks have already guaranteed the string
is long enough for that index to exist. Reorder these and you risk
reading past the end of a short string.

**`is_vowel(c)`**, the simplest function in the file:

```c
static bool is_vowel(char c) {
    return c=='a'||c=='e'||c=='i'||c=='o'||c=='u';
}
```

One `char`, passed by value — there's nothing to point *at* for a single
character — compared against five literals.

**`return 0;`.** No real class is ever numbered 0, so it's a value that
can never be mistaken for a genuine answer. Every caller checks for it
before trusting the result.

### 3.4 The one recursive case: `"nka"` + noun

```c
if (kin_starts_with(w, "nka") && wlen > 6) {
    const char *sub = w + 3;
    int sub_cls = kin_detect_noun_class(sub);   /* calling itself */
    if (sub_cls > 0) return sub_cls;
}
```

*Nka* ("like/as") sometimes fuses directly onto the following noun in
real text — *nkabantu* = *nka* + *abantu*. Rather than duplicating the
entire detection chain, the function strips the three bytes of `nka`
(`sub = w + 3`, a pointer moved forward, no copying) and asks itself the
same question about what's left. This is safe because of two facts:
**a base case that doesn't recurse** (`wlen < 3` at the very top), and
**guaranteed shrinkage** — every recursive call is on a string exactly
three bytes shorter, so it must eventually reach the base case. There is
no input that loops forever.

## 3.5 `kin_strip_noun_prefix`: solving the multiple-return-value puzzle

Section 2's contract needed three answers from one function. Here is
the resolution:

```c
bool kin_strip_noun_prefix(const char *word, char *stem_out, int *class_out);
```

```
   CALLER OWNS THE MEMORY                  FUNCTION WRITES INTO IT
   ┌─────────────────┐                    ┌──────────────────────┐
   │ char stem[96];   │ ── address ──────▶│ writes the stem       │
   │ int  cls;        │ ── address ──────▶│ writes the class num  │
   └─────────────────┘                    └──────────────────────┘
           ▲                                        │
           └──────────── bool success ◀──────────────┘
```

The caller hands the function the *addresses* of its own variables; the
function writes answers directly into that caller-owned memory, and
reserves its one real `return` for a plain success/failure flag. This is
C's standard substitute for the multiple-return-value syntax other
languages have natively.

### 3.6 Two-level dispatch: class first, then exact spelling

```c
switch (cls) {
    case 1: case 3:
        if (kin_starts_with(word, "umw"))      stem_start = word + 3;
        else if (kin_starts_with(word, "umu")) stem_start = word + 3;
        else if (kin_starts_with(word, "mw"))  stem_start = word + 2;
        else if (kin_starts_with(word, "mu"))  stem_start = word + 2;
        break;
    case 2:
        if (kin_starts_with(word,"aba"))       stem_start = word + 3;
        else if (kin_starts_with(word,"ba"))   stem_start = word + 2;
        break;
    /* ... */
}
```

The **outer** `switch (cls)` already knows which of the 16 classes this
is (Section 1.4) — choosing among 16 fixed alternatives is exactly what
`switch` is for. The **inner** `if`/`else if` re-derives *which exact
spelling* matched, because — Section 1.10, again — the same class can
surface as the full form, the dropped-vowel form, or a glide-contracted
form, and each needs a different number of bytes skipped to reach the
stem.

### 3.7 A memory-safety detail worth never skipping

```c
strncpy(stem_out, stem_start, KIN_MAX_STEM - 1);
stem_out[KIN_MAX_STEM - 1] = '\0';
```

`strncpy` does **not** add a terminating `'\0'` if the source is at least
as long as the given limit — it just stops. Skip the second line, and a
95-character stem would leave `stem_out` non-terminated; every later
`strlen`/`printf("%s")` on it would read past the buffer's end into
whatever bytes happen to follow. The fix is always the same: write the
terminator yourself, explicitly, every time.

### 3.8 Sentinel returns, completing the contract

```c
int cls = kin_detect_noun_class(word);
if (cls == 0) { stem_out[0] = '\0'; return false; }
```

`stem_out[0] = '\0'` keeps the output buffer a valid, empty string even
on failure. `return false` tells the caller plainly: don't trust the
out-parameters. Every caller checks this before reading either one.

## 3.9 Full trace: `abahinzi`, start to finish

```
  INPUT:  "abahinzi"   (wlen=8)

  kin_detect_noun_class("abahinzi")
    kin_starts_with(w,"aba") → TRUE, wlen>4 → TRUE  →  returns 2

  kin_strip_noun_prefix("abahinzi", stem_out, &cls)
    cls = 2
    switch(2): kin_starts_with(word,"aba") → TRUE → stem_start = word+3
    strncpy(stem_out, stem_start, 95); stem_out[95]='\0';
    → stem_out = "hinzi"

  FINAL ANSWER:  is_noun=true, class=2 (human plural), stem="hinzi" (farmer)
```

Every fact in that final answer traces back to a specific section you
already read: the `"aba"` prefix existing at all (1.2), it meaning class
2 (1.4/1.5), and the stem being three bytes in (3.6).

## 3.10 What this design honestly does not solve

- **Class 1 vs. 3 cannot be told apart by prefix alone** (Section 1.4) —
  disambiguating "tree" from "person" needs a separate exact-stem lookup,
  not this function.
- **A genuinely novel or very informal spelling can return 0** even
  though a human would recognize it from context — the accepted cost of
  not trying to pattern-match every conceivable input.
- **The dropped-D-vowel guards are heuristics, not guarantees** — the
  "doesn't end in a/e" check exists specifically to dodge verb collisions
  (Section 1.11), and is imperfect by construction.

---

## Try it yourself

1. **Trace `"ubwenge"` ("wisdom") by hand** through both functions before
   running anything. Check your class and stem against Section 1.5.
2. **Compile and confirm:**

```c
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    const char *words[] = { "abahinzi", "ubwenge", "umuti", "inka", NULL };
    for (int i = 0; words[i]; i++) {
        char stem[KIN_MAX_STEM];
        int  cls = 0;
        bool ok  = kin_strip_noun_prefix(words[i], stem, &cls);
        printf("%-12s ok=%d class=%2d stem=%s\n", words[i], ok, cls, stem);
    }
    return 0;
}
```

3. **Pick a noun this chapter never used**, write its D+RT+C breakdown
   on paper first, then check whether the function already handles it —
   and if not, work out exactly where in the ordering a new `if` line
   would need to go.

## Key takeaways

- Every Kinyarwanda noun is D (initial vowel) + RT (class marker) + C
  (stem); the stem can never be missing.
- 16 noun classes, mostly paired singular/plural; classes 1/3 and 9/10
  are genuinely, not accidentally, ambiguous by spelling alone.
- A noun's class echoes onto its agreeing adjective and verb
  (concordance) — already stored, per class, the moment the class is
  known.
- Real speech bends the formula in predictable ways; the code's own
  comments document exactly the same messiness this chapter taught.
- The detection algorithm is a small, fixed, ordered `if`-chain — not
  because nothing fancier exists, but because nothing fancier's
  advantages apply at this scale, and the flat chain stays auditable
  against the exact rulebook it implements.
- C has no multiple-return-value syntax; pointers the caller owns are
  the substitute.
- `strncpy` needs an explicit terminator after it, always.
- This project's one recursive function is provably safe: a real base
  case, and every call strictly shrinks toward it.

## Sources quoted in this chapter

- Kinyarwanda S2–S6 secondary-school textbooks (REB / Drakkar Ltd,
  2017–2024), via `data/textbook_grammar_rules.md`.
- RALC 2017 official orthography (*Amabwiriza ya Minisitiri no
  001/2014*).
- `include/kinyarwanda.h`, `src/lexicon.c` (`NounClass`,
  `NOUN_CLASSES[]`, `NounPluralPair`, `NOUN_PLURAL_PAIRS[]`).
- `src/morphology.c` (`kin_detect_noun_class`, `kin_strip_noun_prefix`,
  `kin_starts_with`, `is_vowel`).

## Coming up in Chapter 2

Section 1.9 named the three concordance columns but stopped at "a future
chapter will show the code that compares them." Chapter 2 is that
chapter: the Kinyarwanda adjective (*ntera*), its own RS + C formula
introduced and anchored the same way this chapter introduced D+RT+C, and
the C code that both checks agreement against the columns you've already
met, and repairs it when it's wrong.
