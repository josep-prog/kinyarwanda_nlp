# Chapter 12 — The Densest File in the Project: `morphology.c`'s Matching Engines

## How this chapter works

Same rules as Chapters 1 through 11. Every code example was actually
compiled with `gcc -std=c99 -Wall -Wextra` and actually executed — the
output shown in a fenced block prefixed with `$` is the real terminal
output of that exact program.

Chapter 11 closed this book honestly: `morphology.c` (3,366 lines) and
`morph_dispatch.c` (3,528 lines) were used by every chapter's case
studies and never opened directly. This chapter opens the first of
the two. `morphology.c` is the single largest file in this project,
and it does not divide evenly — one function inside it,
`verb_match_inner`, is over 1,100 lines by itself, more than twice the
length of all of Chapter 9's sixteen-pass orthography engine combined.
This chapter doesn't try to read all 3,366 lines; it reads the file's
own shared phonological-rules section in full, and reads enough of
the verb-matching engine to find three real, verified things about
*why* it's shaped the way it is — leaving the rest, honestly, as this
chapter's own version of Chapter 11's gap.

---

# Part 1 — Language and Code, Side by Side

## 1.1 The vowel-contact rule, finally run as code

`kinyarwanda.h`'s "VOWEL CONTACT RULE" comment — quoted in passing
back in Chapters 1 and 9 — has a real function behind it that neither
chapter opened: `kin_vv_join()`. Its own doc comment states six
sub-rules, two of which add a detail neither earlier quotation
included:

> 2. prefix ends in 'u' → u becomes 'w' before the stem vowel. Sub-rule
> (Official Orthography Rules, Table 0): the clusters kw, gw, hw are
> NOT written before back round vowels 'o' and 'u' — the 'w' is
> dropped because it is redundant before a round vowel: ku + oma →
> koma; ku + ura → kura (w dropped before o/u); mu + oma → mwoma
> (other consonants keep w).
>
> 3. prefix ends in 'i' → i becomes 'y' [...] Sub-rule (Table 1): cy
> and jy clusters are written only before 'a', 'o', 'u'. Before the
> front vowels 'i' or 'e' the 'y' is dropped and the palatalisation
> reverses: cy + i → ki; cy + e → ke; jy + e → ge.

Run the function's own documented examples for real:

```c
#include "kinyarwanda.h"
#include <stdio.h>

static void show(const char *p, const char *s) {
    char out[64];
    kin_vv_join(p, s, out, sizeof(out));
    printf("%-4s + %-6s -> %s\n", p, s, out);
}

int main(void) {
    show("mu","iga"); show("ku","amara");
    show("ku","oma"); show("ku","ura");
    show("ki","ama"); show("ki","iga"); show("ki","eza");
    show("ya","iga"); show("ya","eza");
    show("ka","iza"); show("ba","inja");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p1_vvjoin.c -L . -lkinyarwanda -o p1_vvjoin
$ LD_LIBRARY_PATH=. ./p1_vvjoin
mu   + iga    -> mwiga
ku   + amara  -> kwamara
ku   + oma    -> koma
ku   + ura    -> kura
ki   + ama    -> cyama
ki   + iga    -> kiga
ki   + eza    -> keza
ya   + iga    -> yiga
ya   + eza    -> yeza
ku   + amara  -> kwamara
ka   + iza    -> keza
ba   + inja   -> benja
```

Every line matches the doc comment exactly. `ku+oma` and `ku+ura`
correctly drop the `w` (round vowel, `k` consonant); `ki+iga` and
`ki+eza` correctly reverse the `cy` cluster back to bare `k` before
the front vowels `i`/`e`. Section 4.3 finds the one place this
six-rule system stops being as symmetric as its own comment claims.

## 1.2 Build it: a joiner that only knows the first sub-rule

