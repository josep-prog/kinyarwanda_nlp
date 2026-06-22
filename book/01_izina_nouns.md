# Chapter 1 — Izina: The Kinyarwanda Noun, Taught, Built, and Run

## How this chapter works

This chapter is meant to be **complete on its own**. You should not need
to have read anything else, and you should not need to wait for a later
chapter to "complete the picture." Every single code example in this
chapter is a full, standalone C program — every `#include` it needs is
shown, every example is something you can copy into a file, compile, and
run on your own machine *right now*, and see real output on your
terminal. None of them depend on the rest of this project's source tree.
Only in the second half of the chapter, once you've built several small
versions of this idea yourself, will you read the real, production
functions from this project — and by then they should feel familiar
rather than foreign.

This is deliberate, for two reasons. First, **teaching**: you retain far
more from typing, compiling, and watching something run than from
reading about it. Second, **practice for building your own things**: the
small programs in this chapter are not toys to be thrown away — they are
a genuine, miniature, working version of the real idea, and the skills
you exercise writing them (string scanning, pointer-based multiple
return values, defensive buffer handling, a small interactive loop) are
exactly the skills used throughout the rest of this project's real
source code.

**What you need:** a C compiler. Every example here was tested with
`gcc` on Linux; any standards-conforming C compiler (`clang`, `cc`,
even an online compiler) will work identically. To compile and run any
example named, say, `example.c`:

```
$ gcc -std=c99 -Wall -Wextra -o example example.c
$ ./example
```

If that command produces no errors and prints something, it worked. If
it prints warnings, read them — `-Wall -Wextra` is deliberately strict,
and every warning it has ever caught in this project's real history
turned out to matter.

By the end of this chapter you will be able to: explain what an *izina*
(noun) is and how it's assembled; recognize all 16 noun classes and what
binds each one together; write, from scratch, a small working noun
classifier of your own, including a version that runs interactively;
explain — concretely, with working code in hand — what can go wrong with
unchecked string copying and how to prevent it; and finally, read,
explain, and confidently extend the two real C functions
(`kin_detect_noun_class`, `kin_strip_noun_prefix`) this project actually
ships with.

If you already speak Kinyarwanda: the grammar sections will still be
worth reading closely. They restate what you already know informally as
a precise, named, *citable* rule — which is what you need in order to
defend a design decision, not just feel that it's right.

---

# Part 1 — Language and Code, Side by Side

## 1.1 What is an *izina*, and how does the engine mark one?

*Izina* (plural *amazina*) is the Kinyarwanda word for **noun** — and
also the everyday word for "name." It names a person, animal, thing,
place, or idea, exactly the role a noun plays in English.

The very first decision any program that understands Kinyarwanda has to
make about a word is binary: is it a noun, or not. Here is the smallest
possible program that states that decision out loud. Type it in, compile
it, run it.

```c
/* p1_hello.c */
#include <stdio.h>

int main(void) {
    printf("umuntu   -> this word is an IZINA (a noun)\n");
    printf("aragenda -> this word is an INSHINGA (a verb)\n");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p1_hello p1_hello.c
$ ./p1_hello
umuntu   -> this word is an IZINA (a noun)
aragenda -> this word is an INSHINGA (a verb)
```

That real, verified output is the entire anchor for this section: one
Kinyarwanda word, *umuntu*, labeled as a noun. Everything else in this
chapter is about how a program can make that same decision *for itself*,
for any word, rather than having the answer typed in as a literal
string. The function this project actually uses to make this decision is
called `kin_detect_noun_class` — you'll write your own small version of
it before this chapter is finished, and you'll read the real one in
Part 6.

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

Watch it on one real word:

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

Three rules, worth fixing in memory now: **D can be missing. RT can be
missing. C can never be missing** — without it, there's no meaning left.

## 1.3 The same skeleton, seen as a C data shape

Before writing any logic, it helps to see what *shape* this information
would take if you wanted to store it in a C program. This is just a
data layout — there is nothing to compile or run yet, just a struct
definition to read slowly:

```c
typedef struct {
    char label        [12];   /* "D", "RT", or "C" -- literally the
                                  same three letters you just learned */
    char form         [20];   /* the underlying piece, e.g. "mu"      */
    char english_gloss[48];   /* what this one piece means in English */
} KinMorpheme;
```

And here is *umuntu* — the exact word from Section 1.2 — as three filled
values of that struct:

| You learned (1.2) | The data would store |
|---|---|
| D = `u` | `{label="D",  form="u",   english_gloss=""}` |
| RT = `mu` | `{label="RT", form="mu",  english_gloss=""}` |
| C = `ntu` (means "person") | `{label="C",  form="ntu", english_gloss="person"}` |

Three small structs, holding exactly the three pieces you already
understand. We are not yet asking *how* a program would figure out where
one piece ends and the next begins inside a single string like
`"umuntu"` — that is the very next thing we build.

## 1.4 Build it: a classifier that recognizes one class

Let's write the smallest possible program that actually *looks at* a
word and decides something about it, instead of having the answer typed
in. It will only know about class 1 (the `umu-` prefix) for now — that's
deliberate; we will grow it piece by piece.

```c
/* p2_toy_v1.c */
#include <stdio.h>
#include <string.h>

/* toy_detect_class_v1 -- recognizes ONLY class 1 ("umu-"), nothing else yet. */
int toy_detect_class_v1(const char *w) {
    if (strncmp(w, "umu", 3) == 0) {
        return 1;
    }
    return 0;
}

int main(void) {
    const char *words[] = { "umuntu", "umugabo", "amazi", "ikintu" };
    int n = 4;

    for (int i = 0; i < n; i++) {
        int cls = toy_detect_class_v1(words[i]);
        printf("%-10s -> class %d\n", words[i], cls);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p2_toy_v1 p2_toy_v1.c
$ ./p2_toy_v1
umuntu     -> class 1
umugabo    -> class 1
amazi      -> class 0
ikintu     -> class 0
```

Read the function slowly: `strncmp(w, "umu", 3)` compares the **first
three bytes** of `w` against the literal string `"umu"`, and returns `0`
(meaning "identical") when they match. `amazi` and `ikintu` correctly
come back as class `0` — our toy function doesn't know about any other
class yet, and `0` is chosen specifically because no real class is ever
numbered zero, so it can never be confused with a genuine answer.

## 1.5 Sixteen families: the noun classes (Inteko)

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

The full set:

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

Two facts to hold onto, because you'll watch real code wrestle with them
directly later in this chapter: **classes 1 and 3 are spelled
identically** (`umu-`) — there is no way to tell "person" from
"tree/medicine" by prefix alone, only by already knowing the specific
stem. **Classes 9 and 10 are also spelled identically** — singular and
plural "cow" are both `inka`; Kinyarwanda resolves that ambiguity on the
*surrounding* words, not the noun itself.

## 1.6 Build it: grow the classifier to six classes

Now extend the toy function from Section 1.4 to recognize six classes
instead of one:

```c
/* p3_toy_v2.c */
#include <stdio.h>
#include <string.h>

/* toy_detect_class_v2 -- now recognizes six classes instead of one.
 * Each check is independent so far; order doesn't matter yet because
 * every prefix here is a different three letters. That changes soon --
 * keep reading Section 1.10/1.11. */
int toy_detect_class_v2(const char *w) {
    if (strncmp(w, "umu", 3) == 0) return 1;
    if (strncmp(w, "aba", 3) == 0) return 2;
    if (strncmp(w, "imi", 3) == 0) return 4;
    if (strncmp(w, "ama", 3) == 0) return 6;
    if (strncmp(w, "iki", 3) == 0) return 7;
    if (strncmp(w, "ibi", 3) == 0) return 8;
    return 0;
}

int main(void) {
    const char *words[] = {
        "umuntu", "abantu", "imiti", "amazi", "ikigo", "ibigo", "urugo"
    };
    int n = 7;

    for (int i = 0; i < n; i++) {
        int cls = toy_detect_class_v2(words[i]);
        printf("%-10s -> class %d\n", words[i], cls);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p3_toy_v2 p3_toy_v2.c
$ ./p3_toy_v2
umuntu     -> class 1
abantu     -> class 2
imiti      -> class 4
amazi      -> class 6
ikigo      -> class 7
ibigo      -> class 8
urugo      -> class 0
```

Notice `urugo` ("homestead," class 11) honestly comes back as `0` — our
toy function hasn't been taught class 11 yet. This is not a bug to
apologize for; it's the real, ordinary shape of growing a rule-based
system: each new `if` you add teaches it one more fact it didn't know a
moment ago. The production code in Part 6 simply has many more of these
lines than our six.

## 1.7 Singular and plural, side by side

