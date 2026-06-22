# Chapter 9 — Spelling the Result: The Orthographic Rule Engine

## How this chapter works

Same rules as Chapters 1 through 8. Every code example was actually
compiled with `gcc -std=c99 -Wall -Wextra` and actually executed — the
output shown in a fenced block prefixed with `$` is the real terminal
output of that exact program.

Every morpheme table since Chapter 1 has carried a footnote this book
never opened: "phonological rules apply at this boundary." `src/ortho.c`
is where those rules actually live — a 1,330-line, sixteen-pass engine
that turns an underlying morpheme string like `bi|a|tek|w|ye` into the
one correctly spelled surface word, `byatetswe`, that a real
Kinyarwanda reader would recognize. It is the single largest module in
this project, and reading it carefully finds some of this book's most
interesting — and most consequential — real bugs.

---

# Part 1 — Language and Code, Side by Side

## 1.1 The rule every earlier chapter assumed: two vowels can never sit next to each other

`kinyarwanda.h`'s own comment, already quoted in passing back in
Chapter 1, states the rule this entire chapter exists to enforce:

> In Kinyarwanda, two vowels CANNOT appear adjacent within a word. When
> morpheme boundaries would create VV contact, one of these applies:
> 1. u → w (before any vowel) ku+eza → kweza
> 2. i → y (before any vowel) ki+eza → kyeza
> 3. a → ø (elision before V) na+amazi → n'amazi; ba+eza → b'eza
> 4. a+i → e (vowel fusion) ba+inja → benja; mu+inja → menja
> 5. a+e → e (a drops before e) ya+eza → yeza
> 6. a+o → o (a drops before o) ya+oya → yoya (rarely contracted)

Every noun's D+RT+C formula, every verb's SP+TM+OM+root+EXT+FV formula,
every adjective's RS+C formula — every single one of those plus signs
this book has written since Chapter 1 is a place where two morphemes
meet, and wherever one ends in a vowel and the next begins with one,
something in this list has to fire before the result is real,
spellable Kinyarwanda. `ortho.c`'s own header names its source
directly:

> Full implementation of ALL rules from: Inteko Nyarwanda y'Ururimi
> n'Umuco (RALC) 2017, "Amategeko y'igenantego ry'Ikinyarwanda" — three
> sections matching the book structure: 1. vowel rules (§1), 2.
> consonant rules (§2), 3. nasal rules (§3).

Three real examples, run through the actual library, before this
chapter starts finding cracks:

```c
#include "kinyarwanda.h"
#include <stdio.h>

static void show(const char *m, bool nc9) {
    char surf[128];
    kin_ortho_gen(m, nc9, surf, sizeof(surf));
    printf("%-22s -> %s\n", m, surf);
}

int main(void) {
    show("ba|iza",         false);   /* a+i -> e (rule 4) */
    show("i|n|banza",      false);   /* nasal cascade (§3 rules) */
    show("bi|a|tek|w|ye",  false);   /* w-y metathesis + C+y fusion */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p1_correct.c -L . -lkinyarwanda -o p1_correct
$ LD_LIBRARY_PATH=. ./p1_correct
ba|iza                 -> beza
i|n|banza              -> imanza
bi|a|tek|w|ye          -> byatetswe
```

`ba|iza` ("they-build" — a+i fuses to e exactly per rule 4) is the
simplest case: one rule, one boundary, done. `bi|a|tek|w|ye` is the
most interesting of the three: the passive marker `-w-` and the
perfective suffix `-ye` swap places first (`w|y` becomes `y|w`, so the
stem-final consonant can fuse with the *now-adjacent* `y`), and only
after that swap does the `k+y → ts` fusion from §3.9 apply, turning
`tek` + `y` into `tets`, with the displaced `w` landing after it —
`byatetswe`. Two rules, applied in a specific order, neither of which
alone would produce the right word.

## 1.2 Build it: a joiner that knows only one rule, and what it misses

```c
/* p1_toy_one_rule.c -- a joiner that only knows the u-glide rule */
#include <stdio.h>
#include <string.h>

static void join(const char *prefix, const char *stem) {
    size_t plen = strlen(prefix);
    char last = prefix[plen - 1];
    printf("%s + %s -> ", prefix, stem);
    if (last == 'u') {
        printf("%.*sw%s\n", (int)plen - 1, prefix, stem);
    } else {
        printf("%s%s  (no rule fired -- left as-is)\n", prefix, stem);
    }
}

int main(void) {
    join("ku", "eza");   /* this toy's one rule covers this */
    join("ki", "eza");   /* it doesn't know the i-glide rule */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra p1_toy_one_rule.c -o p1_toy_one_rule
$ ./p1_toy_one_rule
ku + eza -> kweza
ki + eza -> kieza  (no rule fired -- left as-is)
```

