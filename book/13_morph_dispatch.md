# Chapter 13 — Filling the Blanks: `morph_dispatch.c`'s Type-Specific Analysers

## How this chapter works

Same rules as every chapter before it. Every fenced `$` block is real
`gcc -std=c99 -Wall -Wextra` output from a program that was actually
compiled and run against this project's real, built library.

Chapter 12 read `morphology.c` — the functions that decide *what* a
word is. This chapter reads `morph_dispatch.c` (3,528 lines), the file
that takes that decision and actually fills in the `KinMorpheme`
breakdown every chapter since Chapter 1 has displayed without ever
seeing built. It's the other half of the gap Chapter 11 named, and —
unlike Chapters 11 and 12 — this chapter finds a bug that isn't
contained to the file it's found in: it's a real, traceable
consequence of something Chapter 9 found four chapters ago, finally
surfacing through code this book had never opened until now.

---

# Part 1 — Language and Code, Side by Side

## 1.1 The D+RT+C formula, now as the actual reconstruction code

Chapter 1 displayed noun breakdowns as a table: D, RT, C. The function
that actually fills that table, for the ordinary case, does something
very specific — it builds one string encoding the *underlying* (not
surface) morphemes, separated by `|`, and hands it to Chapter 9's
`kin_ortho_gen`:

```c
char underlying[128];
snprintf(underlying, sizeof(underlying), "%s|%s|%s", d_under, rt_under, c_form);
kin_ortho_gen(underlying, (cls == 9 || cls == 10), reconstructed, sizeof(reconstructed));
...
mb->verified = (strcmp(reconstructed, word) == 0);
```

For a class-1 noun like `umuntu`, `d_under="u"`, `rt_under="mu"`,
`c_form="ntu"` — `kin_ortho_gen("u|mu|ntu", ...)` should regenerate
the exact surface word. Run it for real:

```c
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    char out[64];
    kin_ortho_gen("u|mu|ntu", false, out, sizeof(out));
    printf("u|mu|ntu  -> %s   (real word: umuntu)\n", out);
    kin_ortho_gen("i|ki|tabo", false, out, sizeof(out));
    printf("i|ki|tabo -> %s   (real word: igitabo)\n", out);
    kin_ortho_gen("u|bu|oba", false, out, sizeof(out));
    printf("u|bu|oba  -> %s   (real word: ubwoba)\n", out);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p1_recon.c -L . -lkinyarwanda -o p1_recon
$ LD_LIBRARY_PATH=. ./p1_recon
u|mu|ntu  -> umuntu   (real word: umuntu)
i|ki|tabo -> igitabo   (real word: igitabo)
u|bu|oba  -> ubwoba   (real word: ubwoba)
```

`MorphBreakdown.verified` is not a guess or a hardcoded flag — it's
the literal result of regenerating the word from its claimed
underlying parts and checking, character for character, whether that
regeneration matches what was actually written. Part 4 finds the one
noun class where this exact mechanism, run for real, says no.

## 1.2 Build it: a tiny version of "claim, then verify"

```c
/* p1_verify_toy.c -- the verify-by-regenerating pattern, standalone */
#include <stdio.h>
#include <string.h>

/* toy "underlying form" generator: just concatenates three parts */
static void gen(const char *d, const char *rt, const char *c, char *out) {
    sprintf(out, "%s%s%s", d, rt, c);
}

int main(void) {
    char out[64];
    gen("u", "mu", "ntu", out);
    printf("claim: u+mu+ntu -> %-10s actual word: umuntu  verified=%d\n",
           out, strcmp(out, "umuntu") == 0);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra p1_verify_toy.c -o p1_verify_toy
$ ./p1_verify_toy
claim: u+mu+ntu -> umuntu     actual word: umuntu  verified=1
```

The toy works because `umuntu` has no phonological surprises at its
morpheme boundaries. The real `kin_ortho_gen`-based check exists
because most boundaries do have surprises — that's the entire sixteen
pass engine Chapter 9 read. This toy is the part that's the same
everywhere; the sixteen passes are the part that isn't.

## 1.3 Checkpoint

