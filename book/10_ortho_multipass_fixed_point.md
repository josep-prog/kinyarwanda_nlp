# Chapter 10 — `ortho.c` (II): Multi-Pass Rule Engines & Fixed-Point Iteration

## Zooming out to the driver

Chapter 9 covered the tools — `del_at`, `repl_at`, the `|`-delimited
buffer. This chapter covers `kin_ortho_gen()` itself: the function that
calls every rule, in a specific order, sometimes once, sometimes
repeatedly, until the buffer stops changing. This is the most
sophisticated piece of control flow in the entire project, and it
deserves to be read end to end.

## 10.1 The full pipeline, as a literal assembly line

```c
void kin_ortho_gen(const char *morphemes, bool noun_class_9,
                   char *surface, size_t size) {
    char buf[OB]; int len = 0;
    /* ... load input into buf, normalising '-'/'0' (Chapter 9) ... */

    pass_wy_metathesis(buf, &len);                          /* P1  */

    for (int iter = 0; iter < 4; iter++) {                   /* P2+P3 */
        bool any = false;
        for (int i = 0; i < len; i++)
            if (buf[i]=='|')
                if (apply_cy_fusion(buf, i, &len, noun_class_9)) { any=true; i--; }
        if (!any) break;
    }

    if (!noun_class_9) pass_b_to_m(buf, &len);               /* P4  */

    for (int iter = 0; iter < 4; iter++) { /* ... apply_nasal_assim ... */ }   /* P5 */
    for (int iter = 0; iter < 4; iter++) { /* ... apply_nasal_elision ... */ } /* P6 */

    for (int i = 0; i < len; i++)
        if (buf[i]=='|') apply_voicing(buf, i, &len);         /* P7  */

    for (int iter = 0; iter < 3; iter++) { /* ... apply_cons_loss ... */ }     /* P8 */

    if (noun_class_9)
        for (int i = 0; i < len; i++)
            if (buf[i]=='|') apply_n_y_nz(buf, i, &len);       /* P9  */

    for (int i = 0; i < len; i++)
        if (buf[i]=='|') apply_vowel_fusion(buf, i, &len);     /* P10 */

    for (int iter = 0; iter < 6; iter++) { /* ... apply_vowel_contact ... */ } /* P11 */

    strip_boundaries(buf, &len);                              /* P12 */
    pass_epenthetic(buf, &len);                                /* P13 */
    pass_plosive_assim(buf, &len);                              /* P14 */
    pass_vowel_assim(buf, &len);                                /* P15 */

    strncpy(surface, buf, size-1);
    surface[size-1] = '\0';
}
```

Sixteen named passes, run in a fixed order, each one transforming the
buffer a little more before handing it to the next. This is a **pipeline**
in the most literal sense: one mutable piece of data flows through a
sequence of independent transformation steps, each documented (back in
the file's header, Chapter 9) with its own RALC section number. The
*order* of these sixteen passes is exactly as load-bearing as the
ordering you've seen in every priority chain so far (Ch.6, Ch.8) — moving
`strip_boundaries()` (P12) earlier, for instance, would erase the `|`
markers several rule functions still need to see before they've had a
chance to fire.

## 10.2 Two different loop shapes for two different rule behaviors

Look closely and you'll notice **two distinct loop shapes** used for
different passes. Some passes scan the buffer exactly once:

```c
for (int i = 0; i < len; i++)
    if (buf[i]=='|') apply_voicing(buf, i, &len);
```

Others wrap that same scan inside an outer loop that repeats until
nothing changes:

```c
for (int iter = 0; iter < 4; iter++) {
    bool any = false;
    for (int i = 0; i < len; i++)
        if (buf[i]=='|')
            if (apply_nasal_assim(buf, i, &len)) { any=true; i--; }
    if (!any) break;
}
```

The difference is not arbitrary. A single-pass rule (like P7 consonant
voicing) only ever needs to look at each boundary once, because applying
it can't create a *new* opportunity for the same rule to fire somewhere
it hadn't already covered. A rule like nasal assimilation (P5), by
contrast, can **cascade**: fixing one boundary can shift characters
around in a way that creates a fresh `n`-before-consonant situation right
next door, which the rule needs another look at. Whenever a rule's own
output can become a new input the same rule (or an earlier one in the
same group) needs to re-examine, a single pass isn't enough — you need
to keep re-scanning until a full pass produces *no* changes at all.

## 10.3 Fixed-point iteration, named precisely

```c
for (int iter = 0; iter < 4; iter++) {
    bool any = false;
    for (int i = 0; i < len; i++)
        if (buf[i]=='|')
            if (apply_nasal_assim(buf, i, &len)) { any=true; i--; }
    if (!any) break;
}
```

This shape has a name in computer science: **fixed-point iteration** —
repeatedly apply a transformation to some data until applying it again
produces no further change (the data has reached a "fixed point" of the
transformation). This is not a technique invented for this project; it's
the same underlying idea behind iterative numerical methods, and — more
relevantly to you as a programmer — the same idea real compilers use
internally when applying rewrite rules during optimization (constant
folding, dead-code elimination, and similar passes are frequently run
"until nothing more changes" for exactly this reason). Recognizing this
pattern and being able to name it is a strong, concrete answer if you're
asked "what general computer-science technique does this code use?"

Walk through exactly what makes it work:

- **`bool any = false;`** — a flag reset at the start of every outer
  iteration, tracking "did *anything at all* change during this full
  scan of the buffer."
- **The inner loop scans every position once**, applying the rule
  wherever a `'|'` boundary is found.
- **`{ any=true; i--; }`** is the crucial line. Setting `any = true`
  records that something changed. `i--` is what makes this fixed-point
  iteration rather than a single pass: decrementing `i` means the `for`
  loop's own `i++` will bring execution right back to the **same
  position** on the next iteration, instead of moving on. Why
  deliberately re-visit the same spot? Because `apply_nasal_assim` (or
  whichever rule just fired) may have changed the buffer's length (via
  `del_at`) or changed which characters now sit around position `i` — the
  loop cannot assume the *next* interesting boundary is further ahead; it
  has to check whether the just-modified neighborhood now satisfies the
  *same* rule again, or another rule the inner `if`-chain inside the rule
  function would also want to fire on.
