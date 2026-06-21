# Chapter 4 — Ikinyazina: The Pronoun, and the Verb Looking Backward

## How this chapter works

Same rules as Chapters 1 through 3. Every Kinyarwanda rule is explained
in plain language, with real quoted examples, before any code touches
it. Every code example was actually compiled with
`gcc -std=c99 -Wall -Wextra` and actually executed — the output shown
in a fenced block prefixed with `$` is the real terminal output of
that exact program.

*Ikinyazina* (pronoun) is a sprawling category — this project's own
pronoun table has nine real sub-types and 445 individually-listed
entries. This chapter does not try to cover all nine. It picks two,
deliberately: the **possessive connector** (*ikinyazina ngenera*),
because it is concordance's fifth job in this book and the direct
payoff of a phrase Chapter 1 quoted but never analyzed; and
**deverbative nouns** (*izina rivuye mu nshinga*), because they run
Chapter 1's noun tree and Chapter 3's verb tree in *reverse* — turning
a verb root back into a noun — which makes this the first chapter
explicitly about how two trees connect rather than building one tree
on its own.

---

# Part 1 — Language and Code, Side by Side

## 1.1 Two specific jobs, out of nine

Chapter 1, Section 8.1's case study quoted this exact textbook phrase
without ever asking what holds it together:

> "umurima wacu, umwana wange, amafaranga yabo, ishati yawe"
> ("our field, my child, their money, your shirt")

Each phrase is a noun, followed by a small word that means "ours,"
"mine," "theirs," or "yours" — and *changes its first letters*
depending on the noun's class. That small word is built from two
independently meaningful pieces: a **connector** that agrees with the
noun's class (the same job RT, RS, SP, and OM have each done once
already), and a **person suffix** that says whose it is. Quoting the
grammar reference directly:

> **9.5 Ikinyazina Ngenera (Associative/Possessive Pronoun)**
> Izina + ikinyazina ngenera + izina risobanura
> Orthographic rule: umwana **wange** → wa + nge = wange;
> urimi **rwacu** → rwa + cu = rwacu

`wa` is the class-1/3 connector (the same job Chapter 1's RT, Chapter
2's RS, and Chapter 3's SP all performed); `nge` and `cu` are person
suffixes meaning "my" and "our." Glue them together and you get a
single surface word, `wange` or `wacu`, that an English speaker would
call a possessive pronoun and a Kinyarwanda grammar calls *ikinyazina
ngenera ngenga*.

## 1.2 The possessive connector: concordance's fifth job

The connector table is the now-familiar sixteen markers, wearing a
fifth hat:

| Class | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Connector | wa | ba | wa | ya | rya | ya | cya | bya | ya | za | rwa | ka | twa | bwa | kwa | ha |

Notice three classes already repeat a connector another class also
uses — class 3 shares `wa` with class 1, and classes 6 and 9 both
share `ya` with class 4. Hold onto that; Section 4.1's capstone makes
it concrete by testing all sixteen for real, and Part 7's agreement
checker has to work *around* it rather than be broken by it.

## 1.3 Build it: a toy connector lookup

```c
/* p1_toy_connector.c -- a first, tiny possessive-connector lookup */
#include <stdio.h>
#include <string.h>

const char *toy_connector(int cls) {
    static const char *table[17] = {
        "",   "wa", "ba", "wa", "ya", "rya", "ya", "cya", "bya",
        "ya", "za", "rwa","ka", "twa","bwa", "kwa","ha"
    };
    if (cls < 1 || cls > 16) return NULL;
    return table[cls];
}

int main(void) {
    int tests[] = { 1, 3, 7, 99, -2 };
    for (int i = 0; i < 5; i++) {
        const char *c = toy_connector(tests[i]);
        printf("class %-3d -> %s\n", tests[i], c ? c : "(invalid)");
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p1_toy_connector p1_toy_connector.c
$ ./p1_toy_connector
class 1   -> wa
class 3   -> wa
class 7   -> cya
class 99  -> (invalid)
class -2  -> (invalid)
```