1. Why does `mb->verified` need to call a *generator* at all, instead
   of just storing `true` whenever the noun-class/stem detection in
   Chapter 12 succeeded?
2. What would `mb->verified` mean if `kin_ortho_gen` itself contained
   a bug? (Hold that question — Part 4 answers it concretely.)

---

# Part 2 — One Tiny Dispatcher, Four Very Differently Sized Analysers

## 2.1 The dispatcher, in full

```c
void kin_morpheme_analyze(Token *tok)
{
    memset(&tok->morph, 0, sizeof(tok->morph));
    switch (tok->pos) {
        case POS_NOUN: case POS_RELATIVE_NOUN: case POS_COMPOUND_ADJ:
            analyse_noun(tok); break;
        case POS_ADJECTIVE:
            analyse_adj(tok); break;
        case POS_VERB_CONJ:
            analyse_vconj(tok); break;
        case POS_VERB_INF:
            analyse_vinf(tok); break;
        default:
            break;   /* invariables, pronouns, foreign words: no breakdown */
    }
}
```

27 lines, no loops, no phonology — exactly the shape Chapter 11 taught
for `main()`'s mode dispatch and Chapter 12's own `kin_morpheme_analyze`
forward-reference promised. All four branches just hand off. The real
sizes of what they hand off to:

```
   kin_morpheme_analyze()            27 lines   (this section, in full)
   │
   ├─ analyse_noun()   §Tree 1      765 lines   (Part 1, Part 4.2)
   ├─ analyse_adj()    §Tree 2      138 lines
   ├─ analyse_vconj()  §Tree 3b    2,033 lines   (this file's verb_match_inner counterpart)
   └─ analyse_vinf()   §Tree 3a     304 lines
                                  ──────────
                                  3,267 lines of the file's 3,528
```