- **`if (!any) break;`** — the *real* termination condition. Once an
  entire pass over the whole buffer finds nothing left to change, the
  buffer has reached its fixed point for this rule group, and there's no
  reason to keep looping.
- **The outer `iter < 4` (or `< 3`, or `< 6` for other passes) is a safety
  cap, not the intended termination condition.** In the ordinary case,
  `!any` should trigger `break` well before the cap is reached — these
  rule sets are small and the cascades they're designed for resolve in at
  most two or three rounds. The cap exists purely as **defensive
  programming**: if some unforeseen combination of rules ever caused a
  cycle that never settles (rule A's output triggers rule B, whose output
  re-triggers rule A, forever), the cap guarantees the function still
  returns — with possibly-imperfect output — rather than hanging the
  entire program in an infinite loop. This is a "belt and suspenders"
  pattern worth being able to name directly: trust your own termination
  logic, but bound it anyway, so a latent bug degrades gracefully instead
  of catastrophically.

## 10.4 Inside one rule function: another priority chain, one level deeper

```c
static bool apply_nasal_assim(char *buf, int bpos, int *len) {
    if (bpos < 1 || bpos+1 >= *len) return false;
    char last = buf[bpos-1];
    char next = buf[bpos+1];
    char nxt2 = (bpos+2 < *len) ? buf[bpos+2] : '\0';

    if (last=='r' && next=='n') {
        buf[bpos-1] = 'd';
        del_at(buf, bpos, 1, len);
        return true;
    }
    if (last != 'n') return false;

    if (ov(next)) { buf[bpos] = 'y'; return true; }              /* n→ny/_V   */
    if (next=='f') { buf[bpos-1]='m'; del_at(buf,bpos,1,len); return true; } /* n→m/_f */
    if (next=='b') { buf[bpos-1]='m'; del_at(buf,bpos,1,len); return true; } /* n→m/_b */
    /* ... more rules, each one's own if, each ending in return true ... */
    return false;
}
```

`apply_nasal_assim` is called once per `'|'` boundary in the outer scan,
and is handed `bpos` — the exact position of that boundary. Inside, it's
the *same* ordered-priority-chain technique you met in Chapter 6
(longest-match) and Chapter 8 (POS tagging), now operating one level
deeper: instead of choosing "which grammar tree does this word belong
to," this chain chooses "which specific phonological rule, if any,
applies at this exact boundary," based on the characters immediately
before and after it (`last`, `next`, `nxt2`).

