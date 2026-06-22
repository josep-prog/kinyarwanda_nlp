# Chapter 6 — Before Any Tree: How Text Becomes Tokens

## How this chapter works

Same rules as Chapters 1 through 5. Every code example was actually
compiled with `gcc -std=c99 -Wall -Wextra` and actually executed — the
output shown in a fenced block prefixed with `$` is the real terminal
output of that exact program.

Every chapter so far opened a program with a line like
`SentenceAnalysis sa = kin_analyze(text);` and then immediately started
reading `sa.tokens[i].surface`, `.pos`, `.noun_class` — as if a sentence
arrived already cut into neat, labeled pieces. It doesn't. Before any
of the last five chapters' trees can run at all, something has to look
at a raw byte string like `"N'abana barashaka kurya."` and decide where
one word ends and the next begins, what counts as punctuation, and
what a stray apostrophe in the middle of a word is even doing there.
That something is `tokenizer.c`, and this chapter reads it.

---

# Part 1 — Language and Code, Side by Side

## 1.1 The job nobody mentioned yet

This project's own tokenizer states its job in three bullet points,
right at the top of the file:

> Kinyarwanda orthographic notes (from book p.4-12):
>  - Apostrophe contracts: n'uku, k'ibuye, b'abana (the elided form of
>    the final vowel of the preceding word). We split on apostrophe,
>    yielding two tokens (the clitic attaches to the next word).
>  - Hyphen in reduplicated adjective stems: to-to, re-re, sa-sa.
>    We keep these as single tokens.
>  - Punctuation (.,?!;:) ends a token but is discarded... [no — emitted
>    as its own token, Section 6.4 corrects this stale comment against
>    the real code]

Three sentences, and already a real tension worth sitting with: the
*comment* says punctuation is discarded, but Section 6.4 shows the
real code keeping every punctuation mark as its own token, with its
own POS and its own clause/sentence-boundary flags. The comment is
stale; the code moved on without it. This is worth noticing on page
one of this chapter, because "trust the code over the comment" is
about to become this chapter's first real finding, not just a stray
observation.

## 1.2 The rule that motivates the whole file: vowel elision

The grammar reference's own dedicated section on this:

> **PART 12: IKATA N'ITAKARA RY'INYAJWI (Elision Rules)**
> **12.1 Ikata ry'inyajwi (Vowel elision):** Final vowel of the
> conjunction is elided when followed by a word beginning with a
> vowel:
> ```
> nk'umurwayi (nka + umurwayi -> nk')
> w'ibihumbi  (wa  + ibihumbi -> w')
> ```
> **12.2 Inyajwi zisoza amagambo ntizikatwa:** Regular word-final
> vowels are never elided. Ordinary words keep their final vowel no
> matter what follows.