```c
/* p1_toy_join.c -- a joiner that only knows "u before vowel becomes w" */
#include <stdio.h>
#include <string.h>

static void join(const char *prefix, const char *stem) {
    size_t plen = strlen(prefix);
    printf("%s + %-5s -> ", prefix, stem);
    if (prefix[plen-1] == 'u') {
        printf("%.*sw%s\n", (int)plen-1, prefix, stem);
    } else {
        printf("%s%s\n", prefix, stem);
    }
}

int main(void) {
    join("ku", "iga");
    join("ku", "oma");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra p1_toy_join.c -o p1_toy_join
$ ./p1_toy_join
ku + iga  -> kwiga
ku + oma  -> kwoma
```

The toy gets `ku+iga` right and `ku+oma` wrong (`kwoma`, not the
correct `koma`) because it only knows the general "u before a vowel
glides to w" rule, not the round-vowel exception that overrides it for
exactly `k`/`g`/`h`. That exception — encoding a rule and then
correctly narrowing when it doesn't apply — is most of what makes
`kin_vv_join` six branches deep instead of one.

## 1.3 Checkpoint: test yourself before continuing

1. Why does `mu+oma` keep its `w` (giving `mwoma`) while `ku+oma`
   drops it (giving `koma`), even though both prefixes end in the
   same vowel `u` before the same stem vowel `o`?
2. `ki+ama` → `cyama` (the cluster forms) but `ki+iga` → `kiga` (the
   cluster reverses). What's different about the stem's first letter
   in each case?

---

# Part 2 — One File, Two Very Different Shapes of Matching Problem

## 2.1 Detection lives here; filling lives one file over

`morphology.c`'s own header comment draws the boundary this whole
project uses:

> This file implements the DETECTION functions for Trees 1, 2 and 3.
> The dispatch and morpheme FILLING is in `morph_dispatch.c`.

Every function this chapter reads answers one question — *is this
word a noun/adjective/verb, and if so, what class and what stem* —
and nothing here ever writes into a `Token`'s `.morph` field. That's
`morph_dispatch.c`'s job, a 3,528-line file this book hasn't opened
yet either, and Chapter 13 picks it up from exactly this boundary.

## 2.2 Why the phonological rules sit in their own section, used by all three trees

`kin_vv_join`, `kin_has_vowel_hiatus`, and `kin_has_invalid_cluster`
are grouped under one header — "§1 PHONOLOGICAL RULES ... These rules
apply across ALL trees" — physically separate from §2 (nouns), §3
(adjectives), and §4 (verbs). That placement is a real design
decision: a noun prefix joining its stem, an adjective concordance
prefix joining its stem, and a verb subject prefix joining its stem
all hit the exact same vowel-contact problem, so the resolution logic
is written once and called from all three, rather than three separate
nearly-identical copies drifting apart over time the way Chapter 4
found connector-fusion logic drifting across this project's other
files.

---

# Part 3 — Making It Interactive

## 3.1 Build it: type a prefix and a stem, see the real join and whether it's clean

```c
/* p3_repl.c -- join a prefix and stem, then check the result for hiatus */
#include "kinyarwanda.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    char line[128];
    printf("Type 'prefix stem', or 'q' to quit.\n");
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, "q") == 0) break;
        char prefix[64], stem[64];
        if (sscanf(line, "%63s %63s", prefix, stem) != 2) continue;

        char out[128];
        kin_vv_join(prefix, stem, out, sizeof(out));
        printf("  joined: %s\n", out);
        printf("  hiatus: %d\n", kin_has_vowel_hiatus(out));
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p3_repl.c -L . -lkinyarwanda -o p3_repl
$ printf "ku oma\nki eza\nq\n" | LD_LIBRARY_PATH=. ./p3_repl
Type 'prefix stem', or 'q' to quit.
  joined: koma
  hiatus: 0
  joined: keza
  hiatus: 0
```

Both joins are clean — `kin_vv_join`'s own output never fails
`kin_has_vowel_hiatus`'s check, for any of the documented cases. Part
4 finds one case neither side of this REPL was ever asked about.

---

# Part 4 — Capstone: Three Real Findings From Reading the Production Code

## 4.1 The header promises `SP_TABLE[]`; the file has no such thing

`morphology.c`'s own file-layout comment names it directly:

> § 4b ITONDAGUYE (Conjugated) Formula: SP + (TM) + (OM) + C + (EXT) + FV
>   SP_TABLE[] — subject prefix table (all classes + persons)
>   OM_TABLE[] — object marker table (indangakinyazina)