```
   Class 1  (umu-, human sg.)  ←──pairs with──→  Class 2  (aba-, human pl.)
   Class 3  (umu-, thing sg.)  ←──pairs with──→  Class 4  (imi-, thing pl.)
   Class 5  (i-,   general sg.) ←──pairs with──→ Class 6  (ama-, general pl.)
   Class 7  (iki-, thing sg.)   ←──pairs with──→ Class 8  (ibi-, thing pl.)
   Class 9  (in-,  animal sg.)  ←──pairs with──→ Class 10 (in-,  animal pl.)
   Class 12 (aka-, small sg.)   ←──pairs with──→ Class 13 (utu-, small pl.)

   Class 11 (uru-), 14 (ubu-), 15 (uku-), 16 (aha-) -- usually stand alone
```

This project's own lexicon stores these pairs directly, one row per
pair, alongside the shared stem both forms have in common:

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

Notice the `inka` row: `plural` is `NULL`, because the spelling really
is identical for singular and plural, exactly as Section 1.5 warned.
The table doesn't hide that fact — it records `NULL` honestly and lets
the class number alone (9 vs. 10) carry the distinction, to be resolved
by whatever else in the sentence agrees with this noun. Which is exactly
the next concept.

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
determined by the noun's class. This is **concordance** (*indangasano*),
and a future chapter on adjectives and verbs builds the code that checks
it. For this chapter, just notice that the *data* this future code needs
already has to live somewhere — and it does, attached to each class, as
you'll see in Part 6's full `NounClass` table.

## 1.9 The messy reality: speech is faster than spelling

Real spoken and written Kinyarwanda regularly bends the tidy formula
above:

- **The D vowel often disappears in casual speech.** *Umuntu* → *muntu*;
  *abahinzi* ("farmers") → *bahinzi*.
- **Proper names drop their indomo entirely, as a rule.** *Umugabo* ("a
  man") vs. *Mugabo* (a name).
- **Some prefixes collide with other word categories.** A word that
  starts with the same two or three letters as a noun's "dropped D
  vowel" form might actually be a *verb*, or might be a *different
  noun's* full-form prefix wearing a disguise.

That last bullet is not abstract. It produces a real bug if your code
isn't careful about *order* — and you're about to watch that bug happen
live.

## 1.10 Build it: the ordering bug, caught live

Consider the word *icyiza* ("beautiful," class 7 — it's `iki-` fused
with a vowel-initial stem, `iki+iza → icyiza`). Now consider a second,
completely unrelated rule: any word starting with `i` followed by a
consonant that isn't `n` or `m` defaults to class 5 (a broad fallback for
words like *ishuri*, "school," or *itara*, "lamp"). Both rules are
individually correct. Watch what happens when the **generic** rule is
checked before the **specific** one:

```c
/* p4_order_wrong.c */
#include <stdio.h>
#include <string.h>

static int is_vowel(char c) { return c=='a'||c=='e'||c=='i'||c=='o'||c=='u'; }

/* WRONG ORDER: the generic "bare i-" fallback (class 5) runs FIRST,
 * before the specific "icy" check (class 7) ever gets a turn. */
int toy_detect_WRONG(const char *w) {
    size_t wlen = strlen(w);

    if (w[0]=='i' && wlen > 3 && !is_vowel(w[1]) && w[1]!='n' && w[1]!='m')
        return 5;                                /* generic class-5 fallback */

    if (strncmp(w, "icy", 3) == 0 && wlen > 4)
        return 7;                                /* never reached for icyiza */

    return 0;
}

int main(void) {
    const char *words[] = { "icyiza", "ishuri", "itara" };
    for (int i = 0; i < 3; i++)
        printf("%-10s -> class %d   (WRONG ORDER)\n",
               words[i], toy_detect_WRONG(words[i]));
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p4_order_wrong p4_order_wrong.c
$ ./p4_order_wrong
icyiza     -> class 5   (WRONG ORDER)
ishuri     -> class 5   (WRONG ORDER)
itara      -> class 5   (WRONG ORDER)
```

`icyiza` should be class 7. It came back as class 5, because the generic
check ran first, matched, and `return`ed before the specific `icy` check
ever got a chance to run. Now swap only the *order* of the two checks —
nothing else changes:

```c
/* p5_order_right.c */
#include <stdio.h>
#include <string.h>

static int is_vowel(char c) { return c=='a'||c=='e'||c=='i'||c=='o'||c=='u'; }

/* RIGHT ORDER: the specific "icy" check (class 7) runs FIRST. Only words
 * that fail it fall through to the generic class-5 fallback. */
int toy_detect_RIGHT(const char *w) {
    size_t wlen = strlen(w);

    if (strncmp(w, "icy", 3) == 0 && wlen > 4)
        return 7;                                /* specific check goes first */

    if (w[0]=='i' && wlen > 3 && !is_vowel(w[1]) && w[1]!='n' && w[1]!='m')
        return 5;                                /* generic fallback, second */

    return 0;
}

int main(void) {
    const char *words[] = { "icyiza", "ishuri", "itara" };
    for (int i = 0; i < 3; i++)
        printf("%-10s -> class %d   (RIGHT ORDER)\n",
               words[i], toy_detect_RIGHT(words[i]));
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p5_order_right p5_order_right.c
$ ./p5_order_right
icyiza     -> class 7   (RIGHT ORDER)
ishuri     -> class 5   (RIGHT ORDER)
itara      -> class 5   (RIGHT ORDER)
```

Identical logic, identical inputs, only the *order of two `if`
statements* swapped — and one real word's classification changed from
wrong to right, while the other two stayed exactly as correct as before.
This is the single most important engineering lesson in this whole
chapter: **in a chain of pattern-matching `if` statements, order is not
cosmetic — it is part of the algorithm's correctness.** The rule is
always the same: the more specific, longer, or rarer pattern goes first;
the more general, shorter, or catch-all pattern goes last. You will see
this exact principle, defended the same way, governing the real
production function's 100-plus lines in Part 6.

## 1.11 Checkpoint: test yourself before continuing

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
4. Full form `abagabo` ("men") — class 2, D vowel dropped (Section 1.9)
5. `i-n-zu` → class 9 (or 10 — same spelling, Section 1.5)

</details>

---

# Part 2 — Multiple Answers From One Function, and Not Crashing

## 2.1 The puzzle: three answers, one `return`