Two real Kinyarwanda words, `nka` ("like/as," Chapter 5's preposition)
and `wa` (a class connector, Chapter 4's possessive table), each lose
their own final vowel — and only their own final vowel — when the
next word starts with one. `umurwayi` ("the patient") keeps every one
of its own vowels; only `nka`'s final `-a` disappears, replaced by an
apostrophe marking where it used to be. Section 12.2 is the contrast
that makes this a *rule* rather than a vague tendency: ordinary words
never do this, so when an apostrophe shows up mid-text, it is always
marking a real, specific deletion on a small, identifiable set of
words — not random orthographic decoration.

## 1.3 Build it: a toy tokenizer that only knows about spaces

```c
/* p1_toy_tok.c -- a first, tiny tokenizer: split on whitespace only */
#include <stdio.h>
#include <string.h>

int toy_tokenize(const char *text, char tokens[][32], int max_tokens) {
    int count = 0;
    const char *p = text;
    while (*p && count < max_tokens) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != ' ') p++;
        size_t len = (size_t)(p - start);
        if (len >= 32) len = 31;
        memcpy(tokens[count], start, len);
        tokens[count][len] = '\0';
        count++;
    }
    return count;
}

int main(void) {
    char tokens[16][32];
    int n = toy_tokenize("N'abana barashaka kurya.", tokens, 16);
    printf("Token count: %d\n", n);
    for (int i = 0; i < n; i++) printf("  [%d] \"%s\"\n", i, tokens[i]);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p1_toy_tok p1_toy_tok.c
$ ./p1_toy_tok
Token count: 3
  [0] "N'abana"
  [1] "barashaka"
  [2] "kurya."
```

Two real problems are sitting in this output, both invisible if you
only ever look at well-formed single words the way Chapters 1 through
5 always did. `"N'abana"` stayed glued together — Section 1.2's
elided `na` (here capitalized, sentence-initial) and the noun it
attaches to are one token now, even though they're two completely
different grammatical things (a conjunction and a noun). And
`"kurya."` kept its trailing period, meaning every downstream lookup
this book has built — `kin_is_invariable`, `kin_is_pronoun`, the noun
and verb detectors — would have to strip punctuation themselves,
*every single call*, instead of once, here, before any of them ever
run. Splitting on whitespace alone solves none of the problems that
motivated this chapter.

## 1.4 Checkpoint: test yourself before continuing

1. Why does `w'ibihumbi` need to become two tokens (`w` and `ibihumbi`)
   rather than one (`w'ibihumbi`) for any of Chapters 1–5's detectors
   to work on it correctly?
2. Section 1.3's toy tokenizer would split `"kurya."` into one token.
   Name two different pieces of information a later chapter's analysis
   needs that get lost if the period stays glued to `kurya`.
3. Section 1.1 found the file's own header comment is stale. What does
   that suggest about how much you should trust a comment versus
   actually running the code it describes — and is that a new lesson,
   or one this book has taught before?

---

# Part 2 — A Different Shape of Output, A Different Shape of Risk

## 2.1 The out-parameter is now an entire array

```c
int kin_tokenize(const char *text, Token *out, int max_tokens);
```

Every out-parameter in Chapters 1 through 5 filled in a handful of
scalar fields — a stem, a class, a tense. This one fills in an entire
caller-owned array, one `Token` per call to the internal emit logic,
and returns *how many* of those slots it actually used. This is the
same defensive shape Chapter 1, Section 2.1 first introduced (the
caller owns the memory, the function only ever writes into space the
caller already allocated) — just scaled up from "fill in one struct"
to "fill in as many structs as the input actually contains, but never
more than the caller said was safe."

## 2.2 What happens when there's more text than room

```c
/* p2_overflow.c -- ask for 4 tokens' worth of room, feed it more */
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    const char *text = "Umugabo agiye mu nzu kurya ibiryo byiza cyane none.";
    Token toks[4];
    int n = kin_tokenize(text, toks, 4);
    printf("requested room for 4 tokens; real sentence has far more\n");
    printf("returned count=%d\n", n);
    for (int i = 0; i < n; i++) printf("  [%d] \"%s\"\n", i, toks[i].surface);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p2_overflow.c -L . -lkinyarwanda -o p2_overflow
$ LD_LIBRARY_PATH=. ./p2_overflow
requested room for 4 tokens; real sentence has far more
returned count=4
$ echo $?
0
```

No overflow, no crash, no garbage: `kin_tokenize` simply stops at
exactly 4 tokens and returns, silently discarding the rest of the
sentence. Reading the real source explains why this works cleanly
every time, not just this time: the main loop's own condition is
`while (*p && count < max_tokens)`, and every single place inside that
loop that writes a token first re-confirms there's room. This is the
first chapter in the book where the out-of-bounds risk every previous
chapter eventually found a way to demonstrate (Chapters 1, 2, 4) is
absent not because nobody tried hard enough to break it, but because
the *loop condition itself* makes the unsafe state unreachable — the
guard isn't a single `if` bolted on as an afterthought, it's the thing
that decides whether the loop body runs at all. Chapter 4, Section 2.2
showed what happens when exactly this kind of guard is *missing*,one
layer down, on a single index. Here it's present, one layer up, on
every iteration of the whole function — and the difference in outcome
is the entire lesson repeated from the opposite direction: the same
bug class is catastrophic when the check is missing and a non-event
when it isn't.

---

# Part 3 — Making It Interactive

## 3.1 Build it: watch a sentence become tokens

```c
/* p3_repl.c -- type a sentence, see exactly how it gets tokenized */
#include "kinyarwanda.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    char line[256];
    printf("Type a Kinyarwanda sentence, or 'q' to quit.\n");
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == 'q' && line[1] == '\0') break;
        Token toks[64];
        int n = kin_tokenize(line, toks, 64);
        printf("  %d tokens:\n", n);
        for (int i = 0; i < n; i++)
            printf("    [%d] \"%s\"%s\n", i, toks[i].surface,
                   toks[i].pos == POS_PUNCTUATION ? "  (punctuation)" : "");
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p3_repl.c -L . -lkinyarwanda -o p3_repl
$ printf "N'abana barashaka kurya.\nq\n" | LD_LIBRARY_PATH=. ./p3_repl
Type a Kinyarwanda sentence, or 'q' to quit.
  5 tokens:
    [0] "N"
    [1] "abana"
    [2] "barashaka"
    [3] "kurya"
    [4] "."  (punctuation)
```

Compare this directly against Section 1.3's toy output for the
identical input: five tokens here against three there, with `N` and
`abana` correctly separated and the period correctly pulled off into
its own token rather than staying glued to `kurya`.

---

# Part 4 — Capstone: A Lookup Table That Doesn't Cover Its Own Tokenizer

## 4.1 The tokenizer's splitting policy creates a second table's job

Splitting `"N'abana"` into `"N"` and `"abana"` only helps if something
downstream recognizes `"n"` (lowercased) as the elided conjunction
`na` it actually is, rather than a meaningless single letter. Reading
`src/lexicon.c` finds exactly that second table — a set of entries
built specifically to catch what this tokenizer's apostrophe-splitting
produces:

```c
{ "n",       POS_CONJUNCTION  }, /* elided 'na' before vowel           */
{ "y",       POS_CONJUNCTION  }, /* elided 'ya' possessive connector   */
{ "k",       POS_LOCATIVE     }, /* elided 'ku' before vowel           */
{ "b",       POS_CONJUNCTION  }, /* elided 'ba' before vowel           */
{ "w",       POS_CONJUNCTION  }, /* elided 'wa' before vowel           */
{ "r",       POS_CONJUNCTION  }, /* elided 'rya' before vowel          */
{ "c",       POS_CONJUNCTION  }, /* elided 'cya' before vowel          */
{ "cy",      POS_CONJUNCTION  }, /* elided 'cya' Nt.7 possessive       */
{ "ry",      POS_CONJUNCTION  }, /* elided 'rya' Nt.5 possessive       */
{ "bw",      POS_CONJUNCTION  }, /* elided 'bwa' Nt.14 possessive      */
{ "rw",      POS_CONJUNCTION  }, /* elided 'rwa' Nt.11 possessive      */
{ "by",      POS_CONJUNCTION  }, /* elided 'bya' Nt.8 possessive       */
{ "tw",      POS_CONJUNCTION  }, /* elided 'twa' Nt.13 possessive      */
{ "kw",      POS_LOCATIVE     }, /* elided 'kwa' before vowel          */
```

Look back at Chapter 4's sixteen possessive connectors (`wa, ba, wa,
ya, rya, ya, cya, bya, ya, za, rwa, ka, twa, bwa, kwa, ha`, classes
1–16): `wa`, `ya`, `rya`, `cya`, `bya`, `za`, `rwa`, `twa`, `bwa`, and
`kwa` each have a matching elided-fragment entry above. Two classes
from that same sixteen-entry table do not: class 12's `ka`, and class
16's `ha`. Both are real, and both can genuinely lose their final
vowel before a vowel-initial noun by the exact same Section 1.2 rule
that licenses `w'ibihumbi`. Testing the more surprising of the two —
`ha`, the locative class — against the real, compiled library:

```c
/* p4_capstone.c */
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    SentenceAnalysis sa = kin_analyze("Ahantu h'Imana harakomeye.");
    printf("\"Ahantu h'Imana harakomeye.\"\n");
    for (int i = 0; i < sa.token_count; i++)
        printf("  [%d] \"%-10s\" pos=%d is_kinyarwanda=%d\n",
               i, sa.tokens[i].surface, sa.tokens[i].pos,
               sa.tokens[i].is_kinyarwanda);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_capstone.c -L . -lkinyarwanda -o p4_capstone
$ LD_LIBRARY_PATH=. ./p4_capstone
"Ahantu h'Imana harakomeye."
  [0] "Ahantu    " pos=1 is_kinyarwanda=1
  [1] "h         " pos=16 is_kinyarwanda=0
  [2] "Imana     " pos=1 is_kinyarwanda=1
  [3] "harakomeye" pos=6 is_kinyarwanda=1
  [4] ".         " pos=17 is_kinyarwanda=0
```

`pos=16` is `POS_FOREIGN` — the tokenizer correctly split `h'Imana`
into `h` and `Imana` exactly as Section 1.2's rule says it should, but
nothing in `INVARIABLES[]` or any other lookup table claims the
fragment `"h"`, so the pipeline concludes it isn't even Kinyarwanda
(`is_kinyarwanda=0`) and discards it as foreign. This is a real,
previously-undocumented finding: the elided-fragment table that
exists specifically to support this chapter's apostrophe-splitting
policy is not exhaustive over the very class list it's clearly meant
to cover, and the gap is invisible from either file in isolation —
only running the tokenizer's actual output through the lexicon's
actual lookup, on a sentence using the one connector nobody added,
reveals it.

## 4.2 Why this kind of gap is structural, not careless

This is the same *species* of finding as Chapter 5's shadowed
`INVARIABLES[]` duplicates (Section 4.1 there) — a flat table that
grew across many editing passes, missing exhaustive coverage of a set
that's defined somewhere *else* in the project (Chapter 4's sixteen
noun classes) rather than self-evidently complete on its own terms.
Nothing about reading `lexicon.c` by itself reveals that `ha` is
missing; nothing about reading `tokenizer.c` by itself reveals it
either. The gap only exists in the *relationship* between the two
files — the tokenizer's splitting policy implicitly promises that
every connector's elided form will be recognized downstream, and nine
of the table's own fourteen elided-fragment entries keep that promise
while two real noun classes' worth silently don't. A maintainer
auditing this project for completeness would need to check the
tokenizer's policy against the lexicon's coverage *together*, the way
this section just did, not read either file and trust it in
isolation.

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 Layer 0

Every engineering-contract table in this book so far compared the
five trees against each other. This chapter sits underneath all five,
not beside them:

```
   raw text:  "N'abana barashaka kurya."
       |
       v
   ┌─────────────────────────────────────┐
   │  LAYER 0 — tokenizer.c               │
   │  split into words + punctuation,     │
   │  apply elision splitting,            │
   │  flag sentence/clause boundaries,    │
   │  flag proper-noun capitals           │
   └─────────────────────────────────────┘
       |
       v
   Token[] array, ready for:
       |
   ┌───────┬───────┬───────┬────────┬────────────┐
   │ Tree 1│ Tree 2│ Tree 3│ Tree 4 │ Tree 5      │
   │ nouns │ adjs  │ verbs │ prons  │ invariables │
   │ (Ch.1)│ (Ch.2)│ (Ch.3)│ (Ch.4) │ (Ch.5)      │
   └───────┴───────┴───────┴────────┴─────────────┘
```

Nothing in Chapters 1 through 5's trees ever decides where one word
ends and the next begins, or whether a given byte sequence is a word
at all rather than a comma. Every one of them receives a `Token`
already cut to size, with `.surface` already isolated from its
neighbors and its punctuation. That precondition is this chapter's
entire output, and every chapter before this one was quietly relying
on it being correct without ever being asked to verify it.

## 5.2 Why this layer can't be a lookup table or a rule engine the way the others are

Every other tree in this book chose somewhere on the open-set/closed-
set, rule-derived/flat-lookup spectrum (Chapter 5, Section 5.1's
table). Tokenization can't sit anywhere on that spectrum, because it
isn't classifying *known words* at all — it's deciding, from raw
bytes with no lexicon involved yet, where the word boundaries even
are. A flat lookup table has nothing to look up before tokenization
has run; a rule keyed to noun classes or verb tenses has no classes or
tenses to key on yet. The only tools available at this layer are the
ones Section 1.2 actually used: literal byte/character patterns
(whitespace, specific punctuation marks, the apostrophe, the hyphen)
checked directly against the raw input, because at this point in the
pipeline that is the only information that exists yet to check
against.

---

# Part 6 — Reading the Real Production Code

## 6.1 Two real C details this project's `Token` boundary forces you to handle: multi-byte characters

Every Kinyarwanda *letter* used in this book so far has fit in a
single ASCII byte — the orthography has no accented vowels, and every
consonant cluster Chapter 5's grammar reference quoted (`mb`, `nshy`,
`mpw`...) is built from plain ASCII letters. The *only* place this
project ever has to reason about raw multi-byte UTF-8 sequences is
punctuation: curly quotation marks, guillemets, en/em dashes, and a
couple of invisible whitespace characters that don't exist in ASCII
at all. The real code handles this by comparing raw bytes directly,
with the relevant Unicode code point spelled out in a comment instead
of asked to be taken on faith:

```c
/* UTF-8 U+2019 RIGHT SINGLE QUOTATION MARK: E2 80 99 */
if ((unsigned char)q[0] == 0xE2 &&
    q + 2 < word_end &&
    (unsigned char)q[1] == 0x80 &&
    (unsigned char)q[2] == 0x99) {
    *apos_len = 3;
    return q;
}
```

`(unsigned char)` is doing real, necessary work in this line, not
decoration: on a platform where `char` is signed (most are), a raw
byte like `0xE2` stored in a plain `char` would compare as a negative
number, and `q[0] == 0xE2` would silently and permanently fail no
matter what byte was actually there. Casting to `unsigned char` first
makes the byte's numeric value match what the comment says it is.
This is the same defensive habit Chapter 1's `isupper((unsigned
char)...)` calls (quoted again in Section 6.2 below) were already
modeling — just newly load-bearing here, because this file is the
first one in the book that has to compare against byte values above
0x7F at all.

## 6.2 The "no letter L" rule, and why it's the tokenizer's job to know it

```c
/* Official orthography (Amabwiriza ya Minisitiri no 001/2014 §3a-b)
 * mandates that the FIRST LETTER OF EVERY SENTENCE is always a capital
 * regardless of word class...
 *
 * Single exception: the letter 'l' is explicitly prohibited from
 * native Kinyarwanda words by the official orthography (§2.3). The
 * rules allow 'l' only in the place name "Kigali", "Repubulika",
 * "Leta", and foreign proper names (Abeli, Filipo, Bibiliya, ...).
 * A sentence-initial word containing 'l' is therefore a proper name
 * or foreign loanword regardless of position... */