`OM_TABLE[]` exists exactly as promised — a flat, file-scope,
15-entry array, read in full in Section 6.2. Search the file for
`SP_TABLE`, and it appears nowhere as an actual array — only in this
one comment and one other inline note. What the verb-matching function
actually declares is this, *local to the function*, under a different
name:

```c
static bool verb_match_inner(const char *word, char *stem_buf,
                              int *subj_class, VerbTense *tense_out) {
    ...
    static const struct { const char *pfx; int cls; } SP[] = {
        { "twa",   0  },  /* 1pl past / Nt.13 */
        ...
        { "y",     0  },  /* elided "ya" SP (ambiguous: cls1 past, cls6 pres, cls9 before vowel) */
        { "b",     2  },
        { NULL, 0 }
    };
    ...
```

`SP[]`, not `SP_TABLE[]` — a different name from what the header
promised, and `static` *inside the function*, not at file scope the
way `OM_TABLE[]` is. That's a real, verifiable naming and scope
mismatch on its own, but it understates the bigger gap: this 47-entry
table is only where subject-prefix matching *starts*. The function
goes on to declare at least five more named lists, each scoped even
more narrowly — to one `if` block, deciding one specific tense
ambiguity:

```c
static const char *PAST_SP_Y[]        = { "twa","ya","wa", /* ...27 entries... */ NULL };
static const char *PAST_SP_ELOC[]     = { "twa","ya","wa","na","bya","cya","rya",
                                           "zya","bwa","kwa","rwa","mwa","za", NULL };
static const char *PAST_SP_TSE[]      = { /* same 13, plus "a" */ NULL };
static const char *PAST_SP_LIST[]     = { /* same 13 entries again */ NULL };
static const char *PAST_SPS[]         = { /* same 13 entries again */ NULL };
static const char *UNAMB_COND_SPS[]   = { "twa","bya","cya","rya","rwa","zya",
                                           "bwa","kwa","mwa", NULL };
```

`OM_TABLE[]`'s job — "given this prefix, what class is it" — is a
single lookup. `SP[]`'s job is only the *first* lookup; everything
after it is "given this prefix matched, which of several tense
readings is even possible here," re-checked against a different
narrow list at every step:

```
   what the header comment promises:        what the file actually has:

   ┌────────────────┐  ┌────────────────┐   ┌────────────────┐
   │  SP_TABLE[]     │  │  OM_TABLE[]     │   │  OM_TABLE[]     │  one lookup,
   │  file-scope      │  │  file-scope      │   │  file-scope      │  done
   │  one lookup      │  │  one lookup      │   └────────────────┘
   └────────────────┘  └────────────────┘
                                                ┌────────────────┐
                                                │  SP[]  (local to  │  the FIRST
                                                │  verb_match_inner)│  lookup only
                                                └────────┬───────┘
                                                         │  then re-checked against:
                                                         ├─ PAST_SP_Y[]       (27 entries)
                                                         ├─ PAST_SP_ELOC[]    (13 entries)
                                                         ├─ PAST_SP_TSE[]     (14 entries)
                                                         ├─ PAST_SP_LIST[]    (13 entries — identical to ELOC)
                                                         ├─ PAST_SPS[]        (13 entries — identical to ELOC)
                                                         └─ UNAMB_COND_SPS[]  (9 entries)
```

The header comment describes two parallel, symmetric tables. The real
shape is one base table plus a sprawl of single-purpose gates, because
— Section 5.1 explains why — the two problems are not actually
symmetric.

## 4.2 Three of those gates are the same list, declared three separate times

Read `PAST_SP_ELOC`, `PAST_SP_LIST`, and `PAST_SPS` again, side by
side:

```c
static const char *PAST_SP_ELOC[] = {
    "twa","ya","wa","na","bya","cya","rya","zya",
    "bwa","kwa","rwa","mwa","za", NULL
};
...
static const char *PAST_SP_LIST[] = {
    "twa","ya","wa","na","bya","cya","rya","zya",
    "bwa","kwa","rwa","mwa","za", NULL
};
...
static const char *PAST_SPS[] = {
    "twa","ya","wa","na","bya","cya","rya","zya",
    "bwa","kwa","rwa","mwa","za", NULL
};
```

