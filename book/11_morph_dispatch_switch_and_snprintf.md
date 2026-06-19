# Chapter 11 — `morph_dispatch.c`: Switch-Based Dispatch and Safe String Building

## The biggest file, doing the most synthesis

At 3,528 lines, `morph_dispatch.c` is the single largest file in the
project — and it's where every earlier chapter's machinery finally comes
together. It takes a `Token` that `pos_tagger.c` (Ch.8) has already
classified, and produces the full, human-displayable morpheme breakdown,
calling `kin_ortho_gen()` (Ch.9–10) along the way to *verify* its own
analysis. This chapter is the payoff for everything you've read so far.

## 11.1 The dispatcher: Chapter 2's `switch` pattern, now fully earned

```c
void kin_morpheme_analyze(Token *tok)
{
    switch (tok->pos) {
        case POS_NOUN:
        case POS_RELATIVE_NOUN:
        case POS_COMPOUND_ADJ:
            analyse_noun(tok);
            break;
        case POS_ADJECTIVE:
            analyse_adj(tok);
            break;
        case POS_VERB_CONJ:
            analyse_vconj(tok);
            break;
        case POS_VERB_INF:
            analyse_vinf(tok);
            break;
        default:
            break;
    }
}
```

Back in Chapter 2, this exact `switch` was your first example of an enum
used as a dispatch tag — at the time, you hadn't yet seen what
`analyse_noun()` or `analyse_vconj()` actually *do*. Now you have: each
of these four functions is several hundred lines of dedicated logic,
built specifically around one grammar tree's own morpheme formula
(D+RT+C for nouns, RS+C for adjectives, SP+TM+OM+root+EXT+FV for
conjugated verbs, PREF+root+FV for infinitives). This is what "C's
substitute for polymorphism" really means in practice: four genuinely
different algorithms, selected by one tag, with no shared base class or
virtual table anywhere — just a single `switch` statement standing in for
all of it.

## 11.2 `memset`-then-fill, at struct scale

```c
static void analyse_noun(Token *tok)
{
    MorphBreakdown *mb = &tok->morph;
    memset(mb, 0, sizeof(*mb));
    /* ... */
}
```

This is Chapter 4's zero-then-fill pattern (`emit_punct`'s
`memset(&out[count], 0, sizeof(Token))`) again, here applied to
`MorphBreakdown` — the 1,576-byte nested struct from Chapter 3. Zeroing
it first guarantees that if this particular word only ever fills 3 of its
8 possible `KinMorpheme` slots, the other 5 read as cleanly empty
(`label[0] == '\0'`, etc. — recall Chapter 2's "zero means none"
convention extends naturally to char arrays: an all-zero buffer is
already a valid empty string) rather than carrying over whatever
unrelated data happened to occupy that memory from a previous token's
analysis.

## 11.3 `set_morph`: one helper, instead of four `strncpy` pairs repeated everywhere

```c
static void set_morph(KinMorpheme *m,
                      const char *label, const char *form,
                      const char *surface, const char *rule)
{
    strncpy(m->label,   label,   KIN_MORPH_LABEL_LEN - 1);
    strncpy(m->form,    form,    KIN_MORPH_FORM_LEN  - 1);
    strncpy(m->surface, surface, KIN_MORPH_FORM_LEN  - 1);
    strncpy(m->rule,    rule,    KIN_MORPH_RULE_LEN  - 1);
    m->label  [KIN_MORPH_LABEL_LEN - 1] = '\0';
    m->form   [KIN_MORPH_FORM_LEN  - 1] = '\0';
    m->surface[KIN_MORPH_FORM_LEN  - 1] = '\0';
    m->rule   [KIN_MORPH_RULE_LEN  - 1] = '\0';
}
```

You met the `strncpy`-then-explicit-`'\0'` pair as a gotcha in Chapter 6.
Here it appears four times in a row, because `KinMorpheme` (Ch.3) has
four `char[]` fields, every one of which needs the identical safe-copy
treatment. Rather than writing that two-line pair inline at every one of
the dozens of places across this 3,500-line file that need to fill a
`KinMorpheme`, it's written **once**, here, and called everywhere else.
This is worth naming as a real software-engineering principle, not just
a C trick: **DRY ("don't repeat yourself")** — extracting repeated,
error-prone boilerplate into a single, small, easy-to-verify function
means there is exactly one place to check (and fix, if a bug is ever
found) for "did we null-terminate every field of a `KinMorpheme`
correctly," instead of dozens of copy-pasted call sites that could each
silently drift out of sync with each other over time.

## 11.4 Filling the capacity+count structure by direct index, not by appending

```c
set_morph(&mb->m[0], "D",  "i",          "i",         "");
set_morph(&mb->m[1], "RT", "n",           rt_surface,  rule);
set_morph(&mb->m[2], "C",  c_underlying,  c_surface,   c_rule);
mb->n = 3;
```

Compare this to how Chapter 4's tokenizer filled `Token` slots:
`out[count]`, then `count++` — append, then advance, one at a time,
because the tokenizer doesn't know in advance how many tokens a sentence
will contain. Here, the situation is different: by the time this code
runs, `analyse_noun()` already knows, with certainty, that a standard
noun decomposes into *exactly* three morphemes (D, RT, C — the formula
itself fixes the count). So instead of incrementally appending and
bumping a counter, the code writes directly to known indices `m[0]`,
`m[1]`, `m[2]`, and sets `mb->n = 3` once, as a single final statement.
Both are the same underlying capacity+count idiom from Chapter 3 — the
choice between "append one at a time, incrementing as you go" and "write
every slot directly, then declare the final count in one line" depends
entirely on whether the total count is known in advance. Some sub-cases
in this same file need 4 or 5 morphemes (a possessive-associative form
adds an `"ASSOC"` slot; a negative participial form adds `"PRIV"` and
`"FV"` slots) — and those branches simply write more `set_morph` calls
and set `mb->n` to a different fixed number, no structural change needed.