if (isupper((unsigned char)out[count].surface[0])) {
    bool is_sentence_initial = (count == 0)
                            || out[count-1].is_sent_boundary
                            || out[count-1].is_quote_open;
    if (!is_sentence_initial) {
        out[count].is_proper_noun = true;
    } else if (strchr(out[count].lower, 'l') != NULL) {
        out[count].is_proper_noun = true;
    }
}
```

A real, government-decreed orthographic fact most learners never
encounter explicitly: standard written Kinyarwanda has no native word
containing the letter `l` at all — it's reserved for proper names and
loanwords. That single fact resolves an otherwise-genuine ambiguity:
a capitalized word at the very start of a sentence is normally
*impossible* to tell apart from an ordinary word that just happens to
be sentence-initial (every sentence's first word is capitalized
regardless of its grammar). But if that first word contains an `l`,
the ambiguity disappears — no ordinary Kinyarwanda noun or verb could
ever have produced it, so it must be a name. Verified against the
real library on both sides of the distinction:

```c
/* p_propernoun.c */
#include "kinyarwanda.h"
#include <stdio.h>

static void run(const char *t) {
    SentenceAnalysis sa = kin_analyze(t);
    printf("\"%s\"\n", t);
    for (int i = 0; i < sa.token_count; i++)
        printf("  [%d] \"%-10s\" is_proper_noun=%d noun_class=%d\n",
               i, sa.tokens[i].surface, sa.tokens[i].is_proper_noun,
               sa.tokens[i].noun_class);
    printf("\n");
}

