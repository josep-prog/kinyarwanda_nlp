# Chapter 12 — `syntax.c`: Cross-Token Consistency Checks Over Arrays of Structs

## A different question than every previous chapter

Chapters 4–11 all answered questions about *one word at a time* (with
Chapter 8's neighbor-aware passes as a narrow exception — exactly one
step back or forward). `syntax.c` asks a structurally different kind of
question: does token *i* agree, grammatically, with some *other* token
elsewhere in the same sentence — sometimes its immediate neighbor,
sometimes a token an arbitrary distance away? This is the first file
whose checks genuinely operate over the *whole* `tokens[]` array as a
single unit, not just a sliding window of one or two positions.

## 12.1 `add_error`: the capacity+count idiom, append style, with a cross-struct side effect

```c
static void add_error(SentenceAnalysis *sa, ErrorType type, int idx,
                      const char *msg, const char *suggestion) {
    if (sa->error_count >= KIN_MAX_ERRORS) return;
    Error *e = &sa->errors[sa->error_count++];
    e->type         = type;
    e->token_index  = idx;
    strncpy(e->message,    msg,        KIN_MAX_MSG - 1);
    strncpy(e->suggestion, suggestion, KIN_MAX_MSG - 1);
    if (idx >= 0 && idx < sa->token_count)
        sa->tokens[idx].error_count++;
}
```

This is Chapter 3's capacity+count idiom, but in its **append** form
(contrast Chapter 11's *direct-index* form, where the final count was
known ahead of time): `sa->errors[sa->error_count++]` does two things in
one expression — `sa->error_count` is used as the index *first*, then
incremented, so this writes into the next free slot and advances the
counter in a single line, the same shape Chapter 4's tokenizer used for
`Token`s. The bounds check (`if (sa->error_count >= KIN_MAX_ERRORS)
return;`) comes first, exactly as Chapter 4's `emit_punct` checked
capacity before writing.

The last line is worth pausing on: `sa->tokens[idx].error_count++;`
reaches *back out* of the `Error` struct being built and increments a
field on a completely different struct — the specific `Token` this error
is about. This is exactly the payoff Chapter 3 promised when it explained
why `Error` stores a plain `int token_index` instead of a pointer or a
copy of the `Token` itself: because `idx` is just an index into an array
that `add_error` already has access to (`sa->tokens`), this function can
freely read *and write* through it with a single bounds check
(`idx >= 0 && idx < sa->token_count`), no lifetime or aliasing concerns
at all. A stored pointer to a `Token` would raise the question "is this
pointer still valid"; a stored array index never does, as long as the
array itself is still alive.

## 12.2 Naming your booleans: a readability technique, not just a C feature

Rule 3 (a sentence must have a verb) has several legitimate exceptions —
greetings, single-word fragments, and the "nominal predicate" pattern
where Kinyarwanda allows noun + adjective alone to form a complete
sentence with an implicit copula. Rather than writing one sprawling,
unreadable `if` condition combining every exception inline, the code
computes each exception as its own named boolean first:

```c
bool all_interj = true;
int  content_count = 0;
for (int ii = 0; ii < sa->token_count; ii++) {
    /* ... sets all_interj, increments content_count ... */
}
bool is_fragment = (content_count <= 1);

bool is_nominal_pred = false;
{
    bool has_noun = false, has_adj = false, all_nominal = true;
    for (int ii = 0; ii < sa->token_count; ii++) {
        /* ... sets has_noun, has_adj, all_nominal ... */
    }
    is_nominal_pred = has_noun && has_adj && all_nominal;
}

if (!all_interj && !is_fragment && !is_nominal_pred) {
    add_error(sa, ERR_NO_VERB, -1, /* ... */);
}
```

Each exception gets its own short loop, its own clearly named result, and
the *final* decision reads almost like a sentence: "flag a missing-verb
error unless this is all-interjections, or a fragment, or a nominal
predicate." Notice, too, the bare `{ }` block wrapping the
`is_nominal_pred` computation — the same scoping habit from Chapter 8,
here used so `has_noun`/`has_adj`/`all_nominal` (names that might
otherwise tempt reuse or collide with similar-sounding variables
elsewhere in a long function) exist only for as long as they're needed.
This is a technique worth deliberately adopting in your own code, in any
language: when a single condition would otherwise become an unreadable
wall of `&&`/`||`, compute its pieces as separately named booleans first.