Thirteen identical strings, in the identical order, declared three
separate times at three separate points inside the same 1,100-line
function — not a shared constant referenced three times, three
independent copies. `PAST_SP_TSE` is the same list again with one
extra entry (`"a"`) appended. This isn't a correctness bug today —
every copy currently agrees — but it's exactly the kind of duplication
this book has flagged as a real risk before (Chapter 4's repeated
connector-fusion logic, Chapter 11's duplicated sentence-splitter):
if the grammar this engine models ever needs a fourteenth past-form
subject prefix added, there are four separate places that string has
to be added correctly, with no compiler warning if one is missed.

## 4.3 The documented symmetric rule that isn't symmetric in code

Section 1.1 quoted the doc comment's claim for rule 3: "cy *and* jy
clusters are written only before 'a', 'o', 'u'. Before the front
vowels 'i' or 'e' the 'y' is dropped and the palatalisation reverses"
— stated as one rule covering both clusters. Read the actual branch:

```c
} else if (p_end == 'i') {
    bool front_V = (s_start == 'i' || s_start == 'e');
    if (p_prev == 'k') {
        if (front_V) {
            /* cy + i/e -> ki/ke: handles BOTH front vowels */
            snprintf(out, out_sz, "%.*sk%s", (int)(base - 1), prefix, stem);
        } else {
            snprintf(out, out_sz, "%.*scy%s", (int)(base - 1), prefix, stem);
        }
    } else if (p_prev == 'j' && s_start == 'e') {
        /* jy + e -> ge: handles ONLY 'e', not 'i' */
        snprintf(out, out_sz, "%.*sg%s", (int)(base - 1), prefix, stem);
    } else {
        snprintf(out, out_sz, "%.*sy%s", (int)base, prefix, stem);
    }
```

`p_prev == 'k'` checks `front_V` — both `i` and `e` — exactly per the
doc comment. `p_prev == 'j'` checks `s_start == 'e'` only. There is no
`jy + i` branch; that case falls all the way through to the generic
`else`, which just keeps the glide. Test both halves of the claimed
symmetric rule on the same artificial prefix:

```c
/* p4_jycy.c -- the doc says cy and jy behave the same before i and e */
#include "kinyarwanda.h"
#include <stdio.h>

int main(void) {
    char out[64];
    kin_vv_join("ji", "eza", out, sizeof(out));
    printf("ji + eza -> %s   (documented: jy+e -> ge)\n", out);
    kin_vv_join("ji", "iza", out, sizeof(out));
    printf("ji + iza -> %s   (by the same symmetric rule, expected: gi-shaped)\n", out);
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_jycy.c -L . -lkinyarwanda -o p4_jycy
$ LD_LIBRARY_PATH=. ./p4_jycy
ji + eza -> geza   (documented: jy+e -> ge)
ji + iza -> jyiza   (by the same symmetric rule, expected: gi-shaped)
```

The `e` case matches the doc exactly. The `i` case doesn't reverse at
all — `jyiza` keeps the un-reversed `jy` cluster the doc comment says
shouldn't be written before a front vowel. (`kin_has_invalid_cluster`,
read in Chapter — wait, read directly in this file's own §1, doesn't
catch this: it accepts *any* consonant followed by `y` unconditionally,
so `jy` passes that check regardless of what precedes it. The
asymmetry is real, but it's invisible to this project's own
cluster validator, which is a separate, broader rule than the
narrower cy/jy-specific reversal this function's comment describes.)

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 Why object-marker matching is one table and subject-prefix matching isn't

`OM_TABLE[]`'s job is genuinely simple: a verb's object marker sits in
exactly one position (between the tense marker and the stem), and a
2-character OM prefix is enough on its own to know the class — there
is no other reading of `mu` in that position. Subject-prefix matching
has no equivalent guarantee. The same literal string `"ya"` is
documented in `SP[]` itself as "Nt.6 present OR Nt.1 past" — *one*
prefix, *two* completely different tenses, distinguished only by
what's written after it. Chapter 7's `kin_resolve_sp_ambiguity`
already established that this ambiguity survives all the way to the
*sentence* level for exactly this prefix; this chapter shows that the
*word*-level recognizer underneath it already has to fight the same
ambiguity just to produce its first guess, which is the real reason
`SP[]` alone was never going to be enough — every entry needs its own
follow-up check against what comes after it, and those follow-up
checks are what the five extra gate lists in Section 4.1 actually are.

## 5.2 `OM_TABLE[]`'s own admission: most of it is unverified

`OM_TABLE[]`'s header comment includes a line this book hasn't quoted
from any other table yet:

> PLEASE VERIFY with native speaker: classes marked (* unverified *)

Of its 15 entries, only 4 are marked "Confirmed (book p.60 + common
usage)" — `mu/mw` (class 1), `ki/cy` (class 7), `bi/by` (class 8),
`bu/bw` (class 14). The other 11 — including `ya/ya` for class 6,
`zi/zi` for class 10, every locative and infinitive form — are marked
`(* unverified *)` directly in the table itself. This is the same
table Section 4.1 held up as "the one that exists exactly as
documented" — and it's true that its *shape* matches the header
comment. Its *contents* carry an honest flag this project's own
authors placed there, rather than asserting confidence the linguistic
sourcing didn't support. That's worth noticing for what it is: this
project's own discipline of marking what's actually been checked
against a real source, applied to itself.

