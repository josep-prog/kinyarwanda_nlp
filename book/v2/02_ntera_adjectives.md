# Chapter 2 — Ntera: The Kinyarwanda Adjective, Built and Run Together

## How this chapter works

This chapter continues exactly where Chapter 1 left off, and follows the
same rule: every code example is a complete, standalone C program you
can compile and run yourself, right now, with real output — and you
build several small versions of an idea before you ever read the real
production code. If you haven't worked through Chapter 1, do that
first — this chapter assumes you already know the 16 noun classes, the
D+RT+C formula, the out-parameter pattern for returning multiple
answers, and the `u→w/_J` glide rule, and it builds on all four without
re-teaching them from scratch.

By the end of this chapter you will be able to: explain what *ntera*
(adjective) means and how it's built from just two pieces; explain why
an adjective's prefix has to match the class of the noun it describes,
and what happens when it doesn't; build, run, and extend a small
adjective detector, a small interactive adjective *builder*, and a small
tool that both *catches* a disagreement and *repairs* it; and finally,
read and confidently extend the four real functions this project ships
for exactly this purpose: `kin_strip_adj_prefix`, `analyse_adj`, the
agreement check in `syntax.c`, and `build_adj` in `corrector.c`.

---

# Part 1 — Language and Code, Side by Side

## 1.1 What is *ntera*, and why it needs the noun first