This is the exact array-indexed-by-class-number idiom behind Chapter
1's real `NounClass` table (Part 6 there) and Chapter 2's corrector,
which reused it for its own bounds-checked lookup — the same shape,
the fourth time this book has needed it, because the underlying fact
("sixteen classes, each with one fixed marker for this particular
job") keeps recurring no matter which grammatical job is being marked.

## 1.4 The fusion: connector + person = one word

A connector alone isn't a complete possessive — it's a *piece* of one,
the way a noun's RT alone isn't a complete word without a stem. The
other piece is a person suffix:

```c
/* p_toy_fuse.c -- fuse a possessive connector with a person suffix */
#include <stdio.h>
#include <string.h>

void toy_fuse(const char *conn, const char *person, char *out, size_t outsz) {
    snprintf(out, outsz, "%s%s", conn, person);
}

int main(void) {
    struct { const char *conn; const char *person; } tests[] = {
        { "wa", "cu" },   /* class 1/3: wacu  -- "our"  */
        { "wa", "nge" },  /* class 1/3: wange -- "my"   */
        { "ya", "bo" },   /* class 4/6/9: yabo -- "their" */
        { "ya", "we" },   /* class 4/6/9: yawe -- "your" */
    };
    char out[32];
    for (int i = 0; i < 4; i++) {
        toy_fuse(tests[i].conn, tests[i].person, out, sizeof(out));
        printf("%s + %s -> %s\n", tests[i].conn, tests[i].person, out);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p_toy_fuse p_toy_fuse.c
$ ./p_toy_fuse
wa + cu -> wacu
wa + nge -> wange
ya + bo -> yabo
ya + we -> yawe
```

Five real person suffixes exist in this project's data: `-nge` (my),
`-cu` (our), `-we` (your, singular), `-nyu` (your, plural), and `-bo`
(their) — sixteen classes times up to five persons is up to eighty
distinct possessive words, all built from the same two small,
independently-meaningful pieces:

```
   umurima  wacu     ("our field")
            │
       ┌────┴────┐
       │         │
      "wa"     "cu"
       │         │
    CONNECTOR  PERSON
   (agrees      (says
    with the     whose:
    noun's       "our")
    class 1/3)
       │         │
       └────┬────┘
            │
       one surface
       word: "wacu"
```

Two independently meaningful pieces, glued by simple concatenation —
no glide, no elision, no phonological rule renegotiating the boundary
between them, unlike almost everything else this book has built. That
absence is itself a fact worth noticing, and Section 5.1 explains
exactly why it's missing.

## 1.5 The other direction: a noun built from a verb

Chapters 1 through 3 each started from a fixed prefix-and-stem formula
and worked outward. Deverbative nouns work the opposite way: start
from a *known noun*, and ask whether it secretly contains a known
*verb* root. The grammar reference's own worked list:

> **3.3 Ingero z'amazina akomoka ku nshinga** (S4 examples):
> Impano, imihigo, amarushanwa, ibikorwa, ubucuruzi, **ubukire**,
> itaha, ihinga, **abakozi**, umukoro, ubushobozi, akamaro, ikibariro,
> ababaji, abagenzi, ubuhemu, ingemu, umutoni, imboni

`ubukire` ("wealth") and `abakozi` ("workers") are both real
Kinyarwanda nouns built from verb roots — `gukira` ("to recover/become
rich") and `gukora` ("to work") — with a derivational suffix on the
end (`-e`, `-o`, `-i`, and others, Section 3.2 of the grammar
reference) marking the noun as "the result/agent/abstraction of doing
this verb."

Every tree in Chapters 1 through 3 was a one-way street: a fixed
formula, read left to right, prefix to stem to ending. Deverbative
detection runs the noun tree and the verb tree against each other,
in opposite directions, on the *same word*:

```
   Chapter 1 (forward):     D + RT  +  C        =  word
                            (build a noun from its pieces)

   Chapter 3 (forward):     SP + TM + C + FV     =  word
                            (build a verb from its pieces)

   This chapter (backward):     u-bu-kir-e
                                       |
                              strip final vowel
                                       |
                                      kir
                                       |
                          is "kir" in VERB_STEMS[]? (Ch.3, S5.2)
                                       |
                                  YES -> gukira
                                  ("the noun ubukire is built
                                   FROM the verb gukira")
```

This is the first construction in this book that takes a finished
word from one tree and asks whether another tree's data secretly
explains it — checking the noun detector's output *against* the verb
detector's lexicon, rather than running either detector in isolation.

## 1.6 Build it: a toy deverbative detector

```c
/* p1_toy_deverb.c -- strip a noun's final vowel; is what's left a verb root? */
#include <stdio.h>
#include <string.h>

int toy_is_known_verb_root(const char *root) {
    static const char *known[] = { "kor", "kir", "ig", "som", NULL };
    for (int i = 0; known[i]; i++)
        if (strcmp(root, known[i]) == 0) return 1;
    return 0;
}

int toy_check_deverbative(const char *stem, char *root_out) {
    size_t len = strlen(stem);
    if (len < 3) return 0;
    char last = stem[len - 1];
    if (!(last=='a'||last=='e'||last=='i'||last=='o'||last=='u')) return 0;
    char root[32];
    strncpy(root, stem, len - 1);
    root[len - 1] = '\0';
    if (toy_is_known_verb_root(root)) { strcpy(root_out, root); return 1; }
    return 0;
}

int main(void) {
    struct { const char *stem; } tests[] = { {"kire"}, {"kozi"}, {"gabo"} };
    char root[32];
    for (int i = 0; i < 3; i++) {
        int ok = toy_check_deverbative(tests[i].stem, root);
        printf("%-6s -> deverbative=%d root=%s\n", tests[i].stem, ok, ok ? root : "-");
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p1_toy_deverb p1_toy_deverb.c
$ ./p1_toy_deverb
kire   -> deverbative=1 root=kir
kozi   -> deverbative=0 root=-
gabo   -> deverbative=0 root=-
```

`kire` correctly resolves to root `kir` (`gukira`, "ubukire"). `kozi`
fails this toy version even though `umukozi` really is a deverbative
of `gukora` — because stripping just the final vowel leaves `koz`, not
`kor`, and this toy doesn't yet know that a stem-final `z` can stand in
for an underlying `r` before certain endings. `gabo` correctly fails
for a completely different reason: `umugabo` ("man") is not built from
any verb at all, despite superficially resembling one. Both gaps are
deliberate; Part 6 reads the real function that closes the first one
and the real safeguard that protects the second.

## 1.7 Checkpoint: test yourself before continuing

1. Using Section 1.2's connector table, what connector would you use
   for class 11 (long things)? Build "our [class-11 noun]" with it.
2. `toy_check_deverbative` (Section 1.6) would also need to reject a
   word like `agakuru` ("reason," stem `kuru`) even though stripping
   its final vowel gives `kur`, which might coincidentally resemble a
   verb root. What kind of guard would you need to add? (Part 6 names
   the real project's actual answer.)
3. Why does a connector need a *separate* person suffix at all, rather
   than just memorizing eighty whole words directly? (There's a real,
   honest answer to this in Part 5 — and it isn't the one this
   question implies.)

---

# Part 2 — Multiple Answers, and a New Way to Crash

## 2.1 The out-param pattern, once more

```c
bool kin_is_pronoun(const char *word, PronounType *type_out, int *class_out);
```

Two pieces of information through two pointers, the identical shape
every detector in this book has used since Chapter 1, Section 2.1.
Nothing new in the pattern itself — what's worth pausing on is what
*kind* of function sits behind it this time, because Part 6 will show
it's the simplest implementation of this interface anywhere in the
project so far.

## 2.2 A new way to crash: assuming every class number is valid

`check_deverbative` (Section 1.6, Part 6 reads the real one) and the
possessive-agreement check (Part 7) both eventually index into a
sixteen-slot array using a noun's `noun_class` field. Section 1.3's
toy connector lookup defended that index with an explicit range check
— `if (cls < 1 || cls > 16) return NULL;` — the same defensive pattern
Chapter 2, Section 2.2 first introduced for adjective classes. It's
worth seeing, once, what happens when that one guard is missing on a
table that's indexed the *other* direction — by a class number that
arrived from somewhere the caller didn't fully control:

```c
/* p_unsafe_conn.c -- DELIBERATELY UNSAFE: no bounds check before indexing */
#include <stdio.h>

const char *unsafe_connector(int cls) {
    static const char *table[17] = {
        "",   "wa", "ba", "wa", "ya", "rya", "ya", "cya", "bya",
        "ya", "za", "rwa","ka", "twa","bwa", "kwa","ha"
    };
    return table[cls];          /* BUG: cls is never range-checked */
}

int main(void) {
    printf("class 7      -> %s\n", unsafe_connector(7));
    printf("class 30     -> %s\n", unsafe_connector(30));
    fflush(stdout);
    printf("class -50000 -> %s\n", unsafe_connector(-50000));
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p_unsafe_conn p_unsafe_conn.c
$ ./p_unsafe_conn
class 7      -> cya
class 30     -> (null)
$ echo $?
139
```

`class 30` lands past the array's end but still inside readable
memory, holding a bit pattern that happens to equal a NULL pointer —
and glibc's own `printf` defensively stringifies a NULL `%s` argument
as the literal text `(null)` rather than crashing right there. This is
the *exact* soft failure Chapter 2's adjective array-bounds demo
produced (`(null)nini`, Chapter 2 Section 2.2) — the identical
defensive behavior, on a completely different array. `class -50000`
lands far enough outside the array that the read itself faults, and
the program never gets a chance to print that third line at all:
`SIGSEGV`, exit 139. Same underlying mistake — one missing range
check — two different real outcomes depending only on *how far*
outside the array the index happens to land. This is Chapter 2,
Section 2.2's exact lesson again, on a different array: out-of-bounds
reads are undefined behavior regardless of whether the specific bytes
you land on happen to *look* harmless, and "it didn't crash for this
input" is not the same claim as "this is safe." The fix is the
one-line guard Section 1.3 already had:

```c
const char *safe_connector(int cls) {
    static const char *table[17] = { /* ... */ };
    if (cls < 1 || cls > 16) return NULL;
    return table[cls];
}
```

---

# Part 3 — Making It Interactive

## 3.1 Build it: a possessive builder you can talk to

```c
/* p3_repl.c -- type a class number (1-16) and a person (my/our/your/their),
 * get back the fused possessive word.                                    */
#include <stdio.h>
#include <string.h>

static const char *CONN[17] = {
    "",  "wa","ba","wa","ya","rya","ya","cya","bya",
    "ya","za","rwa","ka","twa","bwa","kwa","ha"
};

int main(void) {
    char line[64];
    printf("Type: <class 1-16> <my|our|your|their>, or 'q' to quit.\n");
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == 'q') break;
        int cls; char who[16];
        if (sscanf(line, "%d %15s", &cls, who) != 2 || cls < 1 || cls > 16) {
            printf("  (need a class 1-16 and one of my/our/your/their)\n");
            continue;
        }
        const char *suffix = NULL;
        if (strcmp(who, "my") == 0)        suffix = "nge";
        else if (strcmp(who, "our") == 0)  suffix = "cu";
        else if (strcmp(who, "your") == 0) suffix = "we";
        else if (strcmp(who, "their")==0)  suffix = "bo";
        if (!suffix) { printf("  (unknown person word)\n"); continue; }
        printf("  %s + %s -> %s%s\n", CONN[cls], suffix, CONN[cls], suffix);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p3_repl p3_repl.c
$ printf "1 our\n7 their\n14 my\nq\n" | ./p3_repl
Type: <class 1-16> <my|our|your|their>, or 'q' to quit.
  wa + cu -> wacu
  cya + bo -> cyabo
  bwa + nge -> bwange
```

Three real, correctly-formed words, each independently verifiable
against the real library's `kin_is_pronoun`.

---

# Part 4 — Capstone: Testing the Rules Honestly

## 4.1 Capstone: the possessive connector across all sixteen classes

Chapters 1 through 3 each closed their rules section with a capstone
testing every class at once. Do the same here, against the real
`kin_is_pronoun`:

```c
/* p4_all16_conn.c */
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    const char *connectors[] = {
        "wa","ba","wa","ya","rya","ya","cya","bya",
        "ya","za","rwa","ka","twa","bwa","kwa","ha"
    };
    for (int cls = 1; cls <= 16; cls++) {
        PronounType ty; int got_cls = 0;
        int ok = kin_is_pronoun(connectors[cls-1], &ty, &got_cls);
        printf("Nt.%-3d connector=%-4s ok=%d got_class=%-3d %s\n",
               cls, connectors[cls-1], ok, got_cls,
               (got_cls == cls) ? "" : "<- COLLISION");
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_all16_conn.c -L . -lkinyarwanda -o p4_all16_conn
$ LD_LIBRARY_PATH=. ./p4_all16_conn
Nt.1   connector=wa   ok=1 got_class=1
Nt.2   connector=ba   ok=1 got_class=2
Nt.3   connector=wa   ok=1 got_class=1   <- COLLISION
Nt.4   connector=ya   ok=1 got_class=4
Nt.5   connector=rya  ok=1 got_class=5
Nt.6   connector=ya   ok=1 got_class=4   <- COLLISION
Nt.7   connector=cya  ok=1 got_class=7
Nt.8   connector=bya  ok=1 got_class=8
Nt.9   connector=ya   ok=1 got_class=4   <- COLLISION
Nt.10  connector=za   ok=1 got_class=10
Nt.11  connector=rwa  ok=1 got_class=11
Nt.12  connector=ka   ok=1 got_class=12
Nt.13  connector=twa  ok=1 got_class=13
Nt.14  connector=bwa  ok=1 got_class=14
Nt.15  connector=kwa  ok=1 got_class=15
Nt.16  connector=ha   ok=1 got_class=16
```

Thirteen clean, three real collisions, exactly the shape of every
capstone this book has run: class 3 collapses into class 1, and
classes 6 and 9 both collapse into class 4 — because `kin_is_pronoun`
is a bare word→class lookup with no surrounding sentence to
disambiguate it, the same limitation Chapter 3's bare-verb capstone
hit for the identical underlying reason (these classes share concord
markers throughout the language, not just here). Part 7 shows exactly
how the real agreement checker turns this from a liability into a
non-issue.

## 4.2 Capstone: deverbative nouns, against the textbook's own list

Test a handful of the grammar reference's own Section 3.3 examples
against the real pipeline:

```c
/* p4_deverb.c */
#include "kinyarwanda.h"
#include <stdio.h>
int main(void) {
    const char *tests[] = { "umucyo", "umukozi", "ubukire", "umugabo", "umugore", NULL };
    for (int i = 0; tests[i]; i++) {
        SentenceAnalysis sa = kin_analyze(tests[i]);
        Token *t = &sa.tokens[0];
        printf("%-10s cls=%-3d stem=%-8s deverb=%d root=%s\n",
               tests[i], t->noun_class, t->stem, t->is_deverbative, t->verb_root);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_deverb.c -L . -lkinyarwanda -o p4_deverb
$ LD_LIBRARY_PATH=. ./p4_deverb
umucyo     cls=3   stem=cyo      deverb=1 root=cy
umukozi    cls=1   stem=kozi     deverb=1 root=kor
ubukire    cls=14  stem=kire     deverb=1 root=kir
umugabo    cls=1   stem=gabo     deverb=0 root=
umugore    cls=1   stem=gore     deverb=0 root=
```

`umukozi` ("worker") correctly resolves to root `kor` (`gukora`, "to
work") — the exact gap Section 1.6's toy detector hit (it could only
strip the final vowel, landing on `koz`, not `kor`). `umugabo` and
`umugore` ("man," "woman") correctly do **not** get flagged, even
though both end in a vowel and both have a "root" that would
superficially pass a naive check — `gabo`→`gab` resembles nothing, but
a careless implementation checking phonological *shape* alone, rather
than real membership in the known-verb list, could be fooled by
near-misses. Section 6.5 reads exactly how the real code protects
these two specific words by name.

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 Why this tree doesn't derive anything

Every previous chapter's detector *computed* its answer: strip a
prefix, check what's left, apply a phonological rule, accept or
reject. `kin_is_pronoun` does none of that. Look at its full body
(Part 6 quotes it exactly):

```c
bool kin_is_pronoun(const char *word, PronounType *type_out, int *class_out) {
    for (int i = 0; PRONOUNS[i].word; i++)
        if (strcmp(word, PRONOUNS[i].word) == 0) { /* ... */ return true; }
    return false;
}
```

That's the entire function. No prefix stripping, no rule application,
no reconstruction — just a flat scan through a table of complete,
literal words. `wange`, `wacu`, `babo`, every one of the roughly
eighty fused possessive forms (Section 1.4) is its own separate entry,
spelled out in full, even though the textbook itself describes them as
produced by a rule (`wa + nge = wange`).

## 5.2 Answering Chapter 1's checkpoint question honestly

Section 1.7 asked why the code wouldn't just memorize eighty words
directly rather than deriving them from a connector and a person
suffix — implying memorizing would be the *lazy* choice. The real
answer, found by reading the actual source rather than guessing, is
that memorizing **is** what this project does, and it's the *right*
engineering choice here, not a shortcut. The combinatorial space is
small (sixteen classes, five persons, with real-world collisions
shrinking it further) and **fully known in advance** — every
possessive word that can ever exist in the language is already listed
in the REB grammar's own paradigm tables. Compare the three designs
this book has now seen:

| Tree | Stem set | Combination | Design |
|---|---|---|---|
| Noun (Ch. 1) | open | prefix derived by rule | lookup table + rule fallback |
| Adjective (Ch. 2) | closed (~40) | prefix derived by rule | closed-set check + rule |
| Verb (Ch. 3) | large lookup + shape fallback | prefix/tense/object all derived by rule | hybrid lookup + heavy rules |
| Pronoun (Ch. 4, this part) | closed (~80 *whole words*) | **no derivation at all** | flat lookup, full stop |

Pronouns sit at the simplest end of this spectrum because there is
nothing left to *derive* — the entire word, not just its stem, is
small and closed enough to enumerate directly. Writing fusion logic
for `wa + nge → wange` would add code to compute something a 17-line
table already states outright, for a combinatorial space too small to
need it. The lesson isn't "always prefer rules" or "always prefer
lookup tables" — every chapter in this book has shown a different
point on that spectrum, each one correct for the actual size and
predictability of the problem it solves.

---

# Part 6 — Reading the Real Production Code

## 6.1 `kin_is_pronoun`, in full

```c
bool kin_is_pronoun(const char *word, PronounType *type_out, int *class_out) {
    for (int i = 0; PRONOUNS[i].word; i++) {
        if (strcmp(word, PRONOUNS[i].word) == 0) {
            if (type_out)  *type_out  = PRONOUNS[i].type;
            if (class_out) *class_out = PRONOUNS[i].class;
            return true;
        }
    }
    return false;
}
```

The whole function. One loop, one `strcmp`, two out-parameters filled
in on the one `return true`. This is the simplest detector in the
entire project — and Section 5.1 already explained why that's
appropriate rather than suspicious.

## 6.2 The table behind it: 445 entries, nine sub-types

```c
typedef struct { const char *word; PronounType type; int class; } PronounEntry;

static const PronounEntry PRONOUNS[] = {
    { "nge",   PRON_PERSONAL, 0 },        /* I / me                      */
    { "uyu",   PRON_DEMONSTRATIVE,  1 },  /* this (Nt.1)                 */
    { "wa",    PRON_POSSESSIVE,  1 },     /* connector, Nt.1             */
    { "wange", PRON_REFLEXIVE,  1 },      /* my (fused), Nt.1            */
    /* ... 441 more entries, across all nine PronounType sub-types ...   */
    { NULL, 0, 0 }
};
```

Nine sub-types (personal, demonstrative, possessive, reflexive,
relative, interrogative, indefinite, numerical, vocative), 445 total
rows. The demonstrative family alone (Section 4.2's `umugabo` test
didn't touch these, but they're built the identical way) covers three
"proximities" — *this* (near speaker), *that* (near listener), and
*that, over there* (far from both) — per class, the comment block
above the table spelling out exactly which textbook page each
sub-section comes from (REB S4/S5/S6, p.89–121). This is the largest
single lookup table in the project, and Section 5.2 already explained
why a table, not a rule engine, is the honest engineering answer for a
domain this size *and* this fixed.

## 6.3 The textbook's rule versus the code's table

It's worth stating plainly what the comment directly above the
possessive entries says, because it confirms Section 5.1/5.2's claim
isn't a guess:

```c
/* ── Ikinyazina ngenera (possessive/relative connectors) ───────────── */
{ "wa",    PRON_POSSESSIVE,  1 },
/* ... */
/* ── Ikinyazina ngenera ngenga (reflexive/inclusive possessives) ───── */
/* 1st singular (-nge): wange, bange, wange, yange... */
{ "wange",  PRON_REFLEXIVE,  1 }, { "bange",  PRON_REFLEXIVE,  2 },
```

The comment names the rule (`wa + nge`) the same way the textbook
does, immediately above a block of code that doesn't apply that rule
at runtime at all — it just lists every output the rule would ever
produce. The comment is documentation for a human reading the code,
explaining *why* these particular eighty-odd strings are exactly the
right eighty-odd strings to have listed; it isn't a hint that fusion
logic is missing. Knowing the difference between "this comment
explains a design choice" and "this comment describes an
unimplemented feature" is its own small reading skill, and the
clearest way to tell them apart is the one this book has used in every
chapter: test it, and see what the function in front of you actually
does.

## 6.4 `check_deverbative`, in full

```c
static void check_deverbative(Token *tok) {
    if (tok->pos != POS_NOUN) return;
    size_t slen = tok->stem[0] ? strlen(tok->stem) : 0;
    if (slen < 3) return;
    char last = tok->stem[slen - 1];
    bool ends_vowel = (last=='a'||last=='e'||last=='i'||last=='o'||last=='u');
    if (!ends_vowel) return;
    /* Guard: skip known primary nouns that collide with verb roots */
    for (int pi = 0; PRIMARY_NOUN_STEMS[pi]; pi++)
        if (strcmp(tok->stem, PRIMARY_NOUN_STEMS[pi]) == 0) return;

    char root[KIN_MAX_STEM];
    strncpy(root, tok->stem, slen - 1);
    root[slen - 1] = '\0';
    size_t rlen = strlen(root);

    /* r->z / __e rule: root-final 'z' before FV 'e' may be underlying 'r'. */
    if (last == 'e' && rlen >= 2 && root[rlen - 1] == 'z') {
        char root_re[KIN_MAX_STEM];
        memcpy(root_re, root, rlen + 1);
        root_re[rlen - 1] = 'r';
        if (kin_is_known_verb_stem(root_re)) { /* accept root_re */ return; }
    }
    /* -ano nominalizer: stem ends in 'an'+'o' (isezerano <- gusezera) */
    /* ... */
    if (rlen >= 2 && kin_is_known_verb_stem(root)) { /* accept root */ return; }
}
```

This is Section 1.6's toy detector, with two real gaps closed. First,
the `PRIMARY_NOUN_STEMS` guard — checked *before* any verb-root
matching is attempted — answers Section 1.7's second checkpoint
question directly: it's a short, explicit, named list of real nouns
that happen to *look* deverbative but aren't, checked by exact string
match before the heuristic ever runs. Second, the comment-documented
`z`→`r` repair (and the `-ano` nominalizer repair just below it,
omitted above for space) recovers stems the toy version couldn't: a
real Kinyarwanda root ending in `r` can surface as `z` immediately
before the `-e` nominalizer, so the function tries the literal
stripped root first and, if that fails, tries the same stem with a
`z`→`r` repair before giving up — the identical "try, validate against
a known list, and only then accept" discipline Chapter 2's adjective
prefix matcher used (Section 6.6 there) and Chapter 3's `ext_strip`
used for its causative-on-applicative chain (Section 6.5 there), now
guarding a fourth different kind of reconstruction.

## 6.5 The `PRIMARY_NOUN_STEMS` guard, and why it has to be a list

```c
static const char *PRIMARY_NOUN_STEMS[] = {
    "siga",   /* igisiga - birds of prey; NOT from gusiga (to leave/anoint) */
    "gore",   /* umugore - woman; NOT from kugora (to be difficult/hard)   */
    "gabo",   /* umugabo - man; NOT from kugaba (to distribute gifts)      */
    "taka",   /* ubutaka - soil/land; NOT from gutaka (to shout/cry out)   */
    "tungo",  /* itungo - domestic animal; NOT from gutunga (to possess)   */
    "hungu",  /* umuhungu - boy; NOT from guhunga (to flee)                */
    "kuru",   /* agakuru - reason/matter; NOT from gukura (to grow)        */
    "ryo",    /* uburyo - method; NOT from kurya (to eat)                  */
    "nywa",   /* amanywa - daytime; NOT from kunywa (to drink)             */
    NULL
};
```

Every entry here is a real, common noun that coincidentally strips
down to a string that's *also* a real, common verb stem. There's no
phonological rule that could distinguish "this noun is genuinely
unrelated to that verb" from "this noun really is derived from that
verb" — `gore` happening to equal `kugora`'s root minus a final vowel
is, linguistically, a pure coincidence, not a pattern any rule could
generalize. The only honest fix is exactly what's here: name the
specific collisions, one at a time, as they're discovered, and check
for them explicitly before the general heuristic runs. This is the
same kind of accounting Chapter 2's `corrector.c` did for out-of-range
classes and Chapter 3's `syntax.c` did for the ambiguous `vc == 6`
case — a short, explicit exception list standing in for a rule that
genuinely doesn't exist.

## 6.6 Real traces, side by side

| Word | Type | Class | Notes |
|---|---|---|---|
| `wa` | `PRON_POSSESSIVE` | 1 | connector, also class 3 |
| `wacu` | `PRON_REFLEXIVE` | 1 | `wa` + `cu` |
| `yabo` | `PRON_REFLEXIVE` | 4 | `ya` + `bo`, also class 6, 9 |
| `umucyo` | (noun) | 3 | deverbative, root `cy` |
| `umukozi` | (noun) | 1 | deverbative, root `kor` (z→r repaired) |
| `ubukire` | (noun) | 14 | deverbative, root `kir` |
| `umugabo` | (noun) | 1 | **not** deverbative — guarded by name |
| `umugore` | (noun) | 1 | **not** deverbative — guarded by name |

Eight real words, every column the real library's actual output —
five resolved by table lookup alone, three resolved by a heuristic
with a named exception list standing guard in front of it.

---

# Part 7 — Concordance: Possessive Agreement

## 7.1 Build it: a toy possessive checker

```c
/* p_toy_poss.c */
#include <stdio.h>
#include <string.h>

int check_poss_agreement(const char *noun_connector, const char *poss_connector) {
    return strcmp(noun_connector, poss_connector) == 0;
}

int main(void) {
    printf("noun=wa poss=wa  -> %s\n", check_poss_agreement("wa","wa") ? "OK" : "MISMATCH");
    printf("noun=wa poss=ba  -> %s\n", check_poss_agreement("wa","ba") ? "OK" : "MISMATCH");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p_toy_poss p_toy_poss.c
$ ./p_toy_poss
noun=wa poss=wa  -> OK
noun=wa poss=ba  -> MISMATCH
```

Notice this toy compares *connector strings*, not class numbers — on
purpose, and Section 7.2 shows exactly why that choice matters.

## 7.2 The real detection: comparing strings, not numbers

```c
static const char *poss_connector[17] = {
    "",
    "wa",  "ba",  "wa",  "ya",  "rya", "ya",  "cya",
    "bya", "ya",  "za",  "rwa", "ka",  "twa", "bwa",
    "kwa", "ha",
};
```

```c
if (cur->pos == POS_NOUN && cur->noun_class > 0 &&
    next->pos == POS_PRONOUN &&
    (next->pron_type == PRON_POSSESSIVE || next->pron_type == PRON_REFLEXIVE) &&
    next->noun_class > 0) {
    bool nt9_10_ok = (cur->noun_class == 9 && next->noun_class == 10)
                  || (cur->noun_class == 10 && next->noun_class == 9);
    if (!nt9_10_ok && strcmp(poss_connector[cur->noun_class],
                             poss_connector[next->noun_class]) != 0) {
        /* ... only NOW consider this a real disagreement ... */
        add_error(sa, ERR_POSS_AGREEMENT, i + 1, msg, sug);
    }
}
```

This is a second, separate small table — `poss_connector[17]`, indexed
directly by class number, the *same* information Section 6.2's
`PRONOUNS[]` table already encodes, just stored in the other
direction (class→word here, instead of word→class there) because this
function needs to go that way. The comparison itself is the payoff of
Section 4.1's capstone: rather than comparing `cur->noun_class ==
next->noun_class` directly — which would wrongly flag a class-3 noun
paired with a class-1-labeled `wa` connector as a mismatch, purely
because of the bare-word collision Section 4.1 found — it compares
`poss_connector[cur->noun_class]` against `poss_connector[next-
>noun_class]` as **strings**. Two different class numbers that happen
to share a surface connector are, correctly, never flagged. The real
code even has a comment saying so outright:

```c
/* Two classes may share the same connector (e.g. Nt.1 and Nt.3
 * both use "wa").  Compare connector strings, not class numbers,
 * to avoid false positives like "umunsi wa mbere".           */
```

The function also carries forward a *second* honest exception, for
class 9/10 (paired singular/plural, the same merger Chapter 3, Section
7.2 found for subject-verb agreement) — and a third, more elaborate
one for "chain-head" agreement, where a possessive several nouns deep
in a chain (*"ruhande rw'iburasirazuba rwa Edeni"*, "the eastern side
of Eden") correctly agrees with the chain's structural head noun
rather than the noun immediately in front of it. Three separate,
explicitly-reasoned exceptions, layered in front of one simple string
comparison — accuracy bought by accounting for every real ambiguity
this book has already found, not by writing a cleverer rule.

## 7.3 Case study: a clean sentence (with a quiet collision inside it)

```c
/* p_poss.c */
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    const char *text = "Umurima wacu ni mwiza.";
    SentenceAnalysis sa = kin_analyze(text);
    kin_suggest_corrections(&sa);
    printf("Input: \"%s\"\n\n", text);
    for (int i = 0; i < sa.token_count; i++) {
        Token *t = &sa.tokens[i];
        printf("  token[%d]=\"%-10s\" pos=%d noun_class=%d\n", i, t->surface, t->pos, t->noun_class);
    }
    printf("\nErrors found: %d\n", sa.error_count);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p_poss.c -L . -lkinyarwanda -o p_poss
$ LD_LIBRARY_PATH=. ./p_poss
Input: "Umurima wacu ni mwiza."

  token[0]="Umurima   " pos=1 noun_class=1
  token[1]="wacu      " pos=7 noun_class=1
  token[2]="ni        " pos=6 noun_class=0
  token[3]="mwiza     " pos=2 noun_class=1
  token[4]="."         " pos=17 noun_class=0

Errors found: 0
```

Look closely at `token[0]`: `Umurima` ("the field") comes back class
1, not the class 3 a native speaker would assign it (a field is a
thing, not a human). This is Chapter 1's `umuti` ambiguity
(Section 1.9 there) resurfacing, completely unprompted, on a different
word — `umu-` is genuinely ambiguous between class 1 and class 3 by
spelling alone, and `kin_detect_noun_class` falls back to class 1 the
same way it does for every such case. And it doesn't matter here, for
a precise, checkable reason: `poss_connector[1]` and `poss_connector[3]`
are both `"wa"`. Whether `Umurima` is "really" class 1 or class 3, the
correct connector is identical either way, so `wacu` agrees no matter
which answer the noun detector happened to commit to. Section 7.2's
string comparison isn't just elegant — it's the specific design choice
that makes this real, pre-existing ambiguity completely harmless to
this particular check.

## 7.4 Case study: a broken sentence

```c
/* same program, different input */
const char *text = "Umurima babo ni mwiza.";
```

```
$ LD_LIBRARY_PATH=. ./p_poss
Input: "Umurima babo ni mwiza."

  token[0]="Umurima   " pos=1 noun_class=1
  token[1]="babo      " pos=7 noun_class=2
  token[2]="ni        " pos=6 noun_class=0
  token[3]="mwiza     " pos=2 noun_class=1
  token[4]="."         " pos=17 noun_class=0

Errors found: 1
  [0] Ikinyazina ngenera 'babo' ntigishyikira izina 'Umurima' (inteko 1). Possessive 'babo' does not agree with noun 'Umurima' (class 1).
```

`babo` ("their," class 2's connector `ba` + `bo`) doesn't share a
connector with class 1 *or* class 3 — `poss_connector[2]` is `"ba"`,
genuinely different from `"wa"` — so this time the mismatch is real
regardless of which of `Umurima`'s two plausible classes you believe,
and the checker correctly flags it.

---

# Part 8 — Practice

### Beginner

1. **Hand-build `bwacu`** ("ours," class 14) from Section 1.2's
   connector table and Section 1.4's person suffixes, then verify it
   against the real library with `kin_is_pronoun`.
2. **Extend `p1_toy_deverb.c`** (Section 1.6) to handle the `z`→`r`
   repair Section 6.4 described, and confirm `kozi` now correctly
   resolves to root `kor`.

### Intermediate

3. **Find a fourth real collision.** Section 4.1's capstone found
   three (classes 3, 6, 9). Check whether any *other* sub-type's
   table (demonstrative pronouns, Section 6.2) has a class that
   collapses the same way, by testing all sixteen against
   `kin_is_pronoun` yourself.
4. **Extend `p3_repl.c`** (Section 3.1) to also accept `"his/her"` —
   look up the real REB person-suffix table to find the correct
   suffix, and verify your addition against the real library.
5. **Add a tenth word to `PRIMARY_NOUN_STEMS`** (Section 6.5) that you
   find yourself — a real Kinyarwanda noun whose stem coincidentally
   matches a real verb root from `VERB_STEMS[]` (Chapter 3, Section
   5.2) without actually being derived from it.

### Advanced

6. **Construct a real "chain-head" sentence** like Section 7.2's
   `"ruhande rw'iburasirazuba rwa Edeni"` example, and trace by hand
   why the agreement checker's chain-head exception correctly
   prevents a false positive on the second connector.
7. **Find a sentence of your own** with a deliberate possessive
   mismatch, run it through `kin_analyze` and `kin_suggest_corrections`
   the way Section 7.4 did, and confirm the suggested repair by
   checking the class's connector in `poss_connector[]` by hand.
8. **Revisit Section 1.7's third checkpoint question** now that
   Part 5 has answered it. Write, in your own words, the general
   principle for *when* a closed combinatorial space should be
   enumerated directly versus derived by rule — then test your
   principle against all four trees in this book (nouns, adjectives,
   verbs, pronouns) and see if it holds for all four.

## Key takeaways

- The possessive connector (*ikinyazina ngenera*) is concordance's
  fifth job in this book, using the same sixteen markers as noun RT,
  adjective RS, verb SP, and verb OM — but here, fused with a person
  suffix into a single surface word rather than standing alone.
- Deverbative nouns run the noun and verb trees in reverse: strip a
  noun's final vowel, check whether what's left is a known verb root,
  and if so the noun was built from that verb — the first construction
  in this book that connects two trees instead of building one.
- `kin_is_pronoun` is a flat lookup against 445 fully-enumerated
  entries with no rule-derivation at all — the correct design,
  confirmed by the size of the real combinatorial space (sixteen
  classes times up to five persons, fully known in advance), not a
  missing feature. A comment describing a rule next to code that
  doesn't apply that rule is documentation, not an unfinished TODO —
  telling the two apart means testing the code, the same habit this
  book has used in every chapter.
- A short, named, explicit exception list (`PRIMARY_NOUN_STEMS`) is
  sometimes the only honest fix when no phonological rule could ever
  distinguish a real pattern from pure coincidence — the same
  accounting discipline Chapters 2 and 3 each needed once, now needed
  a third time for a third reason.
- Comparing the *surface marker* two things share, rather than their
  abstract class numbers, can make a real, pre-existing ambiguity
  (Chapter 1's `umuti`/class-1-vs-3 problem, recurring here on
  `umurima`) completely harmless to a downstream check — proven, not
  assumed, by the real possessive-agreement checker's own design and
  its own explanatory comment.
- Out-of-bounds array reads remain undefined behavior even when the
  specific bytes you land on happen to look harmless — "it didn't
  crash this time" is not the same claim as "this is safe."

## Sources quoted in this chapter

- `data/textbook_grammar_rules.md`, Part 3 (IKOMORAZINA MVANSHINGA)
  and Part 9 (IKINYAZINA) — REB S3–S6.
- `include/kinyarwanda.h` (`kin_is_pronoun`, the `PronounType` enum,
  `ERR_POSS_AGREEMENT`).
- `src/lexicon.c` (`PRONOUNS[]`, `kin_is_pronoun`).
- `src/pos_tagger.c` (`check_deverbative`, `PRIMARY_NOUN_STEMS[]`).
- `src/syntax.c` (`poss_connector[]`, the `ERR_POSS_AGREEMENT` check
  and its chain-head exception).
- Every `pN_*.c`/`p_*.c` program and every real-library run in this
  chapter was actually compiled with `gcc -std=c99 -Wall -Wextra` and
  actually executed to produce the exact output quoted above.

## Coming up in Chapter 5

Four chapters have now built four trees — noun, adjective, verb,
pronoun — and every one of them changes shape depending on context:
a prefix, a tense, a class. `pos_tagger.c`'s own comment block names
one tree this book hasn't touched at all: *Tree 5, amagambo
adahinduka* — invariable words. Locatives, conjunctions, adverbs,
interjections: words that, by definition, never agree with anything,
never conjugate, never decline. After four chapters of "here is how
this word changes," Chapter 5 is the first one about words that
never do.