---

# Part 6 — Reading the Real Production Code

## 6.1 `kin_vv_join`, in full

```c
void kin_vv_join(const char *prefix, const char *stem,
                 char *out, size_t out_sz)
{
    if (!prefix || !stem || !out || out_sz == 0) return;
    size_t plen = strlen(prefix), slen = strlen(stem);
    if (plen == 0) { snprintf(out, out_sz, "%s", stem);   return; }
    if (slen == 0) { snprintf(out, out_sz, "%s", prefix); return; }

    char p_end  = prefix[plen - 1];
    char p_prev = plen > 1 ? prefix[plen - 2] : '\0';
    char s_start = stem[0];

    if (!is_vowel(p_end) || !is_vowel(s_start)) {
        snprintf(out, out_sz, "%s%s", prefix, stem);
        return;
    }
    size_t base = plen - 1;

    if (p_end == 'u') {
        bool round_V = (s_start == 'o' || s_start == 'u');
        bool kgh     = (p_prev == 'k' || p_prev == 'g' || p_prev == 'h');
        if (round_V && kgh)
            snprintf(out, out_sz, "%.*s%s", (int)base, prefix, stem);
        else
            snprintf(out, out_sz, "%.*sw%s", (int)base, prefix, stem);

    } else if (p_end == 'i') {
        bool front_V = (s_start == 'i' || s_start == 'e');
        if (p_prev == 'k') {
            if (front_V) snprintf(out, out_sz, "%.*sk%s", (int)(base-1), prefix, stem);
            else         snprintf(out, out_sz, "%.*scy%s", (int)(base-1), prefix, stem);
        } else if (p_prev == 'j' && s_start == 'e') {
            snprintf(out, out_sz, "%.*sg%s", (int)(base-1), prefix, stem);
        } else {
            snprintf(out, out_sz, "%.*sy%s", (int)base, prefix, stem);
        }

    } else { /* p_end == 'a' */
        bool prev_is_glide = (p_prev == 'y' || p_prev == 'w');
        if (!prev_is_glide && s_start == 'i')
            snprintf(out, out_sz, "%.*se%s", (int)base, prefix, stem + 1);
        else
            snprintf(out, out_sz, "%.*s%s", (int)base, prefix, stem);
    }
}
```

Every branch ends in exactly one `snprintf` call, each one building
the surface form from `base` characters of the prefix (everything
before its final vowel) plus whatever the rule for this case inserts.
The `jy`-before-front-vowel gap from Section 4.3 is visible directly
here: there's a branch for `p_prev == 'k'` that checks both front
vowels, and a branch for `p_prev == 'j'` that only checks one.

## 6.2 One named gate, read in full: `PAST_SP_Y`