int main(void) {
    run("Umugabo agiye mu nzu.");   /* sentence-initial, no 'l' */
    run("Kigali ni umurwa mukuru."); /* sentence-initial, has 'l' */
    run("Umugabo yabonye Petero."); /* mid-sentence capital      */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p_propernoun.c -L . -lkinyarwanda -o p_propernoun
$ LD_LIBRARY_PATH=. ./p_propernoun
"Umugabo agiye mu nzu."
  [0] "Umugabo   " is_proper_noun=0 noun_class=1
  [1] "agiye     " is_proper_noun=0 noun_class=1
  [2] "mu        " is_proper_noun=0 noun_class=0
  [3] "nzu       " is_proper_noun=0 noun_class=9
  [4] ".         " is_proper_noun=0 noun_class=0

"Kigali ni umurwa mukuru."
  [0] "Kigali    " is_proper_noun=1 noun_class=0
  [1] "ni        " is_proper_noun=0 noun_class=0
  [2] "umurwa    " is_proper_noun=0 noun_class=1
  [3] "mukuru    " is_proper_noun=0 noun_class=1
  [4] ".         " is_proper_noun=0 noun_class=0

"Umugabo yabonye Petero."
  [0] "Umugabo   " is_proper_noun=0 noun_class=1
  [1] "yabonye   " is_proper_noun=0 noun_class=1
  [2] "Petero    " is_proper_noun=1 noun_class=1
  [3] ".         " is_proper_noun=0 noun_class=0
```

`Umugabo` (sentence-initial, no `l`) is correctly processed as an
ordinary class-1 noun, exactly the way Chapter 1's detector expects.
`Kigali` (sentence-initial, contains `l`) is correctly recognized as a
proper noun on the strength of that single letter alone, with no
class assigned at all — and `Petero`, capitalized mid-sentence where
the ambiguity never existed in the first place, is flagged a proper
noun immediately regardless of any letter it contains. Three real
sentences, three different reasons for the same flag, each one
traceable to a specific, citable line of official orthography.

## 6.3 Repairing run-on text: the `-fite` compound split

```c
static const char * const FITE_FORMS[] = {
    "bafite", "bifite", "zifite", "rufite", "gafite", "dufite",
    "mufite", "bufite", "gifite", "nfite",  "ufite",  "afite",
    "ifite",  NULL
};
```

```c
/* "bifiteubugingo" -> emit "bifite", re-read "ubugingo" */
Token *last = &out[count - 1];
for (int fi = 0; FITE_FORMS[fi]; fi++) {
    size_t flen = strlen(FITE_FORMS[fi]);
    if (wlen > flen && strncmp(last->lower, FITE_FORMS[fi], flen) == 0) {
        last->surface[flen] = '\0';
        last->lower[flen]   = '\0';
        p = p - (wlen - flen);
        break;
    }
}
```

Verified against the real library:

```
$ gcc -std=c99 -Wall -Wextra -I include p5_fite.c -L . -lkinyarwanda -o p5_fite
$ LD_LIBRARY_PATH=. ./p5_fite
"Bifiteubugingo bwiza." -> 4 tokens
  [0] "Bifite"
  [1] "ubugingo"
  [2] "bwiza"
  [3] "."
```

Everything else in this chapter splits text that already has the
right boundaries marked (whitespace, an apostrophe, a punctuation
mark) and just needs those marks recognized. This is the one place
the tokenizer does the opposite: it recognizes that two real words got
run together with *no* boundary mark at all, by checking whether the
front of an unexpectedly-long token matches one of thirteen known
stative-possessive prefixes, and repairs the omission by re-cutting
the token and backing the input pointer up to re-scan the overflow as
a fresh word. It's a narrow, named-exception fix — thirteen literal
forms, nothing more general — for a narrow, specific real-world
problem: text typed or OCR'd without a space where one belonged.

## 6.4 Closing the loop on Section 1.1's stale comment

Section 1.1 quoted the file's own header claiming punctuation "ends a
token but is discarded," and flagged it as stale without yet proving
it. `emit_punct`, the function every punctuation branch in the main
loop calls, settles it directly:

```c
static int emit_punct(Token *out, int count, int max_tokens,
                      const char *ch, size_t nbytes, PunctType pt) {
    if (count >= max_tokens) return 0;
    memset(&out[count], 0, sizeof(Token));
    memcpy(out[count].surface, ch, nbytes);
    out[count].surface[nbytes] = '\0';
    /* ... */
    out[count].pos             = POS_PUNCTUATION;
    out[count].punct_type      = pt;
    /* ... */
    return 1;
}
```

This writes a full `Token` — surface text, `POS_PUNCTUATION`, a
specific `PunctType`, and (Part 7 reads these next) boundary flags —
into the output array and returns `1` for success, exactly the same
contract every word-emitting path in this file follows. Nothing here
discards anything; the header comment describes a design this file
no longer implements, most likely an accurate description of an
earlier version that changed once Part 7's boundary-flag system was
added and punctuation became something worth keeping. The lesson
isn't that this particular comment is wrong — it's that a comment and
the function eleven lines below it can drift apart with no compiler
warning to flag the mismatch, the same risk Chapter 5's `INVARIABLES[]`
duplicates and this chapter's own Section 4.1 finding both showed from
different angles: nothing in C forces a comment, or a second table, or
an earlier design decision, to stay in sync with what the code actually
does today. Reading the function, not the comment above it, is what
this chapter has done at every single step.

---

# Part 7 — Every Flag This Chapter Sets Gets Read Somewhere Else

## 7.1 Boundaries this chapter computes, agreement this book already built on them

Chapter 3, Section 7.2's real subject-verb agreement scan (re-quoted
in Chapter 5, Section 7.2) stops its backward search the moment it
hits a token with `is_sent_boundary` or `is_clause_boundary` set. Both
of those fields are computed nowhere but here:

```c
out[count].is_clause_boundary = (pt == PUNCT_COMMA || pt == PUNCT_SEMICOLON);
out[count].is_sent_boundary   = (pt == PUNCT_PERIOD || pt == PUNCT_QUESTION ||
                                 pt == PUNCT_EXCLAIM ||
                                 (pt == PUNCT_QUOTE_CLOSE && count > 0 &&
                                  out[count-1].is_sent_boundary));
```

That last line is worth pausing on by itself: a *closing quotation
mark* inherits `is_sent_boundary` from whatever immediately preceded
it, specifically so that direct speech like `«Nagiye.» Mugabo agira
ati...` correctly tells the next word (`Mugabo`) that it's
sentence-initial, even though the literal previous character on the
page is a closing guillemet, not a period. Every later chapter that
ever asked "is this token the start of a sentence" — including
Section 6.2's own proper-noun check, two sections above — is reading a
flag this exact propagation rule set, possibly several tokens
upstream of the period that originally triggered it.

## 7.2 What would happen without that one line

```c
/* p_no_propagate.c -- a toy quote-boundary check WITHOUT propagation */
#include <stdio.h>
#include <string.h>

typedef struct { const char *surface; int is_period; int is_quote_close; } Tok;

int is_sentence_initial_naive(Tok *toks, int i) {
    if (i == 0) return 1;
    return toks[i-1].is_period;   /* BUG: ignores quote-close entirely */
}

int main(void) {
    Tok sent[] = {
        { "Nagiye",  0, 0 },
        { ".",       1, 0 },
        { "\xe2\x80\x9d", 0, 1 },  /* closing curly quote */
        { "Mugabo",  0, 0 },
    };
    printf("Is \"Mugabo\" sentence-initial? %s\n",
           is_sentence_initial_naive(sent, 3) ? "yes" : "no");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p_no_propagate p_no_propagate.c
$ ./p_no_propagate
Is "Mugabo" sentence-initial? no
```

A naive check that only looks one token back, at the literal previous
character, gets this real and common pattern wrong: it sees the
closing quote immediately before `Mugabo`, not the period two tokens
back, and concludes `Mugabo` isn't sentence-initial. Section 6.2's
real proper-noun-vs-ordinary-capital distinction depends entirely on
correctly knowing when a word *is* sentence-initial — get that flag
wrong here, at tokenization, and every later chapter inherits the
mistake silently, with no further opportunity to catch it, because by
the time Chapter 1's noun detector or Chapter 6's own proper-noun
check runs, the flag is the only information left; the original
quote-and-period sequence that should have set it is gone.

---

# Part 8 — Practice

### Beginner

1. **Extend `p1_toy_tok.c`** (Section 1.3) to also split on a literal
   apostrophe character, producing two tokens the way the real
   tokenizer does, and verify it against `"N'abana"`.
2. **Trace `"k'ibuye"` by hand** through Section 1.2's elision rule:
   which word is losing its final vowel, and what is the real word it
   came from?

### Intermediate

3. **Confirm Section 4.1's `ka` gap independently.** Construct a real
   sentence using the class-12 connector `ka` elided before a
   vowel-initial noun, run it through `kin_analyze`, and check whether
   the fragment is recognized or falls through to `POS_FOREIGN` the
   way `ha` did.
4. **Write the fix for Section 4.1's finding**: add the two missing
   entries to `INVARIABLES[]` (or wherever you judge they belong), and
   re-run Section 4.1's exact test sentence to confirm `is_kinyarwanda`
   now reports `1` for the fragment.
5. **Find one more real input** that defeats Section 1.3's toy
   tokenizer in a way this chapter's three named problems (glued
   apostrophe, glued punctuation, run-on compounds) didn't already
   cover.

### Advanced

6. **Build the naive sentence-boundary check from Section 7.2 into
   something safer**, by adding quote-close propagation back in one
   line, and confirm it now agrees with the real tokenizer's logic on
   the same test input.
7. **Decide, and justify in writing**, whether `FITE_FORMS[]`'s
   thirteen-entry named-exception-list approach (Section 6.3) is the
   right fix for run-together compounds in general, or whether it's
   only appropriate because this specific prefix family (`-fite`
   stative forms) happens to be small and closed. What would you do
   differently for a much larger family of prefixes?
8. **Audit one more UTF-8 comparison** in `tokenizer.c` that this
   chapter didn't quote (the guillemets, the en/em dash, or the
   no-break/zero-width space checks), and explain in your own words
   why the `(unsigned char)` cast in front of each byte comparison is
   load-bearing rather than stylistic.

## Key takeaways

- Every chapter before this one silently assumed a sentence was
  already cut into correctly-bounded tokens with punctuation already
  separated out. That assumption is this chapter's entire subject:
  nothing classifies a *known* word until `tokenizer.c` has first
  decided where words even begin and end.
- This is the first chapter where the out-of-bounds risk every earlier
  chapter eventually found a real bug demonstrating (Chapters 1, 2,
  and 4) is structurally unreachable rather than merely guarded: the
  bound check is the *loop condition itself*, not an `if` statement
  that could be forgotten on one path and not another.
- Tokenization can't be placed anywhere on the open-set/closed-set,
  rule/lookup spectrum the other five trees occupy (Chapter 5, Section
  5.1), because it runs *before* any lexicon is consulted — the only
  tools available are literal byte and character comparisons against
  the raw input.
- A second real, previously-undocumented gap: the elided-fragment
  lookup table that exists specifically to support this chapter's
  apostrophe-splitting policy doesn't cover two of Chapter 4's sixteen
  noun-class connectors (`ka`, `ha`) — a fragment like `h'Imana`'s `h`
  tokenizes correctly but is then misclassified as foreign,
  `is_kinyarwanda=0`, by a downstream table that simply never learned
  about it. Like Chapter 5's shadowed duplicates, the gap is only
  visible by checking two files' claims against each other, not either
  file alone.
- This is the only file in the project that has to reason about raw
  multi-byte UTF-8 sequences, because every native Kinyarwanda
  *letter* fits in a single ASCII byte — the need only arises for
  punctuation (curly quotes, guillemets, dashes) borrowed from outside
  the native orthography.
- Flags this chapter computes and nowhere else — `is_sent_boundary`,
  `is_clause_boundary`, `is_proper_noun` — are read by name in
  Chapters 1 through 5's own agreement and classification logic. A
  mistake made here has no opportunity to be caught later, because by
  the time a later chapter's detector runs, the flag is the only
  surviving trace of the original punctuation pattern that set it.

## Sources quoted in this chapter

- `src/tokenizer.c` (`kin_tokenize`, `find_apostrophe`, `is_dquote`,
  `emit_punct`, `FITE_FORMS[]`, the proper-noun/`l`-letter check).
- `include/kinyarwanda.h` (`kin_tokenize`'s signature, `PunctType`,
  the `Token` struct's boundary and proper-noun fields).
- `src/lexicon.c` (the elided-fragment `INVARIABLES[]` entries used in
  Part 4's capstone).
- `data/textbook_grammar_rules.md`, PART 12 (vowel elision rules,
  Section 1.2).
- Every `pN_*.c`/`p_*.c` program and every real-library run in this
  chapter was actually compiled with `gcc -std=c99 -Wall -Wextra` and
  actually executed to produce the exact output quoted above.

## Coming up in Chapter 7

Six chapters have now covered every tree this project tags a word
with, and the layer underneath all of them that decides where words
even begin. What's never been examined is what happens *after* every
token is tagged — how `kin_analyze` decides a sentence `has_verb` or
`is_complete`, how the dozens of `ERR_*` checks scattered across
`syntax.c` actually get invoked in sequence, and how `kin_suggest_corrections`
turns a detected error into the specific, human-readable suggestion
string every case study in this book has been printing without ever
asking how it was built. Chapter 7 reads `analysis.c` and the
correction-suggestion machinery: the layer that turns five trees'
worth of per-token tags into one verdict about a whole sentence.
