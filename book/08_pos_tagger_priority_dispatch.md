# Chapter 8 — `pos_tagger.c`: Priority Dispatch Without OOP Polymorphism

## The question this file answers

Every previous chapter built a *detector*: "is this string a noun
prefix" (Ch.6), "is this string a known adjective stem" (Ch.7), "is this
string an invariable word" (Ch.7). None of them, alone, can answer the
real question: given one specific word in one specific sentence, which
single answer is *correct*? A word's letters can simultaneously satisfy
more than one detector. `pos_tagger.c` is the file that forces a single,
final decision — and it does it with a technique you should be able to
name precisely: an **ordered priority chain**, the same idea Chapter 6
used for matching prefixes, now scaled up to decide a word's entire
grammatical identity.

## 8.1 Why a chain of checks, instead of "ask the word what it is"

In an object-oriented language, you might imagine giving every word
object a `classify()` method and asking it to identify itself
polymorphically. C has no objects, no virtual methods, no inheritance —
there is no mechanism for "ask the data what kind of thing it is." The
only tool C gives you is a function that inspects a string and returns an
answer. So the design problem becomes: **in what order do you ask your
detector functions, when more than one might say yes?**

The file's own header comment states the chosen order explicitly — this
*is* the algorithm:

```
Step 1. Invariable words    → Tree 5 (amagambo adahinduka)
Step 2. Pronouns            → Tree 4 (ikinyazina)
Step 3. Known full word     → Tree 1 override (lexicon exact-match)
Step 4. Verb infinitive     → Tree 3a (imbundo)
Step 5. Proper noun         → Tree 1 heuristic (capitalised mid-sentence)
Step 6. Noun (D+RT prefix)  → Tree 1, with verb/adj guard before committing
Step 7. Adjective (RS+C)    → Tree 2 (ntera)
Step 8. Conjugated verb     → Tree 3b (itondaguye)
Step 9. Foreign/Unknown
```

`kin_tag_token()` implements this as nine sequential `if` blocks, each
one ending in `return` the moment it succeeds:

```c
void kin_tag_token(Token *tok) {
    if (tok->pos == POS_PUNCTUATION) return;
    /* ... digit check ... */

    POS inv_pos;
    if (kin_is_invariable(w, &inv_pos)) {       /* Step 1 */
        tok->pos = inv_pos;
        /* ... */
        return;
    }

    PronounType ptype; int pcls;
    if (kin_is_pronoun(w, &ptype, &pcls)) {     /* Step 2 */
        tok->pos = POS_PRONOUN;
        /* ... */
        return;
    }
    /* ... Steps 3 through 9, same shape ... */
}
```

This is precisely Chapter 6's longest-match discipline, generalized: each
step is checked in a fixed, deliberate order, and **whichever check
succeeds first wins, permanently** — once a `return` fires, none of the
later, lower-priority checks ever run for that word. The comment for
Step 3 makes the reasoning for one particular ordering choice explicit:
"checked before verb heuristics so that specific nouns like `mvura`
(rain) are not misanalysed as conjugated verbs." That sentence is the
entire justification for *why* the exact-match lexicon override sits at
position 3 and not, say, position 8 — get the order wrong, and real words
get silently misclassified.

## 8.2 Guards before the chain even starts

```c
void kin_tag_token(Token *tok) {
    /* Punctuation tokens are already fully tagged by the tokenizer — preserve. */
    if (tok->pos == POS_PUNCTUATION) return;

    const char *w = tok->lower;

    /* Arabic numerals (1, 2, 42, …): all-digit surface → Umubare. */
    {
        const char *d = tok->surface;
        bool all_digits = (*d != '\0');
        while (*d) { if (*d < '0' || *d > '9') { all_digits = false; break; } d++; }
        if (all_digits) {
            tok->pos = POS_NUMBER;
            tok->is_kinyarwanda = true;
            return;
        }
    }
    /* Step 1 begins here ... */
```