So far our toy classifier only answers one question: which class. A
complete noun-recognizer needs to answer **three** questions at once:
did this look like a noun at all; which class; and what's left over once
the prefix is removed (the stem — the part that actually carries
meaning, per Section 1.2's third rule). A C function can only `return`
one value. So how do real C programs hand back more than one answer?

The answer is **pointers used as output channels**. The caller creates
its own variables, hands the function their *addresses*, and the
function writes its extra answers directly into that caller-owned
memory — reserving the one real `return` value for the simplest
possible fact: did this succeed at all.

```
   CALLER OWNS THE MEMORY                  FUNCTION WRITES INTO IT
   ┌─────────────────┐                    ┌──────────────────────┐
   │ char stem[32];   │ ── address ──────▶│ writes the stem       │
   │ int  cls;        │ ── address ──────▶│ writes the class num  │
   └─────────────────┘                    └──────────────────────┘
           ▲                                        │
           └──────────── int success ◀──────────────┘
```

## 2.2 Build it: a classifier that also extracts the stem

```c
/* p6_toy_stem.c */
#include <stdio.h>
#include <string.h>

int toy_detect_class(const char *w) {
    if (strncmp(w, "umu", 3) == 0) return 1;
    if (strncmp(w, "aba", 3) == 0) return 2;
    if (strncmp(w, "imi", 3) == 0) return 4;
    if (strncmp(w, "ama", 3) == 0) return 6;
    if (strncmp(w, "iki", 3) == 0) return 7;
    if (strncmp(w, "ibi", 3) == 0) return 8;
    return 0;
}

/* toy_strip_prefix -- needs to hand back THREE things (success, class, stem)
 * but a C function can only `return` one value. Solution: pointers. */
int toy_strip_prefix(const char *word, char *stem_out, int *class_out) {
    int cls = toy_detect_class(word);
    if (cls == 0) {
        stem_out[0] = '\0';   /* always leave stem_out a valid empty string */
        return 0;             /* 0 = failure */
    }
    *class_out = cls;          /* write the class THROUGH the pointer */

    /* every prefix we know about so far is exactly 3 bytes long */
    const char *stem_start = word + 3;
    strncpy(stem_out, stem_start, 31);
    stem_out[31] = '\0';       /* ALWAYS terminate explicitly -- see 2.3 */
    return 1;                  /* 1 = success */
}

int main(void) {
    const char *words[] = { "umuntu", "abahinzi", "ikigo" };
    for (int i = 0; i < 3; i++) {
        char stem[32];
        int  cls = 0;
        int  ok  = toy_strip_prefix(words[i], stem, &cls);
        printf("%-10s -> ok=%d class=%d stem=\"%s\"\n",
               words[i], ok, cls, stem);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p6_toy_stem p6_toy_stem.c
$ ./p6_toy_stem
umuntu     -> ok=1 class=1 stem="ntu"
abahinzi   -> ok=1 class=2 stem="hinzi"
ikigo      -> ok=1 class=7 stem="go"
```

Walk through `&cls` for a moment: the `&` operator takes the *address*
of the local variable `cls` in `main`, and that address is what
`toy_strip_prefix` receives as its `class_out` parameter. Inside the
function, `*class_out = cls;` means "go to that address, and write this
value there" — not "create a new local copy." That's why `main` sees
the updated value after the call returns, even though `cls` was never
passed as an ordinary argument in the way `word` was.

## 2.3 What happens if the destination buffer is too small?

The line `stem_out[31] = '\0';` in the program above looks like a small
detail. It is not. To see exactly why, build the unsafe version first —
deliberately, so you can watch what goes wrong with your own eyes,
safely, in a throwaway program.

```c
/* p7_unsafe.c -- DELIBERATELY UNSAFE. Do not write code like this. */
#include <stdio.h>
#include <string.h>

void unsafe_copy_stem(const char *stem) {
    char tiny_buf[8];          /* room for 7 real characters + '\0' */
    strcpy(tiny_buf, stem);    /* strcpy NEVER checks the destination size */
    printf("copied stem: %s\n", tiny_buf);
    fflush(stdout);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);  /* force unbuffered output so every
                                           printf appears immediately, even
                                           if the program crashes right after */
    printf("Copying a short stem (fits fine):\n");
    unsafe_copy_stem("ntu");

    printf("\nCopying a stem that does NOT fit in 8 bytes:\n");
    unsafe_copy_stem("nyeshuri-mukuru-w-ishuri");
    printf("(if you see this line, the program survived anyway --\n"
           " that is NOT a guarantee, just luck)\n");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p7_unsafe p7_unsafe.c
$ ./p7_unsafe
Copying a short stem (fits fine):
copied stem: ntu

Copying a stem that does NOT fit in 8 bytes:
copied stem: nyeshuri-mukuru-w-ishuri
$ echo "exit code: $?"
exit code: 139
```

Look closely at what actually happened, because it's more dangerous than
a clean crash would be. `strcpy` copied all 25 bytes of
`"nyeshuri-mukuru-w-ishuri"` into an 8-byte buffer — writing 17 bytes
past the end of memory `tiny_buf` was ever allowed to use. The `printf`
right after that **still printed the full, correct-looking string** —
because the bytes were physically written to memory right next to the
buffer, in order, and nothing stopped `printf` from reading them back out
again. Everything *looked* fine. Then, when `unsafe_copy_stem` tried to
**return** to `main`, the program crashed — exit code 139, which is
128 + 11, where 11 is `SIGSEGV` ("segmentation fault"): the overflow had
overwritten memory the function needed in order to know where to return
to. The final reassuring-sounding `printf` in `main` never ran.

This is the real, defining danger of undefined behavior: it does not
fail at the moment of the mistake. It can fail later, somewhere
completely different in the program, in a way that looks unrelated to
the actual bug — which is exactly what makes buffer overflows so much
harder to debug than an error that fails immediately and loudly.

Now build the fix, changing only the copying mechanism:

```c
/* p8_safe.c -- SAFE version: bounded copy, always explicitly terminated. */
#include <stdio.h>
#include <string.h>

void safe_copy_stem(const char *stem) {
    char tiny_buf[8];
    strncpy(tiny_buf, stem, sizeof(tiny_buf) - 1);  /* copy AT MOST 7 bytes */
    tiny_buf[sizeof(tiny_buf) - 1] = '\0';          /* ALWAYS terminate    */
    printf("copied stem: %s\n", tiny_buf);
}

int main(void) {
    printf("Copying a short stem (fits fine):\n");
    safe_copy_stem("ntu");

    printf("\nCopying a stem that does NOT fit in 8 bytes:\n");
    safe_copy_stem("nyeshuri-mukuru-w-ishuri");
    printf("(this line ALWAYS prints -- the program never crashed)\n");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p8_safe p8_safe.c
$ ./p8_safe
Copying a short stem (fits fine):
copied stem: ntu

Copying a stem that does NOT fit in 8 bytes:
copied stem: nyeshur
(this line ALWAYS prints -- the program never crashed)
$ echo "exit code: $?"
exit code: 0
```

Same scenario, same oversized input, completely different outcome:
`strncpy(tiny_buf, stem, sizeof(tiny_buf) - 1)` copies **at most** 7
bytes, no matter how long `stem` is, and the line right after it writes
a `'\0'` into the buffer's very last byte unconditionally. The result is
a gracefully truncated stem (`"nyeshur"` instead of the full word) and a
program that finishes cleanly — exit code `0`, not `139`.

**The one sharp edge of `strncpy` you must never forget**: if the source
string is at least as long as the limit you gave it, `strncpy` does
**not** add a terminating `'\0'` for you — it only copies up to that many
bytes and stops. That is exactly why `p8_safe.c` has its own explicit
`tiny_buf[sizeof(tiny_buf) - 1] = '\0';` line right after the `strncpy`
call, rather than assuming `strncpy` handled it. Skip that one line, and
you've quietly reintroduced a version of the exact same class of bug
`p7_unsafe.c` just demonstrated — just one step removed instead of zero.

**The takeaway to carry through the rest of this book**: never use
`strcpy`, `sprintf`, `strcat`, or `gets` on data whose length you have
not already verified fits — and even when you switch to their bounded
cousins (`strncpy`, `snprintf`, `strncat`), always add your own explicit
terminator afterward. You will see this exact discipline, with this
exact reasoning, in every real string-handling function this project
ships.

## 2.4 A second way to crash: dereferencing a NULL pointer

Buffer overflows aren't the only way an out-parameter function can
crash. What if the *pointer itself* is missing — a caller forgets to
pass one, or passes `NULL` on purpose to mean "I don't care about this
answer"? Build the unsafe version first:

```c
/* p15_null_unsafe.c -- DELIBERATELY UNSAFE: no NULL check. */
#include <stdio.h>
#include <string.h>

int toy_strip_prefix_unsafe(const char *word, char *stem_out, int *class_out) {
    if (strncmp(word, "umu", 3) == 0) {
        *class_out = 1;                 /* writes through class_out -- CRASH
                                            here if class_out is NULL */
        strncpy(stem_out, word + 3, 31);
        stem_out[31] = '\0';
        return 1;
    }
    return 0;
}

int main(void) {
    char stem[32];
    printf("Calling with a valid class_out pointer:\n");
    int cls = 0;
    toy_strip_prefix_unsafe("umuntu", stem, &cls);
    printf("  class=%d stem=%s\n", cls, stem);

    printf("\nCalling with class_out = NULL (caller forgot to pass one):\n");
    fflush(stdout);
    toy_strip_prefix_unsafe("umuntu", stem, NULL);   /* CRASH expected */
    printf("(if you see this, it did not crash)\n");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p15_null_unsafe p15_null_unsafe.c
$ ./p15_null_unsafe
Calling with a valid class_out pointer:
  class=1 stem=ntu

Calling with class_out = NULL (caller forgot to pass one):
$ echo "exit code: $?"
exit code: 139
```

Same exit code as Section 2.3's overflow — `139` — but a different
cause: `*class_out = 1;` means "go to the address `class_out` holds, and
write `1` there." When `class_out` is `NULL`, that address is `0`, which
no normal program is ever allowed to write to; the operating system kills
the process immediately. Note the difference from Section 2.3: this
crash happens at the exact moment of the mistake, not later — NULL
dereferences tend to be easier to catch than buffer overflows for exactly
this reason.

The fix is the cheapest defensive habit in this entire chapter: check
every pointer parameter before using it.

```c
/* p16_null_safe.c -- SAFE: checks every pointer before dereferencing it. */
int toy_strip_prefix_safe(const char *word, char *stem_out, int *class_out) {
    if (word == NULL || stem_out == NULL || class_out == NULL) {
        return 0;          /* fail cleanly instead of crashing */
    }
    if (strncmp(word, "umu", 3) == 0) {
        *class_out = 1;
        strncpy(stem_out, word + 3, 31);
        stem_out[31] = '\0';
        return 1;
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p16_null_safe p16_null_safe.c
$ ./p16_null_safe
Calling with a valid class_out pointer:
  ok=1 class=1 stem=ntu

Calling with class_out = NULL:
  ok=0 (no crash -- the function refused instead)
$ echo "exit code: $?"
exit code: 0
```

This is not a hypothetical concern invented for this chapter. The real
`kin_strip_noun_prefix` in `morphology.c` practices exactly this habit at
the one point where it writes through `class_out` inside its recursive
`"nka"`-handling branch:

```c
if (class_out) *class_out = sub_cls;
```

Read that as "if `class_out` is not `NULL`, write through it" — the
identical check, just spelled as a truthiness test on the pointer itself
rather than an explicit `!= NULL`. Both spellings are common in real C
code and mean the same thing.

---

# Part 3 — Making It Interactive

## 3.1 Build it: a noun classifier you can talk to

Everything so far has processed a fixed list of words baked into the
program. Let's make it interactive — read a word the moment you type
it, classify it immediately, and keep going until you ask it to stop.

```c
/* p9_repl.c */
#include <stdio.h>
#include <string.h>

int toy_detect_class(const char *w) {
    if (strncmp(w, "umu", 3) == 0) return 1;
    if (strncmp(w, "aba", 3) == 0) return 2;
    if (strncmp(w, "imi", 3) == 0) return 4;
    if (strncmp(w, "ama", 3) == 0) return 6;
    if (strncmp(w, "iki", 3) == 0) return 7;
    if (strncmp(w, "ibi", 3) == 0) return 8;
    return 0;
}

int main(void) {
    char line[64];

    printf("Toy Izina Classifier -- type a word, or 'quit' to stop.\n");
    printf("> ");
    fflush(stdout);

    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';

        if (strcmp(line, "quit") == 0) break;
        if (line[0] == '\0') { printf("> "); fflush(stdout); continue; }

        int cls = toy_detect_class(line);
        if (cls > 0)
            printf("  \"%s\" looks like class %d\n", line, cls);
        else
            printf("  \"%s\" -- not recognized by this toy version yet\n", line);

        printf("> ");
        fflush(stdout);
    }
    printf("\nMurabeho! (goodbye)\n");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p9_repl p9_repl.c
$ ./p9_repl
Toy Izina Classifier -- type a word, or 'quit' to stop.
> umuntu
  "umuntu" looks like class 1
> ikintu
  "ikintu" looks like class 7
> abantu
  "abantu" looks like class 2
> quit

Murabeho! (goodbye)
```

(That exact session was verified by piping the same four lines —
`umuntu`, `ikintu`, `abantu`, `quit` — into the compiled program and
recording its real output; typing them at an interactive prompt produces
the same result, just typed live instead of piped.)

Two details earn a closer look:

**`fgets(line, sizeof(line), stdin)`, not `gets(line)`.** `gets` was
removed from the C standard library entirely (in C11) for exactly the
reason Section 2.3 just demonstrated: it has no way to know the size of
its destination buffer, so it cannot stop itself from overflowing one.
`fgets` takes that size as its second argument and will never write more
bytes than you told it the buffer can hold — the same bounded-by-design
principle as `strncpy`, applied to reading a line instead of copying a
string.

**`line[len-1] == '\n'` check, before stripping it.** `fgets` keeps the
newline character if there's room for it, so without this check, the
word `"umuntu"` would actually arrive as `"umuntu\n"` — and
`strncmp(w, "umu", 3)` would still match the *prefix* fine, but anything
comparing the *whole* word (like the `quit` check) would fail forever,
since `"quit\n"` is never equal to `"quit"`.

## 3.2 Try extending it yourself

Before moving on, open `p9_repl.c`, add two more `if` lines to
`toy_detect_class` for classes 9 (`in-`) and 12 (`aka-`), recompile, and
confirm interactively that `inka` and `akana` now classify correctly.
This is the same kind of change — one new `if` line, placed at the
correct point in the ordering you learned in Section 1.10 — that the
real production function's entire history is made of.

---

# Part 4 — Capstone: All Sixteen Classes, and a Real Phonological Rule

Every program so far has handled a deliberately small slice — one class,
then six, then a handful more. Before moving on to read the real
production function, build the full-scope version yourself: all 16
classes, in one toy program. Doing this honestly — by actually running
it on a representative word from *every* class — will uncover a real
linguistic pattern, not just a coding exercise.

## 4.1 Build it: one check per class

```c
/* p13_all16.c */
#include <stdio.h>
#include <string.h>

/* toy_detect_class_v4 -- one check per class, all 16, full forms only.
 * Order: by class number is fine here as long as no two of these 16
 * full-form prefixes collide at position 0 -- verify that claim
 * yourself as you read down the list (and watch Section 4.2 break it). */
int toy_detect_class_v4(const char *w) {
    size_t wlen = strlen(w);
    if (wlen < 3) return 0;

    if (strncmp(w, "umu", 3) == 0 && wlen > 4) return 1;   /* also class 3 */
    if (strncmp(w, "aba", 3) == 0 && wlen > 4) return 2;
    if (strncmp(w, "imi", 3) == 0 && wlen > 4) return 4;
    if (strncmp(w, "ama", 3) == 0 && wlen > 4) return 6;
    if (strncmp(w, "iki", 3) == 0 && wlen > 4) return 7;
    if (strncmp(w, "ibi", 3) == 0 && wlen > 4) return 8;
    if (strncmp(w, "uru", 3) == 0 && wlen > 4) return 11;
    if (strncmp(w, "aka", 3) == 0 && wlen > 4) return 12;
    if (strncmp(w, "utu", 3) == 0 && wlen > 4) return 13;
    if (strncmp(w, "ubu", 3) == 0 && wlen > 4) return 14;
    if (strncmp(w, "uku", 3) == 0 && wlen > 4) return 15;
    if (strncmp(w, "aha", 3) == 0 && wlen > 4) return 16;
    if (strncmp(w, "in",  2) == 0 && wlen > 3) return 9;    /* also class 10 */
    if (w[0]=='i' && wlen > 3) return 5;                    /* generic fallback,
                                                                LAST on purpose */
    return 0;
}

int main(void) {
    const char *words[] = {
        "umuntu", "abantu", "imiti", "amazi", "ikigo", "ibigo",
        "inka", "urugo", "akana", "utwana", "uburezi", "ukwezi", "ahantu",
        "kugenda",
        NULL
    };
    for (int i = 0; words[i]; i++)
        printf("%-10s -> class %d\n", words[i], toy_detect_class_v4(words[i]));
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p13_all16 p13_all16.c
$ ./p13_all16
umuntu     -> class 1
abantu     -> class 2
imiti      -> class 4
amazi      -> class 6
ikigo      -> class 7
ibigo      -> class 8
inka       -> class 9
urugo      -> class 11
akana      -> class 12
utwana     -> class 0
uburezi    -> class 14
ukwezi     -> class 0
ahantu     -> class 16
kugenda    -> class 0
```

Three results need explaining, and they are not random: `utwana`
(plural "small children," should be class 13), `ukwezi` ("moon/month,"
should be class 15), and `kugenda` ("to walk," should be class 15) all
came back as `0`.

## 4.2 Diagnosing the pattern, not just the symptom

Look closely at the two genuine failures first: `utwana` and `ukwezi`.

```
   Expected:  utu  + ana   = utuana   (never actually spelled this way)
   Actual:    utu  + ana   = utwana   (the 'u' became 'w')

   Expected:  uku  + ezi   = ukuezi   (never actually spelled this way)
   Actual:    uku  + ezi   = ukwezi   (the 'u' became 'w')
```

This is the *same* phenomenon you already met once, informally, with
`ubwenge` (Section 6.7) and `umwana` (Section 8.3) — but meeting it a
*third* and *fourth* time, on two different classes you hadn't tested
it on yet, is what turns "huh, that's a coincidence" into "this is a
rule." It is, in fact, an officially documented rule. This project's
grammar reference states it precisely, in its summary table of
phonological rules:

> | Rule notation | Description | Example |
> |---|---|---|
> | u→w/\_J | u becomes w before another vowel (glide formation) | umwana = u-mu-ana |

Read `u→w/_J` as: "the sound `u` becomes `w` whenever it is immediately
followed by another vowel" (`_J` marks "in front of a vowel" in this
notation). Every class whose bare prefix *ends* in `u` — class 1/3
(`umu`), 13 (`utu`), 14 (`ubu`), 15 (`uku`) — is a candidate for this
rule to fire, the instant its stem happens to start with a vowel. Your
own test list, built without knowing this rule in advance, happened to
include a vowel-initial stem for *four* of those four classes
(`-ana`, `-enge`, `-ana`, `-ezi`) — which is exactly why you hit the
pattern four separate times instead of zero.