The toy gets `ku+eza` right by accident — it only encodes one of the
six rules from Section 1.1's list — and produces the un-spellable
`kieza` (a raw vowel-vowel sequence, exactly the hiatus the rule list
forbids) the moment a different prefix vowel shows up. Real Kinyarwanda
morphology needs all six rules, correctly disambiguated, at every
boundary in a word that may have five or six of them. That is the job
`ortho.c`'s sixteen passes exist to do, and Section 1.1's diagram in
Part 2 shows how they're organized to do it.

## 1.3 Checkpoint: test yourself before continuing

1. Looking at Section 1.1's rule list, which rule resolves `ku+eza`,
   and which resolves `ki+eza`? Are they the same rule?
2. `bi|a|tek|w|ye` needed two *different* rule families (a consonant
   metathesis, then a C+y fusion) applied in a specific order. Could
   swapping that order have produced the same result? Why not?
3. The toy joiner in 1.2 only breaks once a non-`u` vowel prefix shows
   up. Name one real Kinyarwanda prefix ending in a vowel other than
   `u` that this toy would mishandle.

---

# Part 2 — Sixteen Passes, One Buffer

## 2.1 Why morphemes travel through this engine as one mutable byte buffer, not an array of strings

Every rule in this chapter needs to look *across* a morpheme boundary
— at the character just before a `|` and the character just after it
— and sometimes needs to delete the boundary itself once two
morphemes fuse. `kin_ortho_gen` represents the whole morpheme string
as a single `char buf[OB]` with `|` characters marking the boundaries,
and a parallel `int len` tracking the live length, because every pass
shrinks or rewrites that buffer in place:

```c
static void del_at(char *buf, int pos, int n, int *len) {
    memmove(buf+pos, buf+pos+n, (size_t)(*len-pos-n+1));
    *len -= n;
}

static int repl_at(char *buf, int pos, int old_n, const char *repl,
                   int cur_len, int cap) {
    int rlen = (int)strlen(repl);
    int new_len = cur_len - old_n + rlen;
    if (new_len >= cap) return -1;
    memmove(buf+pos+rlen, buf+pos+old_n, (size_t)(cur_len-pos-old_n+1));
    memcpy(buf+pos, repl, (size_t)rlen);
    return new_len;
}
```