Two things before any of the nine numbered steps run. First, an
**idempotency guard**: if Chapter 4's tokenizer already fully tagged this
token as punctuation, `kin_tag_token` does nothing and returns
immediately — there's no reason to run nine detector checks against a
comma. Second, a **self-contained digit scanner**: this is the
pointer-cursor style from Chapter 4 again (`*d`, `d++`), wrapped in its
own `{ }` block — using braces to create a scope for `d` and
`all_digits` that ends right after this check, even though they're not
inside a function of their own. This is a small but real C habit: when a
handful of local variables are only relevant to one self-contained check,
wrapping them in `{ }` keeps them from cluttering the rest of the
function's namespace, without needing to extract a separate named
function for something this short.

## 8.3 A `static` local table — the *other* meaning of `static`

Inside Step 1, handling the `-fite` stative form, you'll find this:

```c
static const struct { const char *sp; int cls; } FITE_SP[] = {
    { "bi",  8  }, { "aba", 2  }, { "ba",  2  }, { "gi",  7  },
    { "zi", 10  }, { "ru", 11  }, { "ga", 12  }, { "du",  1  },
    { "mu",  2  }, { "bu", 14  }, { "u",   1  }, { "a",   1  },
    { "n",   1  }, { "i",   9  }, { NULL,  0  }
};
```

Two things worth pulling apart here, because both are genuinely new.

**An anonymous struct.** `struct { const char *sp; int cls; }` has no
name after the word `struct` — unlike every struct you've seen so far
(`Token`, `NounClass`, `OmEntry`), this type is never given a `typedef`
and is never referred to anywhere else by name. That's fine in C as long
as you only ever need *one* variable (or, as here, one array) of that
exact shape — there's no reason to invent and export a named type for a
14-row table used in exactly one place.

**`static` on a *local* variable.** Chapter 4 introduced `static` at file
scope, where it controls *linkage* (whether other files can see the
symbol). Here, `static` is applied to a variable declared *inside a
function body* — and at that scope, linkage isn't the issue (a local
variable was never visible outside the function anyway). What `static`
changes here is **storage duration**: a normal (non-`static`) local
variable is conceptually recreated every single time the function is
called, and its lifetime ends when the function returns. A `static`
local variable, by contrast, is allocated **once**, the very first time
the program reaches its declaration, and survives for the entire run of
the program — and crucially, for a `static const` array like this one,
its 14 rows are initialized once and never need to be rebuilt or
re-copied on every single call to `kin_tag_token()`, even though this
function may run hundreds of times analyzing one sentence. This is the
same keyword you met in Chapter 4, doing a *related but distinct* job
depending on *where* it's written — being able to explain "`static`
means something different at file scope versus inside a function body,
and here's exactly how" is a sharp, specific thing to have ready for your
defense.

## 8.4 `check_deverbative`: the same priority-chain technique, run in reverse

Chapters 5–7 were all about *forward* detection: given a prefix, find the
class. `check_deverbative()` solves the opposite problem: given a noun
that's already been identified, was it actually *derived* from a verb? (A
"deverbative noun" — `umucyo`, "light," derived from the verb root
`-cy-`, "to shine.") The technique is the identical ordered-attempts
pattern, just running in the other direction:

```c
static void check_deverbative(Token *tok) {
    if (tok->pos != POS_NOUN) return;
    size_t slen = tok->stem[0] ? strlen(tok->stem) : 0;
    if (slen < 3) return;
    char last = tok->stem[slen - 1];
    bool ends_vowel = (last=='a'||last=='e'||last=='i'||last=='o'||last=='u');
    if (!ends_vowel) return;

    for (int pi = 0; PRIMARY_NOUN_STEMS[pi]; pi++)
        if (strcmp(tok->stem, PRIMARY_NOUN_STEMS[pi]) == 0) return;

    char root[KIN_MAX_STEM];
    strncpy(root, tok->stem, slen - 1);
    root[slen - 1] = '\0';
    /* ... try several candidate roots in order, return on first match ... */
}
```