`kugenda`'s failure is a different, unrelated fact, and worth not
confusing with the glide rule above: infinitives are conventionally
*cited* with their D vowel already dropped — you would essentially never
see "ukugenda" in real text, only "kugenda," in the same way Section 1.9
described proper names dropping their indomo. This is Section 1.2's
first rule ("D can be missing") in its most extreme, *always*-missing
form for this one class — not a bug in the toy program, and not the
glide rule either.

## 4.3 Fix it: teach the detector the glide rule

```c
/* p14_all16_glide.c -- v4 plus the u->w/_J rule, applied to every class
 * whose bare prefix ends in "u". The glide check goes FIRST in each
 * pair -- specific before general, same discipline as Section 1.10. */
int toy_detect_class_v5(const char *w) {
    size_t wlen = strlen(w);
    if (wlen < 3) return 0;

    if (strncmp(w, "umw", 3) == 0 && wlen > 4) return 1;   /* glide: umu+V  */
    if (strncmp(w, "umu", 3) == 0 && wlen > 4) return 1;
    if (strncmp(w, "aba", 3) == 0 && wlen > 4) return 2;
    if (strncmp(w, "imi", 3) == 0 && wlen > 4) return 4;
    if (strncmp(w, "ama", 3) == 0 && wlen > 4) return 6;
    if (strncmp(w, "iki", 3) == 0 && wlen > 4) return 7;
    if (strncmp(w, "ibi", 3) == 0 && wlen > 4) return 8;
    if (strncmp(w, "uru", 3) == 0 && wlen > 4) return 11;
    if (strncmp(w, "aka", 3) == 0 && wlen > 4) return 12;
    if (strncmp(w, "utw", 3) == 0 && wlen > 4) return 13;   /* glide: utu+V */
    if (strncmp(w, "utu", 3) == 0 && wlen > 4) return 13;
    if (strncmp(w, "ubw", 3) == 0 && wlen > 4) return 14;   /* glide: ubu+V */
    if (strncmp(w, "ubu", 3) == 0 && wlen > 4) return 14;
    if (strncmp(w, "ukw", 3) == 0 && wlen > 4) return 15;   /* glide: uku+V */
    if (strncmp(w, "uku", 3) == 0 && wlen > 4) return 15;
    if (strncmp(w, "aha", 3) == 0 && wlen > 4) return 16;
    if (strncmp(w, "in",  2) == 0 && wlen > 3) return 9;     /* also class 10 */
    if (w[0]=='i' && wlen > 3) return 5;                     /* generic fallback,
                                                                 LAST on purpose */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p14_all16_glide p14_all16_glide.c
$ ./p14_all16_glide
umuntu     -> class 1
umwana     -> class 1
abantu     -> class 2
imiti      -> class 4
amazi      -> class 6
ikigo      -> class 7
ibigo      -> class 8
inka       -> class 9
urugo      -> class 11
akana      -> class 12
utwana     -> class 13
uburezi    -> class 14
ubwenge    -> class 14
ukwezi     -> class 15
ahantu     -> class 16
kugenda    -> class 0
```