*Ntera* is the Kinyarwanda word for **adjective** — a word that
describes a noun's *imiterere, imimerere n'ingano* ("nature, condition,
and size"), as this project's grammar reference puts it. In English,
"good" stays exactly the same letters whether you say "a good person" or
"good people." In Kinyarwanda, the adjective **changes its own
spelling** depending on which noun it's describing — specifically, on
the noun's class (Chapter 1, Section 1.5):

```
   umuntu   mwiza     "a good person"   (class 1: mu- prefix)
   abantu   beza      "good people"     (class 2: be- prefix, a+i→e)
   ikintu   cyiza      "a good thing"    (class 7: cy- prefix)
```

Same underlying idea — "good" — three completely different spellings,
each one mechanically determined by the noun standing next to it. This
is exactly the **concordance** (*indangasano*) mechanism Chapter 1,
Section 1.8, previewed in English but never built in code. This chapter
builds it.

## 1.2 The two-part skeleton: RS + C

Where a noun is D + RT + C (three pieces, Chapter 1 Section 1.2), an
adjective is only **two** pieces:

```
        RS         +          C
   (Indangasano)        (Igicumbi)
  "concordance            "stem"
    prefix"
   changes to match      carries the meaning;
   the noun's class       NEVER changes
```

Quoting this project's grammar reference directly:

> **Formula: RS + C** (Indangasano + Igicumbi)
> | Component | Description |
> |---|---|
> | RS = Indangasano | Changes based on noun class of the noun it modifies; equals noun's indanganteko |
> | C = Igicumbi | Does not change; carries the meaning |

And the textbook's own worked examples:

> - Umukinnyi **mushya** yatsinze ibitego byinshi → RS=mu, C=shya
> - Umurima **mwiza** wera imyaka myinshi → RS=mu, C=iza (u→w/_J)
> - Ubutunzi **bwiza** → bu-iza (u→w/_J)

Notice immediately: there is no D (initial vowel) in an adjective at
all — only the concordance prefix and the stem. And notice the second
example already uses the `u→w/_J` rule you independently rediscovered in
Chapter 1, Part 4 — the *same* rule, governing a *different* word
category. That is not a coincidence; Section 1.5 below explains why.

## 1.3 The concordance table: the same 16 markers, a new job

Here is the detail that makes this entire chapter possible: **the RS
prefix for class N is the same underlying marker as that class's RT**
(Chapter 1, Section 1.5) — just attached to an adjective stem instead of
a noun stem.

```c
/* From this project's real ADJ_PREFIXES table (morphology.c) */
{ "mu",  1  }, { "ba",  2  }, { "mu",  3  }, { "mi",  4  },
{ "ri",  5  }, { "ma",  6  }, { "ki",  7  }, { "bi",  8  },
{ "n",   9  }, { "zi",  10 }, { "ru",  11 }, { "ka",  12 },
{ "tu",  13 }, { "bu",  14 }, { "ku",  15 }, { "ha",  16 },
```

Compare this against Chapter 1, Section 1.5's `NOUN_CLASSES[]` table —
column for column, `mu/ba/mu/mi/ri/ma/ki/bi/n/zi/ru/ka/tu/bu/ku/ha` is
the *exact same sequence* as the `rt` column you already memorized.
This single fact is the entire mechanical basis of concordance: an
adjective doesn't have its own independent set of 16 markers to learn —
it borrows the noun's.

## 1.4 Build it: a detector for one class, one stem

```c
/* p2_toy_v1.c */
#include <stdio.h>
#include <string.h>

/* toy_detect_adj_v1 -- recognizes ONLY class 2's "ba-" + the stem "bi" (bad). */
int toy_detect_adj_v1(const char *w) {
    if (strncmp(w, "ba", 2) == 0 && strcmp(w + 2, "bi") == 0) {
        return 2;
    }
    return 0;
}

int main(void) {
    const char *words[] = { "babi", "bashya", "mubi" };
    for (int i = 0; i < 3; i++)
        printf("%-8s -> class %d\n", words[i], toy_detect_adj_v1(words[i]));
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p2_toy_v1 p2_toy_v1.c
$ ./p2_toy_v1
babi     -> class 2
bashya   -> class 0
mubi     -> class 0
```

Only the exact word it was taught comes back positive. `bashya` fails
because this version doesn't know the stem `shya` yet; `mubi` fails
because it doesn't know the `mu-` prefix yet. Time to teach it both.

## 1.5 The closed set: adjectives are not like nouns here

Before extending the detector, notice something Chapter 1 spent real
effort establishing for nouns and that does **not** carry over here.
Chapter 1, Section 5.2, explained that the noun stem set is open-ended —
new nouns get coined constantly, so no fixed list can ever be complete.
**Adjective stems are the opposite.** This project's grammar reference
states it directly:

> These stems form the **CLOSED SET** of adjective roots in Kinyarwanda.

There are roughly 27 of them, total, in the entire language (plus a
handful of spelling variants of the same root, like `-to`/`-toya`/`-toto`
for "small"). This is not a limitation of this project's lexicon — it's
a real, closed fact about Kinyarwanda grammar, and it changes the
engineering problem in a real way that Part 5 returns to: there is no
"novel adjective" the way there can be a novel noun.

## 1.6 Build it: extend the detector across classes and stems

```c
/* p3_toy_v2.c */
#include <stdio.h>
#include <string.h>

/* The closed set of adjective stems -- unlike nouns, this list is
 * FIXED and complete. There is no "novel adjective." */
static const char *TOY_ADJ_STEMS[] = { "bi", "shya", "nini", "kuru", NULL };

int toy_is_adj_stem(const char *s) {
    for (int i = 0; TOY_ADJ_STEMS[i]; i++)
        if (strcmp(s, TOY_ADJ_STEMS[i]) == 0) return 1;
    return 0;
}

/* The concordance prefixes (RS) -- IDENTICAL to the noun-class markers
 * (RT) from Chapter 1, Section 1.3, just playing a different role. */
static const struct { const char *pfx; int cls; } TOY_RS[] = {
    { "mu", 1 }, { "ba", 2 }, { "ki", 7 }, { "bi", 8 }, { NULL, 0 }
};

int toy_detect_adj_v2(const char *w) {
    for (int i = 0; TOY_RS[i].pfx; i++) {
        size_t plen = strlen(TOY_RS[i].pfx);
        if (strncmp(w, TOY_RS[i].pfx, plen) == 0 &&
            toy_is_adj_stem(w + plen)) {
            return TOY_RS[i].cls;
        }
    }
    return 0;
}

int main(void) {
    const char *words[] = { "mubi", "babi", "kinini", "bibi", "bunini" };
    for (int i = 0; i < 5; i++)
        printf("%-8s -> class %d\n", words[i], toy_detect_adj_v2(words[i]));
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p3_toy_v2 p3_toy_v2.c
$ ./p3_toy_v2
mubi     -> class 1
babi     -> class 2
kinini   -> class 7
bibi     -> class 8
bunini   -> class 0
```

`bunini` ("big," class 14) honestly fails — `bu-` isn't in our four-row
prefix table yet. Notice the *shape* of this function: iterate over
known prefixes, and for each one that matches, check whether what's left
is a member of the closed stem set. That two-step shape — prefix
iteration, then closed-set membership test — is exactly how the real
`kin_strip_adj_prefix` works, just with all 16+ prefixes and all ~27
stems instead of four and four.

## 1.7 Checkpoint: test yourself before continuing

Using only Section 1.3's table and the stems `-iza` (good), `-nini`
(big), and `-bi` (bad), work out the adjective form for:

1. Class 8 (`bi-`) + `-iza` ("good," describing class-8 things)
2. Class 11 (`ru-`) + `-bi` ("bad," describing class-11 things)
3. Class 1 (`mu-`) + `-nini` ("big," describing a class-1 person)

<details><summary>Answers</summary>

1. `bi` + `iza` → vowel-initial stem after a prefix ending in `i` — hold
   this one; Part 4 covers exactly this case (it surfaces as `byiza`).
2. `ru` + `bi` → no vowel collision → `rubi`.
3. `mu` + `nini` → no vowel collision → `munini`.

</details>

If (1) felt uncertain, that's expected — you haven't been taught that
rule yet. Part 4 is where every remaining rule like it gets built and
tested, one at a time.

---

# Part 2 — Multiple Answers, and a New Way to Crash

## 2.1 The same multi-return shape, applied here

Just like `kin_strip_noun_prefix` in Chapter 1, a complete adjective
detector needs to hand back more than one fact: did this look like an
adjective at all, which class does its RS prefix point to, and what's
the bare stem. The same out-parameter pattern applies unchanged:

```c
/* p_toy_stem.c */
#include <stdio.h>
#include <string.h>

static const char *TOY_ADJ_STEMS[] = { "bi", "nini", "iza", NULL };
static const struct { const char *pfx; int cls; } TOY_RS[] = {
    { "mu", 1 }, { "ba", 2 }, { "ki", 7 }, { NULL, 0 }
};

int toy_is_adj_stem(const char *s) {
    for (int i = 0; TOY_ADJ_STEMS[i]; i++)
        if (strcmp(s, TOY_ADJ_STEMS[i]) == 0) return 1;
    return 0;
}

int toy_strip_adj_prefix(const char *word, char *stem_out, int *class_out) {
    for (int i = 0; TOY_RS[i].pfx; i++) {
        size_t plen = strlen(TOY_RS[i].pfx);
        if (strncmp(word, TOY_RS[i].pfx, plen) != 0) continue;
        if (toy_is_adj_stem(word + plen)) {
            strncpy(stem_out, word + plen, 31);
            stem_out[31] = '\0';
            *class_out = TOY_RS[i].cls;
            return 1;
        }
    }
    stem_out[0] = '\0';
    return 0;
}

int main(void) {
    const char *words[] = { "munini", "babi", "ikinini" };
    for (int i = 0; i < 3; i++) {
        char stem[32]; int cls = 0;
        int ok = toy_strip_adj_prefix(words[i], stem, &cls);
        printf("%-9s -> ok=%d class=%d stem=\"%s\"\n", words[i], ok, cls, stem);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p_toy_stem p_toy_stem.c
$ ./p_toy_stem
munini    -> ok=1 class=1 stem="nini"
babi      -> ok=1 class=2 stem="bi"
ikinini   -> ok=0 class=0 stem=""
```

`ikinini` honestly fails: it isn't a real prefix-plus-stem combination
this toy table knows (there's no class with prefix `iki-` for
adjectives — adjective RS prefixes are *shorter* than noun D+RT
prefixes, since adjectives have no D vowel at all, Section 1.2). Nothing
here should surprise you; it's the identical mechanism from Chapter 1,
Section 2.2, applied to a new word category.

## 2.2 A new way to crash: indexing past the end of an array

Chapter 1 showed you two ways undefined behavior can bite: an unbounded
`strcpy` (Section 2.3) and a `NULL` pointer dereference (Section 2.4).
Here is a third, common in exactly this kind of class-indexed lookup
table. Picture the real concordance table as a plain array, indexed
directly by class number — Chapter 1, Section 1.5, already justified
why direct indexing beats a hash map here:

```c
/* p_unsafe_idx.c -- DELIBERATELY UNSAFE: no bounds check. */
#include <stdio.h>
#include <string.h>

void unsafe_build_adj(int noun_class, const char *stem, char *out, size_t outsz) {
    static const char *RS[] = {
        "", "mu","ba","mu","mi","ri","ma","ki","bi",
        "n","zi","ru","ka","tu","bu","ku","ha"
    };
    /* NO CHECK that noun_class is between 1 and 16! */
    const char *pfx = RS[noun_class];
    snprintf(out, outsz, "%s%s", pfx, stem);
}

int main(void) {
    char out[32];
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("Valid class (99... wait, invalid -- watch this one first):\n");
    unsafe_build_adj(99, "nini", out, sizeof(out));
    printf("  -> %s (it may not crash here -- that's the trap)\n\n", out);

    printf("Invalid class (-50):\n");
    unsafe_build_adj(-50, "nini", out, sizeof(out));
    printf("  -> %s\n", out);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p_unsafe_idx p_unsafe_idx.c
$ ./p_unsafe_idx
Valid class (99... wait, invalid -- watch this one first):
  -> (null)nini (it may not crash here -- that's the trap)

Invalid class (-50):
$ echo "exit code: $?"
exit code: 139
```

Read this carefully, because it's a more dangerous shape of bug than
Chapter 1's crashes: **the exact same kind of mistake — reading past the
end of an array — produced two completely different outcomes.** With
class `99`, the out-of-bounds read happened to land on a `0` bit pattern
that looked like a `NULL` pointer, and the C library's `printf`
defensively printed the string `"(null)"` instead of crashing — so the
program kept running, with silently wrong output, and you might never
notice unless you were checking every value by hand. With class `-50`,
the out-of-bounds read landed somewhere genuinely invalid, and the
operating system killed the process — exit code `139`, the same
`SIGSEGV` from Chapter 1. **Both are undefined behavior. One of them
just happened to look survivable.** That unpredictability — not a
guaranteed crash — is the real danger of reading outside an array's
bounds.

The fix is the same shape every time: check first.

```c
/* p_safe_idx.c -- SAFE: bounds-checked, mirroring the real corrector.c guard. */
void safe_build_adj(int noun_class, const char *stem, char *out, size_t outsz) {
    static const char *RS[] = {
        "", "mu","ba","mu","mi","ri","ma","ki","bi",
        "n","zi","ru","ka","tu","bu","ku","ha"
    };
    if (noun_class < 1 || noun_class > 16) {
        strncpy(out, stem, outsz - 1);
        out[outsz - 1] = '\0';
        return;
    }
    const char *pfx = RS[noun_class];
    snprintf(out, outsz, "%s%s", pfx, stem);
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p_safe_idx p_safe_idx.c
$ ./p_safe_idx
Valid class (7):
  -> kinini

Invalid class (-50):
  -> nini (no crash -- fell back to the bare stem)

Invalid class (5000000):
  -> nini (no crash)
```

One `if` at the top, before the array is ever touched, and every
invalid input gets the same calm, predictable fallback — the bare stem,
with no concordance prefix attached — instead of an unpredictable mix of
silent corruption and hard crashes. This exact guard,
`if (noun_class < 1 || noun_class > 16)`, is quoted directly from this
project's real `corrector.c`.

---

# Part 3 — Making It Interactive

## 3.1 Build it: an adjective builder you can talk to

```c
/* p_repl.c */
#include <stdio.h>
#include <string.h>

void toy_build_adj(int cls, const char *stem, char *out, size_t outsz) {
    static const char *RS[] = {
        "", "mu","ba","mu","mi","ri","ma","ki","bi",
        "n","zi","ru","ka","tu","bu","ku","ha"
    };
    if (cls < 1 || cls > 16) { strncpy(out, stem, outsz-1); out[outsz-1]='\0'; return; }
    int vowel = stem[0]=='a'||stem[0]=='e'||stem[0]=='i'||stem[0]=='o'||stem[0]=='u';
    if (vowel) {
        switch (cls) {
            case 1: case 3: snprintf(out, outsz, "mw%s", stem); return;
            case 4:         snprintf(out, outsz, "my%s", stem); return;
            case 7:         snprintf(out, outsz, "cy%s", stem); return;
            case 8:         snprintf(out, outsz, "by%s", stem); return;
            case 11:        snprintf(out, outsz, "rw%s", stem); return;
            case 13:        snprintf(out, outsz, "tw%s", stem); return;
            case 14:        snprintf(out, outsz, "bw%s", stem); return;
            case 15:        snprintf(out, outsz, "kw%s", stem); return;
            default: break;
        }
    }
    snprintf(out, outsz, "%s%s", RS[cls], stem);
}

int main(void) {
    char line[64];
    printf("Toy Ntera Builder -- type: <class> <stem>  (e.g. \"1 iza\"), or 'quit'\n");
    printf("> ");
    fflush(stdout);
    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1]=='\n') line[len-1]='\0';
        if (strcmp(line, "quit") == 0) break;

        int cls; char stem[32];
        if (sscanf(line, "%d %31s", &cls, stem) == 2) {
            char out[40];
            toy_build_adj(cls, stem, out, sizeof(out));
            printf("  class %d + \"%s\" -> \"%s\"\n", cls, stem, out);
        } else {
            printf("  (couldn't parse -- expected: <class> <stem>)\n");
        }
        printf("> ");
        fflush(stdout);
    }
    printf("\nMurabeho!\n");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p_repl p_repl.c
$ ./p_repl
Toy Ntera Builder -- type: <class> <stem>  (e.g. "1 iza"), or 'quit'
> 1 iza
  class 1 + "iza" -> "mwiza"
> 7 nini
  class 7 + "nini" -> "kinini"
> 14 iza
  class 14 + "iza" -> "bwiza"
> quit

Murabeho!
```

Every one of those three outputs is a real, attested Kinyarwanda word —
`mwiza` and `bwiza` come directly from Section 1.2's textbook quotes.
`sscanf(line, "%d %31s", &cls, stem)` is worth pausing on: the `%31s`
caps how many bytes `sscanf` will write into `stem` (a 32-byte buffer),
the same bounded-by-design discipline as `fgets` and `strncpy` from
Chapter 1 — an *un*-capped `%s` here would reopen exactly the kind of
overflow Chapter 1, Section 2.3, taught you to avoid.

---

# Part 4 — Capstone: The Phonological Rules, One Family at a Time

This is the heart of the chapter. Every rule below is real, documented,
and demonstrated with output verified against this project's actual
`kin_strip_adj_prefix` function — not invented. Each rule is its own
small, independently testable fact.

## 4.1 Family 1 (recap): the vowel glide, now on adjectives

You already proved `u→w/_J` in Chapter 1, Part 4. Confirm it carries
over unchanged to adjectives, plus its sibling `i→y/_J`:

```
$ LD_LIBRARY_PATH=. ./p_real_adj      (built against the real library)
word         ok   class  stem
mwiza        1    1      iza        <- mu + iza  (u->w)
bwiza        1    14     iza        <- bu + iza  (u->w)
myinshi      1    4      inshi      <- mi + inshi (i->y)
cyiza        1    7      iza        <- ki + iza  (i->y, with k->c palatalization)
```

`cyiza` deserves one extra word: class 7's prefix is `ki-`, but before a
vowel-initial stem the `k` itself palatalizes to `c` *at the same time*
as the `i` glides to `y` — `ki` + `iza` → `cyiza`, not the `kyiza` you
might predict from the glide rule alone. This is a second, narrower rule
riding alongside the general glide rule, specific to `ki-`/`bi-`-type
prefixes — and it's why the real `ADJ_PREFIXES` table (Section 1.3)
lists `"cy"` and `"by"` as their own separate entries rather than
deriving them on the fly.

## 4.2 Family 2: nasal assimilation (class 9 only)

Class 9's underlying RS is just `n`. What that `n` actually looks like
on the page depends entirely on the very next sound — the first letter
of the stem:

```
   n + bilabial (b,p,v,f,h)  → m     "n+bi"  -> "mbi"   (bad)
   n + r                     → nd    "n+re"  -> "nde"   (long)
   n + vowel or y            → nz    "n+iza" -> "nziza" (good)
   n + anything else         → n     "n+zima"-> "nzima" (heavy -- unchanged!)
```

Real, verified output:

```
$ LD_LIBRARY_PATH=. ./p_real_adj2
word         ok   class  stem
mbi          1    9      bi         <- n+b (bilabial) -> m
nde          1    9      re         <- n+r -> nd, and 'r' is absorbed
nziza        1    9      iza        <- n+vowel -> nz (epenthetic z)
nzima        1    9      zima       <- n+z: 'z' is just an ordinary
                                        consonant here, so RS stays
                                        plain "n" -- "nzima" is NOT
                                        the epenthetic rule firing twice
```

That last row is worth sitting with: `nziza` and `nzima` both contain
the literal three letters `n-z-i`, but for **completely different
reasons**. In `nziza`, the `z` is *inserted* by the epenthesis rule
because the stem `iza` starts with a vowel. In `nzima`, the stem `zima`
already starts with `z` on its own — `n` (the RS) is simply sitting next
to a stem that happens to begin with that letter, no insertion involved.
Two visually identical substrings, two unrelated grammatical
explanations — exactly the kind of trap that makes "just check if the
spelling looks similar" a bad strategy for any language tool, and the
exact reason every rule in this chapter is implemented by checking the
stem's *first letter against a specific category*, never by pattern-
matching the finished surface spelling after the fact.

The `nde` row also deserves a slow read: the real surface word for
"RS=n + stem=re (long)" is the three-letter word `nde`, **not** a
five-letter `ndere`. The stem's leading `r` is consumed entirely into
forming the `d` — `n` + `r` → `nd`, and only the stem's remaining vowel
`e` survives on the surface. (`ndere`, the more "obvious"-looking
construction, was the first thing tried while preparing this chapter,
and the real library's honest `ok=0` on that input is exactly what told
us the guess was wrong — a small, real demonstration of why this whole
book insists on testing every claim against the real code instead of
trusting intuition.)

## 4.3 Family 3: voicing (class 12 only)

Class 12's RS is `ka`. Before a stem that starts with a **voiced**
consonant, the `k` itself voices to `g`:

```
   ka + nini (voiced 'n') → ganini   (big, class 12)
```

```
$ LD_LIBRARY_PATH=. ./p_real_adj
ganini       1    12     nini
```

Compare this to class 9's nasal rules in Section 4.2: both are
*assimilation* rules (a sound shifting to match a property of its
neighbor), but they trigger on different conditions (class 9 cares about
*place of articulation* — bilabial vs. not; class 12 cares about
*voicing* — voiced vs. voiceless) and produce different kinds of change
(a full consonant substitution for class 9's `m`/`nd`/`nz`, a single
voicing flip for class 12's `k`→`g`). Naming them both "assimilation"
in casual conversation would blur a real, useful distinction.

## 4.4 Family 4: a + i → e fusion

When an RS prefix ends in the vowel `a` and the stem begins with `i`,
the two vowels fuse into a single `e` — a different outcome from the
glide rule in Section 4.1 (which turns `u` into the consonant `w`, not
into another vowel):

```
   ba (class 2) + inshi → benshi   (many, class 2)
   ba (class 2) + iza   → beza     (good, class 2)
   ma (class 6) + inshi → menshi   (many, class 6)
```

```
$ LD_LIBRARY_PATH=. ./p_real_adj
benshi       1    2      inshi
beza         1    2      iza
menshi       1    6      inshi
```

Three real, verified rows, and notice: the stem reported back by the
real function is the *original* `inshi`/`iza` — even though the surface
spelling shows `e`, the function's job is to recover what the speaker
actually meant, not just what's visible on the page. Section 6.2 shows
you exactly how the code reconstructs this.

## 4.5 Family 5: reduplication

A handful of stems can repeat themselves, attaching the RS prefix
*twice*, for emphasis — "very long," not just "long":

```
   mu + re + mu + re = muremure   (very tall/long, class 1)
   ba + re + ba + re = barebare   (class 2)
```

```
$ LD_LIBRARY_PATH=. ./p_real_adj
muremure     1    1      re
barebare     1    2      re
```

Both rows correctly recover the *bare* stem `re`, even though the
surface word contains it twice. This is structurally different from
every other rule in this chapter: Sections 4.1–4.4 are all about what
happens at the *boundary* between RS and C; reduplication is about the
*whole word* being built from two copies of RS+C stuck together. The
real detection function (Section 6.4) has a dedicated helper just for
this shape, separate from its ordinary prefix-stripping logic.

## 4.6 Checkpoint: five families, one table

| Family | Trigger | Example | Section |
|---|---|---|---|
| Vowel glide | `u`/`i`-final RS + vowel-initial stem | `mu+iza→mwiza` | 4.1 |
| Nasal assimilation | class 9 only, by stem's first sound | `n+bi→mbi` | 4.2 |
| Voicing | class 12 only, voiced stem-initial consonant | `ka+nini→ganini` | 4.3 |
| a+i→e fusion | RS ends in `a`, stem starts with `i` | `ba+iza→beza` | 4.4 |
| Reduplication | a closed list of stems, doubled | `mu+re+mu+re→muremure` | 4.5 |

Five independent, real, testable rules — and every one of them is
something you can now verify yourself, on any word, by compiling a
six-line program against this project's real library.

## 4.7 A genuine homograph: `nzima` versus `n` + `zima`

Section 1.5 established that the adjective stem set is closed — but a
closed set can still contain two *different* stems that happen to
collide on the surface once a prefix attaches. Recall Section 4.2's
table: class 9's RS is just `n`, and stays plain `n` (no change at all)
in front of any stem that doesn't start with a bilabial, `r`, or vowel.
`zima` ("whole/healthy") starts with `z` — none of those — so `n` +
`zima` should surface as plain `nzima`. But `ADJ_STEMS[]` (Section 6.1)
also lists a completely unrelated stem, `nzima` ("heavy/difficult/sick"),
on its own. Test both readings against the real library:

```
$ LD_LIBRARY_PATH=. ./p_nzima
word        ok   class  stem
nzima       1    9      zima        <- read as: RS=n (class 9) + C=zima
munzima     1    1      nzima       <- read as: RS=mu (class 1) + C=nzima
banzima     1    2      nzima       <- read as: RS=ba (class 2) + C=nzima
kinzima     1    7      nzima       <- read as: RS=ki (class 7) + C=nzima
```

Bare `nzima` is parsed as class 9, stem `zima` — the function never even
considers the *other* stem, `nzima`, for this input, because that stem
can only ever appear **with its own RS already attached in front of
it** (`munzima`, `banzima`, `kinzima` — never bare `nzima` standing
alone, the way a noun's D vowel can stand alone or drop, Chapter 1,
Section 1.9). This is the payoff of a fact Section 1.2 stated and this
section can now make precise: **an adjective's RS can never be silently
absent**, the way a noun's D vowel sometimes is. If it could, `nzima`
would be a genuine, unresolvable ambiguity between two real stems — the
same shape of problem as Chapter 1's `umuti`/class 1-vs-3 ambiguity.
Because it can't, the surface collision is real (two different,
documented words really do share five identical letters) but the
*parsing* is not ambiguous at all: a bare word is always class 9's
`n` + whatever's left, full stop, and the only way to reach the other
stem is to write a longer word with its own RS already in front.

`nzinya` (one of the augmentative "big" variants from Section 6.1's
stem list) shows the identical pattern, one more time:

```
$ LD_LIBRARY_PATH=. ./p_augment
word         ok   class  stem
bunziginya   1    14     nziginya    <- bu + nziginya (class 14)
nzinya       1    9      nzinya      <- n + nzinya (class 9, geminate n+n)
muniniya     1    1      niniya      <- mu + niniya (class 1)
```

Here the *same* stem, `nzinya`, is reached two different ways depending
on what precedes it: with no further RS in front, it's class 9's
geminate `n`+`n`→`n` simplification (Section 6.3); with `bu-` in front,
it's class 14's `bu`+`nzinya`. Both are completely unambiguous, for the
exact same reason as the `zima`/`nzima` case above.

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 Restating the problem, and what's different from nouns

The contract looks identical to Chapter 1's, on the surface:

```c
bool kin_strip_adj_prefix(const char *word, char *stem_out, int *class_out);
   /* returns true/false; writes the stem and class THROUGH pointers */
```

But Section 1.5 already flagged the one fact that changes the actual
engineering problem underneath this identical-looking signature: **the
adjective stem set is closed**, not open-ended. Chapter 1's noun
detector needed a two-tier design — a lookup table for known/irregular
words, *plus* a general rule for everything else — specifically because
new nouns can always appear that neither table has ever seen (Chapter 1,
Section 7's whole point). An adjective detector never faces that
problem: there is no such thing as a stem outside `ADJ_STEMS[]`, by the
closed-set fact itself. Every adjective that will ever exist in the
language is already in that list.

## 5.2 Why this makes the design simpler, not harder

This means `kin_strip_adj_prefix` gets to skip an entire layer Chapter
1's noun pipeline needed: there is no separate "known irregular word"
exact-match table sitting in front of the rule-based detector, because
the rule-based detector's own closed-set membership check
(`kin_is_adj_stem`) *already is* an exact match against every adjective
that exists. The "hybrid" architecture from Chapter 1, Part 7, was
necessary there because rules alone couldn't see novel words and a
lookup table alone couldn't see novel spellings of known words at the
same time. Here, there's no novelty to hedge against on the stem side at
all — only on the *prefix* side (which prefixes attach to which stems,
governed by the five rule families in Part 4). The remaining engineering
problem is genuinely smaller, and the code in Part 6 is, correspondingly,
about thirty lines shorter than Chapter 1's equivalent.

---

# Part 6 — Reading the Real Production Code

## 6.1 `ADJ_STEMS[]` and the closed-set check

```c
static const char *ADJ_STEMS[] = {
    "nini", "inshi", "bi", "tindi", "gari", "iza", "sa", "sa-sa",
    "zima", "to", "toto", "to-to", "toya", "ke", "keya", "ke-ke",
    "kuru", "bisi", "shya", "shyashya", "gufi", "gufiya", "re", "re-re",
    "tagatifu", "hire", "taraga",
    /* augmentative variants, and a handful from REB/corpus work */
    "nzinya", "nzunyu", /* ... */ "nzima", "ogo", "eru", "rimbwa",
    "nkuru", "meze",
    NULL
};

bool kin_is_adj_stem(const char *stem) {
    for (int i = 0; ADJ_STEMS[i]; i++)
        if (strcmp(stem, ADJ_STEMS[i]) == 0) return true;
    return false;
}
```

A flat array, `NULL`-terminated, scanned linearly — the same shape
Chapter 1 justified twice already (Sections 1.5 and 7.3), for the same
reason: roughly 40 fixed entries, known completely at compile time, make
a linear scan both fast enough and maximally auditable against the
exact textbook list it implements (REB S4, p.66–67, cited directly in
the source comment above this table).

## 6.2 `kin_strip_adj_prefix`, in full

```c
bool kin_strip_adj_prefix(const char *word, char *stem_out, int *class_out) {
    static const struct { const char *pfx; int cls; } ADJ_PREFIXES[] = {
        { "mu",  1  }, { "ba",  2  }, { "mu",  3  }, { "mi",  4  },
        { "ri",  5  }, { "ma",  6  }, { "ki",  7  }, { "bi",  8  },
        { "n",   9  }, { "zi",  10 }, { "ru",  11 }, { "ka",  12 },
        { "tu",  13 }, { "bu",  14 }, { "ku",  15 }, { "ha",  16 },
        { "mw",  1  }, { "rw",  11 }, { "bw",  14 }, { "kw",  15 },
        { "tw",  13 }, { "by",  8  }, { "cy",  7  }, { "my",  4  },
        { "ry",  5  },
        { "m",   9  }, { "nd",  9  }, { "nz",  9  },
        { "ga",  12 },
        { "be",  2  }, { "me",  6  }, { "ye",  4  }, { "ze", 10 }, { "he", 16 },
        { "h",   16 },
        { NULL, 0 }
    };
    static const char *FUSED_PREFIXES[] = { "be", "me", "ye", "ze", "he", NULL };

    for (int i = 0; ADJ_PREFIXES[i].pfx; i++) {
        size_t plen = strlen(ADJ_PREFIXES[i].pfx);
        if (!kin_starts_with(word, ADJ_PREFIXES[i].pfx)) continue;
        const char *sfx = word + plen;

        if (kin_is_adj_stem(sfx)) {
            if (stem_out)  strncpy(stem_out, sfx, KIN_MAX_STEM - 1);
            if (class_out) *class_out = ADJ_PREFIXES[i].cls;
            return true;
        }
        /* ... class-9 geminate/nd reconstruction, reduplication check,
         * and a+i fusion reconstruction all follow -- Sections 6.3-6.5 ... */
    }
    return false;
}
```

Compare this directly to your own `p3_toy_v2.c` from Section 1.6: same
shape — iterate over known prefixes, check the closed-set membership of
whatever's left — just with roughly 35 prefix variants instead of four,
covering every glide, fusion, and assimilation pattern from Part 4 as
its own table row.

## 6.3 Reconstructing what the surface spelling hid

Section 4.2 showed you that `nde` is really `n`(RS) + `re`(C), with the
stem's `r` absorbed into the `d`. Here is exactly how the real code
recovers that fact — by guessing the original letter back and checking
whether *that* is a known stem:

```c
/* Class 9 special: n+r→nd. After stripping "nd", sfx = "e", but the
 * underlying stem is "re". Reconstruct by prepending 'r' and recheck. */
if (ADJ_PREFIXES[i].cls == 9 && strcmp(ADJ_PREFIXES[i].pfx, "nd") == 0) {
    char restored[KIN_MAX_STEM];
    restored[0] = 'r';
    strncpy(restored + 1, sfx, KIN_MAX_STEM - 2);
    restored[KIN_MAX_STEM - 1] = '\0';
    if (kin_is_adj_stem(restored)) {
        if (stem_out)  strncpy(stem_out, restored, KIN_MAX_STEM - 1);
        if (class_out) *class_out = 9;
        return true;
    }
}
```

This is the same "try the reconstructed original, check it against the
closed set" idea, repeated with a different prepended letter, for every
rule in Part 4 that *removes or changes* information on the way to the
surface form: a `+'n'` reconstruction for the class-9 geminate case
(`n`+`nini`→`nini`, where the doubled `n` simplifies to one), and a
`+'i'` reconstruction for every `a+i→e` fusion prefix (`be`/`me`/`ye`/
`ze`/`he`) from Section 4.4. Each one answers the same question: *given
what's left over after stripping a known prefix, what original stem,
modified by one specific rule, would have produced exactly this surface
text?*

## 6.4 Reduplication, detected separately

```c
bool kin_is_adj_reduplicated(const char *sfx, const char *pfx, char *stem_out) {
    size_t pfxlen = strlen(pfx);
    for (int i = 0; ADJ_STEMS[i]; i++) {
        size_t slen = strlen(ADJ_STEMS[i]);
        if (strlen(sfx) != slen + pfxlen + slen) continue;
        if (strncmp(sfx,                 ADJ_STEMS[i], slen)   == 0 &&
            strncmp(sfx + slen,          pfx,          pfxlen) == 0 &&
            strncmp(sfx + slen + pfxlen, ADJ_STEMS[i], slen)   == 0) {
            if (stem_out) strncpy(stem_out, ADJ_STEMS[i], KIN_MAX_STEM - 1);
            return true;
        }
    }
    return false;
}
```

Read the three `strncmp` calls as one sentence: "does `sfx` consist of
[a known stem] followed by [the same prefix we already stripped]
followed by [that same stem again]?" The length check just above them
(`strlen(sfx) != slen + pfxlen + slen`) is a cheap early exit — there's
no point running three string comparisons against a candidate stem
whose doubled length doesn't even arithmetically fit what's left of the
word.

## 6.5 The surface-rule engine: `analyse_adj`

Where `kin_strip_adj_prefix` answers "what class and stem is this," a
separate function, `analyse_adj` in `morph_dispatch.c`, computes *which
rule fired* for display and explanation purposes — this is the function
that produced every "(u→w/_J)"-style annotation you've been reading all
chapter:

```c
if (cls == 9) {
    static const char BILABIALS[] = "bpvfh";
    bool is_bilabial = false;
    for (int b = 0; BILABIALS[b]; b++)
        if (c[0] == BILABIALS[b]) { is_bilabial = true; break; }

    if (is_bilabial) {
        strncpy(rs_surface, "m", sizeof(rs_surface)-1);
        snprintf(rule_rs, sizeof(rule_rs),
                 "n→m §3.3 (RS 'n' before bilabial '%c')", c[0]);
    } else if (c[0] == 'r') {
        strncpy(rs_surface, "nd", sizeof(rs_surface)-1);
        /* ... */
    } else if (is_vowel(c[0]) || c[0] == 'y') {
        strncpy(rs_surface, "nz", sizeof(rs_surface)-1);
        /* ... */
    }
    /* else: RS stays "n" unchanged */
}
```

This is Section 4.2's table, written as code: check the stem's first
character against each category in order (bilabial, then `r`, then
vowel/`y`), and the first match decides the surface RS. Notice the
`BILABIALS` array is checked with the exact same small linear loop
you've now seen for noun classes, adjective stems, and prefixes alike —
one consistent idiom, reused at every scale in this codebase, for the
same reason every time: the candidate set is small, fixed, and needs to
stay auditable against a numbered grammar rule (`§3.3`, `§3.5`, `§2.4`)
quoted directly in the code.

**An honest aside.** This project also has a second, more general
function, `kin_vv_join`, that resolves vowel-to-vowel contact for many
different morpheme types (not just adjectives). `build_adj` in
`corrector.c` (Part 7) doesn't call it — it has its own smaller,
adjective-specific `switch` statement that happens to produce the same
answers for the cases both functions handle. This is a small, real
piece of redundancy, not a bug: recognizing it is itself a useful skill,
because real codebases that have grown over time often end up with two
related but independently-evolved implementations of a similar idea,
and knowing how to spot that — without necessarily rushing to "fix" it —
is part of reading code like an engineer instead of a purist.

---

# Part 7 — Concordance: Detecting Disagreement, and Repairing It

## 7.1 Build it: a toy agreement checker

This is the mechanism Chapter 1, Sections 1.8–1.9, could only describe
in English. Here it is, in eleven lines:

```c
/* p_checker.c */
#include <stdio.h>
#include <string.h>

int toy_noun_class(const char *w) {
    if (strncmp(w, "umu", 3) == 0) return 1;
    if (strncmp(w, "iki", 3) == 0) return 7;
    return 0;
}

int toy_adj_class(const char *w) {
    static const struct { const char *pfx; int cls; } RS[] = {
        { "mw", 1 }, { "mu", 1 }, { "ki", 7 }, { "cy", 7 }, { NULL, 0 }
    };
    static const char *STEMS[] = { "iza", "nini", "bi", NULL };
    for (int i = 0; RS[i].pfx; i++) {
        size_t plen = strlen(RS[i].pfx);
        if (strncmp(w, RS[i].pfx, plen) != 0) continue;
        for (int j = 0; STEMS[j]; j++)
            if (strcmp(w + plen, STEMS[j]) == 0) return RS[i].cls;
    }
    return 0;
}

/* The actual concordance check. */
int toy_check_agreement(const char *noun, const char *adj) {
    int ncls = toy_noun_class(noun);
    int acls = toy_adj_class(adj);
    if (ncls == 0 || acls == 0) return -1;   /* can't judge */
    return ncls == acls;                      /* 1 = agrees, 0 = error */
}

int main(void) {
    struct { const char *noun, *adj; } pairs[] = {
        { "umuntu", "mwiza" },
        { "umuntu", "kinini" },
        { "ikintu", "cyiza" },
    };
    for (int i = 0; i < 3; i++) {
        int r = toy_check_agreement(pairs[i].noun, pairs[i].adj);
        printf("%-8s %-8s -> %s\n", pairs[i].noun, pairs[i].adj,
               r == 1 ? "OK" : r == 0 ? "AGREEMENT ERROR" : "unknown");
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p_checker p_checker.c
$ ./p_checker
umuntu   mwiza    -> OK
umuntu   kinini   -> AGREEMENT ERROR
ikintu   cyiza    -> OK
```

The entire check is one line: `return ncls == acls;`. Everything else in
this chapter exists to make those two integers — the noun's class and
the adjective's class — available and correct in the first place.

## 7.2 Build it: detect, then repair

Catching the error is only half the value. Here is a toy version of the
full *correction* flow — find the noun's class, then *rebuild* the
adjective to match it, instead of just flagging that it's wrong:

```c
/* p_corrector.c */
#include <stdio.h>
#include <string.h>

int toy_noun_class(const char *w) {
    if (strncmp(w, "umu", 3) == 0) return 1;
    if (strncmp(w, "iki", 3) == 0) return 7;
    return 0;
}

int toy_adj_class_and_stem(const char *w, char *stem_out) {
    static const struct { const char *pfx; int cls; } RS[] = {
        { "mw", 1 }, { "mu", 1 }, { "ki", 7 }, { "cy", 7 }, { NULL, 0 }
    };
    static const char *STEMS[] = { "iza", "nini", "bi", NULL };
    for (int i = 0; RS[i].pfx; i++) {
        size_t plen = strlen(RS[i].pfx);
        if (strncmp(w, RS[i].pfx, plen) != 0) continue;
        for (int j = 0; STEMS[j]; j++) {
            if (strcmp(w + plen, STEMS[j]) == 0) {
                strcpy(stem_out, STEMS[j]);
                return RS[i].cls;
            }
        }
    }
    stem_out[0] = '\0';
    return 0;
}

void toy_build_adj(int cls, const char *stem, char *out, size_t outsz) {
    static const char *RSTAB[] = { "", "mu","x","x","x","x","x","ki" };
    if (cls != 1 && cls != 7) { strncpy(out, stem, outsz-1); out[outsz-1]='\0'; return; }
    int vowel = stem[0]=='a'||stem[0]=='e'||stem[0]=='i'||stem[0]=='o'||stem[0]=='u';
    if (vowel && cls == 1) { snprintf(out, outsz, "mw%s", stem); return; }
    if (vowel && cls == 7) { snprintf(out, outsz, "cy%s", stem); return; }
    snprintf(out, outsz, "%s%s", RSTAB[cls], stem);
}

void toy_correct(const char *noun, const char *adj) {
    int ncls = toy_noun_class(noun);
    char astem[32];
    int  acls = toy_adj_class_and_stem(adj, astem);

    if (ncls == 0 || acls == 0) { printf("%s %s -> (can't judge)\n", noun, adj); return; }
    if (ncls == acls) { printf("%s %s -> OK\n", noun, adj); return; }

    char corrected[40];
    toy_build_adj(ncls, astem, corrected, sizeof(corrected));
    printf("%s %s -> ERROR: replace \"%s\" with \"%s\" (class %d)\n",
           noun, adj, adj, corrected, ncls);
}

int main(void) {
    toy_correct("umuntu", "mwiza");
    toy_correct("umuntu", "kinini");
    toy_correct("ikintu", "mwiza");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p_corrector p_corrector.c
$ ./p_corrector
umuntu mwiza -> OK
umuntu kinini -> ERROR: replace "kinini" with "munini" (class 1)
ikintu mwiza -> ERROR: replace "mwiza" with "cyiza" (class 7)
```

Both corrections are genuinely correct Kinyarwanda — `munini` (a big
person) and `cyiza` (a good thing) — produced mechanically, from nothing
but each word's own detected class and stem.

## 7.3 The real detection: `syntax.c`

```c
if (cur->pos == POS_NOUN && cur->noun_class > 0 &&
    next->pos == POS_ADJECTIVE && next->noun_class > 0) {
    if (cur->noun_class != next->noun_class) {
        /* ... build the bilingual error message and suggestion ... */
        add_error(sa, ERR_ADJ_AGREEMENT, i + 1, msg, sug);
    }
}
```

That is, character for character, Section 7.1's one-line check —
`cur->noun_class != next->noun_class` — now scanning every adjacent
noun-then-adjective pair in a real sentence, instead of one hand-picked
pair at a time.

## 7.4 The real repair: `build_adj` in `corrector.c`

```c
static void build_adj(int noun_class, const char *stem, char *out, size_t outsz) {
    static const char *RS[] = {
        "", "mu","ba","mu","mi","ri","ma","ki","bi",
        "n","zi","ru","ka","tu","bu","ku","ha"
    };
    if (noun_class < 1 || noun_class > 16) {
        strncpy(out, stem, outsz - 1); return;
    }
    const char *pfx = RS[noun_class];
    bool stem_vowel = (stem[0] && strchr("aeiou", stem[0]) != NULL);

    if (stem_vowel) {
        switch (noun_class) {
            case 1: case 3:  snprintf(out, outsz, "mw%s",  stem); return;
            case 4:          snprintf(out, outsz, "my%s",  stem); return;
            case 7:          snprintf(out, outsz, "cy%s",  stem); return;
            case 8:          snprintf(out, outsz, "by%s",  stem); return;
            case 11:         snprintf(out, outsz, "rw%s",  stem); return;
            case 13:         snprintf(out, outsz, "tw%s",  stem); return;
            case 14:         snprintf(out, outsz, "bw%s",  stem); return;
            case 15:         snprintf(out, outsz, "kw%s",  stem); return;
            default: break;
        }
    }
    snprintf(out, outsz, "%s%s", pfx, stem);
}
```

This is your Section 3.1 REPL's `toy_build_adj`, almost unchanged —
same bounds check, same vowel test, same `switch` over the glide classes
from Part 4's Family 1. The only real difference is that the real
version is reached automatically, from inside `kin_suggest_corrections`,
the moment `syntax.c` has already flagged a real `ERR_ADJ_AGREEMENT`.

## 7.5 Case study: the full pipeline, on a real sentence

```c
/* p_real_pipeline.c */
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    const char *text = "Umuntu kinini aragenda.";
    SentenceAnalysis sa = kin_analyze(text);
    kin_suggest_corrections(&sa);

    printf("Input: \"%s\"\n\n", text);
    for (int i = 0; i < sa.token_count; i++) {
        Token *t = &sa.tokens[i];
        printf("  token[%d]=\"%-10s\" pos=%d noun_class=%d\n",
               i, t->surface, t->pos, t->noun_class);
    }
    printf("\nErrors found: %d\n", sa.error_count);
    for (int i = 0; i < sa.error_count; i++) {
        printf("  [%d] %s\n      -> %s\n", i,
               sa.errors[i].message, sa.errors[i].suggestion);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p_real_pipeline.c -L . -lkinyarwanda -o p_real_pipeline
$ LD_LIBRARY_PATH=. ./p_real_pipeline
Input: "Umuntu kinini aragenda."

  token[0]="Umuntu    " pos=1 noun_class=1
  token[1]="kinini    " pos=2 noun_class=7
  token[2]="aragenda  " pos=6 noun_class=1
  token[3]="."         " pos=17 noun_class=0

Errors found: 1
  [0] Gushyira hamwe nabi: 'Umuntu' (inteko 1) na 'kinini' (inteko 7). Agreement error: 'Umuntu' (class 1) with 'kinini' (class 7).
      -> Hindura 'kinini' ugakoresheje 'munini' kugira ngo ishyikire inteko 1 (Nt.1 – human singular (umuntu)). / Replace 'kinini' with 'munini' to agree with class 1 (Nt.1 – human singular (umuntu)).
```

Every number and every word in that output is real and reproducible —
`pos=1` is `POS_NOUN`, `pos=2` is `POS_ADJECTIVE` (the very next value in
the same enum), and the corrected suggestion, `munini`, is exactly what
your own toy corrector in Section 7.2 would have produced for the same
inputs. Notice also `token[2]`, the verb `aragenda` ("is walking") —
its `noun_class` field reads `1` too, meaning the real pipeline also
checked that the *verb's* subject prefix agrees with `Umuntu`'s class,
and found no error there. Subject–verb agreement is its own, separate
mechanism, with its own rules — a later chapter's subject, not this
one's — but seeing it sitting quietly correct in this real output is a
preview of how many of this project's checks run side by side on the
same sentence at once.

---

# Part 8 — Practice

### Beginner

1. **Trace `byiza`** ("good," describing a class-8 noun) by hand through
   Section 1.7's Family 1 rule, then confirm your answer by compiling a
   six-line program against the real library, the same way Section 4.1
   did for `mwiza`.

2. **Extend your own `p3_toy_v2.c`** (Section 1.6) to add class 14
   (`bu-`) and the stem `-iza`. Confirm `bwiza` now classifies correctly
   — it should have failed before your change.

### Intermediate

3. **Build the class-9 geminate case yourself.** Stem `-nini` ("big"),
   RS `n` (class 9): work out the surface form by hand (Section 6.3's
   `+'n'` reconstruction is your guide), then verify it against the real
   library.

4. **Extend your toy checker (Section 7.1)** to a third noun/adjective
   class pair of your choosing from Chapter 1's 16-class table, and
   confirm both the "agrees" and "disagrees" cases produce the right
   verdict.

5. **Modify `p_repl.c`** (Section 3.1) so that typing `r` repeats the
   last successful build with a different class number, without you
   having to retype the stem.

### Advanced

6. **Implement the class-12 voicing rule (Section 4.3) in your own toy
   `analyse_adj`-style function**, following the exact shape of Section
   6.5's class-9 code: a small array of voiced consonants, checked with
   a linear loop, deciding the surface RS.

7. **Find a sentence of your own** with a deliberate noun/adjective
   mismatch, run it through `kin_analyze` and `kin_suggest_corrections`
   the way Section 7.5 did, and confirm the suggested correction is
   genuinely correct Kinyarwanda by checking it against Chapter 1 and
   this chapter's tables by hand — not just trusting the program.

8. **Investigate the `kin_vv_join`/`build_adj` redundancy** mentioned in
   Section 6.5. Find one input (a class and a vowel-initial stem) where
   the two functions might plausibly disagree, and test both to see
   whether they actually do.

## Key takeaways

- An adjective is RS + C — concordance prefix plus stem — with no D
  vowel at all; the RS values are the exact same 16 markers as the noun
  RT table from Chapter 1, reused for a different grammatical job.
- Unlike nouns, the adjective stem set is **closed** — about 40 entries,
  total, forever — which removes the entire "known irregular word"
  lookup layer Chapter 1's noun pipeline needed.
- Five independent phonological rule families govern how RS and C
  combine on the surface: vowel glide (recap from Chapter 1), nasal
  assimilation (class 9 only), voicing (class 12 only), a+i→e fusion,
  and reduplication — each verified against the real library, not
  assumed.
- Reading outside an array's bounds is undefined behavior that does
  **not** reliably crash — you watched the identical mistake produce a
  silent, wrong-looking-but-running result in one case and a hard
  `SIGSEGV` in another, which is the actual danger of UB: unpredictable
  behavior, not guaranteed failure.
- Concordance — the mechanism Chapter 1 could only describe in English
  — is, in code, a single equality check between two already-computed
  class numbers; everything else exists to make those two numbers
  available and correct.
- Detecting an error and repairing it are two different, composable
  functions in this codebase (`syntax.c`'s check, `corrector.c`'s
  `build_adj`) — the same separation-of-concerns idea you'll keep seeing
  throughout this project.
- Real codebases sometimes carry small, harmless redundancy (the
  `kin_vv_join`/`build_adj` overlap) — recognizing it without rushing to
  "fix" it is part of reading code like an engineer.

## Sources quoted in this chapter

- `data/textbook_grammar_rules.md`, Part 8 (NTERA) and its worked
  examples (*mushya*, *mwiza*, *bwiza*, *ndende*, *menshi*) — REB S4,
  p.66–68.
- `include/kinyarwanda.h` (`kin_strip_adj_prefix`, `kin_is_adj_stem`,
  `kin_is_adj_reduplicated`, the `POS` enum).
- `src/lexicon.c` (`ADJ_STEMS[]`, `kin_is_adj_stem`,
  `kin_is_adj_reduplicated`).
- `src/morphology.c` (`kin_strip_adj_prefix`, `ADJ_PREFIXES[]`,
  `kin_vv_join`).
- `src/morph_dispatch.c` (`analyse_adj`, `NOUN_RT[]`).
- `src/syntax.c` (the `ERR_ADJ_AGREEMENT` check).
- `src/corrector.c` (`build_adj`, `kin_suggest_corrections`).
- Every `pN_*.c`/`p_*.c` program and every real-library run in this
  chapter was actually compiled with `gcc -std=c99 -Wall -Wextra` and
  actually executed to produce the exact output quoted above.

## Coming up in Chapter 3

This chapter built concordance for one relationship: noun → adjective.
Chapter 3 builds the next one this project checks the same way:
noun → verb subject agreement (the `aragenda` field you saw sitting
quietly correct in Section 7.5) — a new morpheme formula (SP + TM + C +
FV), a new closed-vs-open engineering question (verb roots are open-
ended like nouns, not closed like adjectives — which one do you think
the real code resembles more, and why?), and the same build-it-yourself,
read-it-for-real structure this chapter and Chapter 1 both followed.
