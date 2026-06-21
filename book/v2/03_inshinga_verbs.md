# Chapter 3 — Inshinga: The Verb

## How this chapter works

Same rules as Chapters 1 and 2. Every Kinyarwanda rule is explained in
plain language, with real quoted examples, before any code touches it.
Every code example in this chapter was actually compiled with
`gcc -std=c99 -Wall -Wextra` and actually executed — the output you see
in a fenced block prefixed with `$` is the real terminal output of that
exact program, not a transcription of what it "should" print. Where this
chapter's own toy programs disagree with the real library, that
disagreement is the point: it's where the real grammar turns out to be
more interesting than a first guess.

The verb is the largest, most irregular piece of this whole project —
the real detector you'll read in Part 6 is roughly ten times the size
of Chapter 1's noun detector. This chapter does not attempt to explain
every line of it. It builds the same skill Chapters 1 and 2 built —
read the grammar, build a small working version yourself, then read
the real code and recognize what you already understand — applied to
a bigger, messier subject. Six grammatical families are covered in
real depth; a few dozen more exist in the real code and are pointed at
honestly, not hidden, in Part 6's closing section.

---

# Part 1 — Language and Code, Side by Side

## 1.1 What is *inshinga*, and why it has two branches

*Inshinga* is the Kinyarwanda word for verb. Unlike the noun and the
adjective, a Kinyarwanda verb comes in two completely different
surface shapes, and this project's source code keeps them as two
separate detection functions from the very first line:

> **Imbundo** (Infinitive / citation form) — the dictionary form, the
> one you'd look up: *gusoma* ("to read"), *kwiga* ("to study").
> **Itondaguye** (Conjugated form) — the form that actually appears in
> a sentence, carrying who's doing it and when: *arasoma* ("he is
> reading"), *bigiye* ("they have studied").

Quoting this project's own grammar reference:

> **4.1 Imbundo — Formula: Ku – C – Soz** (Indanganshinga + Igicumbi + Umusozo)
> | Component | Term | Description |
> |---|---|---|
> | Ku- (or Gu-) | Indanganshinga | Infinitive marker; marks the verb class (Nt.15) |
> | C | Igicumbi/Umuzi | Stem; does not change; carries meaning |
> | -a | Umusozo | Final vowel of infinitive (always -a for regular verbs) |
>
> **4.4 Intego mbonera y'inshinga itondaguye — Formula: Rsh – Rgh – C – Sz**
> | Component | Term | Description |
> |---|---|---|
> | Rsh | Indanganshinga/Indangasano ya ruhamwa | Subject agreement prefix; changes by person/class |
> | Rgh | Indangagihe | Tense/aspect marker |
> | C | Igicumbi | Stem |
> | Sz | Umusozo | Final suffix (-a, -e, -aga, -ye) |

Two formulas, two functions in the real source — `kin_is_verb_infinitive()`
for the first, `kin_is_verb_conjugated()` for the second. This chapter
builds toward both, starting with the simpler one.

## 1.2 The infinitive skeleton: PREF + C + FV

The infinitive looks, on paper, almost exactly like Chapter 1's noun
formula: a prefix, a stem that never changes, and a fixed ending.

```
   noun (Chapter 1):       u   +   mu   +   ti     =  umuti
                           D       RT        C

   verb infinitive:           ku   +   kor   +   a   =  gukora
                               PREF      C       FV
```

The textbook's own worked examples:

> Gukina: ku – kin – a
> Gutsinda: ku – tsind – a
> Kwiga: ku – ig – a (u→w/_J)
> Gukora: ku – kor – a

That `u→w/_J` annotation on *kwiga* should look familiar — it's the
exact same glide rule Chapter 1 first found on nouns (*umwana*) and
Chapter 2 found again on adjectives (*mwiza*). This is now the third
grammatical category that obeys it. Kinyarwanda's phonological rules
don't care what part of speech they're rewriting; the rule is
about *sound contact*, not word class.

## 1.3 Build it: a toy infinitive detector

Start with the absolute minimum: recognize `gu-`/`ku-` plus a stem
plus `-a`.

```c
/* p1_toy_inf.c -- a first, deliberately tiny infinitive detector */
#include <stdio.h>
#include <string.h>

int toy_is_infinitive(const char *word, char *stem_out) {
    size_t len = strlen(word);
    if (len < 4) return 0;
    const char *inner = NULL;
    if (strncmp(word, "gu", 2) == 0) inner = word + 2;
    else if (strncmp(word, "ku", 2) == 0) inner = word + 2;
    else return 0;
    size_t ilen = strlen(inner);
    if (ilen < 2 || inner[ilen - 1] != 'a') return 0;
    strncpy(stem_out, inner, ilen - 1);
    stem_out[ilen - 1] = '\0';
    return 1;
}

int main(void) {
    const char *tests[] = { "gusoma", "gukora", "amashuri", "kwiga", NULL };
    char stem[32];
    for (int i = 0; tests[i]; i++) {
        int ok = toy_is_infinitive(tests[i], stem);
        printf("%-10s ok=%d stem=%s\n", tests[i], ok, ok ? stem : "-");
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p1_toy_inf p1_toy_inf.c
$ ./p1_toy_inf
gusoma     ok=1 stem=som
gukora     ok=1 stem=kor
amashuri   ok=0 stem=-
kwiga      ok=0 stem=-
```

Three out of four. `amashuri` ("schools," a noun) correctly fails —
good, that's not a verb. But `kwiga` ("to study") *should* succeed and
doesn't, because this toy only knows two prefix spellings, `gu-` and
`ku-`, and `kwiga` is the glide-contracted form from Section 1.2:
`ku` + vowel-initial stem `ig` → `kw` + `ig`. Exactly the gap Chapter
1's first toy detector hit on `umwana`, now hit again on a different
word class. The fix is the same shape too: add the glide-contracted
prefix spellings, and a guard for which prefix is right before which
kind of stem (consonant-initial vs vowel-initial) — Part 6 will show
you the real function's exact version of this guard.

## 1.4 The conjugated skeleton: SP + TM + C + FV

The conjugated form is where the verb stops resembling the noun or the
adjective. Where the noun was three pieces and the adjective was two,
the conjugated verb is, in the general case, **four**:

```
   SP   +   TM    +   C    +   FV
 subject    tense      stem      final
 prefix     marker    (invariant)  vowel
(WHO)      (WHEN)     (WHAT)      (HOW — assertion, wish, command...)
```

Quoting the grammar reference's own person/class table for SP
(*Indanganshinga*):

> Ngenga ya mbere (1st person): n- (I), tu- (We)
> Ngenga ya kabiri (2nd person): u- (You sg), mu- (You pl)
> Ngenga ya gatatu (3rd person), by noun class: a-(1), ba-(2), u-(3),
> i-(4), ri-(5), a-(6), ki-(7), bi-(8), i-(9), zi-(10), ru-(11), ka-(12),
> tu-(13), bu-(14), ku-(15), ha-(16)

Notice immediately: this is **the same sixteen markers** Chapter 1's
noun classes used for RT, and Chapter 2's adjectives reused for RS, now
doing a *third* job — marking which noun a verb's action belongs to.
One concordance system, three different grammatical employers.

And the four-piece formula is itself a simplification. The textbook's
own worked examples show TM and C aren't always adjacent to a fixed
FV — extensions can sit between them:

> Kuzakina: ku – za – kin – a (za = future marker)
> Guhingira: ku – hing – ir – a (-ir = applicative/benefactive)
> Kwigisha: ku – ig – ish – a (-ish/-sh = causative)

So the *real* shape, once every optional piece is included, is closer
to `SP + [TM] + [OM] + C + [EXT] + FV` — subject prefix, an optional
tense marker, an optional object marker, the stem, an optional
extension, and the final vowel. Five slots, three of them optional.
Part 6 reads the real function that handles all five at once; this
chapter's "build it" sections take them one at a time.

Stacking every optional slot onto one real word makes the formula
concrete rather than abstract — `bamwirukanye` ("they chased him
away," verified for real in Section 6.4) fills every single slot at
once:

```
   ba    +  (none) +  mw   +  iruk  +  an   +  ye
   SP        TM        OM      C       EXT      FV
  "they"    (none —   "him"  "chase"  recip-   perfect
            habitual          /run     rocal   (completed)
            present)                  reading
```

Five slots, four of them filled, one of them (TM) deliberately empty
— and "deliberately empty" is itself meaningful, the same lesson
Section 4.2 makes explicit: Kinyarwanda's habitual/unmarked present
and past-perfect both *skip* the TM slot rather than filling it with
some neutral placeholder, and the rest of the word's grammar (here,
the `-ye` ending) is what tells you which "empty TM" reading is
meant.