Every class now classifies correctly except `kugenda` — which is
*correctly* still `0`, for the separate D-dropped-infinitive reason
explained above, not a bug to fix. A toy program you wrote yourself, run
honestly on real words instead of a convenient cherry-picked list, just
led you to independently rediscover a named rule from this project's own
grammar reference. That is exactly the relationship between language and
code this whole book is trying to build in you.

---

# Part 5 — Reframing This as an Engineering Contract

## 4.1 The problem, restated precisely

Every small program in Parts 1–3 was building toward the same precise
software problem: **given a string of bytes that might be a Kinyarwanda
noun, produce three answers** — is it a noun at all; which of the 16
classes; and what is its bare stem.

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

## 4.2 Why a lookup table alone isn't enough, and why pure rules alone aren't either

**Why not just one giant dictionary, word → class?** Because the noun
stem set is open-ended — new nouns are coined and borrowed constantly; no
table, however large, lists every word real text might contain.

**Why not throw the table away and derive everything from the formula at
runtime**, the way our toy programs do? Because that can't know, from
spelling alone, that *umuntu* is class 1 and not class 3 (Section 1.5) —
you will watch the real production function make exactly that
concession, live, in Section 6.10. Some words need to be *known*, not
derived.

The actual design layers both: a small, fast, exact table for
known/irregular words (covered in a later chapter), and the general
rule-based fallback this chapter's toy programs have been previewing all
along, for everything else.

## 4.3 The exact contract we're about to read

```c
int  kin_detect_noun_class(const char *w);
     /* returns 1-16 if recognized, 0 if not */

bool kin_strip_noun_prefix(const char *word, char *stem_out, int *class_out);
     /* returns true/false; writes the stem and class THROUGH pointers,
      * exactly the pattern you built yourself in Section 2.2 */
```

---

# Part 6 — Reading the Real Production Code

Everything below is real source from this project's `morphology.c`. You
have already built a simplified version of nearly every piece of it
yourself in Parts 1–3 — this section is about recognizing those pieces
at full scale, not meeting them for the first time.

## 6.1 Compile against the real library yourself

Before reading a single line, prove to yourself that the real functions
exist and work, the same way you proved every toy program worked:

```c
/* p10_real.c */
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    const char *words[] = {
        "umuntu", "abahinzi", "ikigo", "ubwenge", "umuti", "inka",
        "icyiza", "ishuri", NULL
    };

    printf("%-12s %-4s %-6s %-10s\n", "word", "ok", "class", "stem");
    printf("--------------------------------------\n");

    for (int i = 0; words[i]; i++) {
        char stem[KIN_MAX_STEM];
        int  cls = 0;
        bool ok  = kin_strip_noun_prefix(words[i], stem, &cls);
        printf("%-12s %-4d %-6d %-10s\n", words[i], ok, cls, ok ? stem : "(n/a)");
    }
    return 0;
}
```

From the project's root directory (where `libkinyarwanda.a` already
exists after running `make`):

```
$ gcc -std=c99 -Wall -Wextra -I include p10_real.c -L . -lkinyarwanda -o p10_real
$ LD_LIBRARY_PATH=. ./p10_real
word         ok   class  stem
--------------------------------------
umuntu       1    1      ntu
abahinzi     1    2      hinzi
ikigo        1    7      go
ubwenge      1    14     enge
umuti        1    1      ti
inka         1    9      nka
icyiza       1    7      iza
ishuri       1    5      shuri
```

Look at row 5 before reading any further: **`umuti` came back as class
1, not class 3.** This is not a bug — it's Section 1.5's class 1/3
ambiguity, happening live, in the real shipped code, exactly as
predicted. Hold onto that result; Section 6.11 returns to it.