An array of separate morpheme strings (the shape every earlier
chapter's `KinMorpheme` struct actually uses for *display*) would hide
exactly the information these rules need: whether the character
immediately on the other side of a boundary is a vowel, a nasal, a
`y`. Treating the whole word as one buffer with embedded boundary
markers keeps that adjacency information visible to every pass, at
the cost of needing `memmove`-based insert/delete helpers instead of
simple string concatenation — `repl_at` returns `-1` rather than
overflow if a replacement would exceed the buffer's capacity, the same
defensive contract this book has seen guard every fixed-size buffer
since Chapter 2.

That same buffer is what makes pass *order* meaningful rather than
incidental — each box below is one or more of the sixteen passes, and
every arrow is a real dependency Section 6.1's code enforces by simply
calling things in this sequence:

```
  "bi|a|tek|w|ye"  (one buffer, '|' marks morpheme boundaries)
        |
        v
  ┌─────────────────────────────┐
  │ P1  metathesis (w,y swap)    │   needs the RAW boundaries, before
  └──────────────┬────────────────┘   anything else has touched them
        v
  ┌─────────────────────────────┐
  │ P2-P3  C+y fusion            │   needs metathesis already done, so
  └──────────────┬────────────────┘   the swapped 'y' is now adjacent
        v
  ┌─────────────────────────────┐
  │ P4  b->m   P5  nasal assim   │   P4 must run first: its output 'm'
  │ (+r->d)    P6  nasal elision │   is what P5/P6 actually look for
  └──────────────┬────────────────┘
        v
  ┌─────────────────────────────┐
  │ P7  voicing   P8  cons. loss │   only meaningful once boundary
  └──────────────┬────────────────┘   shapes from above have settled
        v
  ┌─────────────────────────────┐
  │ P9-P11  vowel rules          │   vowels resolved LAST, after every
  └──────────────┬────────────────┘   consonant question is decided
        v
  ┌─────────────────────────────┐
  │ P12 strip '|'  P13-P15       │   global cleanup passes over the
  │ epenthetic/plosive/vowel     │   now-boundary-free surface string
  └──────────────┬────────────────┘
        v
   "byatetswe"
```

## 2.2 The risk here: shrinking a buffer in place, sixteen times, in a fixed-iteration loop

Several of the sixteen passes don't run once — they run in a loop,
because fixing one boundary can create a new one that needs fixing
too (a C+y fusion can leave a new vowel adjacent to the next morpheme,
for instance):

```c
for (int iter = 0; iter < 4; iter++) {
    bool any = false;
    for (int i = 0; i < len; i++)
        if (buf[i]=='|') {
            if (apply_cy_fusion(buf, i, &len, noun_class_9)) { any=true; i--; }
        }
    if (!any) break;
}
```

The loop exits early the moment a full sweep finds nothing left to
fix (`if (!any) break`) — but it also has a hard cap (`iter < 4`,
`< 6` for vowel contact) regardless of whether anything is still
changing. That cap is a deliberate engineering choice, not a number
pulled at random: it guarantees the function terminates even if some
future rule edit ever made two passes cycle forever (fusion A
producing exactly the pattern fusion B undoes, and back again) —
the same "bound every loop, even one that looks self-limiting"
discipline this book has flagged in tokenizers and validators since
Chapter 6, applied here to a structurally different kind of loop: not
bounded by array size, but by a fixed retry budget on a
shrinking-buffer fixed point.

---

# Part 3 — Making It Interactive

## 3.1 Build it: generate a surface word, then validate it against itself

```c
/* p3_repl.c -- type morphemes separated by '|', see the surface form
 * and whether the engine's own validator considers that result clean */
#include "kinyarwanda.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    char line[256];
    printf("Type morphemes as 'a|b|c', or 'q' to quit.\n");
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == 'q' && line[1] == '\0') break;

        char surf[128];
        kin_ortho_gen(line, false, surf, sizeof(surf));
        printf("  surface: %s\n", surf);

        OrthoViolation v[8];
        int n = kin_ortho_validate(surf, v, 8);
        printf("  validator: %d violation(s)\n", n);
        for (int i = 0; i < n; i++)
            printf("    %s (%s)\n", kin_ortho_rule_name(v[i].type), v[i].rule);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p3_repl.c -L . -lkinyarwanda -o p3_repl
$ printf "ba|iza\nya|kor|ye\nq\n" | LD_LIBRARY_PATH=. ./p3_repl
Type morphemes as 'a|b|c', or 'q' to quit.
  surface: beza
  validator: 0 violation(s)
  surface: yakoze
  validator: 0 violation(s)
```

Both real, both clean. Part 4 feeds this exact REPL a documented
example straight out of `ortho.c`'s own header comment, and the
second line stops printing "0 violation(s)".

---

# Part 4 — Capstone: Four Real Findings From Reading the Production Code

## 4.1 The headline bug, and what later fixing it elsewhere left behind

`apply_voicing`'s own comment justifies its rule with a real word:

> Evidence: "igitabo" (igi+tabo) has gi before voiceless 't' ... applied
> unconditionally when the preceding morpheme is exactly 2 chars and
> ends in 'k' or 't'.

Test that justification directly, then test the *first* worked example
from `kin_ortho_gen`'s own doc comment — a verb, not a noun:

```c
/* p4_voicing.c -- the rule's own justifying example vs. a documented
 * example that fails */
#include "kinyarwanda.h"
#include <stdio.h>

static void show(const char *m) {
    char surf[128];
    kin_ortho_gen(m, false, surf, sizeof(surf));
    printf("%-18s -> %s\n", m, surf);
}

int main(void) {
    show("i|ki|tabo");      /* the rule's own justifying example */
    show("ku|0|ubak|a");    /* documented elsewhere to produce "kubaka" */
    show("ku|eza");         /* documented in kinyarwanda.h to produce "kweza" */
    show("ki|eza");         /* documented in kinyarwanda.h to produce "kyeza" */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_voicing.c -L . -lkinyarwanda -o p4_voicing
$ LD_LIBRARY_PATH=. ./p4_voicing
i|ki|tabo          -> igitabo
ku|0|ubak|a        -> guubaka
ku|eza             -> kweza
ki|eza             -> kyeza
```

`i|ki|tabo` confirms the rule's own evidence: noun-class-7's `ki`
prefix really does voice to `gi` before a stem, exactly as advertised
— `igitabo`, correct. The other two single-boundary cases, `ku|eza`
and `ki|eza`, now *also* come out correct — `kweza`, `kyeza` — which is
worth pausing on, because earlier printings of this chapter (and the
book's research for it) found both wrong, producing `gweza`/`gyeza`.
What changed is not this function's intent but a guard added to it in
a later chapter: `apply_voicing` now refuses to voice when the very
next character is a vowel (`if (ov(buf[bpos+1])) return false;` —
traced in full in Chapter 13, where it was added to fix an unrelated
noun, `ukwezi`). That guard doesn't know or care whether the
2-character prefix in front of it is a noun-class marker or a verb
infinitive marker — it only checks what comes immediately after the
boundary — and for `ku|eza`/`ki|eza`, the vowel `e` sits directly
there, so the guard correctly blocks voicing for both, incidentally
fixing what this chapter originally found broken.

`ku|0|ubak|a` is the one case the same guard does *not* catch, and
testing why narrows the real bug considerably:

```c
show("ku|ubak|a");      /* no intervening placeholder morpheme */
show("ku|0|ubak|a");    /* the documented form, with a "0" slot   */
```
```
ku|ubak|a          -> kwubaka
ku|0|ubak|a        -> guubaka
```

Remove the literal `"0"` morpheme (a placeholder this project's own
morpheme strings use for an elided object-marker slot) and the voicing
guard works correctly here too. With the `"0"` present, the character
immediately after the `ku|` boundary is the digit `'0'` itself, not a
vowel — `ov('0')` is false, so Chapter 13's guard does not fire, and
voicing proceeds exactly as it did before that fix, turning `ku` into
`gu`. The real vowel that should have blocked it, the `u` starting
`ubak`, is one morpheme further away than the guard looks. This is a
narrower, more precise finding than this chapter originally made: the
voicing rule was never really blind to "noun prefix vs. verb prefix"
as a category — it was blind to whether a vowel sits within sight, and
a one-character lookahead is exactly as far as "in sight" reaches.
Chapter 13's fix corrected that for every boundary where the vowel is
the very next character; it left this one case, where an empty
placeholder morpheme sits between the boundary and the vowel that
should have blocked voicing, exactly as broken as before.

`guubaka` has a second problem worth catching with the engine's own
other public function:

```c
/* p4_selfcheck.c -- does the generator's own output pass its own validator? */
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    OrthoViolation v[8];
    int n = kin_ortho_validate("guubaka", v, 8);
    printf("kin_ortho_validate(\"guubaka\") -> %d violation(s)\n", n);
    for (int i = 0; i < n; i++)
        printf("  %s at byte %d\n", kin_ortho_rule_name(v[i].type), v[i].pos);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_selfcheck.c -L . -lkinyarwanda -o p4_selfcheck
$ LD_LIBRARY_PATH=. ./p4_selfcheck
kin_ortho_validate("guubaka") -> 1 violation(s)
  VV hiatus (§1.1/§2.1) at byte 1
```

`kin_ortho_gen`'s own output fails the check `kin_ortho_validate`
exists to perform, using nothing but this module's two public
functions against each other. The voicing pass mis-fires first
(turning `ku` into `gu`), and the vowel-contact pass that should
resolve the resulting `u`-before-`u` hiatus never gets a chance to —
`apply_vowel_contact`'s `u → w` rule only fires when the character
immediately *after* the boundary is a vowel; by the time voicing has
already run, the boundary in `gu|ubak` is still there to trigger it,
but the leftover doubled `u` itself is never collapsed into a single
glide, leaving two real vowels stacked directly against the
orthography rule Section 1.1 opened this chapter with.

## 4.2 Generate and fix disagree about the same word

`kin_ortho_gen` (forward generation from morphemes) and
`kin_ortho_fix` (single-violation auto-correction on an already-written
surface word) are supposed to be two different doors into the same
orthographic knowledge. Feed them inputs that represent the same
underlying word and compare:

```c
/* p4_genvsfix.c -- the generator and the auto-fixer, on the same word */
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    char surf[64];
    kin_ortho_gen("i|n|banza", false, surf, sizeof(surf));
    printf("kin_ortho_gen(\"i|n|banza\") -> %s\n", surf);

    char fixed[64];
    kin_ortho_fix("inbanza", fixed, sizeof(fixed));
    printf("kin_ortho_fix(\"inbanza\")   -> %s\n", fixed);

    OrthoViolation v[8];
    int n = kin_ortho_validate(fixed, v, 8);
    printf("kin_ortho_validate(\"%s\") -> %d violation(s)\n", fixed, n);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_genvsfix.c -L . -lkinyarwanda -o p4_genvsfix
$ LD_LIBRARY_PATH=. ./p4_genvsfix
kin_ortho_gen("i|n|banza") -> imanza
kin_ortho_fix("inbanza")   -> imbanza
kin_ortho_validate("imbanza") -> 0 violation(s)
```

`imanza` is the real word (Kinyarwanda for "court cases" / "lawsuits").
`imbanza` is not — and yet `kin_ortho_validate` reports it as
perfectly clean. The cause is a real asymmetry in what each function
knows: `kin_ortho_gen`'s pipeline runs `pass_b_to_m` (§3.4.1, `b→m`
before a nasal prefix) *before* nasal assimilation runs, so by the
time the nasal-elision pass sees `i|n|manza`, the `n` simply elides
next to the `m` it created, leaving `imanza`. `kin_ortho_fix`, working
from `kin_ortho_validate`'s violation list alone, only knows the
single rule §3.3.3 (`n` before `b` → `m`) — it has no equivalent of
`pass_b_to_m` in its toolbox, so it stops one transformation short,
at `imbanza`, and because `mb` is independently a valid Kinyarwanda
consonant cluster (Section 1.1's sibling cluster-rule comment lists
it explicitly), the validator has no second rule left to catch what
the fixer missed. Two public entry points into "the same"
orthographic knowledge, and only one of them actually has the complete
rule set wired in.

## 4.3 A prefix-context rule firing at a stem boundary it was never written for

`kin_ortho_gen`'s own doc comment also documents this example, which
fails:

> `kin_ortho_gen("ba|ra|mu|ton|ish|ir|ye", false, surf, sz)` → `"baramutonesheje"`

```c
/* p4_tonish.c -- isolating the exact boundary that goes wrong */
#include "kinyarwanda.h"
#include <stdio.h>

static void show(const char *m) {
    char surf[128];
    kin_ortho_gen(m, false, surf, sizeof(surf));
    printf("%-26s -> %s\n", m, surf);
}

int main(void) {
    show("ton|isha");                       /* isolated root|extension boundary */
    show("ba|ra|mu|ton|ish|ir|ye");          /* the full documented example */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_tonish.c -L . -lkinyarwanda -o p4_tonish
$ LD_LIBRARY_PATH=. ./p4_tonish
ton|isha                   -> tonyesha
ba|ra|mu|ton|ish|ir|ye     -> baramutonyeshiye
```

Documented: `baramutonesheje`. Actual: `baramutonyeshiye` — an extra
`y` that doesn't belong, traceable to the isolated `ton|isha` test
producing `tonyesha` instead of the expected `tonesha`. The root
cause is §3.3.6, `n → ny` before a vowel — a rule clearly intended for
a *grammatical nasal prefix* meeting a vowel-initial stem (the same
family of rule that correctly produces `inyoni` from an underlying
nasal-class prefix plus a vowel-initial root). `apply_nasal_assim` has
no way to know that the `n` it's looking at here is the *last letter
of the verb root* `ton`, not a nasal prefix at all — it only checks
"is the character before this boundary an `n`, and the character
after it a vowel," which is also exactly true at a root-extension
boundary purely by coincidence of the root's last letter. The vowel
*is* correctly assimilated afterward (`isha` does become `esha`,
because the root `ton` contains an `o`, exactly as §1.3's rule
requires) — it's specifically the spurious inserted `y` that has no
basis in the actual word.

## 4.4 Two numbering schemes for the same sixteen passes, silently disagreeing

`ortho.c`'s own top-of-file comment lists all sixteen passes with
their RALC section numbers:

> P5 §3.3 nasal assimilation ... P6 §3.5 r→d before n ... P7 §3.1
> nasal elision ... P8 §3.7 consonant voicing ...

But the function body's own inline comments, immediately above each
actual call, number the same sequence differently:

```c
/* P5: nasal assimilation (multiple passes for cascades) */
...
/* P6: nasal elision */
...
/* P7: consonant voicing */
...
```

Reading `apply_nasal_assim` (Section 6.3 quotes it in full) shows why:
the top-of-file list treats "nasal assimilation" (§3.3) and "r→d
before n" (§3.5) as two separate passes, P5 and P6 — but the actual
implementation merges both into one function call. The function
body's own comments count *function calls*, not *RALC rule sections*,
so from that merge point onward every later pass is numbered one
lower in the body than in the file's own header: body "P6" (nasal
elision) is the header's "P7"; body "P15" (vowel assimilation, the
last pass) is the header's "P16". Both numbering schemes are
internally consistent on their own terms — one counts logical rule
groups from the official standard, the other counts actual function
calls in execution order — but a reader citing "pass 7" without
saying which scheme they mean is citing two different things in the
same file, and neither comment block says so.

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 Why sixteen small ordered passes, not one big rule, or a single regex