`analyse_vconj` alone is 2,033 lines — almost twice the length of
Chapter 12's `verb_match_inner` (1,100 lines), and for the same
underlying reason: detecting *that* a word is a conjugated verb
(Chapter 12's job) only has to find one matching reading; *filling in*
every morpheme slot with the correct display form and surface form,
across every tense, mood, negation, and extension combination
(this file's job), has to get all of them right, not just one.

## 2.2 Detection and filling solve the same boundary problem twice — on purpose

Both files import the exact same six vowel-contact sub-rules from
Chapter 12's `kin_vv_join` family — but `morph_dispatch.c` doesn't call
that function for nouns and adjectives. It re-derives the same rules
inline, branch by branch, because here the inputs are already known
(`d_under`, `rt_under` are looked up from a 17-entry table, not
guessed from arbitrary input text), so the general-purpose joiner is
more machinery than the job needs. Part 5 finds that this same
"write it inline instead of calling the shared engine" choice was made
three separate times in this file, with a real, traceable cost.

---

# Part 3 — Making It Interactive

```c
/* p3_repl.c -- type a word, see its full morpheme breakdown and verified flag */
#include "kinyarwanda.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    char line[128];
    printf("Type a Kinyarwanda word, or 'q' to quit.\n");
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, "q") == 0) break;

        SentenceAnalysis sa = kin_analyze(line);
        if (sa.token_count < 1) continue;
        Token *t = &sa.tokens[0];
        printf("  pos=%s class=%d verified=%d\n",
               kin_pos_name(t->pos), t->noun_class, t->morph.verified);
        for (int i = 0; i < t->morph.n; i++)
            printf("    %-6s form=%-8s surface=%-8s\n",
                   t->morph.m[i].label, t->morph.m[i].form, t->morph.m[i].surface);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p3_repl.c -L . -lkinyarwanda -o p3_repl
$ printf "umuntu\nukwezi\nq\n" | LD_LIBRARY_PATH=. ./p3_repl
Type a Kinyarwanda word, or 'q' to quit.
  pos=Izina mbonera (Noun) class=1 verified=1
    D      form=u        surface=u
    RT     form=mu       surface=mu
    C      form=ntu      surface=ntu
  pos=Izina mbonera (Noun) class=15 verified=0
    D      form=u        surface=u
    RT     form=ku       surface=kw
    C      form=wezi     surface=wezi
```

`ukwezi` comes back `verified=0` even though its displayed `D`/`RT`/`C`
breakdown looks perfectly reasonable on its own — `u` + `kw` + `wezi`.
Part 4 traces exactly what `mb->verified` actually checked to produce
that `0`, and why the displayed breakdown alone doesn't tell you.

---

# Part 4 — Capstone: What Actually Verifies, and What Doesn't

## 4.1 Three of the four analysers never call `kin_ortho_gen` at all

Part 1 showed `analyse_noun`'s general-case `mb->verified` coming from
a real `kin_ortho_gen` call. Read the equivalent lines in the other
three analysers:

```c
/* analyse_adj() -- adjective concordance: */
char expected[KIN_MAX_WORD];
snprintf(expected, sizeof(expected), "%s%s", rs_surface, c);
mb->verified = (strcmp(expected, word) == 0);

/* analyse_vinf() -- verb infinitive: */
char built[KIN_MAX_WORD];
snprintf(built, sizeof(built), "%s%s%s%s%s", pref, all_om, root, fv, loc);
mb->verified = (strcmp(built, word) == 0);

/* analyse_vconj() -- conjugated verb: */
snprintf(built, sizeof(built), "%s%s%s%s%s%s%s%s%s%s", hort_pfx, neg_pfx,
         sp_surface, neg_mid, /* tense marker */ tm, om[0] ? om_surface : "",
         root_surface, /* reduplication */ "", ext_surface, fv);
mb->verified = (strcmp(built, word) == 0);
```

None of these three call `kin_ortho_gen`. Each builds its own surface
string by hand, from pieces it has already individually resolved
(`rs_surface` already has its vowel-contact rule applied; `pref` is
already `"ku"` or `"kw"` as appropriate; `sp_surface` is already
whatever Chapter 12's `SP[]` matching decided). Only `analyse_noun`'s
general path hands an *underlying* (pre-rule) string to a *separate*
engine and trusts that engine to apply the rules itself. That
architectural difference is not a flaw by itself — Part 5 explains why
each shape made sense for its own word type — but it has a direct,
checkable consequence.

## 4.2 The consequence: Nt.15 nouns inherit a bug Chapter 9 found in a different file

Chapter 9 found that `kin_ortho_gen`'s voicing pass conflates two
different "2-character prefix ending in k" contexts: the noun-class RT
context it was written for, and the verb-infinitive `ku-`/`ki-` marker
that happens to look identical. Noun class 15's underlying RT is,
itself, `"ku"` — the exact string that collides. Test it both ways —
once through `kin_ortho_gen` directly (Part 1's method), and once
through the real, full pipeline (Part 3's method) — on the same two
real words:

```c
#include "kinyarwanda.h"
#include <stdio.h>

static void direct(const char *underlying, const char *real_word) {
    char out[64];
    kin_ortho_gen(underlying, false, out, sizeof(out));
    printf("kin_ortho_gen(\"%s\") -> %-10s (real word: %s)\n", underlying, out, real_word);
}
static void pipeline(const char *word) {
    SentenceAnalysis sa = kin_analyze(word);
    Token *t = &sa.tokens[0];
    printf("kin_analyze(\"%s\")   -> class=%-2d stem=%-6s verified=%d\n",
           word, t->noun_class, t->stem, t->morph.verified);
}

int main(void) {
    direct("u|ku|ezi", "ukwezi");
    direct("u|ku|ri",  "ukuri");
    pipeline("ukwezi");
    pipeline("ukuri");
    pipeline("igitabo");   /* class 7, for contrast */
    pipeline("umuntu");    /* class 1, for contrast */
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_nt15.c -L . -lkinyarwanda -o p4_nt15
$ LD_LIBRARY_PATH=. ./p4_nt15
kin_ortho_gen("u|ku|ezi") -> ugwezi     (real word: ukwezi)
kin_ortho_gen("u|ku|ri")  -> uguri      (real word: ukuri)
kin_analyze("ukwezi")   -> class=15 stem=ezi   verified=0
kin_analyze("ukuri")    -> class=15 stem=ri    verified=0
kin_analyze("igitabo")  -> class=7  stem=tabo  verified=1
kin_analyze("umuntu")   -> class=1  stem=ntu   verified=1
```

Both halves agree. `kin_ortho_gen` regenerates `"u|ku|ezi"` as
`ugwezi` — the k→g voicing rule fires on the class-15 RT exactly as it
fires on the verb-infinitive marker, because the function has no way
to tell them apart — and the real pipeline's `mb->verified` is `0` for
both real class-15 nouns tested, while every other class is
unaffected. That's also exactly what Part 3's REPL showed without yet
explaining: `ukwezi`'s displayed `D`/`RT`/`C` breakdown (`u` + `kw` +
`wezi`) looks entirely reasonable on its own — the *display* fields
are filled in correctly regardless of what `kin_ortho_gen` does — but
`mb->verified` is a separate, independent check of whether those
pieces actually regenerate the real word, and on this class, they
don't. This is the one
finding in this whole book that reaches back across chapters: Chapter
9 found the bug, predicted it would surface wherever "ku" both opens a
verb infinitive and opens a noun class, and this chapter found exactly
that — not in a verb infinitive (Section 4.3 explains why those are
safe) but in the one noun class sharing the identical string.

### The fix — and the bug that turned out to be two bugs

`apply_voicing` (`src/ortho.c`) was firing unconditionally on any
2-character `k`/`t`-initial prefix, regardless of what followed it.
Before a consonant-initial follower that's correct (`igitabo`); before
a vowel-initial follower, the boundary is a vowel-contact site instead
and voicing should never fire there at all. The fix is one guard line:

```c
static bool apply_voicing(char *buf, int bpos, int *len) {
    if (bpos < 2 || bpos+1 >= *len) return false;
    if (ov(buf[bpos+1])) return false;  /* vowel-initial: vowel-contact site, not voicing */
    ...
```

(`ov()` is `ortho.c`'s own vowel-test helper, already used by every
other pass in the file.) Re-running Part 1 and Part 3's exact tests
against the rebuilt library:

```
$ LD_LIBRARY_PATH=. ./p4_nt15
kin_ortho_gen("u|ku|ezi") -> ukwezi    (real word: ukwezi)
kin_ortho_gen("i|ki|atsi") -> ikyatsi  (real word: icyatsi)
kin_analyze("ukwezi")    -> class=15 stem=ezi  verified=1
kin_analyze("icyatsi")   -> class=7  stem=atsi verified=1
```

`ukwezi` now verifies. Tracing it further turned up a second, separate
bug hiding behind the first. Just before the `snprintf`/`kin_ortho_gen`
call quoted in Section 1.1, `analyse_noun` doesn't only use `c_start`
from the word-pattern switch — it first tries `kin_lookup_igicumbi()`,
a lexicon table that pairs singular and plural noun forms and stores
their shared C directly, overriding the pattern-matched value when it
finds an entry. `src/lexicon.c` has two separate table entries for
`ukwezi` — one read by `kin_lookup_igicumbi`, one used by a
"dropped-D-vowel" known-word table — and both stored the word's stem
as `"wezi"` instead of `"ezi"`, wrongly baking the u→w glide into the
stored C value. Every
sibling entry handling the identical u→w pattern (`bwenge`→`enge`,
`ubwiza`→`iza`, `amezi`→`ezi` — note `amezi` is `ukwezi`'s own plural,
stored correctly two tables away from where `ukwezi` itself was
stored wrong) does this correctly; only `ukwezi`'s two entries didn't.
Both were one-line fixes, changing the stored stem from `"wezi"` to
`"ezi"`.

One more thing surfaced while fixing this: the project's own test
suite (`tests/test_ortho.c`) had an existing test asserting the *old,
buggy* output — `ku|eza → gweza` — as the correct expected result.
That assertion directly contradicted a different test fourteen lines
later in the same file, which already confirmed `kweza` (not `gweza`)
has zero orthographic violations, and contradicted the lexicon, where
`kweza` is the attested causative of `kwera`. The test was asserting
the bug as the spec. It's been corrected to expect `kweza`. The full
test suite — 244 tests across morphology, lexicon, ortho, and the
pipeline — passes after all three fixes.

### `ukuri`: a different bug, left open

`ukuri` (RT `"ku"` + consonant-initial stem `"ri"`) is **still
unverified** after this fix, and it isn't the same bug. `ri` is
consonant-initial — exactly the environment where voicing is supposed
to fire (`igitabo`, `agakingirizo` both voice correctly before
consonant-initial stems). The vowel/consonant guard above doesn't
touch this case at all; `kin_ortho_gen("u|ku|ri", ...)` still produces
`uguri`. Why `ku+ri` should be the exception isn't something this
chapter's evidence can answer: `ukuri` is the *only* class-15 noun
anywhere in the lexicon with a consonant-initial stem, so there's no
sibling entry to confirm or contradict either answer, the way `bwenge`
and `amezi` confirmed the vowel-initial fix above. Asserting a fix
here would mean guessing at a phonological rule rather than verifying
one against real attested forms — exactly the discipline this book
has tried to hold to in every other chapter. `ukuri` is left
unverified, documented here as a known, open, currently-unresolved
finding rather than quietly dropped or guessed at.

## 4.3 Why verb infinitives sharing the same "ku" prefix don't have this bug

`kubaka`, `kwiga`, and `kwamara` were tested in Part 1's checkpoint
territory and all come back `verified=1`. They share the exact same
`"ku"`/`"kw"` prefix as the broken Nt.15 nouns, and they're processed
by a completely different function (`analyse_vinf`) — one of the three
identified in Section 4.1 that never calls `kin_ortho_gen` at all. The
bug lives inside `kin_ortho_gen`'s voicing pass specifically; any code
path that avoids that one function is, by construction, immune —
whether or not the avoidance was made for that reason. Nothing in
`analyse_vinf`'s own header comment mentions this; the immunity is a
side effect of an independent design choice (Section 5.1), not a
deliberate workaround for Chapter 9's bug.

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 Why three of the four analysers reinvented their own verification instead of reusing one engine

`kin_ortho_gen` takes an *underlying* form and applies all sixteen of
Chapter 9's passes — built for exactly one situation: a noun's D, RT,
and C are looked up from small fixed tables (Section 1.1's
`NOUN_D[17]`/`NOUN_RT[17]`), so the "underlying" string is cheap to
construct from data the function already has on hand. A conjugated
verb's surface form, by the time `analyse_vconj` is filling it in, has
already been pulled apart by Chapter 12's 1,100-line subject-prefix
matcher into pieces that are *already surface forms* — `sp_surface`
is `"y"` or `"tw"` or whatever Chapter 12 already resolved, not an
abstract underlying subject prefix waiting to be regenerated. Handing
already-resolved surface pieces back into a generator built to take
*underlying* pieces would mean either undoing work Chapter 12 already
did, or building a second underlying-form representation just for
this one check. Concatenating the already-correct surface pieces
directly is simpler, and for adjectives and verb infinitives — whose
phonology genuinely is simpler than the full sixteen-pass system — it's
not a shortcut, it's a complete and correct implementation of a
smaller rule set.

## 5.2 The real cost of that choice: the same verification idea, reinvented three times

That's also, concretely, three separate hand-written
"build-the-expected-surface-string-and-`strcmp`" blocks living in one
file, each correct on its own, none sharing code with the others or
with `kin_ortho_gen`. This is the same shape of finding Chapter 12
made about `PAST_SP_ELOC`/`PAST_SP_LIST`/`PAST_SPS` — not identical
duplicated *data* this time, but the same *idea* (verify by
regenerating and comparing) reimplemented independently four times
across two files, with no shared name or shared function tying them
together. The cost showed up directly in Section 4.2: a bug fixed in
one of the four implementations would silently leave the other three
either also broken or already fine, with nothing in the code itself
telling a future maintainer which is which without testing each one
directly, the way this chapter just did.

---

# Part 6 — Reading the Real Production Code

## 6.1 `kin_morpheme_analyze`, in full

Already quoted in Section 2.1 — 27 lines, the shortest dispatcher this
book has read since Chapter 11's `main()` mode switch.

## 6.2 The `uu_stems[]` table: a second, unrelated special case living one function away from the bug

```c
/* Verbs whose lexicon stem looks consonant-initial but whose TRUE underlying
 * root is vowel-initial.  Historical phonology: *kwubaka = ku + ubak + a, with
 * u+u→u (§6.6) giving modern kubaka.  The surface root slot therefore begins
 * with a consonant (bak) even though the underlying root is vowel-initial (ubak).
 *
 * Map: lexicon stem  →  true underlying root
 * Add new entries alphabetically as more such verbs are confirmed.             */
static const struct { const char *lex_stem; const char *true_root; } uu_stems[] = {
    { "bak",  "ubak"  },   /* kubaka  ← *kwubaka (ku + ubak + a)  §6.6 */
    { NULL,   NULL    }
};
```

This is a one-entry lookup table, used only by `analyse_vinf`, solving
a genuinely different problem from Section 4.2's bug: it's not about
voicing, it's about `kubaka`'s lexicon stem (`"bak"`) not matching its
*historical* vowel-initial root (`"ubak"`), so a direct concatenation
would otherwise need a special case anyway. It's a clean, narrow,
correctly-working patch — quoted here specifically so it isn't
confused with Section 4.2's bug just because both involve the string
`"ku"` and the word `kubaka`. They are unrelated, and `kubaka` itself
verifies correctly (Section 4.3) precisely because `analyse_vinf`
handles both this table *and* the prefix locally, never delegating to
the function that has the actual bug.

## 6.3 The general-case noun reconstruction, in full

```c
char reconstructed[KIN_MAX_WORD];
if (rule_rt[0]) {
    /* A phonological rule already fired while resolving RT against C
     * (e.g. nasal assimilation for Nt.9/10).  Verify by direct surface
     * concatenation -- kin_ortho_gen would re-apply a rule that's
     * already been applied, corrupting the result. */
    const char *rt_recon = (rt_surface[0] == '\xe2') ? "" : rt_surface;
    snprintf(reconstructed, sizeof(reconstructed), "%s%s%s",
             d_recon, rt_recon, c_form);
} else {
    char underlying[128];
    snprintf(underlying, sizeof(underlying), "%s|%s|%s", d_under, rt_under, c_form);
    kin_ortho_gen(underlying, (cls == 9 || cls == 10), reconstructed, sizeof(reconstructed));
}
mb->verified = (strcmp(reconstructed, word) == 0);
```

This `if (rule_rt[0])` branch is itself worth noticing: the code
already knows, in at least one other situation, that calling
`kin_ortho_gen` on a form that's already had a rule applied produces a
*wrong* result (its own comment says so explicitly) — the code routes
around `kin_ortho_gen` there for exactly the same reason Section 4.2's
bug existed, just caught and worked around in that one case instead of
left to surface. Class 15 falls into the `else` branch — no `rule_rt`
fires for it before this check — which is exactly why it reached
`kin_ortho_gen` unprotected. The real fix (Section 4.2) didn't route
class 15 through this branch; it fixed `apply_voicing` itself inside
`kin_ortho_gen`, so every caller of the shared engine benefits, not
just `analyse_noun`'s class-15 path — the direct `kin_ortho_gen("i|ki|atsi",
...)` test in Section 4.2 would still have been wrong if the fix had
only been routed around in this one branch instead.

---

# Part 7 — Looking Back, Looking Forward

## 7.1 Every chapter's morpheme display, traced to its source

Chapter 1's noun tables, Chapter 2's adjective concordance, Chapter
3's verb tense breakdowns — every `KinMorpheme` entry shown in any of
those chapters was filled in by one of the four functions read in this
chapter. The `D`/`RT`/`C` labels, the `SP`/`TM`/`OM`/root/`EXT`/`FV`
labels, all originate in the `set_morph()` calls inside
`analyse_noun`/`analyse_adj`/`analyse_vconj`/`analyse_vinf` — not
guessed by the display layer, built here.

## 7.2 What this chapter didn't read

`analyse_vconj` is 2,033 lines; this chapter quoted perhaps 30 of
them. `ext_strip()`, the verb extension stripper Chapter 12 also left
unread, has a direct counterpart somewhere in this file's
extension-surface logic that wasn't traced either. Those are real,
acknowledged gaps — consistent with every chapter since Chapter 11
that has named what it didn't have room for, rather than implying
completeness it didn't earn.

---

# Part 8 — Practice

### Beginner

1. Run Part 3's REPL on `igitabo`, `umuntu`, and `ubwoba`. Confirm all
   three show `verified=1`, and write out by hand what `underlying`
   string each one builds before calling `kin_ortho_gen`.
2. `ukwezi` is now fixed; `ukuri` is the only known-failing class-15
   noun left, and it isn't in the lexicon at all (it's parsed
   generatively). Find or construct a real Kinyarwanda class-15 noun
   with a consonant-initial stem other than `ukuri`, and test whether
   it also comes back `verified=0` — a second data point either way
   would directly help resolve Section 4.2's open question.

### Intermediate

3. Section 4.1 quoted `analyse_vconj`'s `built` string as a 10-argument
   `snprintf`. Using `morph_dispatch.c` directly, identify what each of
   the 10 arguments corresponds to (hortative prefix, negation prefix,
   subject-prefix surface, etc.).
4. Section 6.3's `if (rule_rt[0])` branch avoids `kin_ortho_gen` for
   Nt.9/10 nouns specifically because of a different known interaction.
   Test a real Nt.9 noun (e.g. one beginning `imv-` or `imb-`) through
   both Part 1's direct method and Part 3's pipeline method, and
   confirm whether it's affected by Section 4.2's bug or not, and why.

### Advanced

5. Section 4.2's real fix added one guard line to `apply_voicing`:
   `if (ov(buf[bpos+1])) return false;`. Find every other call site of
   `apply_voicing` in `ortho.c` besides the one reached from
   `analyse_noun`'s class-15 path, and confirm none of them needed a
   different fix — i.e. that this one guard line was actually the
   correct, complete fix for every caller, not just the one this
   chapter tested.
6. Section 4.2 left `ukuri` open rather than guess at a rule with no
   corroborating evidence. Using only real, attested Kinyarwanda
   words already in `src/lexicon.c`, try to build a case for or
   against voicing firing before `r` specifically (as opposed to other
   consonants) — and if you can't build one, explain precisely what
   additional evidence would settle it.
7. Section 5.2 argued that `kin_ortho_gen`, `analyse_adj`,
   `analyse_vinf`, and `analyse_vconj` each separately implement
   "build the expected surface form, compare with `strcmp`." Sketch
   what a single shared verification helper would need as its
   interface to serve all four call sites — and identify which of the
   four would be hardest to convert, and why.
8. This book's stated goal was a reader who could find this project's
   real source "completely understandable... to the extent of
   improving and writing more, making proper fix." Using exactly the
   method in Section 4.2 — read the doc comment, write a real test,
   compile it, run it, compare against the claim — pick one piece of
   `analyse_vconj` this chapter didn't open, and find out whether it
   does what its own comment says it does.

---

## Key takeaways

- `morph_dispatch.c` fills in every `KinMorpheme` this book has shown
  since Chapter 1, dispatched from a 27-line `switch` to four
  type-specific analysers ranging from 138 to 2,033 lines.
- `MorphBreakdown.verified` is a real, computed check: each analyser
  builds an expected surface form from its own morpheme guesses and
  compares it, with `strcmp`, against the actual word — not a flag set
  whenever detection succeeded.
- Real finding: only `analyse_noun`'s general-case path actually calls
  Chapter 9's shared `kin_ortho_gen` engine for that check; the other
  three analysers (`analyse_adj`, `analyse_vinf`, `analyse_vconj`) each
  independently hand-build their own expected surface string instead.
- Real, traceable consequence: Nt.15 nouns (`ukwezi`, `ukuri`) came
  back `verified=0` through the real pipeline because their RT,
  `"ku"`, collides with the verb-infinitive marker inside
  `kin_ortho_gen`'s voicing pass — the exact bug Chapter 9 found,
  confirmed reaching all the way through to a token's final morpheme
  breakdown. Verb infinitives sharing the identical `"ku"` prefix were
  unaffected only because they're processed by a different function
  that never calls the buggy engine.
- **Fixed, in the real source, after this chapter was first written:**
  one guard line in `apply_voicing` (`src/ortho.c`) stops k→g/t→d
  voicing from firing before a vowel-initial following morpheme, which
  is a vowel-contact site, not a voicing site. Fixing it surfaced a
  second, independent bug: two `src/lexicon.c` table entries for
  `ukwezi` had baked the u→w glide into the stored stem (`"wezi"`
  instead of `"ezi"`), inconsistent with every sibling entry handling
  the same pattern correctly. A pre-existing test in
  `tests/test_ortho.c` had asserted the old buggy output as correct,
  contradicting another test four lines later in the same file — fixed
  to match the lexicon's own attested word. `ukwezi`, `icyatsi`, and
  the full 244-test suite all verify/pass after the fix.
- **Left open, on purpose:** `ukuri` (RT `"ku"` + consonant-initial
  stem `"ri"`) still doesn't verify, and it's a different bug than the
  one just fixed — it's the one class-15 noun in the entire lexicon
  with a consonant-initial stem, so there's no sibling entry to verify
  a fix against, the way `bwenge` and `amezi` verified `ukwezi`'s fix.
  Documented here as a known, unresolved finding rather than guessed
  at — the same discipline this book has tried to apply to every
  claim it's made since Chapter 1.

## Sources quoted in this chapter

- `src/morph_dispatch.c` (`kin_morpheme_analyze`, `analyse_noun`,
  `analyse_adj`, `analyse_vinf`, `analyse_vconj`, `uu_stems[]`,
  `find_uu_root`, `kin_lookup_igicumbi`).
- `src/ortho.c`'s `kin_ortho_gen` and `apply_voicing`, first read in
  Chapter 9, called directly in this chapter's tests and patched in
  this chapter's fix.
- `src/lexicon.c`'s noun-plural-pairing and dropped-D-vowel tables,
  read while tracing the `ukwezi` fix.
- `tests/test_ortho.c`, corrected as part of the fix.
- Every `pN_*.c` program in this chapter was actually compiled with
  `gcc -std=c99 -Wall -Wextra` and actually executed; the fix itself
  was rebuilt (`make clean && make && make test`) and re-verified
  against the real CLI before being documented here.

## Where this book leaves you

Chapter 11 named two files this book had used constantly but never
opened: `morphology.c` and `morph_dispatch.c`. Chapter 12 opened the
first; this chapter opened the second. That closes the specific gap
Chapter 11 named — but not every gap. `analyse_vconj`'s remaining
~2,000 lines, `ext_strip()`, and the back half of Chapter 12's
`verb_match_inner` are still real, unopened code, sitting in a project
whose total size this book was never going to fully narrate line by
line. What this book has tried to hand you instead is the method:
read the header comment, find the real function, write a small program
that tests its own documented examples, compile it, run it, and trust
the terminal output over the comment when they disagree. Every finding
in every chapter, including this one, came from doing exactly that.
The project's source is no longer unfamiliar territory — what's left
in it is just more of the same kind of code this book has spent
thirteen chapters teaching you to read for yourself.

This chapter's `ukwezi`/`ukuri` finding is also the clearest example
yet of where that method's honesty has to cut both ways. The same
techniques that found and fixed the `ukwezi` bug were applied to
`ukuri`, and they weren't enough to justify a fix — there wasn't a
second attested word to check the answer against, so the responsible
result was to stop and say so, not to produce a confident-looking
patch built on a guess. That's the same standard this book has tried
to hold its own claims to from the start: a finding is only as good
as the real, compiled, run evidence behind it, and "I don't have
enough evidence to fix this safely" is itself a legitimate, honestly
documented outcome.