## 11.5 The `verified` flag: using your own generator as a self-check oracle

This is the payoff Chapter 3 promised when it first introduced
`MorphBreakdown.verified`. Here it is, fully in context, for the Nt.9/10
noun case:

```c
char morph_str[KIN_MAX_WORD];
snprintf(morph_str, sizeof(morph_str), "i|n|%s", c_underlying);
char reconstructed_nt9[KIN_MAX_WORD];
kin_ortho_gen(morph_str, true, reconstructed_nt9, sizeof(reconstructed_nt9));
mb->verified = (strcmp(reconstructed_nt9, word) == 0);
```

Walk through exactly what's happening: `analyse_noun()` has just worked
*backward* — starting from a surface word like `"imvura"` and
reverse-engineering what it believes the underlying morphemes must be
(`D="i"`, `RT="n"`, `C="vura"`). Rather than simply trusting that
reverse-engineering, it immediately runs the *same* underlying morphemes
back **forward** through `kin_ortho_gen()` — the entire 16-pass pipeline
from Chapters 9–10 — and checks whether the regenerated surface form
(`reconstructed_nt9`) exactly matches the original input word (`word`).
If the analysis was correct, generating forward from the recovered
morphemes must reproduce the exact original word; if they don't match
byte for byte, `mb->verified` is `false`, and anything downstream
displaying this breakdown can flag it as unconfirmed rather than
presenting a possibly-wrong guess with false confidence.

This is a genuinely valuable engineering technique worth naming directly:
**using your own forward-generation engine as a runtime self-check on
your own reverse-analysis**, for every single word the program ever
processes — not just in a test suite that runs occasionally, but live, in
production, on every call. If you're asked "how do you know your
morphological analysis is actually correct," this `verified` flag, and
the round-trip it performs, is a precise, concrete, and honestly
*satisfying* answer: the program checks its own work, every time, using
the same rule engine it would use to generate the word from scratch.

## 11.6 `snprintf` as the file's primary tool for building explanation strings

`morph_dispatch.c` calls `snprintf` well over a hundred times — by far
the heaviest use of it anywhere in the project. The reason: every
`KinMorpheme.rule` field needs a *human-readable explanation*, built at
runtime by interpolating values that aren't known until the specific word
is being analyzed:

```c
snprintf(rule, sizeof(rule),
         "n→m §3.3 (before bilabial '%c')", after_i[1]);
```

This single line builds a complete, citable explanation string — rule
number, the actual rule, and the actual triggering character pulled
straight from the word being analyzed — entirely at runtime, safely
bounded by `sizeof(rule)` so it can never overflow the buffer regardless
of what gets interpolated into it. Contrast this with the unsafe
alternative C offers, `sprintf` (no size limit at all — it will happily
write past the end of `rule` if the formatted text turns out longer than
expected) or manually building the string piece by piece with repeated
`strcat` calls (each one re-scanning the string from the start to find
where to append, and just as capable of overflowing a fixed buffer if you
don't separately track the remaining space yourself). `snprintf` solves
both problems in one call: it takes the buffer's capacity directly, and
it supports the same `%`-format interpolation you'd use for any
formatted output, in one bounded, safe operation. Chapter 21 will return
to `snprintf` vs. `sprintf` as a general defensive-programming topic —
this file is the single best place in the whole codebase to see *why*
that general lesson matters in concrete, repeated practice.

## Key takeaways

- A `switch` on a POS tag dispatching to four structurally different
  analysis functions is C's complete substitute for polymorphism — no
  shared base type is needed, because the tag alone selects the right
  code path.
- `memset`-then-fill scales up cleanly from a single `Token` (Ch.4) to a
  large nested struct like `MorphBreakdown` — same principle, larger
  target.
- Extracting a repeated, error-prone operation (safe-copy-with-
  termination, four fields at a time) into one small helper function is
  the DRY principle in action — one place to verify correctness, instead
  of dozens of copy-pasted call sites.
- The capacity+count idiom (Ch.3) can be filled either incrementally
  (append + increment, when the final count is unknown in advance) or
  directly (write known indices, then set the count once, when the
  formula already fixes how many slots are needed) — both are the same
  underlying idiom, used differently depending on what's knowable ahead
  of time.
- The `verified` flag is a real, runtime self-check: regenerate a surface
  form forward from the recovered morphemes and compare it byte-for-byte
  against the original word, using the project's own generation engine as
  the oracle for its own analysis.
- `snprintf` is the safe tool for building formatted, runtime-interpolated
  strings in C — bounded by the buffer's real capacity, unlike `sprintf`
  or manual `strcat` chains.

## Search YouTube for

- "DRY principle don't repeat yourself explained"
- "round trip testing explained"
- "snprintf vs sprintf vs strcat in C"
- "switch statement as polymorphism substitute in C"

## Coming up in Chapter 12

Every chapter so far has analyzed one token (or, in Chapter 8's context
passes, a token plus its immediate neighbor). `syntax.c` looks at a whole
sentence's worth of already-tagged, already-analyzed tokens at once, to
check whether they actually *agree* with each other — does the
adjective's class match the noun it modifies, does the verb's subject
prefix match the subject noun's class. Chapter 12 covers how this project
checks consistency across an entire array of structs, not just within
one.
