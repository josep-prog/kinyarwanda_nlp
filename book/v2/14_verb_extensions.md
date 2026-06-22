# Chapter 14 — Verb Extensions: One Idea, Three Files, and a Gap Between Two of Them

## How this chapter works

Same rules as every chapter before it. Every fenced `$` block is real
`gcc -std=c99 -Wall -Wextra` output from a program actually compiled
and run against this project's real, built library.

Chapter 13 closed by naming what it hadn't read: `ext_strip()`, the
verb-extension stripper inside `morphology.c`'s giant verb-matching
machinery, and its counterpart on the generation side of
`morph_dispatch.c`. This chapter reads both, plus a third piece
neither Chapter 12 nor 13 had opened — a vowel-harmony validation rule
inside `syntax.c` — and finds a real, verified gap in how completely
that third piece does its job.

---

# Part 1 — Language and Code, Side by Side

## 1.1 Six families, and the rule that picks between two spellings of each

A Kinyarwanda verb root can carry one or more *itondaguranshinga*
(derivational extensions) between the root and the final vowel:
passive, causative, applicative, reciprocal, stative, reversive — each
changing the verb's meaning (who acts, who's acted upon, whether it's
repeated or undone). Several of these have two surface spellings,
chosen by *vowel harmony*: if the root's last vowel is mid (`e`/`o`),
the extension takes its mid-vowel form; otherwise, the non-mid form.

```
  root vowel        causative        stative
  ─────────         ──────────       ────────
  a / i / u    →     -ish-            -ik-
  e / o        →     -esh-            -ek-
```

Real, attested pairs confirm this directly:

```c
#include "kinyarwanda.h"
#include <stdio.h>

static void show(const char *w) {
    SentenceAnalysis sa = kin_analyze(w);
    Token *t = &sa.tokens[0];
    printf("%-12s stem=%-8s verified=%d\n", w, t->stem, t->morph.verified);
}

int main(void) {
    show("kwigisha");    /* root ig  (i, non-mid) -> -ish- */
    show("gukoresha");   /* root kor (o, mid)     -> -esh- */
    show("guhingika");   /* root hing (i, non-mid) -> -ik- */
    show("gutekeka");    /* root tek (e, mid)      -> -ek- */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p1_harmony.c -L . -lkinyarwanda -o p1_harmony
$ LD_LIBRARY_PATH=. ./p1_harmony
kwigisha     stem=igish   verified=1
gukoresha    stem=koresh  verified=1
guhingika    stem=hingik  verified=1
gutekeka     stem=tekek   verified=1
```

All four match the rule, all four verify. Section 4.1 finds a real
word that breaks the same rule and is not caught.

## 1.2 Build it: a stripper that doesn't know about harmony

```c
/* p1_toy_strip.c -- strip a 3-char or 2-char suffix, no harmony awareness */
#include <stdio.h>
#include <string.h>

static int strip(const char *stem, char *root_out) {
    size_t len = strlen(stem);
    if (len > 3 && (!strcmp(stem+len-3, "ish") || !strcmp(stem+len-3, "esh"))) {
        strncpy(root_out, stem, len-3); root_out[len-3] = '\0';
        return 1;
    }
    return 0;
}

int main(void) {
    char root[32];
    if (strip("korish", root)) printf("korish -> root=%s, ext=ish\n", root);
    if (strip("koresh", root)) printf("koresh -> root=%s, ext=esh\n", root);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra p1_toy_strip.c -o p1_toy_strip
$ ./p1_toy_strip
korish -> root=kor, ext=ish
koresh -> root=kor, ext=esh
```

The toy strips both `korish` and `koresh` successfully — and that's
exactly the problem this chapter is about. It has no way to say that
only one of them is the *correct* spelling for root `kor`. The real
`ext_strip()` (Section 6.1) has the exact same blind spot, on purpose
(Section 5.1 explains why) — and Part 4 traces what's supposed to
catch it afterward, and where that catch falls short.

## 1.3 Checkpoint

1. `kuremeza` (root `rem`, vowel `e`) and `kuremiza` are both
   structurally valid splits — `rem`+`ez`+`a` and `rem`+`iz`+`a`. Which
   one is correct, and by which rule from Section 1.1?