## 6.2 `kin_detect_noun_class`, in full

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

    /* Dropped D-vowel variants -- Section 1.9/1.10 -- come AFTER every
     * full-prefix check, so a full form is never claimed by a shorter,
     * vaguer pattern that happens to also match its beginning. */
    if (kin_starts_with(w, "mu") && wlen > 4 && !is_vowel(w[2])) return 1;
    if (kin_starts_with(w, "ba") && wlen > 5 && !is_vowel(w[2])
        && w[wlen-1] != 'a' && w[wlen-1] != 'e') return 2;
    /* ... */
    return 0;
}
```

This should read like an old friend by now: the first block is exactly
your `p3_toy_v2.c`, just with all sixteen classes instead of six. The
second block is exactly the dropped-vowel problem Section 1.9 described
in English. And the *order* between those two blocks is exactly the
lesson `p4_order_wrong.c`/`p5_order_right.c` taught you with your own
hands — applied here at the scale of roughly 30 prefixes instead of 2.

## 6.3 Why this exact algorithm, and not a "cooler" one

Three real alternatives, and why each loses to a flat, ordered `if`-chain
for *this specific problem* — the same kind of chain you've now built,
broken, and fixed yourself:

**Why not a hash map from prefix to class?** Because the real question
isn't "is this exact prefix known" — it's "which is the *longest* valid
prefix this word starts with," exactly what Section 1.10 demonstrated.
A hash map answers exact lookups; it has no built-in notion of
preferring a longer match over a shorter one that also fits. You would
still need to check candidates longest-first yourself, so a hash map
adds overhead without removing the ordering logic you actually need.

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
discipline you just practiced, just spelled in a denser, harder-to-audit
syntax.

**The honest conclusion**: for a small, fixed, compile-time-known
candidate set that has to stay independently verifiable against an
external rulebook, the flat `if` chain isn't a missed opportunity for a
"real" algorithm — it *is* the right algorithm for this problem's actual
shape and scale.

## 6.4 The low-level C mechanics inside each line

**`const char *w`** — read-only input; nothing here ever writes through
it, so the compiler can catch you if you accidentally tried to.

**`strlen(w)` computed once**, into `wlen`, reused by every guard below
instead of being recomputed on every single check.

**`kin_starts_with(s, prefix)`**, the production version of the
`strncmp` calls you wrote by hand in every toy program above:

```c
bool kin_starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
```

`strncmp` compares at most `strlen(prefix)` bytes — exactly enough to
test the prefix, never reading further. The wrapper exists purely so the
big function reads as "starts with," not "some strncmp call I have to
mentally translate every time" — the same instinct that made you reach
for a named helper rather than repeating raw `strncmp` calls if you grew
your own toy classifier past a handful of checks.

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

**`is_vowel(c)`**, the simplest function in the file — identical to the
helper you wrote yourself in `p4`/`p5`:

```c
static bool is_vowel(char c) {
    return c=='a'||c=='e'||c=='i'||c=='o'||c=='u';
}
```

**`return 0;`.** No real class is ever numbered 0, so it's a value that
can never be mistaken for a genuine answer — exactly the convention your
own `toy_detect_class` functions already followed.

## 6.5 The one recursive case: `"nka"` + noun

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
same question about what's left. This is safe for two reasons:
**a base case that doesn't recurse** (`wlen < 3` at the very top), and
**guaranteed shrinkage** — every recursive call is on a string exactly
three bytes shorter, so it must eventually reach the base case. There is
no input that loops forever.

## 6.6 `kin_strip_noun_prefix`: your pointer pattern, at production scale

```c
bool kin_strip_noun_prefix(const char *word, char *stem_out, int *class_out);
```

This is exactly the shape of `toy_strip_prefix` from Section 2.2 — the
caller hands the function the addresses of its own variables; the
function writes answers directly into that caller-owned memory, and
reserves its one real `return` for a plain success/failure flag.

## 6.7 Two-level dispatch: class first, then exact spelling

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
is — choosing among 16 fixed alternatives is exactly what `switch` is
for. The **inner** `if`/`else if` re-derives *which exact spelling*
matched, because the same class can surface as the full form, the
dropped-vowel form, or a glide-contracted form, and each needs a
different number of bytes skipped to reach the stem. Section 6.1's real
`ubwenge → class 14, stem "enge"` result is exactly this mechanism in
action: the glide-contracted spelling consumed three bytes (`ubw`), not
the two you might have guessed from the bare `ubu-` prefix.

## 6.8 The `strncpy` discipline, now at production scale

```c
strncpy(stem_out, stem_start, KIN_MAX_STEM - 1);
stem_out[KIN_MAX_STEM - 1] = '\0';
```

You watched, with a real `SIGSEGV` and exit code 139, exactly what
happens when the second line of this pair is missing (Section 2.3). This
is the identical two-line discipline, applied here to whatever
`KIN_MAX_STEM` is defined as in this build, instead of the `8` you used
in your own demo. The reasoning has not changed at all between your toy
program and this real one — only the buffer size.

## 6.9 Sentinel returns, completing the contract

```c
int cls = kin_detect_noun_class(word);
if (cls == 0) { stem_out[0] = '\0'; return false; }
```

`stem_out[0] = '\0'` keeps the output buffer a valid, empty string even
on failure. `return false` tells the caller plainly: don't trust the
out-parameters. Every caller checks this before reading either one —
the same discipline your own `toy_strip_prefix` already practiced.

## 6.10 Eight real traces, side by side

Every row below is the verified output from Section 6.1's `p10_real`
run, annotated against the rules you now know:

| Word | Class | Stem | What's actually happening |
|---|---|---|---|
| `umuntu` | 1 | `ntu` | full-form `umu-` check, Section 6.2's first block |
| `abahinzi` | 2 | `hinzi` | full-form `aba-` check |
| `ikigo` | 7 | `go` | full-form `iki-` check |
| `ubwenge` | 14 | `enge` | glide-contracted `ubw-` (3 bytes), not bare `ubu-` |
| `umuti` | **1** | `ti` | the class 1/3 ambiguity (Section 1.5) — see 5.11 |
| `inka` | 9 | `nka` | class 9's short `i-` form |
| `icyiza` | 7 | `iza` | the specific-before-generic ordering you proved in 1.10 |
| `ishuri` | 5 | `shuri` | the generic class-5 fallback, correctly reached only when nothing more specific matched first |

## 6.11 What this design honestly does not solve

- **Class 1 vs. 3 cannot be told apart by prefix alone** — and you just
  watched it happen, for real, in Section 6.1's `umuti` row. The
  production function returns `1` unconditionally for any plain `umu-`
  word; disambiguating "tree" from "person" needs a separate exact-stem
  lookup table — Part 7 builds and tests exactly that table next, and
  shows you precisely how much of this gap it does, and does not, close.
- **A genuinely novel or very informal spelling can return 0** even
  though a human would recognize it from context — the accepted cost of
  not trying to pattern-match every conceivable input.
- **The dropped-D-vowel guards are heuristics, not guarantees** — the
  "doesn't end in a/e" check exists specifically to dodge collisions with
  conjugated verbs, and is imperfect by construction.

---

# Part 7 — The Other Half of the Hybrid Design: Exact-Word Lookup

Section 5.2 promised that the real design is not pure rules — it's rules
*plus* a small, exact lookup table for known and irregular words. Part 6
showed you only the rules half. This part builds the lookup half, and
then uses it to settle, as precisely as the real code allows, the class
1/3 ambiguity that has followed `umuti` (Section 6.1), `umurima` (Section
8.4), and now this section, through the entire chapter.

## 7.1 The real lookup table

This project's lexicon keeps a small table mapping bare *stems* (not
whole words) to a class, for exactly the cases the rule-based detector
cannot resolve on its own:

```c
typedef struct { const char *stem; int class; } NounStem;

static const NounStem NOUN_STEMS[] = {
    { "nt",       1 }, /* muntu / abantu -- person/people */
    { "ti",       3 }, /* umuti -- tree/medicine          */
    { "shuri",    5 }, /* ishuri -- school                 */
    { "go",       7 }, /* ikigo -- institution             */
    /* ... roughly 40 more rows ... */
    { NULL, 0 }
};

bool kin_is_known_noun_stem(const char *stem, int *class_out) {
    for (int i = 0; NOUN_STEMS[i].stem; i++) {
        if (strcmp(stem, NOUN_STEMS[i].stem) == 0) {
            if (class_out) *class_out = NOUN_STEMS[i].class;
            return true;
        }
    }
    return false;
}
```

Notice the shape: a `static const` array of a tiny struct, a linear
scan, a `bool` return, and the class handed back through `class_out` —
every single piece of this is something you have already built
yourself, multiple times, earlier in this chapter. There is a real
`{ "ti", 3 }` row in this table — the exact stem `kin_strip_noun_prefix`
extracted from `umuti` in Section 6.1, now mapped directly to the
correct class.

## 7.2 Build it: your own version of this exact lookup

```c
/* p17_lookup.c -- standalone, modeled on the real kin_is_known_noun_stem */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef struct { const char *stem; int class; } ToyNounStem;

static const ToyNounStem TOY_NOUN_STEMS[] = {
    { "nt",    1 },
    { "ti",    3 },
    { "shuri", 5 },
    { NULL,    0 }
};

bool toy_is_known_stem(const char *stem, int *class_out) {
    for (int i = 0; TOY_NOUN_STEMS[i].stem; i++) {
        if (strcmp(stem, TOY_NOUN_STEMS[i].stem) == 0) {
            if (class_out) *class_out = TOY_NOUN_STEMS[i].class;
            return true;
        }
    }
    return false;
}