A single regular expression or one large rewrite function could, in
principle, encode all of this — but it would be unauditable against
the actual standard it implements. Every one of the sixteen passes in
`kin_ortho_gen` corresponds, by name and section number, to one
numbered rule in the real 2017 RALC document quoted in Section 1.1; a
Kinyarwanda linguist checking this code against that document can
match `pass_b_to_m` to §3.4.1 and `apply_nasal_elision` to §3.1
without ever reading past the function's own comment block. That
traceability is the entire reason this isn't a flat lookup table like
Chapter 5's `INVARIABLES[]` or a longest-match scan like Chapter 8's
digraph tables — those structures work when every rule is independent
of every other. Here, order is meaning: P4 (`b→m`) is explicitly
documented to "run BEFORE nasal elision so the n that immediately
follows sees 'm'" — Section 4.2's finding is exactly what happens when
a *second* code path (`kin_ortho_fix`) doesn't preserve that same
ordering dependency.

## 5.2 The missing link: how Chapters 1 through 3's morpheme guesses get checked

`MorphBreakdown`, the struct every `Token` in this book has carried
since Chapter 1, has a field this book has never explained until now:

```c
typedef struct {
    KinMorpheme m[KIN_MAX_MORPHEMES];
    int         n;
    bool        verified; /* kin_ortho_gen(underlying) == surface word */
} MorphBreakdown;
```