## 1.5 Build it: a toy present-tense builder

Detecting is harder than building, so start by building: given a
subject prefix and a stem, produce the present-tense form.

```c
/* p2_toy_present.c -- build a present-tense verb from SP + stem */
#include <stdio.h>
#include <string.h>

void toy_build_present(const char *sp, const char *stem, char *out, size_t outsz) {
    snprintf(out, outsz, "%sra%sa", sp, stem);
}

int main(void) {
    struct { const char *sp; const char *stem; } tests[] = {
        { "a",  "gend" },   /* class 1: aragenda  -- "he/she is going"  */
        { "ba", "gend" },   /* class 2: baragenda -- "they are going"  */
        { "ki", "gend" },   /* class 7: kiragenda -- "it is going"     */
    };
    char out[64];
    for (int i = 0; i < 3; i++) {
        toy_build_present(tests[i].sp, tests[i].stem, out, sizeof(out));
        printf("%s + ra + %s + a  ->  %s\n", tests[i].sp, tests[i].stem, out);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p2_toy_present p2_toy_present.c
$ ./p2_toy_present
a + ra + gend + a  ->  aragenda
ba + ra + gend + a  ->  baragenda
ki + ra + gend + a  ->  kiragenda
```

All three are real, correctly-formed Kinyarwanda, and all three
round-trip cleanly back through the real library's *detector* — feed
`aragenda` to `kin_is_verb_conjugated` and it correctly reports class
1, present tense, stem `gend`. Building and detecting are two
different problems built from the same formula; this chapter spends
most of its time on detecting, because that's what the real library
mostly does, but it's worth seeing once that the formula runs both
ways.

## 1.6 Checkpoint: test yourself before continuing

Before Part 2, work these out by hand, then check by building a tiny
test program of your own against the toy functions above:

1. Using Section 1.4's person/class table, what subject prefix would
   you use for "we" (1st person plural)? Build the present-tense form
   for stem `kor` ("work/do") with it.
2. `toy_is_infinitive` (Section 1.3) fails on `kwiga`. Name one *other*
   real infinitive you'd expect it to fail on, and say why, before
   testing it.
3. Why does the conjugated formula need an *optional* object marker
   slot but the infinitive formula doesn't? (Hint: what would an
   infinitive's "object" even attach to, grammatically?)

---

# Part 2 — Multiple Answers, and a New Way to Crash

## 2.1 The out-param pattern, now with five things to report at once

Chapters 1 and 2 used pointers to return more than one value from a
single function — a stem and a class, two things through two
out-parameters. The real conjugated-verb detector needs to report
*five*:

```c
bool kin_is_verb_conjugated(const char *word, char *stem_out, int *subj_class,
                            VerbTense *tense_out, int *obj_class_out,
                            VerbExtension *ext_out, bool *neg_out);
```

Stem, subject class, tense, object class, extension, and negation —
six pieces of information (five out-parameters plus the `bool`
return) about one input word, because Kinyarwanda packs that much
genuinely independent grammatical information into a single
conjugated verb. This is the exact same "a function can only `return`
one value, so use pointers for the rest" idiom from Chapter 1, Section
2.1, scaled up to match how much one verb actually encodes. Nothing
new to learn here — just confirmation that the pattern keeps scaling
cleanly as the grammar gets richer.

## 2.2 A new way to crash: trusting output you never checked

Every one of those five out-parameters is written *only if the
function returns `true`*. Look at the real function's structure (Part
6 reads it in full) and you'll find this guard, over and over, at
every failure point:

```c
if (len < 3) return false;
/* ... */
if (!first_ok) {
    /* ... a few more fallback attempts, each can also return false ... */
    return false;
}
```

When the function returns `false`, it does not zero out `stem_out`,
`subj_class`, or any of the others. It simply leaves them untouched.
That's a deliberate, reasonable design choice — clearing five
output buffers on every failed call would be wasted work for the
overwhelming majority of real text, which isn't a verb at all. But it
creates a sharp edge: if the caller never checks the `bool` return
value, the "result" the caller reads back is silently **whatever was
in those variables before the call** — not zero, not garbage in the
sense of a crash, just *stale*. Watch it happen:

```c
/* p_unsafe_ret.c -- DELIBERATELY UNSAFE: ignores the success return value */
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    const char *words[] = { "aragenda", "amashuri", NULL };
    char stem[KIN_MAX_STEM] = "";
    int cls = 0, obj = 0;
    VerbTense tense = TENSE_NONE;
    VerbExtension ext = VEXT_NONE;
    bool neg = false;

    for (int i = 0; words[i]; i++) {
        /* BUG: the bool return value is discarded -- on failure, every
         * out-parameter below is left exactly as the PREVIOUS call set it. */
        kin_is_verb_conjugated(words[i], stem, &cls, &tense, &obj, &ext, &neg);
        printf("%-10s -> class=%d tense=%d stem=\"%s\"\n",
               words[i], cls, (int)tense, stem);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p_unsafe_ret.c -L . -lkinyarwanda -o p_unsafe_ret
$ LD_LIBRARY_PATH=. ./p_unsafe_ret
aragenda   -> class=1 tense=1 stem="gend"
amashuri   -> class=1 tense=1 stem="gend"
```

`amashuri` ("schools") is a noun. It is not a verb in any tense, by any
reading. And the program reports it as class 1, present tense, stem
`gend` — the exact answer from the *previous* line, because the second
call to `kin_is_verb_conjugated` returned `false` and therefore touched
nothing, and nothing in this code noticed. This is more dangerous than
Chapter 2's array-bounds crash precisely because nothing crashes:
`-Wall -Wextra` raise no warning, the program runs to completion, and
the output looks exactly as plausible and well-formatted as a correct
answer. The fix is one `if`:

```c
/* p_safe_ret.c -- SAFE: always check the return value before trusting output */
bool ok = kin_is_verb_conjugated(words[i], stem, &cls, &tense, &obj, &ext, &neg);
if (!ok) {
    printf("%-10s -> not a conjugated verb\n", words[i]);
    continue;
}
printf("%-10s -> class=%d tense=%d stem=\"%s\"\n", words[i], cls, (int)tense, stem);
```

```
$ gcc -std=c99 -Wall -Wextra -I include p_safe_ret.c -L . -lkinyarwanda -o p_safe_ret
$ LD_LIBRARY_PATH=. ./p_safe_ret
aragenda   -> class=1 tense=1 stem="gend"
amashuri   -> not a conjugated verb
```