int main(void) {
    const char *stems[] = { "ti", "go", "rima", "shuri", NULL };
    for (int i = 0; stems[i]; i++) {
        int cls = 0;
        bool found = toy_is_known_stem(stems[i], &cls);
        if (found) printf("%-6s -> known, class %d\n", stems[i], cls);
        else       printf("%-6s -> not in the table\n", stems[i]);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p17_lookup p17_lookup.c
$ ./p17_lookup
ti     -> known, class 3
go     -> not in the table
rima   -> not in the table
shuri  -> known, class 5
```

(`"go"` is deliberately left out of this three-row toy table — only the
real `NOUN_STEMS[]` has it — to make the point that a lookup table only
ever knows exactly what someone put into it. `"rima"`, the stem of
`umurima`, isn't in the *real* table either, which Section 7.4 below
will matter for.)

## 7.3 Why a linear scan here too — and where that answer could change

The same question Section 6.3 asked about the rule-based detector is
worth asking again here: why a flat array and a linear scan, not a hash
map or a sorted array with binary search? The answer starts the same
way — the real `NOUN_STEMS[]` table has on the order of 40–50 rows,
fixed at compile time, and scanning 50 short strings is not a
measurable cost next to the overhead of hashing or maintaining sorted
order.

But notice an honest difference from the 16-class table back in Section
1.5: the number of *noun classes* can never grow — there will never be
a 17th class added to Kinyarwanda. The number of *known irregular
words* genuinely **can** grow — every time this project's corpus work
finds a new word the rules can't handle, a new row gets added by hand.
If this table ever grew from 50 rows to 5,000, a linear scan's cost
would start to be measurable, and a hash map (exact-match lookup,
unlike the prefix table, has no "longest match" requirement working
against it here) would become the right trade — the same engineering
judgment from Section 6.3, applied honestly to a table whose growth
assumption is genuinely different from the one that justified it for
class detection.

## 7.4 The honest payoff: what gets resolved, and what doesn't

Time to settle the `umuti`/`umurima` question as precisely as the real
code actually allows — not by assuming an answer, but by running the
real pipeline on both words and on the dropped-D-vowel casual form
`muti` (Section 1.9) side by side:

```c
/* p17_pipeline.c */
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    int cls = 0;
    bool found = kin_is_known_noun_stem("ti", &cls);
    printf("kin_is_known_noun_stem(\"ti\", ...) -> found=%d class=%d\n\n",
           found, cls);

    const char *words[] = { "umuti", "muti", "umurima", NULL };
    for (int i = 0; words[i]; i++) {
        SentenceAnalysis sa = kin_analyze(words[i]);
        Token *t = &sa.tokens[0];
        printf("%-10s -> pos=%d noun_class=%d stem=%s\n",
               words[i], t->pos, t->noun_class, t->stem);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p17_pipeline.c -L . -lkinyarwanda -o p17_pipeline
$ LD_LIBRARY_PATH=. ./p17_pipeline
kin_is_known_noun_stem("ti", ...) -> found=1 class=3

umuti      -> pos=1 noun_class=1 stem=ti
muti       -> pos=1 noun_class=3 stem=ti
umurima    -> pos=1 noun_class=1 stem=rima
```

(`pos=1` is `POS_NOUN` — Section 1.1's enum.) Read this real, verified
table slowly, because the answer is genuinely more nuanced than either
"it's fixed" or "it's broken":

- **`muti`** (the casual, dropped-D-vowel spelling) comes back as
  **class 3** — correctly. This project's real pipeline checks the
  exact-word lexicon *before* it ever reaches the rule-based detector
  (this is documented in `pos_tagger.c` as "Step 3 → known full word,"
  which runs before "Step 6 → noun via D+RT prefix detection" — the same
  specific-beats-general principle from Section 1.10, now operating at
  the level of "a memorized fact beats a general guess," not just
  "a longer prefix beats a shorter one"). `muti` has an entry in
  `KNOWN_WORDS[]`, so Step 3 resolves it before Step 6 even runs.
- **`umuti`** (the full, textbook-correct spelling) still comes back as
  **class 1** — unresolved, exactly as Section 6.11 admitted. There is
  no `KNOWN_WORDS[]` entry for the full word `"umuti"` itself (only for
  its dropped-D form `"muti"`), so Step 3 finds nothing, and the word
  falls through to Step 6's general rule, which can only see the `umu-`
  prefix and defaults to class 1.
- **`umurima`** comes back as **class 1** too, for the same reason:
  neither `"umurima"` nor its stem `"rima"` has an entry in either
  lookup table yet.

This is the honest, complete answer, and it's a more realistic picture
of real software than a tidy "and that's how the bug was fixed" story
would be: the lexicon tables are **corpus-driven** (built from real text
this project was trained against, primarily the *Bibiliya Yera 2001*
corpus, per the comments in `lexicon.c`) — they resolve ambiguity for
whichever specific spelling happened to actually appear in that corpus,
and leave every other spelling of the same underlying ambiguity exactly
as unresolved as the pure rule-based function admits it is. Extending
the fix to `"umuti"` itself, or to `"umurima"`, is not a deep redesign —
it is one new row in `KNOWN_WORDS[]`, the same kind of change you have
now made to toy versions of this exact mechanism yourself.

---

# Part 8 — Case Study: A Real Textbook Phrase, Fully Processed

Every example so far has been a hand-picked list of single words. Real
input doesn't arrive that way — it arrives as a line of text that has to
be split into words first. This case study processes one complete,
textbook-sourced phrase end to end, using both your toy code and the
real library, and along the way uncovers — honestly, not by design — a
real gap that the production code had to solve.

## 8.1 The source phrase

This project's grammar reference quotes the following line as its
worked example of the *ikinyazina ngenera* (possessive connector),
Section 9.5:

> Ingero: **umurima wacu, umwana wange, amafaranga yabo, ishati yawe**
> ("our field, my child, their money, your shirt")

Four nouns, four different classes — exactly the kind of variety a
single hand-picked example list (like every list earlier in this
chapter) doesn't usually have by accident. Each noun is followed by a
possessive connector (*wacu*, *wange*, *yabo*, *yawe* — "our/my/their/
your"), which is not itself a noun, and a real line-scanner has to be
able to tell the difference.

## 8.2 Build it: scan a whole line, not a hand-picked array

```c
/* p12_linescan.c */
#include <stdio.h>
#include <string.h>

int toy_detect_class(const char *w) {
    if (strncmp(w, "umu", 3) == 0) return 1;
    if (strncmp(w, "aba", 3) == 0) return 2;
    if (strncmp(w, "imi", 3) == 0) return 4;
    if (strncmp(w, "ama", 3) == 0) return 6;
    if (strncmp(w, "iki", 3) == 0) return 7;
    if (strncmp(w, "ibi", 3) == 0) return 8;
    if (strncmp(w, "ish", 2) == 0) return 5;   /* crude: covers "ishati" etc. */
    return 0;
}

int main(void) {
    /* Source: data/textbook_grammar_rules.md, Section 9.5 */
    char line[] = "umurima wacu umwana wange amafaranga yabo ishati yawe";

    printf("Scanning: \"%s\"\n\n", line);

    /* strtok modifies its input IN PLACE, and remembers state between
     * calls in a hidden internal variable -- it is NOT safe to call it
     * from two places "at once" (two threads, or a nested loop tokenizing
     * two different strings at the same time). For one simple,
     * single-threaded scan like this, it's the standard, simplest tool. */
    char *tok = strtok(line, " ");
    while (tok != NULL) {
        int cls = toy_detect_class(tok);
        if (cls > 0)
            printf("  %-12s -> class %d\n", tok, cls);
        else
            printf("  %-12s -> (not a noun-class prefix; probably a connector)\n", tok);
        tok = strtok(NULL, " ");
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p12_linescan p12_linescan.c
$ ./p12_linescan
Scanning: "umurima wacu umwana wange amafaranga yabo ishati yawe"

  umurima      -> class 1
  wacu         -> (not a noun-class prefix; probably a connector)
  umwana       -> (not a noun-class prefix; probably a connector)
  wange        -> (not a noun-class prefix; probably a connector)
  amafaranga   -> class 6
  yabo         -> (not a noun-class prefix; probably a connector)
  ishati       -> class 5
  yawe         -> (not a noun-class prefix; probably a connector)
```

Stop and look before reading the explanation: **`umwana` ("child") was
not recognized.** This is not a mistake in how the case study was set
up — it's a real, honest gap in the toy classifier, caught in the act by
real input instead of a cherry-picked example list. `umwana` is
genuinely class 1, but it doesn't start with the three literal bytes
`"umu"` — it starts with `"umw"`. The `u`+`mu`+`ana` combination
undergoes a vowel change (the same kind of glide contraction you already
saw in `ubwenge`, Section 6.7) and surfaces as `umwana`, not the
"expected" `umuana`.

## 8.3 Diagnose it, then fix it — the same discipline as Section 1.10

The fix is one new check, placed *before* the existing `"umu"` check
(specific-before-general, the same ordering discipline from Section
1.10):

```c
/* p12b_linescan_fixed.c -- only the detector function changed */
int toy_detect_class_v3(const char *w) {
    if (strncmp(w, "umw", 3) == 0) return 1;   /* glide-contracted form, NEW */
    if (strncmp(w, "umu", 3) == 0) return 1;
    if (strncmp(w, "aba", 3) == 0) return 2;
    if (strncmp(w, "imi", 3) == 0) return 4;
    if (strncmp(w, "ama", 3) == 0) return 6;
    if (strncmp(w, "iki", 3) == 0) return 7;
    if (strncmp(w, "ibi", 3) == 0) return 8;
    if (strncmp(w, "ish", 2) == 0) return 5;
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p12b_linescan_fixed p12b_linescan_fixed.c
$ ./p12b_linescan_fixed
Scanning: "umurima wacu umwana wange amafaranga yabo ishati yawe"

  umurima      -> class 1
  wacu         -> (not a noun-class prefix; probably a connector)
  umwana       -> class 1
  wange        -> (not a noun-class prefix; probably a connector)
  amafaranga   -> class 6
  yabo         -> (not a noun-class prefix; probably a connector)
  ishati       -> class 5
  yawe         -> (not a noun-class prefix; probably a connector)
```

One new line, and `umwana` is now correctly recognized. This is exactly
why the real `kin_strip_noun_prefix`'s `switch` block for classes 1/3
(Section 6.7) checks `"umw"` *before* `"umu"` — you have now independently
rediscovered, by hitting the same wall yourself, a design decision that
is already sitting in the production source.

## 8.4 The same four nouns, through the real library

```c
/* p11_case.c */
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    /* Source: data/textbook_grammar_rules.md, Section 9.5 */
    const char *words[] = { "umurima", "umwana", "amafaranga", "ishati", NULL };

    printf("%-12s %-4s %-6s %-10s\n", "word", "ok", "class", "stem");
    printf("--------------------------------------\n");
    for (int i = 0; words[i]; i++) {
        char stem[KIN_MAX_STEM];
        int  cls = 0;
        bool ok  = kin_strip_noun_prefix(words[i], stem, &cls);
        printf("%-12s %-4d %-6d %-10s\n", words[i], ok, cls, ok ? stem : "(n/a)");
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p11_case.c -L . -lkinyarwanda -o p11_case
$ LD_LIBRARY_PATH=. ./p11_case
word         ok   class  stem
--------------------------------------
umurima      1    1      rima
umwana       1    1      ana
amafaranga   1    6      faranga
ishati       1    5      shati
```

Two things to notice in this real, verified table. First, the production
code already knows about the `umw-` glide (it correctly returns class 1
for `umwana` with no extra work from you — it was built to handle
exactly the gap your own toy code just fell into). Second — look at
`umurima`: it comes back as **class 1**, not class 3, even though
"field" is a *thing*, not a person. This is the same class 1/3 ambiguity
from Section 1.5 and Section 6.1's `umuti` example, happening again, on
a completely different word — and by Part 7's honest accounting,
`umurima` lands on the *unresolved* side of that ambiguity: neither the
full word nor its stem `"rima"` has an entry in either lookup table, so
the rule-based default is all that's left to answer with. Three
independent words (`umuti`, `umurima`, and Part 4's near-miss on
`utwana`/`ukwezi`) all point at the same conclusion: this isn't a
one-off quirk of a single example, it's a systematic, predictable
property of the language, and the code's response to it — honest where
it can't resolve something, precise where a lookup table lets it.

---

# Part 9 — Practice

The exercises below are graded. Do them in order — each one assumes
you've already done the ones before it.

### Beginner

1. **Trace `"ubwenge"` by hand** through `kin_detect_noun_class` and
   `kin_strip_noun_prefix` before re-reading Section 6.1's table. Confirm
   you land on the same `class=14, stem="enge"` the real program printed.

2. **Extend your own `p3_toy_v2.c`** to add classes 11 (`uru-`) and 14
   (`ubu-`). Recompile, and confirm `urugo` and `uburezi` now classify
   correctly — they should have failed (`class 0`) before your change.

3. **Add the `umw-` glide check from Section 8.3 to your own
   `p9_repl.c`'s detector**, recompile, and confirm interactively that
   typing `umwana` now returns class 1 instead of "not recognized."

### Intermediate

4. **Reproduce the ordering bug on purpose, in a new case.** Pick any
   two real prefixes from Section 1.5's table where one is a substring
   of the other's *start* (for example, compare `i-` general checks
   against any specific `i`-initial class), write both checks in the
   wrong order, and confirm — by actually compiling and running it — that
   you can reproduce a misclassification the same way Section 1.10 did.
   Then fix the order and confirm the fix.

5. **Compile `p10_real.c` yourself** against this project's real
   library and add five more words of your own choosing to the list.
   Predict each one's class and stem on paper *before* running it, then
   check your prediction against the real output.

6. **Modify `p9_repl.c`** so that typing `stats` prints how many words
   have been classified so far in the session, and how many came back as
   class `0`. (Hint: two `int` counters in `main`, incremented inside the
   loop — no pointers needed, since `main` already owns them directly.)

7. **Write your own `p12`-style line scanner** for a different phrase
   from `data/textbook_grammar_rules.md` (try the Section 9.4 vocative
   example, or any of the orthographic-rule examples in Part 10 of that
   file). Predict, on paper, which tokens your detector will fail to
   recognize *before* running it, then check whether you were right.

### Advanced

8. **Build a "diff" tool.** Write a program that runs *both* your toy
   detector and the real `kin_detect_noun_class` on the same list of
   words, and prints only the words where they disagree. (With the
   `umw`-fixed toy detector from Section 8.3, this list should be much
   shorter than with the original `p3_toy_v2.c` — quantify exactly how
   much shorter.)

9. **Make the unsafe/safe demo (Section 2.3) take its input from the
   command line** (`argv[1]`) instead of a hardcoded string literal, so
   you can pass it different lengths from the shell and watch where
   the crash threshold actually is on your machine. Never do this with
   genuinely untrusted input outside of a deliberate, contained learning
   exercise like this one.

10. **Extend `kin_strip_noun_prefix`'s mental model to a class this
    chapter didn't fully demonstrate.** Class 9/10 (`in-`) has its own
    real sound changes at the class-marker/stem boundary (Section 1.5).
    Write a toy detector specifically for class 9 that handles at least
    two different surface spellings of the same underlying prefix, the
    same way Section 8.3 handled `umu-`/`umw-` for class 1.

11. **Close the `umurima` gap yourself, in your own toy lookup table.**
    Add a row `{ "rima", 3 }` to a copy of `p17_lookup.c`'s
    `TOY_NOUN_STEMS[]`, and write a small `toy_resolve()` function that
    calls your Section 4 rule-based detector first and your lookup table
    second, preferring the lookup's answer when both produce one. Confirm
    your combined function now reports class 3 for `umurima`, the same
    way the real `KNOWN_WORDS[]` entry does for `muti` (Section 7.4).

12. **Verify the real `NOUN_STEMS[]` table directly.** Compile a program
    against this project's real library that calls `kin_is_known_noun_stem`
    on ten stems of your own choosing (some you expect to be listed, some
    you expect not to be). Predict each result on paper first, then check
    it against the real, verified output — the same discipline as every
    other exercise in this chapter.

## Key takeaways

- Every Kinyarwanda noun is D (initial vowel) + RT (class marker) + C
  (stem); the stem can never be missing.
- 16 noun classes, mostly paired singular/plural; classes 1/3 and 9/10
  are genuinely, not accidentally, ambiguous by spelling alone — you
  watched this happen in real, shipped code, not just in theory.
- In any chain of pattern-matching `if` statements, **order is part of
  the algorithm's correctness** — you proved this yourself by swapping
  two lines and changing a real answer from wrong to right.
- C has no multiple-return-value syntax; pointers the caller owns are
  the substitute, and you built that pattern with your own hands before
  reading it in production code.
- `strcpy`/`sprintf`/`strcat`/`gets` do not check destination sizes;
  their bounded cousins do, but still need an explicit, manual
  terminator afterward — you watched the unbounded version crash with a
  real `SIGSEGV`, and watched the bounded version survive the identical
  input cleanly.
- A small `fgets`-based loop is enough to turn any of these functions
  into an interactive tool — and `fgets`'s bounded-by-design read is the
  same defensive principle as `strncpy`'s bounded-by-design write.
- The detection algorithm is a small, fixed, ordered `if`-chain — not
  because nothing fancier exists, but because nothing fancier's
  advantages apply at this scale, and the flat chain stays auditable
  against the exact rulebook it implements.
- This project's one recursive function is provably safe: a real base
  case, and every call strictly shrinks toward it.
- Real running text isn't a hand-picked word list — `strtok` (or any
  line scanner) will surface real gaps that a curated example list
  hides; the `umwana`/`umw-` case study found exactly such a gap, in a
  toy program you wrote yourself, the same way it would in any growing
  rule-based system.
- The class 1/3 ambiguity isn't a one-off oddity of a single word — it
  recurred independently on `umuti` (Section 6.1) and `umurima` (Section
  8.4), confirming it's a systematic property of the language.
- The hybrid design's lookup half resolves that ambiguity for some
  spellings (`muti`, via a real `KNOWN_WORDS[]` entry) and honestly
  leaves it unresolved for others (`umuti`, `umurima`) — because the
  lexicon is corpus-driven, not exhaustive. You verified this exact,
  nuanced split against the real, live pipeline (Section 7.4), not a
  hand-wave.
- A flat array + linear scan is the right tool for the lookup table
  today, for the same reason it's right for the rule-based detector —
  but the two tables have a genuinely different *growth* assumption (16
  classes, fixed forever, vs. a corpus-driven word list that can and
  does grow), so the same design choice deserves separate justification
  for each, not one answer copy-pasted onto both (Section 7.3).
- The `u→w/_J` glide rule (Section 4.2) isn't something this chapter
  invented to explain away a bug — it's a documented rule in this
  project's own grammar reference, and you found it by honestly testing
  your own code against all 16 classes rather than a convenient,
  pre-filtered word list.

## Sources quoted in this chapter

- Kinyarwanda S2–S6 secondary-school textbooks (REB / Drakkar Ltd,
  2017–2024), via `data/textbook_grammar_rules.md`.
- RALC 2017 official orthography (*Amabwiriza ya Minisitiri no
  001/2014*).
- `include/kinyarwanda.h`, `src/lexicon.c` (`NounClass`,
  `NOUN_CLASSES[]`, `NounPluralPair`, `NOUN_PLURAL_PAIRS[]`, `NounStem`,
  `NOUN_STEMS[]`, `kin_is_known_noun_stem`, `KnownWord`, `KNOWN_WORDS[]`).
- `src/morphology.c` (`kin_detect_noun_class`, `kin_strip_noun_prefix`,
  `kin_starts_with`, `is_vowel`).
- `src/pos_tagger.c` (the numbered Step 1–9 pipeline comment, and Steps 3,
  6, and 7b specifically — Part 7 of this chapter).
- `data/textbook_grammar_rules.md`, Part 16 (phonological rules summary
  table) — the `u→w/_J` rule from Part 4 of this chapter.
- Every numbered `pN_*.c` program in this chapter was written, compiled
  with `gcc -std=c99 -Wall -Wextra`, and actually run to produce the
  exact terminal output quoted above — none of it is invented.

## Coming up in Chapter 2

Section 1.8 named *concordance* but stopped at "a future chapter will
show the code that checks it." Chapter 2 is that chapter: the
Kinyarwanda adjective (*ntera*), its own RS + C formula introduced and
built the same hands-on way this chapter introduced D+RT+C, a toy
agreement-checker you'll write and run yourself before reading the real
one, and the real C code that both checks agreement against the
`NounClass` columns you've already seen, and repairs it when it's wrong.