## 12.3 Reaching into a nested struct array because C has no runtime field lookup

Rule 1 (noun-adjective agreement) needs to construct a *corrected* form
of a mismatched adjective. To do that, it needs that adjective's bare
stem — which isn't a top-level field on `Token`, it's buried inside that
token's own `MorphBreakdown`, labeled `"C"`:

```c
char corrected_adj[KIN_MAX_WORD] = {0};
const char *adj_core = NULL;
for (int m = 0; m < next->morph.n; m++) {
    if (strcmp(next->morph.m[m].label, "C") == 0) {
        adj_core = next->morph.m[m].form;
        break;
    }
}
if (adj_core && adj_core[0])
    kin_vv_join(expected_pfx, adj_core, corrected_adj, sizeof(corrected_adj));
```

This is a small but genuinely important C limitation showing through:
**C structs have no runtime reflection.** In a language with that
feature, you might write something like `next.morph["C"]` and let the
language look up the field named `"C"` for you at runtime. C cannot do
this — every struct field access (`next->morph.n`, `m->label`) must name
the field at *compile time*; there is no way to ask "give me the field
whose name matches this string I only know while the program is
running." `KinMorpheme`'s entire design (Ch.3) is the workaround: instead
of trying to access a field by name, the morpheme array stores its own
field *names* as data (`m->label`), right alongside the corresponding
value (`m->form`), so that "find the morpheme labeled C" becomes an
ordinary linear search over (at most 8) name/value pairs — `strcmp`
against `m->label`, exactly like searching any other small table in this
project. This is the same underlying idea as Chapter 7's lexicon tables,
turned inward: when you need to look something up by a name only known
at runtime, you store (name, value) pairs in an array and search it,
because the language itself won't do that lookup for you over a plain
struct's fields.

## 12.4 Direct-indexed tables, the simplest variant

```c
static const char *adj_concordance[17] = {
    "",     /* 0 = unused */
    "mu",   /* Nt.1  */
    "ba",   /* Nt.2  */
    /* ... through Nt.16 ... */
};
static const char *poss_connector[17] = {
    "",
    "wa",  "ba",  "wa",  "ya",  "rya", "ya",  "cya",
    "bya", "ya",  "za",  "rwa", "ka",  "twa", "bwa",
    "kwa", "ha",
};
```

This is Chapter 7's "index directly into a table using the enum/int value
itself" technique (`kin_class_name`), at its simplest: a plain array of
17 strings, where index `0` is deliberately left empty (noun classes are
numbered 1–16; reserving index 0 means `adj_concordance[cls]` works
directly with the real class number, with no `cls - 1` adjustment to
remember or get wrong at every call site). `cur->noun_class != next->noun_class`
is the actual mismatch check; once a mismatch is found, `adj_concordance
[cur->noun_class]` immediately gives the *correct* concordance prefix the
adjective should have used — direct indexing again standing in for what,
in another language, might be a small `switch` or a map lookup.

## 12.5 Rule 5: a real backward search, not just a single-step peek

Chapter 8's context passes only ever looked exactly one token back
(`&sa->tokens[i-1]`). Subject-verb agreement needs something stronger,
because the subject noun and its verb aren't always adjacent
(`"Abantu munini baragenda"` — "the big people are going" — has an
adjective sitting between the noun and the verb). This is a genuine
**bounded backward search**, walking as far back as necessary, with
explicit stopping conditions:

```c
int noun_idx = -1;
for (int j = vi - 1; j >= 0; j--) {
    const Token *t = &sa->tokens[j];
    if (t->pos == POS_PUNCTUATION) {
        if (t->is_sent_boundary || t->is_clause_boundary) break;
        continue;
    }
    if (t->pos == POS_NOUN && t->noun_class > 0) {
        noun_idx = j;
        break;
    }
    if (t->pos == POS_ADJECTIVE  || t->pos == POS_ADVERB    ||
        t->pos == POS_CONJUNCTION || t->pos == POS_LOCATIVE  ||
        t->pos == POS_PRONOUN)
        continue;
    break;   /* anything else: stop, we can't safely attribute a subject */
}
if (noun_idx < 0) continue;
```