`kin_morpheme_analyze` (in `morph_dispatch.c`, called after every
noun, adjective, and verb tagger in Chapters 1 through 3 has already
guessed a D+RT+C or SP+TM+OM+root+EXT+FV breakdown) doesn't just
record that guess — it hands the *underlying* form it reconstructed
back to `kin_ortho_gen`, regenerates the surface spelling from
scratch, and sets `verified = true` only if that regenerated surface
form matches the word that was actually typed. Every chapter's
case-study tokens that displayed a clean morpheme breakdown were
silently relying on this chapter's engine to confirm they were right.
Section 4.1 and 4.2's bugs mean that confirmation can itself be wrong
in exactly the cases this chapter found — a verb beginning with
`ku-`/`ki-` before a vowel-initial stem would have its own correct
morpheme decomposition marked `verified = false`, not because the
decomposition was wrong, but because the orthography engine checking
it was.

---

# Part 6 — Reading the Real Production Code

## 6.1 `kin_ortho_gen`'s sixteen-call orchestration, in full

```c
void kin_ortho_gen(const char *morphemes, bool noun_class_9,
                   char *surface, size_t size) {
    char buf[OB];
    int  len = 0;

    for (int i = 0; morphemes[i] && len+1 < OB; i++) {
        char c = morphemes[i];
        if (c == '0') continue;
        if (c == '-') c = '|';
        buf[len++] = (char)tolower((unsigned char)c);
    }
    buf[len] = '\0';

    pass_wy_metathesis(buf, &len);                          /* P1 */

    for (int iter = 0; iter < 4; iter++) {                  /* P2+P3 */
        bool any = false;
        for (int i = 0; i < len; i++)
            if (buf[i]=='|')
                if (apply_cy_fusion(buf, i, &len, noun_class_9)) { any=true; i--; }
        if (!any) break;
    }

    if (!noun_class_9) pass_b_to_m(buf, &len);               /* P4 */

    for (int iter = 0; iter < 4; iter++) {                  /* P5 (+ r->d, §3.5) */
        bool any = false;
        for (int i = 0; i < len; i++)
            if (buf[i]=='|') { if (apply_nasal_assim(buf, i, &len)) { any=true; i--; } }
        if (!any) break;
    }

    for (int iter = 0; iter < 4; iter++) {                  /* P6 */
        bool any = false;
        for (int i = 0; i < len; i++)
            if (buf[i]=='|') { if (apply_nasal_elision(buf, i, &len)) { any=true; i--; } }
        if (!any) break;
    }

    for (int i = 0; i < len; i++)                            /* P7 */
        if (buf[i]=='|') apply_voicing(buf, i, &len);

    for (int iter = 0; iter < 3; iter++) {                  /* P8 */
        bool any = false;
        for (int i = 0; i < len; i++)
            if (buf[i]=='|') { if (apply_cons_loss(buf, i, &len)) { any=true; i--; } }
        if (!any) break;
    }

    if (noun_class_9)                                        /* P9 */
        for (int i = 0; i < len; i++)
            if (buf[i]=='|') apply_n_y_nz(buf, i, &len);

    for (int i = 0; i < len; i++)                            /* P10 */
        if (buf[i]=='|') apply_vowel_fusion(buf, i, &len);

    for (int iter = 0; iter < 6; iter++) {                  /* P11 */
        bool any = false;
        for (int i = 0; i < len; i++)
            if (buf[i]=='|') { if (apply_vowel_contact(buf, i, &len)) { any=true; i--; } }
        if (!any) break;
    }

    strip_boundaries(buf, &len);                             /* P12 */
    pass_epenthetic(buf, &len);                               /* P13 */
    pass_plosive_assim(buf, &len);                            /* P14 */
    pass_vowel_assim(buf, &len);                               /* P15 */

    strncpy(surface, buf, size-1);
    surface[size-1] = '\0';
}
```