Notice `strncpy(root, tok->stem, slen - 1); root[slen - 1] = '\0';` —
this copies *all but the last character* of `tok->stem` into `root`,
then places the terminator right after. Passing `slen - 1` as the length
limit means the final character (the vowel just confirmed by
`ends_vowel`) is simply never copied — a clean way to "strip the last
character" without any string-removal function (C has none) by
controlling exactly how many bytes get copied in the first place.

`PRIMARY_NOUN_STEMS` is a fifth instance of the `NULL`-terminated-array
idiom you've now seen across four chapters (Ch.4's `FITE_FORMS`, Ch.6's
`OM_TABLE`, Ch.7's `ADJ_STEMS`/`VERB_STEMS`) — here used as an **exception
list**: words like `gore` ("woman") happen to end in a vowel and have a
stem that, if you blindly stripped the vowel, *coincidentally* resembles
part of an unrelated verb. Before attempting any deverbative analysis at
all, the function checks this denylist and bails out — a guard clause
protecting against false positives the main algorithm would otherwise
produce.

Further down, when a direct match fails, the function tries *phonological
reversals* before giving up — for instance, undoing the `r→z` sound
change to recover a root the lexicon actually has on file:

```c
if (last == 'e' && rlen >= 2 && root[rlen - 1] == 'z') {
    char root_re[KIN_MAX_STEM];
    memcpy(root_re, root, rlen + 1);
    root_re[rlen - 1] = 'r';
    if (kin_is_known_verb_stem(root_re)) {
        tok->is_deverbative = true;
        /* ... */
    }
}
```

`memcpy(root_re, root, rlen + 1)` duplicates `root` (including its
`'\0'`, hence `rlen + 1`) into a second buffer, and then a single
direct-index write, `root_re[rlen - 1] = 'r';`, overwrites just the last
character of the *copy* — leaving the original `root` untouched in case
this guess turns out wrong and a different reversal needs to be tried
next. Copy first, mutate the copy, test it, only commit if it succeeds —
this pattern (copy-then-edit-then-validate) is worth recognizing
generally: it lets a function try a risky, possibly-wrong transformation
without ever damaging data it might still need if that attempt fails.

## 8.5 `kin_tag_sentence`: pointers as aliases into the array, not copies

`kin_tag_token()` only ever looks at one word in isolation. Some
decisions, though, genuinely need to know about *neighboring* words —
Chapter 3's `SentenceAnalysis.tokens[]` array is what makes that possible,
and `kin_tag_sentence()` is where neighbor-aware corrections happen, in
clearly separated passes run *after* every token has already received its
first, context-free tag:

```c
void kin_tag_sentence(SentenceAnalysis *sa) {
    sa->has_verb = false;
    for (int i = 0; i < sa->token_count; i++) {
        kin_tag_token(&sa->tokens[i]);
        if (sa->tokens[i].pos == POS_VERB_INF || sa->tokens[i].pos == POS_VERB_CONJ)
            sa->has_verb = true;
    }

    /* ── Context Pass A: Indomo elision recovery (Tree 1) ───────────────── */
    for (int i = 1; i < sa->token_count; i++) {
        Token *prev = &sa->tokens[i-1];
        Token *curr = &sa->tokens[i];
        if (curr->pos == POS_NOUN) continue;
        if (prev->pos != POS_PRONOUN ||
            (prev->pron_type != PRON_DEMONSTRATIVE && prev->pron_type != PRON_RELATIVE))
            continue;
        if (strlen(curr->lower) < 5) continue;
        int icls = 0;
        if (!kin_is_known_noun_stem(curr->lower, &icls)) continue;

        curr->pos        = POS_NOUN;       /* mutates the REAL array element */
        curr->noun_class = icls;
        /* ... */
    }
    /* ... further context passes ... */
}
```