Read this loop as three possible outcomes at every step walking
backward from the verb: **found it** (a noun — record `noun_idx` and
`break`), **keep looking** (punctuation that isn't a boundary, or one of
a known list of "transparent" word types that can sit between a subject
and its verb without breaking the relationship), or **give up entirely**
(a sentence/clause boundary, or any token type not on the transparent
list — `break` with `noun_idx` left at `-1`). This is the disambiguation
philosophy from Chapters 6, 8, and 10 applied to *searching*, not just
*matching*: a deliberately ordered set of cases, checked in priority
order, each one either continuing the search, claiming success, or
aborting — never left to fall through ambiguously.

Once a candidate subject noun is found, the actual agreement check layers
specific, documented exceptions on top of a plain equality test — the
same "guard exceptions on top of a basic check" discipline from Chapters
6 and 9:

```c
if ((nc == 1 || nc == 3) && (vc == 1 || vc == 3)) continue;  /* shared SP "a/u" */
if ((nc == 4 || nc == 9) && (vc == 4 || vc == 9)) continue;  /* shared SP "i"   */
```

Classes 1 and 3 happen to share the same surface subject-prefix form, so
treating them as compatible isn't a bug in the agreement check — it's an
accurate reflection of the actual language, and the comment says so
directly.

## 12.6 Rule 7 looks the other way — forward, not backward

Worth noting briefly, to round out the picture: Rule 7 (choosing between
`kugenda`, "to move/walk," and `kujya`, "to go to") looks **forward**
one token, the same direction as Rules 1 and 2's `cur`/`next` pairs:

```c
bool is_kugenda = (strcmp(verb->stem, "gend") == 0 || strcmp(verb->stem, "end") == 0);
bool is_kujya   = (strcmp(verb->stem, "jy")   == 0 || strcmp(verb->stem, "giy") == 0);
if (!is_kugenda && !is_kujya) continue;

if (is_kugenda && next->pos == POS_NOUN && !next->is_proper_noun) {
    /* "kugenda" directly followed by a bare destination noun: wrong verb choice */
}
```

The lesson worth taking from seeing both directions in the same file:
**which way you search the array depends entirely on the linguistic
relationship being checked**, not on any general rule about C array
traversal. A relationship that's always strictly local (does this verb's
own following object look right) reads forward one step; a relationship
that can span an unknown distance (which noun, possibly several words
back, does this verb's subject prefix actually agree with) requires a
real bounded search, not a fixed-offset peek.

## Key takeaways

- The capacity+count idiom (Ch.3) appears here in its append form
  (`sa->errors[sa->error_count++]`), the same shape as Chapter 4's
  token-emitting code, contrasted with Chapter 11's direct-index form.
- Storing an `int` index (not a pointer, not a copy) lets one function
  safely read *and write* through it into a different struct entirely,
  with nothing more than a bounds check — exactly the design Chapter 3
  argued for when introducing `Error.token_index`.
- When a single condition has several legitimate exceptions, compute each
  exception as its own named boolean first; the final combined condition
  then reads as a sentence instead of a wall of operators.
- C has no runtime reflection over struct field names — when you need to
  look something up by a name only known at runtime (like "the morpheme
  labeled `C`"), the workaround is to store (name, value) pairs in an
  array and search it, exactly as `KinMorpheme` does.
- A direct-indexed lookup table can deliberately waste index `0` to keep
  every real lookup free of an off-by-one adjustment, when the domain's
  natural numbering doesn't start at zero (noun classes are 1–16).
- A bounded backward (or forward) search through an array of structs,
  with explicit "found it" / "keep going" / "give up" cases at every
  step, is a stronger and more general tool than a fixed single-step
  neighbor peek — use it when the relationship being checked can span an
  unpredictable distance.

## Search YouTube for

- "naming boolean variables for readability"
- "linear search for a key in an array of structs C"
- "reflection vs no reflection in C explained"
- "backward array traversal with early termination C"

## Coming up in Chapter 13

`corrector.c` is the shortest substantive file in the project — only 107
lines. Chapter 13 covers how it reuses almost everything from this
chapter and Chapter 11 (the `adj_concordance` table, `kin_vv_join`, the
morpheme-label search) to actually *generate* the corrected text a
caller should use instead of the original, completing the loop from
"detect a problem" to "propose a fix."