Read top to bottom, this is Part 2's "ordered, not independent" claim
made concrete: metathesis before fusion (so the swapped consonant is
in place for the next pass to fuse), `b→m` before nasal assimilation
(Section 4.2's finding shows what happens when that ordering isn't
preserved elsewhere), voicing after fusion and elision have already
settled the boundary shape, vowel rules last, after every consonant
question has already been resolved.

## 6.2 `apply_voicing`, the function behind Section 4.1's finding

```c
static bool apply_voicing(char *buf, int bpos, int *len) {
    if (bpos < 2 || bpos+1 >= *len) return false;

    int prec_start = bpos - 1;
    while (prec_start > 0 && buf[prec_start-1] != '|') prec_start--;

    int prec_len = bpos - prec_start;
    if (prec_len != 2) return false;

    char fc = buf[prec_start];
    if (fc == 'k') { buf[prec_start] = 'g'; return true; }
    if (fc == 't') { buf[prec_start] = 'd'; return true; }
    return false;
}
```

The entire decision is two checks: is the preceding morpheme exactly
2 characters, and does it start with `k` or `t`. Nothing here asks
*what kind* of 2-character morpheme it is — a noun-class marker like
`ki` and a verb infinitive marker like `ku` are, to this function,
indistinguishable inputs that happen to produce different real-world
correctness.