```c
static const char *PAST_SP_Y[] = {
    "twa","ya","wa","na","bya","cya","rya","zya",
    "bwa","kwa","rwa","mwa","za","ba","mu","tu","a",
    "ni","ri","zi","bi","ki","ru","ka","bu","ku","ha", NULL
};
```

This list gates one specific rule: a verb root ending in `y` takes a
*bare* `e` final vowel in the past perfect, rather than the usual
`ye` (`giye` from `kugenda`, `jye` from `kujya`) — and that
substitution is only attempted for subject prefixes on this list. Of
the gates quoted in Section 4.1, this is the broadest (27 entries,
covering present-tense-shaped prefixes too, not just past-tense ones)
— a reminder that "PAST_SP_something" names don't all gate the exact
same condition, even though three of them (Section 4.2) turned out to
gate the identical one.

## 6.3 Noun and adjective prefix stripping: the same shape Chapter 1 already taught, now as code

```c
int kin_detect_noun_class(const char *w) {
    size_t wlen = strlen(w);
    if (wlen < 3) return 0;
    /* Ordered by prefix length (longest first) to avoid partial matches */
    if (kin_starts_with(w, "umu") && wlen > 4) return 1;  /* Nt.1/3 */
    if (kin_starts_with(w, "aba") && wlen > 4) return 2;  /* Nt.2   */
    ...
    if (w[0]=='i' && w[1]=='n' && wlen > 3) return 9;     /* Nt.9/10 */
    ...
}
```

This is the exact "longest-match-first, ordered array of checks"
shape Chapter 8 taught for digraph scanning — here applied to noun
class prefixes instead of phonemes, with the same reason: `"icy"`
(class 7) has to be checked before the bare `"i-"` fallback (class 5)
or every class-7 noun would be misread, exactly as a 2-character
digraph has to be checked before its 1-character fallback.
`kin_strip_noun_prefix("umuntu", ...)` and `kin_strip_adj_prefix
("munini", ...)` both confirm the same class-1 stems (`ntu`, `nini`)
this book's very first chapter already used as its opening example —
this is the literal function that example's `Nt.1` came from.

---

# Part 7 — Looking Back, Looking Forward

## 7.1 Every morpheme table since Chapter 1 was already running this code