2. Both `koresh` and `korish` strip cleanly with the toy above. What
   information would the toy need to be given (not infer) to reject one
   of them?

---

# Part 2 — One Concept, Three Files

## 2.1 Detection, generation, and validation are three separate mechanisms

```
   morphology.c                morph_dispatch.c              syntax.c
   ext_strip()                 ext_suffix_surface()           RULE 9
   ───────────                 ────────────────────           ───────
   "does this stem             "given a root and an           "did the WRITER
    END in a recognized         extension TYPE, which          choose the
    extension shape,            spelling should be              correct
    structurally?"              DISPLAYED?"                     spelling?"
        │                              │                            │
        ▼                              ▼                            ▼
   accepts -ish- AND            picks -esh- for kor          checks -ish-/-esh-
   -esh- unconditionally        (root_has_mid_vowel)          and -ik-/-ek- ...
   for ANY root                                                (Part 4 finds what
                                                                 it doesn't check)
```

`ext_strip` (morphology.c, used by the giant `verb_match_inner` from
Chapter 12 — it's called from roughly twenty different points inside
that one function) only needs to know that a word's tail *looks like*
an extension, so it can be peeled off and the rest checked against the
lexicon. `ext_suffix_surface` (morph_dispatch.c) runs in the opposite
direction: given a root and an extension *type* already decided, which
of the two spellings to *display*. Neither one is checking whether a
given real word picked the right spelling for its own root — that's a
third, independent job, and it lives in `syntax.c`, a file this book
has visited before (Chapter 11's `kin_correct`) but never for this
purpose.

## 2.2 Why detection deliberately ignores harmony

If `ext_strip` refused to recognize `korish` (the wrong spelling) as
an extension at all, a real typo like `gukorisha` would fail to parse
as a verb-with-extension entirely — the pipeline would have no
structure to point at and no specific correction to offer, just an
unrecognized word. Accepting both spellings at the detection stage
keeps the word readable as "causative of `kor`, spelled wrong" instead
of "not Kinyarwanda" — but only if something downstream actually
checks the spelling. Part 4 tests how completely that downstream check
covers the extensions detection itself is willing to accept.

---

# Part 3 — Making It Interactive

```c
/* p3_repl.c -- type a verb, see its extension and any harmony error */
#include "kinyarwanda.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    char line[128];
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, "q") == 0) break;

        SentenceAnalysis sa = kin_analyze(line);
        Token *t = &sa.tokens[0];
        printf("  stem=%s verified=%d errors=%d\n",
               t->stem, t->morph.verified, sa.error_count);
        for (int i = 0; i < sa.error_count; i++)
            printf("    [error] %s\n", sa.errors[i].message);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p3_repl.c -L . -lkinyarwanda -o p3_repl
$ printf "gukorisha\nkuremiza\nq\n" | LD_LIBRARY_PATH=. ./p3_repl
  stem=korish verified=1 errors=1
    [error] Uvuguruye amazina (-ish-/-esh-, -ik-/-ek-): 'gukorisha' — igicumbi 'kor' gifite inshuro y'icyembe (e/o), bityo ikinyabiziga kigomba kuba '-esh-', si '-ish-'. Vowel harmony: 'kor' has mid vowel (e/o), so extension must be '-esh-', not '-ish-'.
  stem=remiz  verified=1 errors=0
```

`gukorisha` gets caught — `verified` stays `1` (Section 4.2 explains
why that's not a contradiction) but `errors` correctly reports the
harmony mistake. `kuremiza`, the exact same *kind* of mistake on a
different extension, gets nothing. Part 4 confirms why.

---

# Part 4 — Capstone: A Real, Verified Coverage Gap

## 4.1 RULE 9 checks two allomorph pairs by name — and only those two

`syntax.c`'s RULE 9 is a real, fully-implemented harmony checker, with
its own worked examples in its header comment:

```c
/* RULE 9: Vowel harmony on verb extensions (-ish-/-esh-, -ik-/-ek-)
 * ...
 * gukorisha  → root "kor" has /o/ (mid) → -esh- required → flag gukorisha
 * gusomisha  → root "som" has /o/ (mid) → -esh- required → flag gusomisha
 */
for (int i = 0; i < sa->token_count; i++) {
    ...
    /* Only check harmony-sensitive allomorphs */
    if (strcmp(ext_surf,"ish") != 0 && strcmp(ext_surf,"esh") != 0 &&
        strcmp(ext_surf,"ik")  != 0 && strcmp(ext_surf,"ek")  != 0) continue;
    ...
```

That `if` is a hardcoded list of exactly four strings. `morph_dispatch.c`'s
own `ext_suffix_surface` (Section 6.2) treats a *third* allomorph pair
as equally harmony-sensitive — `VEXT_CAUSATIVE_IZ`'s `-iz-`/`-ez-` — and
a fourth, `VEXT_APPLIC_PASSIVE`'s `-irw-`/`-erw-`. RULE 9's list
doesn't mention either. Test the exact same mistake on each pair:

```c
#include "kinyarwanda.h"
#include <stdio.h>

static void show(const char *w) {
    SentenceAnalysis sa = kin_analyze(w);
    Token *t = &sa.tokens[0];
    printf("%-12s stem=%-8s verified=%d errors=%d\n",
           w, t->stem, t->morph.verified, sa.error_count);
}

int main(void) {
    show("gukoresha");  /* -esh-: correct for root "kor" (mid vowel) */
    show("gukorisha");  /* -ish-: WRONG for root "kor" -- RULE 9 covers this pair */
    show("kuremeza");   /* -ez-: correct for root "rem" (mid vowel)  */
    show("kuremiza");   /* -iz-: WRONG for root "rem" -- same mistake, different pair */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_gap.c -L . -lkinyarwanda -o p4_gap
$ LD_LIBRARY_PATH=. ./p4_gap
gukoresha    stem=koresh  verified=1 errors=0
gukorisha    stem=korish  verified=1 errors=1
kuremeza     stem=remez   verified=1 errors=0
kuremiza     stem=remiz   verified=1 errors=0
```

`gukorisha` — wrong harmony on the `-ish-`/`-esh-` pair — is flagged,
exactly as RULE 9's own comment promises. `kuremiza` — the identical
mistake, same root vowel, same harmony rule, just on the `-iz-`/`-ez-`
pair instead — produces zero errors. `t->morph.verified` is `1` for
both the correct and the incorrect form, in both pairs; it was never
the signal checking this in the first place (Section 4.2). The actual
checking mechanism, RULE 9, covers exactly the two pairs its own
comment names and nothing past them, even though the rule it's
implementing — and `morph_dispatch.c`'s own generation logic — treats
all four pairs the same way.

## 4.2 `verified` was never the harmony check, and that's by design — until it silently isn't

It would be easy to read `gukorisha`'s `verified=1` in Section 4.1 as
the bug. It isn't: `mb->verified` (Chapter 13) only checks that the
pieces `analyse_vinf` extracted, concatenated back together, reproduce
the literal input word — and they do, because `ext_str` in that
concatenation came from parsing `gukorisha` itself, not from an
independent source of truth the way Chapter 13's noun `D`/`RT` tables
were. Asking `verified` to catch a harmony violation is asking the
wrong mechanism the wrong question. The right mechanism is RULE 9 —
and it answers correctly for `gukorisha`. The gap isn't that
`verified` missed it; the gap is that the *only* mechanism built to
catch it doesn't cover every allomorph pair it was modeled on.

## 4.3 A small irony: the header's own example is the mistake

`kinyarwanda.h`'s `VEXT_CAUSATIVE` comment reads:

```c
VEXT_CAUSATIVE,      /* Integeko:   -ish-/-esh-  gukorisha, kwigisha      */
```

`kwigisha` is correct (root `ig`, non-mid, `-ish-`). `gukorisha` —
offered in the same breath as a second example of the *same* rule —
is the wrong-harmony form RULE 9 exists specifically to flag; the
correct citation form is `gukoresha`, confirmed by `lexicon.c`'s own
comment on the root entry: `"kor", /* gukora – to work/do; gukoresha
(kor+esh) = to use */`. Low-stakes on its own, but a clean,
self-contained illustration of the chapter's main point: the harmony
rule is real and consistently implemented everywhere this project
*generates* a form, and just as consistently absent from one place
that *describes* one — the same gap, at a much smaller scale.

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 Why three separate mechanisms, instead of one

Folding harmony checking into `ext_strip` itself would mean detection
fails outright on any harmony mistake — exactly the outcome Section
2.2 argued against. Folding it into `ext_suffix_surface` doesn't work
either: that function is never given the *original word*, only a root
and an extension type already decided elsewhere; it has nothing to
compare against. Keeping validation as a third, separate pass over
already-analyzed tokens (RULE 9) is the only one of the three that
naturally has both the parsed extension *and* the real surface word in
hand at the same time — which is exactly what catching a harmony
mistake requires.

## 5.2 The cost: a third, separately-maintained list has to be kept in sync with the other two

`ext_suffix_surface` enumerates which extension types are
harmony-sensitive in one place (Section 6.2): `VEXT_CAUSATIVE`,
`VEXT_CAUSATIVE_IZ`, `VEXT_STATIVE`, `VEXT_APPLIC_PASSIVE`. RULE 9
enumerates the *surface strings* for harmony-sensitive extensions in a
completely separate place, in a different file, as a hardcoded
4-string list instead of a reference to the same four `VerbExtension`
values. The two lists were two harmony-sensitive types short of
agreeing with each other, and nothing in either file would have
flagged that disagreement — it took writing and running the test in
Section 4.1 to surface it. This is the same shape of finding as
Chapter 12's duplicated `PAST_SP_*` tables and Chapter 13's
four-times-reimplemented verification idea: the same fact, encoded
independently in more than one place, with no mechanism keeping the
copies aligned as the project grows.

---

# Part 6 — Reading the Real Production Code

## 6.1 `ext_strip()`, the bulk of it

```c
static bool ext_strip(const char *stem, VerbExtension *ext_out,
                      char *bare_out, size_t bare_sz) {
    size_t slen = strlen(stem);
    char tmp[KIN_MAX_STEM];

    /* Causative: -ish or -esh (check first -- longer match) */
    if (slen > 4 && (kin_ends_with(stem, "ish") || kin_ends_with(stem, "esh"))) {
        size_t blen = slen - 3;
        strncpy(tmp, stem, blen); tmp[blen] = '\0';
        if (blen >= 2) {
            if (ext_out)  *ext_out = VEXT_CAUSATIVE;
            if (bare_out) { strncpy(bare_out, tmp, bare_sz-1); bare_out[bare_sz-1]='\0'; }
            return true;
        }
    }
    /* Applicative: -ir or -er */
    if (slen > 4 && (kin_ends_with(stem, "ir") || kin_ends_with(stem, "er"))) {
        ...
    }
    /* Stative: -ik- / -ek- (vowel harmony, same as causative) */
    if (slen > 4 && (kin_ends_with(stem, "ik") || kin_ends_with(stem, "ek"))) {
        ...
    }
    /* Reversive -ur, reversive -uk, reciprocal -an,
     * passive-perfect -ejw/-ijw, passive -w,
     * causative -iz-/-ez- allomorph (guarded by kin_is_known_verb_stem),
     * causative-y -z (guarded by kin_is_known_verb_stem on the r-restored form) */
    ...
    return false;
}
```

Eleven ordered checks in total (the full function was quoted in
Chapter 13's research, not reproduced here in full to avoid repeating
160 lines) — every harmony-sensitive pair (`ish`/`esh`, `ik`/`ek`)
accepted unconditionally, exactly as Section 1.2's toy was, and
exactly as Section 2.2 argued it should be. Two of the later checks —
causative `-iz-`/`-ez-` and causative-y `-z` — are *not* unconditional:
they require `kin_is_known_verb_stem(tmp)` to succeed before accepting
the split at all, because by that point in the list a bare
`-z`-ending stem is common enough that without a lexicon check it
would produce far too many false splits. That guard checks the
*root's* validity, never the *extension's* spelling — it's a different
kind of safety check from what RULE 9 does afterward.

## 6.2 `ext_suffix_surface` and `root_has_mid_vowel`, in full

```c
/* Vowel harmony (§2.5.13): roots with last vowel e/o take -esh-/-ek-;
 * roots with last vowel a/i/u take -ish-/-ik-.
 * Returns true when root's last vowel is mid (e/o).                    */
static bool root_has_mid_vowel(const char *root) {
    if (!root || !root[0]) return false;
    for (int k = (int)strlen(root) - 1; k >= 0; k--) {
        char c = root[k];
        if (c == 'e' || c == 'o') return true;
        if (c == 'a' || c == 'i' || c == 'u') return false;
    }
    return false;
}

/* Surface form of harmony-sensitive extensions (causative, stative). */
static const char *ext_suffix_surface(VerbExtension e, const char *root) {
    if (e == VEXT_CAUSATIVE)      return root_has_mid_vowel(root) ? "esh" : "ish";
    if (e == VEXT_CAUSATIVE_IZ)   return root_has_mid_vowel(root) ? "ez"  : "iz";
    if (e == VEXT_STATIVE)        return root_has_mid_vowel(root) ? "ek"  : "ik";
    if (e == VEXT_APPLIC_PASSIVE) return root_has_mid_vowel(root) ? "erw" : "irw";
    return ext_suffix(e);
}
```

This is the function whose own scope (four harmony-sensitive types)
disagrees with RULE 9's scope (two), as established in Section 5.2.
`root_has_mid_vowel` scans from the root's end backward for the first
character that's any vowel at all, returning as soon as it finds one —
the same "scan back to the nearest deciding character" shape Chapter
12's `kin_vv_join` used for its own boundary checks.

## 6.3 RULE 9's checking logic, in full

Quoted and tested in Section 4.1 — the four-string `if`, the
mid-vowel scan (a second, independent copy of the same scan
`root_has_mid_vowel` performs, written directly inline here rather
than calling that function), and the corrected-word construction that
feeds `corrected_word` (Chapter 11's fix) for `kin_correct` to use.

---

# Part 7 — Looking Back, Looking Forward

## 7.1 This was always running underneath every conjugated and infinitive verb shown since Chapter 3

Every `[EXT]` row in every morpheme table this book has displayed came
from `ext_strip` (detection) and was labelled using `ext_suffix`/
`ext_suffix_surface` (display). Chapter 3 showed the table; this
chapter showed the machine.

## 7.2 One more thing noticed, not chased

While testing this chapter's examples, a real word —
`yandikijwe` ("it was written," passive-perfect of `kwandika`, the
exact derivation `ext_strip`'s own comment cites as its motivating
example for the `-ijw-` passive-perfect pattern) — failed to be
recognized as Kinyarwanda at all when run through the full pipeline,
even though a structurally similar form, `yapeshejwe`, parsed
correctly. That's a real, reproducible result, and it would take
re-opening `verb_match_inner` — the 1,100-line function Chapter 12
already named as unread territory — to find out why. Named here and
left there, the same way Chapter 13 left `ukuri` open: a real result,
not chased to its root cause, rather than silently dropped.

---

# Part 8 — Practice

### Beginner

1. Using Section 1.1's table, predict by hand whether `gusiba`'s
   causative (root `sib`) should take `-ish-` or `-esh-`, then verify
   with `kin_analyze("gusibisha")` and `kin_analyze("gusibesha")`.
2. Run Section 4.1's test on one more root of your choosing with a mid
   vowel, constructing both the correct and incorrect `-iz-`/`-ez-`
   form, and confirm the gap reproduces.

### Intermediate

3. RULE 9's corrected-word logic (Section 6.3) finds the wrong
   extension string with `strstr` and splices in the right one. Find a
   real word where the wrong extension string also happens to appear
   earlier in the word for an unrelated reason, and check whether
   `strstr` finds the wrong occurrence.
4. `ext_strip`'s causative-`iz`/`ez` branch (Section 6.1) requires
   `kin_is_known_verb_stem(tmp)` before accepting the split. Construct
   a stem ending in `-iz` whose bare form is *not* a known verb stem,
   and confirm `ext_strip` correctly refuses to split it.

### Advanced

5. Propose the smallest change to RULE 9 that would close the gap
   found in Section 4.1 for both `-iz-`/`-ez-` and `-irw-`/`-erw-`,
   reusing `root_has_mid_vowel` instead of RULE 9's own inline copy of
   the same scan.
6. Section 7.2 named a real, unexplained failure (`yandikijwe`) without
   chasing it. Using this book's standard method, open
   `verb_match_inner` specifically looking for where `-ijw-`-shaped
   stems are handled, and report what you find.
7. Section 5.2 compared this chapter's finding to Chapter 12's
   duplicated `PAST_SP_*` tables and Chapter 13's four-times-
   reimplemented verification idea. All three involve the same project
   fact encoded in more than one place. Sketch one general mechanism
   (a shared table, a generated check, a test) that would have caught
   any of the three automatically, without a human reading both sides
   and noticing the mismatch.

---

## Key takeaways

- Kinyarwanda verb extensions (passive, causative, applicative,
  reciprocal, stative, reversive) split into derivational families,
  several with two vowel-harmony-conditioned spellings chosen by the
  root's last vowel (mid `e`/`o` vs. non-mid `a`/`i`/`u`).
- Three separate mechanisms across three files implement this:
  `ext_strip` (`morphology.c`) detects the *shape* of an extension
  without checking harmony, by design; `ext_suffix_surface`
  (`morph_dispatch.c`) generates the harmony-correct spelling given a
  root and a known extension type; RULE 9 (`syntax.c`) is the only one
  of the three that checks whether a real word's actual spelling
  matches the harmony rule.
- Real, verified finding: RULE 9 only checks two of the four
  harmony-sensitive allomorph pairs `ext_suffix_surface` itself
  recognizes. `gukorisha` (wrong `-ish-`/`-esh-` choice) is correctly
  flagged; `kuremiza` (the identical kind of mistake, on the
  `-iz-`/`-ez-` pair) produces zero errors — confirmed by running both
  through the real pipeline.
- `MorphBreakdown.verified` was never the mechanism meant to catch
  this — it checks internal consistency of the parse, not linguistic
  correctness — and stays `1` for both the correct and incorrect form
  in every case tested, which is expected, not a separate bug.
- A small, self-contained instance of the same gap: `kinyarwanda.h`'s
  own `VEXT_CAUSATIVE` comment cites `gukorisha` as an example
  alongside the correct `kwigisha`, when the linguistically correct
  citation form (per `lexicon.c`'s own comment) is `gukoresha`.
- One result noticed but not chased to root cause: `yandikijwe`, a
  real word matching `ext_strip`'s own motivating example for the
  `-ijw-` passive-perfect pattern, isn't recognized as Kinyarwanda at
  all by the full pipeline — left as an open, documented result rather
  than guessed at, consistent with how Chapter 13 left `ukuri` open.

## Sources quoted in this chapter

- `src/morphology.c`'s `ext_strip()`, called from roughly twenty sites
  inside Chapter 12's `verb_match_inner`.
- `src/morph_dispatch.c`'s `ext_suffix`, `root_has_mid_vowel`, and
  `ext_suffix_surface`.
- `src/syntax.c`'s RULE 9 (vowel harmony on verb extensions).
- `include/kinyarwanda.h`'s `VerbExtension` enum and its inline
  documentation comments.
- `src/lexicon.c`'s comment on the `"kor"` root entry, used to confirm
  the correct citation form against the header's own (incorrect)
  example.
- Every `pN_*.c` program in this chapter was actually compiled with
  `gcc -std=c99 -Wall -Wextra` and actually executed.

## Coming up in Chapter 15

This chapter's open result — `yandikijwe` failing to parse at all —
points straight back into `verb_match_inner`, the function Chapter 12
named as the largest in the project and left mostly unread. Chapter 15
goes back in, this time looking specifically for how passive-perfect
and other compound-extension surface forms are matched, to find out
whether `yandikijwe` is a gap in that matching logic or something
narrower.