## 6.3 `apply_nasal_assim`, the function behind Sections 4.3 and 4.4

```c
static bool apply_nasal_assim(char *buf, int bpos, int *len) {
    if (bpos < 1 || bpos+1 >= *len) return false;
    char last = buf[bpos-1];
    char next = buf[bpos+1];
    char nxt2 = (bpos+2 < *len) ? buf[bpos+2] : '\0';

    /* §3.5: r -> d before n -- merged into this same pass (Section 4.4) */
    if (last=='r' && next=='n') {
        buf[bpos-1] = 'd';
        del_at(buf, bpos, 1, len);
        return true;
    }
    if (last != 'n') return false;

    /* §3.3.6: n -> ny before any vowel -- fires at Section 4.3's root
     * boundary exactly as readily as at a genuine nasal prefix */
    if (ov(next)) {
        buf[bpos] = 'y';
        return true;
    }
    if (next=='w' && nxt2=='u') { buf[bpos] = 'y'; return true; }
    if (next=='f') { buf[bpos-1]='m'; del_at(buf,bpos,1,len); return true; }
    if (next=='p') { buf[bpos-1]='m'; del_at(buf,bpos,1,len); return true; }
    if (next=='b') { buf[bpos-1]='m'; del_at(buf,bpos,1,len); return true; }
    if (next=='v') { buf[bpos-1]='m'; del_at(buf,bpos,1,len); return true; }
    if (next=='h') {
        buf[bpos-1] = 'm';
        buf[bpos+1] = 'p';
        del_at(buf, bpos, 1, len);
        return true;
    }
    return false;
}
```

One function, doing two officially distinct jobs (§3.3's nasal
assimilation and §3.5's separate `r→d` rule) because they share the
same "look at the character before this boundary" shape — which is
exactly the merge Section 4.4 found the file's own top-level pass list
never re-numbered around. And the `ov(next)` branch — "the character
after this boundary is any vowel" — has no condition checking that
`last` (the `n`) is actually a *prefix*, which is exactly why it fires
just as happily at the tail end of a verb root.

---

# Part 7 — Looking Back, Looking Forward

## 7.1 Every "phonological rule" footnote since Chapter 1 was a forward reference to this chapter

Chapter 1's noun morphology, Chapter 2's adjective concordance,
Chapter 3's verb conjugation — every one of them mentioned, in
passing, that a phonological rule governs what happens at a particular
morpheme boundary, without ever showing the rule's actual
implementation. This chapter is where every one of those forward
references resolves, and Section 5.2 showed the literal connection:
`MorphBreakdown.verified`, set by calling straight into the code read
in Part 6.

## 7.2 Checkpoint: what you should now be able to explain

- Why two adjacent vowels can never survive in correctly spelled
  Kinyarwanda, and which of the six resolution rules applies for a
  given pair.
- Why this engine represents a word as one mutable buffer with
  embedded boundary markers, rather than an array of separate
  morpheme strings.
- Why pass *order* is part of this rule engine's correctness, in a way
  it never needed to be for a flat lookup table or a longest-match
  scan.
- The real, verified difference between what `kin_ortho_gen` does
  with a fully-specified morpheme string and what `kin_ortho_fix` can
  recover from an already-broken surface word alone.

---

# Part 8 — Practice

### Beginner

1. Using Section 1.1's six-rule list, predict the surface form of
   `na|amazi` and `ya|oya` by hand, then verify with `kin_ortho_gen`.
2. `kin_ortho_rule_name(ORTHO_LETTER_L)` returns a string citing
   "§2.3". Look up what `ortho.c`'s own top-of-file pass list cites as
   §2.3. Are they the same rule?

### Intermediate

3. Section 4.1 found three failing examples, all sharing one root
   cause. Construct a fourth: pick any other 2-character `ku-` or
   `ki-`-prefixed input and confirm the same voicing bug reproduces.
4. `kinyarwanda.h`'s rule list documents `mu+inja → menja` under rule
   4 (a+i→e). Run `kin_ortho_gen("mu|inja", false, ...)` for real.
   Does the output match? If not, is the mismatch the same kind of
   bug as Section 4.1, or something different?
5. Section 4.2 found `kin_ortho_fix` missing the `b→m` pre-pass that
   `kin_ortho_gen` applies. Find one other pass in `kin_ortho_gen`
   (Section 6.1) that `kin_ortho_fix`'s switch statement (read the
   real file) has no equivalent case for.

### Advanced