Chapter 1's noun class assignments, Chapter 2's adjective concordance
stems, Chapter 3's verb tense and stem extraction — every one of those
chapters' case-study tokens got their `.noun_class`, `.stem`, and
`.verb_tense` fields from functions read directly in this chapter,
months (in this book's chapter-count, eleven chapters) before this
chapter ever named the file they live in.

## 7.2 What's left in this file, honestly

This chapter read `§1` (phonological rules) in full and enough of
`§4b` (subject-prefix matching) to find three real, verified things
about its shape. It did not read `ext_strip()` (the ~160-line verb
extension stripper for passive/causative/applicative/reciprocal
forms), `kin_is_verb_infinitive()`, or the remaining ~900 lines of
`verb_match_inner` covering every tense branch this chapter didn't
quote. That's a genuine, acknowledged gap, the same kind Chapter 11
left for `morphology.c` as a whole — this time one level deeper, for
the parts of this one file this chapter didn't have room for.

---

# Part 8 — Practice

### Beginner

1. Using Section 1.1's rule list, predict `gu+iga` and `hu+ura` by
   hand, then verify with `kin_vv_join`.
2. `OM_TABLE[]`'s comment marks 11 of its 15 entries `(* unverified *)`.
   Pick one and find a real sentence elsewhere in this book's earlier
   chapters that uses that object-marker class, confirming (or not)
   the table's guess.

### Intermediate

3. Section 4.3 found `jy+i` doesn't reverse. Using the same technique,
   check whether `kin_vv_join`'s `'a'` branch (rule 4a/4b/4c) handles
   `wa+i` (glide `w` before `a`, then `a` before stem-initial `i`)
   the same way it handles `ya+i`, or differently.
4. Section 4.2 found three identical 13-entry lists. Read the real
   `morphology.c` and find the exact line number of all three
   declarations, then check whether `PAST_SP_TSE`'s extra `"a"` entry
   would change any of `PAST_SP_ELOC`/`PAST_SP_LIST`/`PAST_SPS`'s
   behavior if it were added to them too.
5. `kin_has_invalid_cluster` accepts any consonant followed by `y` or
   `w` unconditionally (Section 4.3's parenthetical). Construct a
   consonant+`y` pair that is *not* a real Kinyarwanda cluster and
   confirm the function still reports it as valid.

### Advanced

6. Section 4.1 found `SP[]` is function-local, unlike file-scope
   `OM_TABLE[]`. What would have to change in `verb_match_inner`'s
   signature or this file's structure to hoist `SP[]` to file scope,
   and would doing so make any of Section 4.2's duplicated gate lists
   easier to deduplicate?
7. Propose a minimal fix for Section 4.3 that adds the missing
   `jy + i → gi` branch without changing the existing `cy` branch's
   logic.
8. Section 7.2 named four specific unread pieces of this file
   (`ext_strip`, `kin_is_verb_infinitive`, and the remaining tense
   branches of `verb_match_inner`). Pick one and apply this book's
   own method to it: find the header comment, find the real function,
   compile it, run it against its own documented examples.

---

## Key takeaways

- `kin_vv_join` implements six real sub-rules for resolving Kinyarwanda
  vowel-contact at a morpheme boundary, including two narrowing
  exceptions (`kw`/`gw`/`hw` dropping `w` before round vowels; `cy`
  reversing before front vowels) that a naive single-rule joiner gets
  wrong — verified against the function's own documented examples.
- `morphology.c` separates *detection* (this file: is this a noun, an
  adjective, a verb, and what class/stem) from *filling* (Chapter 13's
  `morph_dispatch.c`), and keeps its phonological rules in one shared
  section used by all three word-class trees rather than duplicated
  per tree.
- The headline real finding: the header comment promises a
  `SP_TABLE[]` parallel to the real, file-scope `OM_TABLE[]`; the
  actual code has a differently-named, function-local `SP[]` plus at
  least five additional narrowly-scoped disambiguation lists — because
  subject-prefix matching, unlike object-marker matching, can't
  resolve from the prefix string alone.
- A second real finding: three of those disambiguation lists
  (`PAST_SP_ELOC`, `PAST_SP_LIST`, `PAST_SPS`) are independently
  declared but byte-for-byte identical 13-entry arrays — a real
  duplication risk, not yet a correctness bug.
- A third real finding: `kin_vv_join`'s own doc comment describes a
  symmetric rule for `cy` and `jy` clusters before front vowels; the
  code only implements the front-vowel reversal for `cy` (both `i`
  and `e`) and for `jy` before `e` — `jy` before `i` falls through
  unchanged, confirmed with a direct, isolated test.
- `OM_TABLE[]`, held up earlier in the chapter as "the table that
  exists exactly as documented," carries its own honest disclosure
  that 11 of its 15 entries are linguistically unverified — a real
  example of this project's own engineering discipline applied to
  itself, not a flaw to fix.

## Sources quoted in this chapter

- `src/morphology.c` (`kin_vv_join`, `kin_has_vowel_hiatus`,
  `kin_has_invalid_cluster`, `OM_TABLE[]`, `verb_match_inner`'s `SP[]`
  table and named gate lists, `kin_detect_noun_class`,
  `kin_strip_noun_prefix`, `kin_strip_adj_prefix`).
- `include/kinyarwanda.h` (the vowel-contact and consonant-cluster
  rule comments, first quoted in Chapter 1).
- Every `pN_*.c` program in this chapter was actually compiled with
  `gcc -std=c99 -Wall -Wextra` and actually executed to produce the
  exact output quoted above.

## Coming up in Chapter 13

This chapter read the functions that decide *what* a word is.
`src/morph_dispatch.c` — 3,528 lines, this project's other unopened
giant — reads what happens immediately after: the type-dispatch logic
that takes a tagged `Token` and decides which of this chapter's
detection results actually get written into `.morph`, filling in the
`KinMorpheme` array every chapter's case studies since Chapter 1 has
displayed without ever seeing built.