This is worth slowing down for, because it ties directly back to Chapter
3's struct-copy rule. `Token *prev = &sa->tokens[i-1];` does **not** copy
a `Token`. The `&` takes the *address* of the real element sitting inside
`sa->tokens[]`; `prev` is a pointer **aliasing** that exact memory — there
is only ever one `Token` here, and `prev`/`curr` are just two different
names for looking at two of its neighboring slots. When the code later
writes `curr->pos = POS_NOUN;`, it is reaching through that alias and
mutating the *actual* array element inside `sa`, permanently — the next
loop, and every later pipeline stage, will see this corrected tag. Had the
code instead written `Token prev = sa->tokens[i-1];` (no `&`, no `*`),
Chapter 3's value-copy rule would apply: `prev` would be an entirely
separate, independent `Token`, and any attempt to mutate `prev.pos` would
do nothing to `sa->tokens[i-1]` at all. The single character `&` is the
entire difference between "look at, and be able to change, the real
data" and "work on a disposable copy that vanishes when this loop body
ends."

Why is this split into a first pass (tag everything in isolation) and
then *separate* later passes (fix up using neighbors), instead of trying
to do it all in one combined pass? Because Pass A's check
(`prev->pron_type != PRON_DEMONSTRATIVE`) needs `prev`'s tag to already be
its **final** answer — if the single loop tried to tag word `i` while
also peeking at word `i-1`'s tag, and word `i-1` hadn't been *fully*
decided yet (or might itself still be corrected by a later pass), the
neighbor check could be reading stale, not-yet-final information.
Finishing the entire first pass before starting any context-sensitive
correction guarantees every `prev->pos` a later pass reads is already
settled.

## Key takeaways

- C has no polymorphism; deciding "what kind of thing is this" is solved
  with a strict, ordered chain of detector calls, each ending in an early
  `return` the moment it succeeds — order of the checks **is** the
  disambiguation logic, exactly as in Chapter 6's longest-match.
- Wrapping a few short-lived local variables in their own `{ }` block
  scopes them without requiring a separate named function.
- `static` means something different depending on where it's written:
  at file scope (Ch.4) it controls *linkage* (visibility to other files);
  on a local variable inside a function, it controls *storage duration*
  (the variable is built once and persists for the program's whole run,
  instead of being recreated on every call).
- An anonymous `struct { ... }` (no name, no `typedef`) is fine for a
  one-off table shape that's never referenced anywhere else.
- `strncpy(dst, src, n-1)` is a clean way to copy "all but the last
  character" of a string — control the length, not the content, to strip
  a known trailing character.
- Copy a buffer before mutating it when you need to *try* a
  transformation and might need to discard it if the attempt fails —
  never edit data in place if you still need the original on failure.
- `Token *p = &array[i];` makes `p` an alias for the *real* array
  element — mutating `p->field` changes the array permanently.
  `Token p = array[i];` (no `&`) makes `p` an independent copy —
  mutating it changes nothing in the array. This single difference is
  why multi-pass, neighbor-aware corrections are written with pointers,
  never with copies.

## Search YouTube for

- "C anonymous struct explained"
- "static keyword in C — linkage vs storage duration"
- "pointer aliasing vs copying a struct in C"
- "multi-pass algorithms explained"
- "scope blocks in C curly braces"

## Coming up in Chapter 9

Every chapter so far has *read* strings to classify them. `ortho.c` is
where the project starts *writing* new strings — applying RALC's sound-
change rules to actually transform an underlying morpheme sequence into
its correct surface spelling. Chapter 9 covers the low-level machinery
that makes that possible: a single mutable buffer edited in place with
`memmove`/`memcpy`, carrying its own length alongside it, exactly the
kind of manual, bounds-checked buffer surgery Chapter 1 previewed before
you knew enough C to appreciate it.