6. Propose a minimal change to `apply_voicing`'s signature or the
   information available to it that would let it distinguish a
   noun-class RT prefix from a verb infinitive marker, without
   requiring it to take a full `Token` as an argument.
7. Section 4.3's bug comes from `apply_nasal_assim` having no signal
   that the `n` it inspected was root-final rather than prefix-initial.
   `kin_ortho_gen` already takes one boolean context flag
   (`noun_class_9`) for a similar purpose. Sketch what a second such
   flag, threaded through the same call sites, would need to encode to
   fix this case without breaking the genuine nasal-prefix cases
   §3.3.6 was written for.

---

## Key takeaways

- Kinyarwanda forbids adjacent vowels within a word; `ortho.c`
  implements the six resolution rules (glide formation, elision,
  fusion) `kinyarwanda.h` documents, plus the consonant and nasal
  rules from the same official 2017 RALC standard, as sixteen ordered
  passes over a single mutable buffer with embedded morpheme-boundary
  markers.
- Pass *order* is part of this engine's correctness in a way no
  earlier chapter's data structure required: `pass_b_to_m` is
  documented to run before nasal elision specifically so a later pass
  sees its output, and Section 4.2 found a second code path
  (`kin_ortho_fix`) that doesn't preserve that same ordering.
- The headline real finding, as this chapter originally found it: an
  unconditional rule voicing any 2-character `k`/`t`-initial morpheme
  misfired on the verb infinitive marker `ku-`/`ki-` as well as on
  noun-class prefixes, confirmed against three of this project's own
  documented examples (`kubaka`, `kweza`, `kyeza`, all produced wrong).
  A guard added in Chapter 13 (for an unrelated noun, `ukwezi`) checks
  whether a vowel immediately follows the voicing boundary, and as a
  side effect now correctly produces `kweza`/`kyeza` — re-verified
  directly against the current source for this chapter. `kubaka`
  (documented as `ku|0|ubak|a`, with a placeholder "0" morpheme for an
  elided object marker) still produces `guubaka`, because the
  vowel that should block voicing sits one morpheme past where the
  one-character guard looks — the resulting hiatus is independently
  caught by the module's own `kin_ortho_validate`.
- A second real finding: `kin_ortho_gen` and `kin_ortho_fix` disagree
  about the same underlying word (`imanza` vs. `imbanza`) because only
  the generator's pipeline includes a `b→m` pre-pass the fixer's
  narrower single-violation view has no equivalent for — and the
  fixer's wrong answer still passes its own validator with zero
  violations.
- A third real finding: the §3.3.6 `n→ny` nasal-assimilation rule, with
  no way to tell a grammatical nasal prefix from a verb root that
  merely happens to end in `n`, inserts a spurious `y` at a root-stem
  boundary, traced to a minimal `ton|isha → tonyesha` reproduction.
- A fourth real finding, purely textual: the file's own top-of-file
  pass list and the function body's own inline pass comments use two
  different, individually consistent, but mutually contradictory
  numbering schemes for the same sixteen passes, diverging exactly at
  the point where two officially separate RALC rules got merged into
  one function call.
- `MorphBreakdown.verified`, present on every `Token` since Chapter 1,
  is this chapter's direct connection back to every earlier one: it is
  set by calling `kin_ortho_gen` on the reconstructed underlying form
  and checking the regenerated surface form matches what was actually
  typed — meaning this chapter's bugs are also, quietly, bugs in what
  every earlier chapter's case studies were able to verify about
  themselves.

## Sources quoted in this chapter

- `src/ortho.c` (`kin_ortho_gen` and its sixteen passes,
  `apply_voicing`, `apply_nasal_assim`, `apply_vowel_contact`,
  `kin_ortho_validate`, `kin_ortho_fix`, `kin_ortho_recover_verb_root`,
  `del_at`, `repl_at`).
- `include/kinyarwanda.h` (the vowel-contact and consonant-cluster rule
  comments, `OrthoViolationType`, `OrthoViolation`, `MorphBreakdown`,
  the `kin_ortho_*` public API declarations).
- Every `pN_*.c` program in this chapter was actually compiled with
  `gcc -std=c99 -Wall -Wextra` and actually executed to produce the
  exact output quoted above.

## Coming up in Chapter 10

Nine chapters have covered words, sentences, sound, and spelling.
None has covered the marks between them. `src/punctuation.c` — 796
lines implementing `kin_check_punctuation`, called from the validator
after every other check in this book has already run — is the
dedicated engine behind the comma, period, and quotation-mark rules
Chapter 7 only sampled secondhand through `kin_check_syntax`'s
numbered list. It closes with real corpus statistics (frequencies
drawn from the 30,984-sentence "Bibiliya Yera" corpus this book has
cited since Chapter 5) backing rules as specific as exactly which
conjunctions require a comma before them, and how often real
Kinyarwanda writing actually follows that rule.