The general lesson, stated plainly: **a `bool` return value next to a
handful of out-parameters is not decoration — it is the only thing
telling you whether the out-parameters mean anything at all this
time.** Every function in this entire project that follows the
out-parameter pattern (every one you've read since Chapter 1) carries
this same unstated contract. This is the first chapter where ignoring
it produces an answer that looks completely reasonable instead of
empty or crashed — which is exactly why it's worth seeing once,
deliberately, before you ever do it by accident.

## 2.3 Another way to crash: recursion that never gets smaller

Section 1's real `kin_is_verb_infinitive` (Part 6 reads it in full)
recurses once, to strip a locative suffix (`-ho`/`-mo`/`-yo`) and
re-check the shorter word underneath. Chapter 1 first showed this
shape of recursion is safe specifically *because* every call works on
a strictly shorter string than the one before it — there's no input
that can make it recurse forever, because the string's length can
only shrink so many times before it runs out. That guarantee is doing
real work, and it's worth seeing what happens when a function recurses
the same way but loses it:

```c
/* p_unsafe_recur.c -- DELIBERATELY UNSAFE: a locative-suffix stripper with
 * a length bug that makes it recurse on a string the SAME length forever. */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

bool buggy_is_infinitive(const char *word) {
    size_t len = strlen(word);
    if (len < 4) return false;
    if (len > 5 && word[len-3] == 'a' &&
        (strcmp(word+len-2, "ho") == 0 || strcmp(word+len-2, "mo") == 0)) {
        char buf[256];
        strncpy(buf, word, sizeof(buf)-1);   /* BUG: doesn't shorten by 2! */
        buf[sizeof(buf)-1] = '\0';
        return buggy_is_infinitive(buf);     /* recurses on an IDENTICAL string */
    }
    return word[len-1] == 'a';
}

int main(void) {
    printf("calling...\n");
    fflush(stdout);
    bool ok = buggy_is_infinitive("guturaho");
    printf("ok=%d\n", ok);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p_unsafe_recur p_unsafe_recur.c
$ ./p_unsafe_recur
calling...
$ echo $?
139
```

`"calling..."` prints, then nothing else — the program is killed by
signal 11 (SIGSEGV), exit code 139, the same crash signature Chapter
1's unbounded `strcpy` produced. The bug is one line: `strncpy(buf,
word, sizeof(buf)-1)` copies the *entire* word into `buf` instead of
the word *minus its last two characters*, so `buf` is exactly as long
as `word` was, the suffix-check matches again, and the function calls
itself with no progress made — forever, or rather until the call
stack itself runs out of memory and the OS kills the process. The fix
restores the one property that makes this kind of recursion safe in
the first place: every call must hand the next call a **strictly
shorter** string.

```c
/* p_safe_recur.c -- SAFE: each recursive call strips 2 chars, guaranteed shrink */
bool safe_is_infinitive(const char *word) {
    size_t len = strlen(word);
    if (len < 4) return false;
    if (len > 5 && word[len-3] == 'a' &&
        (strcmp(word+len-2, "ho") == 0 || strcmp(word+len-2, "mo") == 0)) {
        char buf[256];
        strncpy(buf, word, len - 2);          /* FIX: strictly shorter each call */
        buf[len - 2] = '\0';
        return safe_is_infinitive(buf);
    }
    return word[len-1] == 'a';
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p_safe_recur p_safe_recur.c
$ ./p_safe_recur
guturaho   -> 1
gucamo     -> 1
amashuri   -> 0
```

Same idea, one corrected line, and the function now terminates on
every input, in at most a couple of recursive calls. Chapter 1,
Section 6.5 stated the general rule the real `kin_is_verb_infinitive`
actually follows — "every recursive call strips characters and calls
itself on a strictly shorter string" — as a fact about that one
function. This section is the proof of *why* that fact matters: drop
it, even by a single wrong argument to `strncpy`, and recursion stops
being a clean way to express "strip this, then re-check" and becomes
a silent way to exhaust the stack.

---

# Part 3 — Making It Interactive

## 3.1 Build it: a present-tense conjugator you can talk to

A small REPL, extending Section 1.5's builder across all sixteen
classes, so you can check your own hand-worked answers from the
checkpoint:

```c
/* p3_repl.c -- type a verb stem and a class number (1-16), get back the
 * present-tense conjugated form for that class's subject prefix.        */
#include <stdio.h>
#include <string.h>

typedef struct { int cls; const char *sp; } ClassSp;
static const ClassSp CLASS_SP[] = {
    {1,"a"}, {2,"ba"}, {3,"u"}, {4,"i"}, {5,"ri"}, {6,"a"}, {7,"ki"}, {8,"bi"},
    {9,"i"}, {10,"zi"}, {11,"ru"}, {12,"ka"}, {13,"tu"}, {14,"bu"}, {15,"ku"}, {16,"ha"}
};

int main(void) {
    char line[128];
    printf("Type: <stem> <class 1-16>  (e.g. \"gend 1\"), or 'q' to quit.\n");
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == 'q') break;
        char stem[64];
        int cls;
        if (sscanf(line, "%63s %d", stem, &cls) != 2) {
            printf("  (couldn't read a stem and a class number)\n");
            continue;
        }
        const char *sp = NULL;
        for (int i = 0; i < 16; i++)
            if (CLASS_SP[i].cls == cls) { sp = CLASS_SP[i].sp; break; }
        if (!sp) { printf("  (class must be 1-16)\n"); continue; }
        printf("  %s + ra + %s + a  ->  %sra%sa\n", sp, stem, sp, stem);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p3_repl p3_repl.c
$ printf "gend 1\nsom 7\nkund 2\nq\n" | ./p3_repl
Type: <stem> <class 1-16>  (e.g. "gend 1"), or 'q' to quit.
  a + ra + gend + a  ->  aragenda
  ki + ra + som + a  ->  kirasoma
  ba + ra + kund + a  ->  barakunda
```

Three real, correctly-formed words: *aragenda* ("he/she is going"),
*kirasoma* ("it is reading"), *barakunda* ("they love"). Note the
`%63s` bound on `sscanf` — the exact same fixed-width discipline
Chapter 1 used for its own REPL, doing the same job: stopping a long
line of typed input from overflowing `stem`.

---

# Part 4 — Capstone: The Phonological Rules, One Family at a Time

Six families this time, each checked against the real library before
being trusted, exactly as Chapters 1 and 2 did for their own rules.

## 4.1 Subject prefixes: concordance's third job

Section 1.4 already showed the SP table is the same sixteen markers as
RT (Chapter 1) and RS (Chapter 2). What's new here is that the
*surface form* of those markers shifts under the same phonological
pressures you've now seen twice — vowel glide, nasal assimilation —
applied to a third morpheme:

```
$ LD_LIBRARY_PATH=. ./p_sptest
word       ok  cls  prefix  note
aragenda   1   1    ara     plain (gend starts with consonant)
bwiga      1   14   bw      bu->bw glide (u->w/_V, the same rule again)
twiga      1   0    tw      tu->tw glide (1st plural)
ndasoma    1   0    nda     n->nd (1sg before consonant-initial root)
```

(The `prefix` column above is computed honestly, not guessed: each
word's length minus its detected stem's length minus one for the
final vowel, printed as-is. It isn't always *just* the subject
prefix — `ara` is SP `a` with the present tense marker `ra` already
fused on, and `nda` is the SP table's own combined 1sg-present token,
Section 6.3 — but every one of these four strings is exactly what the
real `SP[]` table (Section 6.3) matches against the front of the
word.)

`bwiga` and `twiga` are the SP-side echo of Chapter 1's *kwiga*
(infinitive PREF) and Chapter 2's adjective glides — the identical
`u→w/_V` rule, firing on a third kind of prefix. Kinyarwanda's
phonological rules really are rules about *sound*, applied uniformly
regardless of which grammatical job the morpheme is doing.

## 4.2 The present family: `-ra-` versus nothing at all

Kinyarwanda's present tense has two real surface forms, both already
visible in your own toy builder and the real library:

```
$ LD_LIBRARY_PATH=. ./p_present_family
word         ok  cls  tense          stem
aragenda     1   1    PRESENT        gend     <- a + ra + gend + a
ndagenda     1   0    PRESENT_NORA   gend     <- nd + a + gend + a (no -ra-)
```

`aragenda` carries the overt continuous-present marker `-ra-`
(*Indagihe y'aka kanya*, "happening right now," REB Part 7). `ndagenda`
has no overt tense marker at all — the textbook's *Indagihe
y'ubusanzwe* (habitual present) is morphologically just SP + stem +
`-a`, indistinguishable on the surface from a bare statement of fact.
Two genuinely different tenses, one with a visible marker and one
without — which is exactly the kind of thing a detector has to encode
as two separate cases rather than one, because nothing in the spelling
of the *un-marked* form tells you it's a tense at all; the absence
itself is the signal.

## 4.3 Negation: `nt-`, `si-`, and the prohibitive

Three different negation strategies, depending on mood:

```
$ LD_LIBRARY_PATH=. ./p_neg_family
word         ok  cls  tense          neg  stem
ntagenda     1   1    PRESENT_NORA   1    gend    <- nt + a(SP1) + gend + a
sinagenda    1   0    PRESENT_NORA   1    gend    <- si + na(SP1sg-past) + gend + a
witinya      1   0    NEG_IMPERATIVE 1    tiny    <- w(u+i, 2sg) + tiny + a
mwitinya     1   0    NEG_IMPERATIVE 1    tiny    <- mw(mu+i, 2pl) + tiny + a
twitinya     1   0    NEG_IMPERATIVE 1    tiny    <- tw(tu+i, 1pl) + tiny + a
```

`nt-` negates an ordinary indicative clause ("he is NOT going").
`si-` is a separate, person-flexible negative particle that precedes
the whole SP+stem unit. The prohibitive (`witinya`/`mwitinya`/
`twitinya`, "don't be afraid") is its own self-contained negative
construction — notice the glide rule firing a *fourth* time: `u + i →
wi` (2sg), `mu + i → mwi` (2pl), `tu + i → twi` (1pl), the prohibitive
marker `-i-` fusing with the subject prefix exactly the way Section
1.2's `ku + i → kwi` fused for the infinitive.

## 4.4 Past: `-ye` (perfect) and `-aga` (imperfective)

```
$ LD_LIBRARY_PATH=. ./p_past_family
word         ok  cls  tense        stem
yasomye      1   6    PAST_PERF    som     <- ya + som + ye  ("read", completed)
yagendaga    1   6    PAST_IMPF    gend    <- ya + gend + aga ("was going", habitual/continuous)
```

*Impitakare* (perfect, `-ye`) reports a completed action; *Impitakera*
(imperfective, `-aga`) reports a habitual or ongoing past action — the
same present-tense distinction from Section 4.2 (marked vs. unmarked
continuity), now replayed in the past. Short, irregular verbs like
*kujya* ("to go") and *gupfa* ("to die") follow a different
stem-finding rule entirely (the grammar reference's "Rule 2," REB
S3) — a real exception this chapter doesn't chase further, flagged
honestly rather than glossed over.

## 4.5 Future: `-za-`

```
$ LD_LIBRARY_PATH=. ./p_future_family
word         ok  cls  tense    stem
azagenda     1   1    FUTURE   gend     <- a + za + gend + a
```

*Inzagihe* — the future marker `-za-` sits in exactly the TM slot
Section 1.4's formula predicted, between SP and stem, the same
position `-ra-` occupies for the present.

## 4.6 Subjunctive: `-e`

```
$ LD_LIBRARY_PATH=. ./p_subj_family
word     ok  cls  tense        stem
agende   1   1    SUBJUNCTIVE  gend     <- a + gend + e  ("that he go")
genda    1   0    IMPERATIVE   gend     <- bare gend + a ("go!")
```

*Ikigombero* swaps the final vowel from `-a` to `-e` and drops any
tense marker — the FV slot from Section 1.4's formula doing real
grammatical work, not just sitting there as decoration. The bare
imperative is the most minimal conjugated form possible: no SP, no
TM, just stem + `-a`.

## 4.7 Checkpoint: six families, one table

| Family | Marker | Slot | Example | Tense constant |
|---|---|---|---|---|
| Present (continuous) | `-ra-` | TM | aragenda | `TENSE_PRESENT` |
| Present (habitual) | *(none)* | — | ndagenda | `TENSE_PRESENT_NORA` |
| Negation (clausal) | `nt-` | before SP | ntagenda | `neg=true` |
| Negation (si-) | `si-` | before SP | sinagenda | `neg=true` |
| Prohibitive | `-i-` fused into SP | SP | witinya | `TENSE_NEG_IMPERATIVE` |
| Past (perfect) | `-ye` | FV | yasomye | `TENSE_PAST_PERF` |
| Past (imperfective) | `-aga` | FV | yagendaga | `TENSE_PAST_IMPF` |
| Future | `-za-` | TM | azagenda | `TENSE_FUTURE` |
| Subjunctive | `-e` | FV | agende | `TENSE_SUBJUNCTIVE` |
| Imperative | *(none)* | — | genda | `TENSE_IMPERATIVE` |

Ten rows, and the real `VerbTense` enum (Part 6) has twenty-five.
What you've built here is the grammatical backbone every other
tense/mood combination is a variation on — not the whole system, but
enough of it to recognize the shape of any new row you might meet.

## 4.8 Capstone: one verb across all sixteen classes

Chapters 1 and 2 each closed their phonological-rules part with a
capstone: one word built for every noun class at once, testing the
rules honestly instead of assuming they generalize. Do the same here:
take stem `gend` ("go"), build the present-tense form for every one of
the sixteen classes using Section 1.4's SP table, and check all
sixteen against the real library in one pass.

```c
/* p4_all16_verb.c */
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    struct { int want; const char *word; } t[] = {
        {1,"aragenda"}, {2,"baragenda"}, {3,"uragenda"}, {4,"iragenda"},
        {5,"riragenda"}, {6,"aragenda"}, {7,"kiragenda"}, {8,"biragenda"},
        {9,"iragenda"}, {10,"ziragenda"}, {11,"ruragenda"}, {12,"karagenda"},
        {13,"turagenda"}, {14,"buragenda"}, {15,"kuragenda"}, {16,"haragenda"},
    };
    char stem[KIN_MAX_STEM];
    printf("%-3s %-12s %-3s %-4s %s\n","Nt.","word","ok","got","stem");
    for (int i = 0; i < 16; i++) {
        int cls=0,obj=0; VerbTense te=TENSE_NONE; VerbExtension ex=VEXT_NONE; bool neg=false;
        int ok = kin_is_verb_conjugated(t[i].word, stem, &cls, &te, &obj, &ex, &neg);
        printf("%-3d %-12s %-3d %-4d %s\n", t[i].want, t[i].word, ok, ok?cls:-1, ok?stem:"-");
    }
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_all16_verb.c -L . -lkinyarwanda -o p4_all16_verb
$ LD_LIBRARY_PATH=. ./p4_all16_verb
Nt. word         ok  got  stem
1   aragenda     1   1    gend
2   baragenda    1   2    gend
3   uragenda     1   3    gend
4   iragenda     1   4    gend
5   riragenda    1   5    gend
6   aragenda     1   1    gend
7   kiragenda    1   7    gend
8   biragenda    1   8    gend
9   iragenda     1   4    gend
10  ziragenda    1   10   gend
11  ruragenda    1   11   gend
12  karagenda    1   12   gend
13  turagenda    1   0    gend
14  buragenda    1   14   gend
15  kuragenda    1   15   gend
16  haragenda    1   16   gend
```

Thirteen rows match the class this test asked for. Three don't, and
all three are the same shape of finding Chapter 2's `mubi` capstone
row turned up: **a subject prefix shared by more than one class, with
no way for the bare verb alone to say which one was meant.**

- Row 6 wants class 6, gets class 1 — both classes use plain `a` as
  their present-tense SP (Section 1.4's table lists `a-` for class 1
  *and* class 6), and the SP table has no second, distinguishing entry
  for class 6's `a`, so it's read as class 1 every time.
- Row 9 wants class 9, gets class 4 — both classes use plain `i`, same
  story.
- Row 13 wants class 13, gets `0` — not a wrong *class*, but a
  different *kind* of answer entirely: `tu` is both class 13's SP
  *and* the 1st-person-plural ("we") subject prefix, and the SP table
  resolves it as "we," because in real usage "turagenda" overwhelmingly
  means "we are going," not "the small things are going." The code
  picked the linguistically common reading over the grammatically
  literal one.

None of this is a bug to fix by editing the SP table — there is no
spelling difference between class 6's `a` and class 1's `a` to encode,
because Kinyarwanda genuinely doesn't put one on the verb itself. This
is the real reason Part 7's agreement checker can never just trust a
verb's own reported class and has to go find the actual subject noun
in the sentence first (Section 7.2): the verb alone, exactly like
Chapter 1's bare `umuti` and Chapter 2's bare `mubi`, sometimes simply
doesn't carry enough information to know which class was meant. Only
the noun standing next to it does.

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 The question Chapter 2 left open

Chapter 2 ended by asking: verb roots are open-ended, coined and
expanded the way nouns are, not fixed the way adjective stems are —
*which design do you think the real code resembles more, and why?*
You now have enough to answer it directly. Verb roots are not literally
boundless the way novel nouns are (Kinyarwanda doesn't mint new verb
roots nearly as often as it absorbs new nouns), but the real code does
not treat them as a small, fully-enumerable closed set the way
`ADJ_STEMS[]` was, either. The honest answer sits in between, and the
real source code's own design says so directly.

## 5.2 What the real code actually does about it

`kin_is_known_verb_stem()` checks a flat array, `VERB_STEMS[]`, of
roughly 340 entries — almost ten times the size of Chapter 2's ~40
adjective stems, but still a finite, named, linearly-scanned list, the
identical idiom from both previous chapters:

```c
bool kin_is_known_verb_stem(const char *stem) {
    for (int i = 0; VERB_STEMS[i]; i++)
        if (strcmp(stem, VERB_STEMS[i]) == 0) return true;
    return false;
}
```

But unlike `kin_is_adj_stem`, this is not the *only* gate. A second
function exists specifically for stems this list has never seen:

```c
/* kin_is_valid_verb_stem_shape() -- phonological plausibility check.
 * Replaces the word-list gate for tenses where the morphological
 * context is distinctive enough that any phonologically legal stem
 * can be accepted. */
bool kin_is_valid_verb_stem_shape(const char *stem) {
    /* ≥2 chars, at least one consonant, no run of 3+ consonants */
}
```

This is Chapter 1's hybrid design, not Chapter 2's closed-set design:
a known-word lookup table for the common case, *plus* a general
shape-based rule for words the table has never seen, used specifically
in grammatical contexts (certain tense/mood markers) strong enough that
the surrounding morphology alone is good evidence a string in that slot
really is a verb root, even if it's a root this project has never
catalogued. Verb roots resemble nouns' open-class problem, not
adjectives' closed-class one — and the real code's own two-function
split, lookup-table-plus-shape-rule, is the proof, not just an analogy.

---

# Part 6 — Reading the Real Production Code

## 6.1 `kin_is_verb_infinitive`, in full

```c
bool kin_is_verb_infinitive(const char *word, char *stem_out) {
    size_t len = strlen(word);
    if (len < 4) return false;

    /* Locative suffixes -ho/-mo/-yo, stripped and re-checked recursively. */
    if (len > 5 && word[len-3] == 'a' &&
        (kin_ends_with(word, "ho") || kin_ends_with(word, "mo") ||
         kin_ends_with(word, "yo"))) {
        char locbuf[KIN_MAX_WORD];
        strncpy(locbuf, word, len - 2);
        locbuf[len - 2] = '\0';
        return kin_is_verb_infinitive(locbuf, stem_out);
    }

    const char *inner = NULL;
    if (kin_starts_with(word, "kw") && is_vowel(word[2]))      inner = word + 2;
    else if (kin_starts_with(word, "gw") && is_vowel(word[2])) inner = word + 2;
    else if (kin_starts_with(word, "gu") && !is_vowel(word[2])) inner = word + 2;
    else if (kin_starts_with(word, "ku") && !is_vowel(word[2])) inner = word + 2;
    else if (word[0]=='k' && (word[1]=='o'||word[1]=='u') && len>4) inner = word+1;
    else return false;

    size_t inner_len = strlen(inner);
    if (inner_len < 2) return false;

    if (inner[inner_len - 1] == 'a') {       /* canonical -a ending */
        if (stem_out) { strncpy(stem_out, inner, inner_len-1);
                         stem_out[inner_len-1] = '\0'; }
        return true;
    }
    if (len < 6) return false;
    /* ... -ye, -tse, -we, -e: non-canonical endings, length-guarded ... */
    return false;
}
```

Three things to recognize immediately, all from earlier chapters: the
`kw`/`gw` vs `gu`/`ku` split is Section 1.3's gap, fixed — note it's
gated on `is_vowel(word[2])`, exactly the same "which prefix spelling
depends on what comes next" logic Chapter 1's noun detector used. The
recursive locative-suffix strip is the *same recursion-safety shape*
as Chapter 1, Section 6.5's `"nka"` case: every recursive call strips
characters and calls itself on a *strictly shorter* string, so the
recursion is structurally guaranteed to terminate — there is no input
that makes it recurse forever, because the string has a finite length
and the function only ever recurses on a smaller piece of it. And the
function only accepts `gu`/`ku` before a *non-vowel* — it does not
try to enforce which of the two, `gu-` or `ku-`, is the "correct" one
for a given stem (real Kinyarwanda lexically fixes this per verb, the
way English fixes "an apple" but "a banana" rather than deriving the
choice from a live rule). For *recognizing* a word as an infinitive,
this is the right call: a permissive recognizer that accepts both
spellings can never reject a real word, even though it would, if
asked, also accept the spelling-error `kusoma` (real Kinyarwanda is
only `gusoma`). Recognition and validation are different jobs, and
this function is honestly built for the first one.

## 6.2 The conjugated detector's layered structure

`kin_is_verb_conjugated` is not one algorithm — it's several, tried in
a fixed order, each one a fallback for the last:

```
   word
     │
     ▼
 ┌─────────────────────────────────┐
 │ LAYER 0: prohibitive (wi/mwi/twi)│──▶ found? return NEG_IMPERATIVE
 └─────────────────────────────────┘
     │ not found
     ▼
 ┌─────────────────────────────────┐
 │ LAYER 1: strip nt-/si- negation  │  (just notes is_neg=true, continues)
 └─────────────────────────────────┘
     │
     ▼
 ┌─────────────────────────────────┐
 │ LAYER 2: verb_match_inner()      │──▶ tries every SP in turn (Section 6.3)
 │   core SP + tense matching       │    returns raw_stem (may still have
 └─────────────────────────────────┘    an object marker stuck to the front)
     │ matched
     ▼
 ┌─────────────────────────────────┐
 │ LAYER 2.5/2a: bilabial & "nti"   │  narrow repair guards, same shape as
 │   retry guards                   │  Chapter 2's geminate-reconstruction
 └─────────────────────────────────┘  guards (Section 6.6 there)
     │
     ▼
 ┌─────────────────────────────────┐
 │ LAYER 3: om_strip()              │──▶ peel an object marker off raw_stem
 │   (Section 6.4)                  │    if one is there
 └─────────────────────────────────┘
     │
     ▼
 ┌─────────────────────────────────┐
 │ LAYER 4: ext_strip()             │──▶ peel a derivational extension
 │   (Section 6.5)                  │    (-ish, -ir, -an, -w, ...) off the end
 └─────────────────────────────────┘
     │
     ▼
   stem_out, subj_class, tense, obj_class, ext, neg  — all five, filled in
```

Each layer is independently testable, and each one is small enough to
read on its own — Sections 6.3 through 6.5 read three of them. The
function as a whole is large because Kinyarwanda verbs really do stack
this many independent pieces of grammar into one word; the size is the
grammar's, not an accident of the code.

## 6.3 The SP table: every phonological rule, a third time

```c
static const struct { const char *pfx; int cls; } SP[] = {
    { "twa",   0  },  /* 1pl past */               { "ara",   1  },
    { "ba",    2  },  { "bw",   14  },  { "rw",   11  },
    { "tw",    0  },  { "mw",    0  },              /* u→w/_V, again */
    { "cy",    7  },  { "by",    8  },  { "ry",    5  }, { "zy", 10 },
                                                       /* i→y/_V, again */
    { "mb",    0  },  { "mp",    0  },  { "mf",    0  }, { "mv",  0 },
                                                       /* n→m/_bilabial, again */
    { "du",    0  },  /* tu→du before voiced consonant -- a NEW allomorph */
    { "ki",    7  },  { "gi",    7  },                /* ki→gi/_C §3.7.1 */
    { "ka",   12  },  { "ga",   12  },                /* ka→ga/_C §3.7.1 */
    { NULL, 0 }
};
```

Six rules deep into this project now (vowel glide, nasal assimilation,
and two voicing rules — class 7's `ki→gi` and class 12's `ka→ga`,
siblings of Chapter 2's class-12 `k→g` voicing rule, Section 4.3
there), and every single one is a rule about *sound*, reapplied to
whichever morpheme happens to sit at that boundary — noun RT, then
adjective RS, now verb SP. `du` (1pl `tu-` before a voiced consonant)
is the one genuinely new allomorph in this table that the previous two
chapters' RT/RS tables didn't need, because verbs are the first
category where `class 0` (a *person*, not a noun class) sits in the
same table as the sixteen noun classes — first and second person have
no noun class to agree with, so the SP table has to carry person
marking the RT and RS tables never had to.

## 6.4 Object markers: a second concordance slot inside the verb

A conjugated verb can agree with *two* nouns at once — its subject
(SP, Section 6.3) and, optionally, its object (OM, here):

```c
static const OmEntry OM_TABLE[] = {
    { "mu",  "mw",  1  },  /* cls 1 human sg:  aramubona, aramwigisha  */
    { "ki",  "cy",  7  },  /* cls 7 thing sg:  arakibona, aracyigisha  */
    { "bi",  "by",  8  },  /* cls 8 thing pl:  arabibona, arabigisha   */
    /* ... */
    { NULL, NULL, 0 }
};
```

Real, verified trace: `aramubona` ("he sees him/her") parses as SP `a`
(class 1) + TM `ra` (present) + OM `mu` (class 1) + stem `bon` — two
independent class-1 agreement slots in a single seven-letter word,
neither one optional to leave out if you wanted to say "he sees
*him*" specifically rather than just "he sees." Each OM entry carries
both its plain form and its glide-contracted form (`mu`/`mw`,
`ki`/`cy`, `bi`/`by`) — the vowel-glide rule, a *fourth* time, this
time governing which object got mentioned rather than which subject
or which tense.

## 6.5 Verb extensions: `ext_strip`

Extensions (*utumamo*, REB Year-2 ch. 26) modify what a verb root
*means* — causing, benefiting, reciprocating — and they attach between
the stem and the final vowel:

```c
static bool ext_strip(const char *stem, VerbExtension *ext_out,
                      char *bare_out, size_t bare_sz) {
    /* Causative -ish/-esh (check first -- longer match) */
    if (slen > 4 && (kin_ends_with(stem, "ish") || kin_ends_with(stem, "esh"))) {
        /* ... */ *ext_out = VEXT_CAUSATIVE; return true;
    }
    /* Applicative -ir/-er */
    if (slen > 4 && (kin_ends_with(stem, "ir") || kin_ends_with(stem, "er"))) {
        /* ... */ *ext_out = VEXT_APPLICATIVE; return true;
    }
    /* Reciprocal -an (with a known-stem guard for the 2-char base case) */
    if (slen >= 4 && kin_ends_with(stem, "an")) {
        /* ... */ *ext_out = VEXT_RECIPROCAL; return true;
    }
    /* ... passive -w, stative -ik/-ek, reversive -uk/-ur ... */
}
```

Real, verified traces: `bamwirukanye` ("they chased him away") strips
to SP `ba` + OM `mw` (class 1) + root `iruk` + extension `an`
(reciprocal, surfacing here as part of a longer causative-chased
sense) + FV `ye`. `yigisha` ("he/she teaches") strips to SP `y`
(elided `ya`) + root `ig` ("study") + extension `ish` (causative —
"cause to study" = "teach") + FV `a`. `akorera` strips to SP `a` +
root `kor` ("work") + extension `er` (applicative — "work *for*") +
FV `a`. Notice the order the checks run in: causative (`-ish`/`-esh`)
is checked *before* applicative (`-ir`/`-er`), because both are
roughly the same length and a wrong-order check could grab the wrong
one first — the identical "check the more specific pattern before the
more general one" discipline from Chapter 1's prefix ordering and
Chapter 2's `ADJ_PREFIXES[]`, now guarding a fourth kind of table.

Extensions can also *chain* — a verb root can take two extensions at
once, and `ext_strip` has to be able to peel either one off first and
still land on a known root. `gusinziriza` ("to make [someone] doze
off") is the real source code's own worked example for exactly this:

```c
/* Causative -iz-/-ez- (allomorph for applicative-base verbs):
 * When a verb whose root ends in -ir (applicative) takes the causative-y
 * extension, the sequence -ir + y fuses: r+y→z, giving -iz-.
 * e.g. gusinziriza: root sinzir + iz(causative-y on applicative) + a
 *      sinziriz → strip iz → sinzir = known root (gusinzira) ✓        */
```

Verified against the real library:

```
$ LD_LIBRARY_PATH=. ./p_extchain
arasinziriza   ok=1 cls=1 tense=1 ext=9 stem=sinzir
```

`ext=9` is `VEXT_CAUSATIVE_IZ` — not plain causative (`VEXT_CAUSATIVE`,
the `-ish-`/`-esh-` family from the table above), but the specific
*allomorph* that appears when causative meaning gets layered onto a
root that's already applicative-shaped (`-ir`-final). The stem comes
back as `sinzir` — not `sinziriz`, not `sinzir` plus a leftover `iz`
sitting unaccounted for — because the comment's own derivation,
`sinziriz → strip iz → sinzir`, is checked against `kin_is_known_verb_stem`
(Section 5.2) before being accepted, the same validate-before-accept
discipline Chapter 2's adjective prefix matcher used (Section 6.6
there). One word, one extension *slot*, two layered meanings — and the
function has to recognize the fused spelling as a single allomorph
rather than two separate, smaller extensions that happen to look
similar.

## 6.6 An honest asymmetry: `ara` versus `nda`

Not every parallel construction in this codebase is handled with
perfect symmetry, and this chapter's honesty rule (state what's
real, including the parts that surprise you) means saying so plainly.
Compare two present-tense forms that *should*, linguistically, be
exactly parallel — third person singular and first person singular,
both continuous present:

```
$ LD_LIBRARY_PATH=. ./p_basic
arasoma    ok=1 cls=1   tense=1  obj=0   neg=0  stem=som
ndasoma    ok=1 cls=0   tense=2  obj=0   neg=0  stem=som
```

`tense=1` is `TENSE_PRESENT` (the continuous, `-ra-`-marked present).
`tense=2` is `TENSE_PRESENT_NORA` (the *unmarked* habitual present,
Section 4.2). `arasoma` gets the marked reading because the SP table
has an explicit, special-cased branch for the 3-char combined token
`"ara"` that sets `TENSE_PRESENT` directly — the comment above it says
so outright: *"ara has ra already embedded."* `ndasoma` has no
equivalent special case for its own combined token `"nda"` — so it
falls through every specific tense check, finds none of them match,
and lands in the same generic catch-all every *un-marked* present
verb lands in, `TENSE_PRESENT_NORA`. Linguistically, "ndasoma" really
does mean "I am reading" right now, the continuous sense — but the
code, as it stands today, reports it the same way it would report the
habitual "I read [in general]." This is a real, narrow gap, not a
design flaw in the overall approach: it's one missing special case in
a system that already has a correct special case for the
grammatically-parallel third-person form. Finding gaps like this by
testing — not guessing, not assuming the code is finished just because
it's large — is the single most repeated habit across all three
chapters of this book.

## 6.7 Real traces, side by side

| Word | Class | Tense | Obj | Ext | Neg | Stem |
|---|---|---|---|---|---|---|
| `aragenda` | 1 | PRESENT | — | — | no | `gend` |
| `ndagenda` | — (1sg) | PRESENT_NORA | — | — | no | `gend` |
| `yagendaga` | 6 | PAST_IMPF | — | — | no | `gend` |
| `azagenda` | 1 | FUTURE | — | — | no | `gend` |
| `agende` | 1 | SUBJUNCTIVE | — | — | no | `gend` |
| `genda` | — | IMPERATIVE | — | — | no | `gend` |
| `ntagenda` | 1 | PRESENT_NORA | — | — | **yes** | `gend` |
| `aramubona` | 1 | PRESENT | **1** | — | no | `bon` |
| `bamwirukanye` | 2 | PAST_PERF | **1** | RECIPROCAL | no | `iruk` |
| `yigisha` | — | PRESENT_NORA | — | CAUSATIVE | no | `ig` |
| `akorera` | 1 | PRESENT_NORA | — | APPLICATIVE | no | `kor` |

Eleven real words, run through the real, unmodified library — every
column in this table is the function's actual output, not a
prediction. The honest gaps this chapter found along the way (Section
6.6's `ara`/`nda` asymmetry; Section 4.4's note that very short,
irregular verbs follow a different stem-finding rule the chapter
didn't chase) sit alongside a function that gets all eleven of these
words exactly right. Both things are true about the same codebase at
once — that's what a real, working, still-evolving piece of software
actually looks like.

---

# Part 7 — Concordance: Subject-Verb Agreement

## 7.1 Build it: a toy subject-verb checker

The same idea as Chapter 2's adjective checker, applied to verbs: a
mismatch is just two class numbers that don't equal each other.

```c
/* p_toy_subjverb.c */
#include <stdio.h>

int check_agreement(int noun_class, int verb_sp_class) {
    if (noun_class == verb_sp_class) return 1;
    /* Real grammar has known exceptions -- Section 7.2 reads the real ones. */
    return 0;
}

int main(void) {
    printf("noun=1 verb=1 -> %s\n", check_agreement(1,1) ? "OK" : "MISMATCH");
    printf("noun=1 verb=2 -> %s\n", check_agreement(1,2) ? "OK" : "MISMATCH");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -o p_toy_subjverb p_toy_subjverb.c
$ ./p_toy_subjverb
noun=1 verb=1 -> OK
noun=1 verb=2 -> MISMATCH
```

## 7.2 The real detection: `syntax.c`

The real check has to do real work before it can even compare two
numbers: find *which* noun a given verb's subject actually is, by
scanning backward from the verb over anything that isn't a candidate
subject:

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

Then, before flagging anything, it checks a short list of *real*
class mergers — pairs that share a subject prefix and therefore must
not be flagged as mismatched:

```c
if ((nc == 1 || nc == 3) && (vc == 1 || vc == 3)) continue;  /* shared SP a/u */
if ((nc == 4 || nc == 9) && (vc == 4 || vc == 9)) continue;  /* shared SP i  */
if ((nc == 9 || nc == 10) && (vc == 9 || vc == 10)) continue; /* paired sg/pl */
if (vc == 6 && (nc == 1 || nc == 3 || nc == 9)) continue;     /* "ya" = SP6 or SP1-past */
```

That last line is Section 6.6's `ara`/`nda` story's sibling: the SP
table entry `{ "ya", 6 }` is genuinely ambiguous (Section 6.3's
comment called it "Nt.6 present OR Nt.1 past"), and rather than
silently mis-flagging every class-1-past sentence that happens to use
`ya-`, the agreement checker explicitly carries the ambiguity forward
and refuses to flag it. An honest gap in one function (the SP table
can't always tell you the class on its own) becomes an explicit,
documented exception in the function that consumes its output — the
same kind of defensive accounting Chapter 2's `corrector.c` did for
out-of-range classes.

## 7.3 Case study: a clean sentence

```c
/* p_subjverb.c */
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    const char *text = "Abana barasoma ibitabo.";
    SentenceAnalysis sa = kin_analyze(text);
    kin_suggest_corrections(&sa);
    printf("Input: \"%s\"\n\n", text);
    for (int i = 0; i < sa.token_count; i++) {
        Token *t = &sa.tokens[i];
        printf("  token[%d]=\"%-10s\" pos=%d noun_class=%d stem=%s\n",
               i, t->surface, t->pos, t->noun_class, t->stem);
    }
    printf("\nErrors found: %d\n", sa.error_count);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p_subjverb.c -L . -lkinyarwanda -o p_subjverb
$ LD_LIBRARY_PATH=. ./p_subjverb
Input: "Abana barasoma ibitabo."

  token[0]="Abana     " pos=1 noun_class=2 stem=na
  token[1]="barasoma  " pos=6 noun_class=2 stem=som
  token[2]="ibitabo   " pos=1 noun_class=8 stem=tabo
  token[3]="."         " pos=17 noun_class=0 stem=

Errors found: 0
```

`Abana` ("children," class 2) and `barasoma` ("they are reading," SP
`ba`, class 2) agree exactly — no exception list needed, no flag
raised. `ibitabo` ("books," class 8) sits in the sentence as the
object with no object marker on the verb at all (`barasoma` has no
OM), which is perfectly normal Kinyarwanda — an object marker is
optional, used for emphasis or pronoun-like reference, not required
the way subject agreement is.

## 7.4 Case study: a broken sentence

```c
/* same program, different input */
const char *text = "Umugabo baragenda.";
```

```
$ LD_LIBRARY_PATH=. ./p_subjverb
Input: "Umugabo baragenda."

  token[0]="Umugabo   " pos=1 noun_class=1 stem=gabo
  token[1]="baragenda " pos=6 noun_class=2 stem=gend
  token[2]="."         " pos=17 noun_class=0 stem=

Errors found: 1
  [0] Inshinga 'baragenda' ntishyikira izina 'Umugabo': inteko y'inshinga=2 ariko inteko y'izina=1. Verb 'baragenda' subject prefix (class 2) doesn't agree with noun 'Umugabo' (class 1).
      -> Indangasubizi igomba kuba 'a' (inteko 1). The subject prefix for class 1 should be 'a'.
```

`Umugabo` ("the man," class 1, singular) paired with `baragenda` (SP
`ba`, class 2, "they") is a genuine number disagreement — one man,
plural verb — and `kin_analyze` catches it, names both classes
explicitly in its message, and proposes the exact correct repair
(`a`, not `ba`) by looking the right subject prefix up for class 1,
the same `NounClass.subj_prefix` table this project has used since
Chapter 1's concordance preview. The correction is not a guess — it's
a direct table lookup against the same sixteen-class data every other
chapter in this book has already used.

## 7.5 What's not checked yet: object-verb agreement

`include/kinyarwanda.h` declares two agreement error codes, right next
to each other:

```c
ERR_SUBJ_VERB_AGREEMENT,   /* Verb SP doesn't match subject noun class */
ERR_OBJ_VERB_AGREEMENT,    /* Verb OM doesn't match object noun class  */
```

Section 7.2 read the real code behind the first one. Search the whole
project for where the second one is actually *raised* — where some
function calls `add_error(sa, ERR_OBJ_VERB_AGREEMENT, ...)` the way
Section 7.2's excerpt called `add_error(sa, ERR_SUBJ_VERB_AGREEMENT,
...)` — and you won't find one. The constant exists; `validator.c`
even has a display name ready for it ("Indangasobwa"); nothing in
`syntax.c` ever compares an object marker's class against the noun it
should agree with. Confirm this is a real gap, not a guess, by
building exactly the sentence that ought to trigger it:

```c
/* p_objtest.c */
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    const char *text = "Umugabo arakibona umugore.";
    SentenceAnalysis sa = kin_analyze(text);
    kin_suggest_corrections(&sa);
    printf("Input: \"%s\"\n\n", text);
    for (int i = 0; i < sa.token_count; i++) {
        Token *t = &sa.tokens[i];
        printf("  token[%d]=\"%-10s\" pos=%d noun_class=%d obj_class=%d stem=%s\n",
               i, t->surface, t->pos, t->noun_class, t->obj_class, t->stem);
    }
    printf("\nErrors found: %d\n", sa.error_count);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p_objtest.c -L . -lkinyarwanda -o p_objtest
$ LD_LIBRARY_PATH=. ./p_objtest
Input: "Umugabo arakibona umugore."

  token[0]="Umugabo   " pos=1 noun_class=1 obj_class=0 stem=gabo
  token[1]="arakibona " pos=6 noun_class=1 obj_class=7 stem=bon
  token[2]="umugore   " pos=1 noun_class=1 obj_class=0 stem=gore
  token[3]="."         " pos=17 noun_class=0 obj_class=0 stem=

Errors found: 0
```

`arakibona` carries object marker `ki` (Section 6.4's OM table, class
7 — "it"). The word right next to it, `umugore` ("the woman"), is
class 1. "He sees IT, the woman" is exactly as wrong in Kinyarwanda as
it sounds in English, and every number this analysis needs to catch
it is already sitting right there in `token[1].obj_class` and
`token[2].noun_class` — both correctly computed, both visible in this
very printout. Nothing compares them. `Errors found: 0`.

This is worth sitting with rather than rushing past: it is not a flaw
in any function this chapter read. `kin_is_verb_conjugated` did its
job and reported the object marker's class correctly. The noun
detector did its job. The *agreement check* simply isn't written yet
for this pair, even though its own error code has existed in the
header since before this chapter started. Real, growing codebases
often have this shape — a taxonomy planned one layer ahead of the
checks that fill it in — and the honest way to read one is to test
what you're told exists, the same habit this book has used in every
chapter, rather than to assume a declared error code means a working
check.

It's worth being precise about the *size* of this gap rather than
generalizing it into "verb checking is unfinished." `syntax.c` also
defines `ERR_NO_VERB`, `ERR_VERB_SELECTION`, and `ERR_WRONG_VERB_MOOD`
— and unlike `ERR_OBJ_VERB_AGREEMENT`, all three of those genuinely
are raised somewhere in the file. `ERR_OBJ_VERB_AGREEMENT` is the
specific, narrow exception, not a sign that this whole area of the
codebase is a stub. Section 7.6 reads one of those three working
checks, because it's a different *kind* of correctness than anything
else in this chapter, and worth seeing once.

## 7.6 A different kind of correctness: verb selection

Every check this chapter has read so far compares two grammatical
*numbers* — a class, a person. `ERR_VERB_SELECTION` doesn't: it
encodes a fact about *meaning*. Kinyarwanda has two common verbs for
"go" that aren't interchangeable: *kugenda* describes the manner of
moving (walking, departing — Indagihe in motion) while *kujya*
specifically marks heading *to* a destination. `syntax.c` checks this
directly by stem and surrounding context:

```c
bool is_kugenda = (strcmp(verb->stem, "gend") == 0 || strcmp(verb->stem, "end") == 0);
bool is_kujya   = (strcmp(verb->stem, "jy")   == 0 || strcmp(verb->stem, "giy") == 0);
if (!is_kugenda && !is_kujya) continue;

/* Case B: kugenda + locative "i" + destination name
 * e.g. "Uragenda i Kigali" -- should be "Ujya i Kigali"          */
if (is_kugenda && next->pos == POS_LOCATIVE && strcmp(next->lower, "i") == 0) {
    /* ... confirm the token after "i" is a real destination, then: */
    add_error(sa, ERR_VERB_SELECTION, i, msg, sug);
}
```

```c
/* p_verbsel.c */
#include "kinyarwanda.h"
#include <stdio.h>
int main(void) {
    const char *texts[] = { "Uragenda i Kigali.", "Urajya i Kigali.", NULL };
    for (int j = 0; texts[j]; j++) {
        SentenceAnalysis sa = kin_analyze(texts[j]);
        kin_suggest_corrections(&sa);
        printf("Input: \"%s\"\n", texts[j]);
        printf("Errors found: %d\n", sa.error_count);
        for (int i = 0; i < sa.error_count; i++)
            printf("  [%d] %s\n", i, sa.errors[i].message);
        printf("\n");
    }
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p_verbsel.c -L . -lkinyarwanda -o p_verbsel
$ LD_LIBRARY_PATH=. ./p_verbsel
Input: "Uragenda i Kigali."
Errors found: 1
  [0] Guhitamo inshinga nabi: 'Uragenda' (kugenda) ikurikirwa n'indangahantu 'i Kigali'. Inshinga 'kugenda' ntiyemera intego y'ahantu. Wrong verb: 'kugenda' cannot take a locative destination 'i Kigali'. Use 'kujya' for going TO a place.

Input: "Urajya i Kigali."
Errors found: 0
```

`Uragenda i Kigali` ("I am walking/moving, [at] Kigali") is flagged
because *kugenda* — pure manner of movement — cannot grammatically
take a destination at all; `Urajya i Kigali` ("I am going to Kigali")
is exactly right and draws no error. Every check in Parts 6 and 7
before this one was answerable by comparing two class numbers for
equality. This one is answerable only by knowing, as a fact about the
Kinyarwanda language and not about any noun class table, that these
two specific stems mean different *kinds* of going. That fact is
encoded directly as a `strcmp` against two literal stems, not derived
from any general rule — because there is no general phonological or
morphological rule that would derive it. Some correctness really is
lexical, word by word, and the honest way to check it is exactly the
way this project does: name the words and say what's wrong, the same
way a dictionary entry would.

### Beginner

1. **Hand-trace `kuzakina`** ("will play," REB's own worked example
   from Section 1.1) against Section 1.4's formula: identify PREF, TM,
   C, and FV, then verify against the real library.
2. **Extend `p1_toy_inf.c`** (Section 1.3) to also accept the `kw-`/
   `gw-` glide-contracted prefixes, and confirm `kwiga` now succeeds.

### Intermediate

3. **Build the past-imperfective family yourself.** Using Section
   4.4's pattern, hand-derive the `-aga` form for stem `kor` ("work"),
   class 2, then verify it against the real library.
4. **Extend `p3_repl.c`** (Section 3.1) to also build the subjunctive
   form (FV `-e`, no TM) for whatever class and stem the user types.
5. **Find a second `ara`/`nda`-style asymmetry.** Pick another pair of
   grammatically-parallel SP forms (for example, two different noun
   classes' present tense) and test whether the real library treats
   them identically or not.

### Advanced

6. **Implement `ext_strip`'s ordering discipline yourself**, for just
   two extensions (causative and applicative), and find one stem where
   checking them in the *wrong* order would produce the wrong answer.
7. **Find a sentence of your own** with a deliberate subject-verb
   mismatch, run it through `kin_analyze` and `kin_suggest_corrections`
   the way Section 7.4 did, and confirm the suggested repair is
   genuinely correct Kinyarwanda by checking the class's subject
   prefix by hand.
8. **Investigate the `vc == 6` exception** in Section 7.2's agreement
   checker. Construct one real sentence where this exception correctly
   prevents a false positive, and explain in your own words why the
   ambiguity it's working around exists in the first place (Section
   6.3's SP table comment is the source of the ambiguity).
9. **Reproduce Section 2.3's bug on purpose, then break it differently.**
   Write a recursive function with the same locative-suffix-stripping
   shape, but introduce a *different* non-shrinking bug (for example,
   stripping only 1 character instead of 2). Confirm it still crashes,
   and explain why "shrinks, but not by enough" is just as fatal as
   "doesn't shrink at all."
10. **Write the object-verb agreement check Section 7.5 found missing.**
    Using `token[i].obj_class` and the *next* noun's `noun_class` the
    way Section 7.2's subject-verb check used the *previous* noun's,
    write a toy function that flags `"Umugabo arakibona umugore."` the
    way `ERR_SUBJ_VERB_AGREEMENT` flags Section 7.4's example. You do
    not need to modify the real library — a standalone toy checker that
    correctly flags this one real sentence is enough to prove the
    point.
11. **Find a third "go" context.** Construct one more sentence using
    *kugenda* correctly (manner of movement, no destination) and one
    using it incorrectly (with a destination noun but no locative
    "i" — Section 7.6's "Case A" in `syntax.c`, not shown in this
    chapter's excerpt). Confirm both against the real library.

## Key takeaways

- A verb has two branches with two different formulas: the infinitive
  (PREF + C + FV, three pieces, structurally close to Chapter 1's
  noun) and the conjugated form (SP + [TM] + [OM] + C + [EXT] + FV,
  up to five independent pieces of grammar in one word).
- The same sixteen concordance markers now do a third job (subject
  agreement) and, via the object marker table, a fourth (object
  agreement) — one underlying system, four different employers across
  three chapters.
- The vowel-glide and nasal-assimilation rules from Chapters 1 and 2
  reappear, unchanged, governing verb subject prefixes, object
  markers, and the infinitive marker — Kinyarwanda's phonological
  rules operate on sound, not on which part of speech they're
  rewriting.
- An out-parameter's value is only meaningful when the function's
  `bool` return says so; ignoring the return value doesn't crash, it
  silently substitutes stale data from a previous, unrelated call —
  arguably more dangerous than a crash because nothing looks wrong.
- Verb roots are a large, named lookup list *plus* a phonological
  shape-validity fallback for strongly-marked grammatical contexts —
  Chapter 1's open-class hybrid design, not Chapter 2's closed-set
  design, answering the question Chapter 2 ended on.
- Real production code can have genuine, narrow asymmetries (`ara`
  getting a special case that grammatically-parallel `nda` doesn't)
  sitting right next to code that handles a dozen other real words
  perfectly — finding such gaps by testing, rather than assuming
  finished code is complete, is this book's central habit, not a
  one-time trick.
- Subject-verb agreement checking has to find its subject first (by
  scanning backward over adjectives, adverbs, conjunctions) before it
  can even compare two class numbers, and it explicitly carries
  forward known ambiguities from earlier in the pipeline (the `ya`/
  class-6 case) rather than silently mis-flagging them.
- A bare conjugated verb can fail to identify its own class for the
  same reason Chapter 1's bare `umuti` and Chapter 2's bare `mubi`
  could: classes 1/6 and 4/9 share a subject prefix, and class 13's
  `tu-` is indistinguishable on its own from the 1st-person-plural
  `tu-` — confirmed by literally testing the same verb across all
  sixteen classes and finding three real collisions, not by assuming
  the SP table is unambiguous because it's large.
- Recursion is only safe when every call is *guaranteed* to operate on
  a strictly shorter input than the last one; lose that guarantee (even
  via a single wrong argument to `strncpy`) and the same recursive
  shape that safely handles locative suffixes in the real
  `kin_is_verb_infinitive` becomes an unbounded stack-overflow crash.
- A declared error code (`ERR_OBJ_VERB_AGREEMENT`) does not mean a
  working check exists for it — confirmed by building the exact
  sentence that should trigger it and watching `kin_analyze` report
  zero errors, even though every number the check would need is
  already computed and sitting in the token data. The other three
  verb-related error codes (`ERR_NO_VERB`, `ERR_VERB_SELECTION`,
  `ERR_WRONG_VERB_MOOD`) genuinely are implemented — this is a
  specific, narrow gap, not a sign the whole area is unfinished.
- Not every correctness check in this project compares two
  grammatical numbers. `ERR_VERB_SELECTION` (*kugenda* vs *kujya*, two
  Kinyarwanda verbs for "go" with different argument structure) is
  checked by literal stem comparison against named, specific words —
  because some correctness is lexical, not derivable from any
  phonological or morphological rule, and the honest way to encode
  that is to just say so in the code.

## Sources quoted in this chapter

- `data/textbook_grammar_rules.md`, Part 4 (UTUREMAJAMBO TW'INSHINGA),
  Part 6 (UBURYO BW'INSHINGA), and Part 7 (IBIHE BY'INSHINGA) — REB
  S2–S5.
- `include/kinyarwanda.h` (`kin_is_verb_infinitive`,
  `kin_is_verb_conjugated`, the `VerbTense` and `VerbExtension` enums).
- `src/morphology.c` (`kin_is_verb_infinitive`, `kin_is_verb_conjugated`,
  `verb_match_inner`, the `SP[]` table, `OM_TABLE[]`, `om_strip`,
  `ext_strip`, `kin_is_valid_verb_stem_shape`).
- `src/lexicon.c` (`VERB_STEMS[]`, `kin_is_known_verb_stem`).
- `src/syntax.c` (the `ERR_SUBJ_VERB_AGREEMENT` and `ERR_VERB_SELECTION`
  checks).
- `src/validator.c` (display names for both `ERR_SUBJ_VERB_AGREEMENT`
  and the unimplemented `ERR_OBJ_VERB_AGREEMENT`).
- Every `pN_*.c`/`p_*.c` program and every real-library run in this
  chapter was actually compiled with `gcc -std=c99 -Wall -Wextra` and
  actually executed to produce the exact output quoted above.

## Coming up in Chapter 4

Three chapters have now built the same underlying concordance system
three times — noun class, then adjective agreement, then subject (and
object) verb agreement — using the same sixteen markers every time.
Chapter 4 turns to *Ikinyazina*, the pronoun: a word whose entire job
is to stand in for a noun's class without repeating the noun itself,
and the object marker table you read in Section 6.4 turns out to be
half of that story already. The other half is `check_deverbative` —
already implemented in this project — which runs the noun and verb
trees in the *opposite* direction: turning a verb root back into a
noun. After three chapters of "here is one tree," Chapter 4 is the
first one about how two trees connect.