Notice the two different ways a matching rule mutates the buffer:
sometimes it's a **direct overwrite** with no length change
(`buf[bpos] = 'y';` — the boundary character itself is simply replaced,
buffer stays the same length), and sometimes it's an **overwrite plus a
deletion** (`buf[bpos-1]='m'; del_at(buf,bpos,1,len);` — one character is
changed, and the boundary marker itself is removed, shrinking the
buffer by one). This mixture is *exactly* why the outer fixed-point loop
in Section 10.3 cannot safely assume anything about the buffer's shape
after calling a rule function — sometimes the length changes, sometimes
it doesn't, and the only safe response is to re-examine the same spot
(`i--`) rather than guess.

## 10.5 One boolean parameter, redirecting an entire pipeline

```c
if (!noun_class_9) pass_b_to_m(buf, &len);                /* P4 */
/* ... */
if (noun_class_9) {
    for (int i = 0; i < len; i++)
        if (buf[i]=='|') apply_n_y_nz(buf, i, &len);       /* P9 */
}
```

`noun_class_9` is a single `bool` parameter passed all the way in from
`kin_ortho_gen`'s public signature, and it silently switches entire
passes on or off depending on whether the word being generated is a
Nt.9/10 noun. Rather than writing two near-duplicate versions of
`kin_ortho_gen` — one for Nt.9/10 words, one for everything else — a
single flag threaded through the function lets one implementation serve
both cases, with `if`/`if (!...)` guards deciding, pass by pass, which
rules are even relevant to the current call. This is a clean,
minimal way to parameterize pipeline *behavior*, not just pipeline
*data* — worth contrasting with Chapters 5–8's `bool` out-parameters,
which reported answers; this `bool` instead **controls which code runs
at all**.

## 10.6 Try it yourself: trace a real example through the pipeline

The file's own P1 comment documents this worked example:
`bi|a|tek|w|ye → byatetswe` ("it was cooked"). Confirm it yourself:

```c
#include "kinyarwanda.h"
#include <stdio.h>
int main(void) {
    char out[64];
    kin_ortho_gen("bi|a|tek|w|ye", false, out, sizeof(out));
    printf("%s\n", out);   /* expect: byatetswe */
    return 0;
}
```

Compile and link against `libkinyarwanda.a` exactly as you did in Chapter
1's exercise. While you watch the result come out, trace the *passes* by
hand against the file's own commentary: P1 (`pass_wy_metathesis`) swaps
the lone `w` and the following `y` (`tek|w|ye` → `tek|y|w|e`); later, the
C+y fusion group (P2/P3) sees `k` immediately before a (now-relocated)
`y` and applies the `k+y→ts` rule the file documents; P12 then strips the
remaining `|` markers; by the time the buffer reaches `strncpy`, it reads
`byatetswe`. You don't need to single-step a debugger to verify this —
matching the function's own inline comments against the actual output is
enough to demonstrate, concretely, that you understand which pass did
which part of the transformation.

## Key takeaways

- A pipeline of named, ordered passes over one mutable buffer is the
  entire structure of `kin_ortho_gen()` — order matters exactly as much
  here as in any other priority chain in this project.
- Use a single pass when a rule's effects can't create new opportunities
  for itself to fire; use fixed-point iteration (repeat until a full pass
  changes nothing) when a rule's output can cascade into new matches.
- Fixed-point iteration is a named, general technique — the same idea
  used by real compiler optimization passes — not something specific to
  this project; recognizing and naming it is worth more in a defense than
  describing it from scratch each time.
- `i--` after a successful in-loop mutation re-examines the same position
  on the next iteration, because the mutation may have changed the
  buffer's length or its neighboring characters in ways the loop can't
  predict in advance.
- An outer iteration cap alongside a `break`-on-no-change condition is
  defensive programming: trust the natural termination logic, but bound
  it anyway so a latent bug can't hang the program forever.
- A single `bool` parameter threaded through a pipeline function can
  switch entire passes on or off for different contexts, avoiding the
  need to duplicate the whole function for each variant.

## Search YouTube for

- "fixed point iteration explained"
- "compiler optimization passes rewrite rules until no change"
- "defensive programming bounded loops"
- "C function parameters controlling control flow vs data"

## Coming up in Chapter 11

`morph_dispatch.c` is the file that takes everything `pos_tagger.c`
decided (Ch.8) and everything `ortho.c` can generate (Ch.9–10) and
assembles the final, displayable morpheme breakdown for a token — using
a `switch` on `tok->pos` exactly like Chapter 2 introduced, but now
calling four entirely different assembly functions, one per grammar
tree, and building every output string safely with `snprintf`.
