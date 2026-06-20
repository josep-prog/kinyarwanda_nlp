# Chapter 20 — `validator.c`: Bilingual Reporting and the Presentation Layer

## Where this file sits

`syntax.c` (Chapter 12) and `punctuation.c` (Chapter 13) both populate the
exact same `Error[]` array inside a `SentenceAnalysis` — a `type`, a
`token_index`, a `message`, a `suggestion`, nothing about how any of it
should be *displayed*. `validator.c` (385 lines) is the file that takes
that raw array and turns it into the bilingual, boxed, rule-numbered
report you actually see scroll past when you run `kinyarwanda_nlp
-validation`. This is a clean **separation of detection from
presentation**: neither `syntax.c` nor `punctuation.c` contains a single
`printf` call building user-facing output, and `validator.c` contains no
grammar logic at all — it only ever reads fields another file already
decided. Splitting these concerns into separate files means either side
can change independently: a new error type added to `syntax.c` needs one
new `case` line in this file's lookup tables, never a change to the
detection logic itself; a complete redesign of the report's visual layout
never touches a single grammar rule.

## 20.1 Two parallel `switch`-based name lookups for one enum

```c
static const char *error_type_tag(ErrorType t) {
    switch (t) {
        case ERR_ADJ_AGREEMENT:       return "ADJ_AGREEMENT";
        case ERR_POSS_AGREEMENT:      return "POSS_AGREEMENT";
        /* ... */
        default:                      return "ERROR";
    }
}
static const char *error_type_rw(ErrorType t) {
    switch (t) {
        case ERR_ADJ_AGREEMENT:       return "Indangasano ya ntera";
        case ERR_POSS_AGREEMENT:      return "Ikinyazina ngenera";
        /* ... */
        default:                      return "Ikindi";
    }
}
```

You met the `kin_*_name()` pattern back in Chapter 2 — a `switch` mapping
an enum value to a string, because C gives enums no name of their own at
runtime. Here the project takes that pattern and deliberately doubles it:
**one enum, two separate naming functions**, because this report has two
audiences' worth of label to produce for every single error — a short,
all-caps machine-style tag (`ADJ_AGREEMENT`) for anyone scanning output
or grepping logs, and a full Kinyarwanda phrase (`Indangasano ya ntera`)
for the human reading the bilingual report. Notice both functions share
the identical structure and even fall back to a generic default
(`"ERROR"` / `"Ikindi"`) for any `ErrorType` neither one explicitly lists
— exactly Chapter 2's `default:`-as-honest-catch-all reasoning, now
serving two parallel tables instead of one.

## 20.2 Classifying errors into two buckets with a single boolean function

```c
static bool is_punct_error(ErrorType t) {
    return (t == ERR_MISSING_PERIOD   || t == ERR_MISSING_COMMA    ||
            t == ERR_MISSING_QMARK    || t == ERR_MISSING_COLON    ||
            t == ERR_WRONG_PUNCT      || t == ERR_EXTRA_PUNCT      ||
            t == ERR_MISSING_EXCLAIM  || t == ERR_UNBALANCED_QUOTE  ||
            t == ERR_UNBALANCED_PAREN);
}
```

Every `ErrorType` the header defines comes from one of exactly two
sources: `syntax.c`'s agreement/completeness checks (Chapter 12) or
`punctuation.c`'s sixteen rules (Chapter 13) — but nothing in the `Error`
struct itself records *which file* raised a given error. `is_punct_error`
recovers that distinction after the fact, purely by listing every
`ErrorType` value `punctuation.c` is capable of producing. This is a
**closed-list membership test** — the same shape as Chapter 7's
`kin_is_adj_stem`, just written as a chain of `||` comparisons against
named enum constants instead of a loop over a table, because the list is
short and fixed rather than a large, data-driven table. The report uses
this single function to compute the per-sentence summary line ("3
violation(s) — 2 grammar/spelling, 1 punctuation") with one pass over the
already-collected errors:

```c
int gram_n = 0, punct_n = 0;
for (int i = 0; i < sa->error_count; i++) {
    if (is_punct_error(sa->errors[i].type)) punct_n++;
    else gram_n++;
}
```

## 20.3 `build_context`: incremental string assembly with `strncat`, and a highlight trick

```c
static void build_context(const SentenceAnalysis *sa, int err_tok,
                           char *out, size_t outsz) {
    out[0] = '\0';
    for (int i = 0; i < sa->token_count; i++) {
        const Token *t = &sa->tokens[i];
        if (t->pos == POS_PUNCTUATION) {
            strncat(out, t->surface, outsz - strlen(out) - 1);
            continue;
        }
        if (out[0] != '\0' && i > 0 && sa->tokens[i-1].pos != POS_PUNCTUATION)
            strncat(out, " ", outsz - strlen(out) - 1);

        if (i == err_tok) strncat(out, ">>", outsz - strlen(out) - 1);
        strncat(out, t->surface, outsz - strlen(out) - 1);
        if (i == err_tok) strncat(out, "<<", outsz - strlen(out) - 1);
    }
}
```

This rebuilds the entire sentence as one printable line, reassembling
spacing the tokenizer (Chapter 4) had already discarded, and wraps
whichever single token is `err_tok` in `>>...<<` markers so the error's
exact location jumps out visually in the printed report — for example,
`"Yagiye >>ariko<< aragaruka."`. Three things are worth naming precisely.

**`strncat(out, s, outsz - strlen(out) - 1)`** is a different bounded-copy
idiom from every `strncpy` you've seen so far (Chapters 6, 11, 13):
`strncat` *appends* to whatever's already in `out`, rather than
overwriting from the start, and the length argument has to be computed
fresh on every call — `outsz - strlen(out) - 1` is "however much room is
genuinely left in the buffer," recalculated each time because `strlen
(out)` grows with every append. Forgetting to subtract the buffer's
*current* length (and just passing `outsz` every time, as if appending
into an empty buffer) is a classic way to silently overflow a buffer with
`strncat` — this project never makes that mistake, but it's exactly the
kind of subtle, call-site-specific arithmetic worth checking carefully
any time you read `strncat` in someone else's code.

**The "no leading space before punctuation" rule**
(`sa->tokens[i-1].pos != POS_PUNCTUATION`) is doing real linguistic work:
without it, `"Yagiye ariko aragaruka ."` would print with a stray space
before the period — correct token-by-token, wrong as *displayed prose*.
Checking the previous token's tag before deciding whether to insert a
space is the same "look at what's already been built before deciding the
next step" discipline Chapter 16's `seq_append` used for its own
delimiter logic, applied here to natural-language spacing instead of a
debug token stream.

**The `>>...<<` markers are plain text, not terminal color codes.** This
project never emits ANSI escape sequences anywhere — a deliberate
simplicity choice (no terminal-capability detection needed, the output
stays identical whether piped to a file, viewed in a plain terminal, or
captured by a test harness) at the cost of a less visually striking
highlight than colored text would give.

## 20.4 `print_bilingual`: splitting one string in two with `strstr`

Every `Error.message`/`Error.suggestion` string, all the way back since
Chapter 12 introduced `add_error`, has been built as one string with a
literal `" / "` in the middle — Kinyarwanda text, then the separator, then
the English translation. This is where that convention finally gets
*consumed*:

```c
static void print_bilingual(const char *label_rw, const char *label_en,
                             const char *text) {
    const char *slash = strstr(text, " / ");
    if (slash) {
        printf("  %-10s: %.*s\n", label_rw, (int)(slash - text), text);
        printf("  %-10s: %s\n",   label_en, slash + 3);
    } else {
        printf("  %-10s: %s\n", label_en, text);
    }
}
```

`strstr(haystack, needle)` finds the first occurrence of one whole string
inside another, returning a pointer to where it starts (or `NULL` if it's
never found) — the multi-character cousin of `strchr`'s single-character
search from Chapter 13. Once `slash` points at the separator, the
Kinyarwanda half is everything *before* it (printed with `%.*s` and a
runtime-computed length `(int)(slash - text)` — the same pointer-
subtraction-for-length trick Chapter 4 used, combined with the
runtime-precision format specifier Chapter 5 introduced), and the English
half is everything starting three bytes *after* it (`slash + 3`, skipping
past the literal `" / "`). The `if (slash)` guard matters: not every
message in this codebase necessarily contains the separator (a
defensively-written caller can't simply assume it always will), so the
function falls back to printing the whole string under the English label
alone rather than risking a wrong split — or worse, indexing past a
`NULL` pointer — if the expected separator just isn't there.

This is genuinely the *inverse* operation of Chapter 16's `seq_append`
delimiter-joining: there, the problem was building one delimited string
out of several pieces incrementally; here, the problem is taking one
already-built delimited string and recovering its original pieces. C
gives you no built-in `split()` for either direction — `strstr` plus
pointer arithmetic is the standard, idiomatic way to do the splitting half
by hand.

## 20.5 File-format dispatch: a hand-rolled case-insensitive extension check

```c
static bool has_ext(const char *path, const char *ext_lower) {
    const char *dot = strrchr(path, '.');
    if (!dot) return false;
    char low[16] = {0};
    int  k = 0;
    while (k < 15 && dot[k]) {
        char ch = dot[k];
        low[k++] = (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
    }
    return strcmp(low, ext_lower) == 0;
}
```

`strrchr` (Chapter 13 met `strstr`/`strchr`; this is the third member of
that family) finds the **last** occurrence of a character — exactly what
a file extension needs, since a path like `report.v2.pdf` should match on
the final `.pdf`, not an earlier dot. The lowercasing loop that follows is
worth comparing directly against Chapter 4's `tolower((unsigned char)c)`
calls from `<ctype.h>`: here, the same operation is written out by hand —
`(ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch` — relying on the fact
that, in ASCII, every uppercase letter is exactly 32 code points before
its lowercase counterpart. Both approaches are completely correct for
ASCII text (a file extension is never going to contain non-ASCII
characters in practice); the difference is purely stylistic, and it's a
fair, honest observation that this project doesn't apply perfect
uniformity here either — one file reaches for the standard library
function, another reimplements the identical idea from first principles.
Recognizing that both are doing the same arithmetic, just spelled two
different ways, is more valuable than assuming the library call is always
what you'll find.

## 20.6 A multi-tool fallback chain, and the temp-file lifecycle it manages

`kin_validate_file` dispatches on `has_ext()` to decide how to turn a
`.pdf`, `.doc`/`.docx`, or plain-text file into the line-by-line text
`kin_analyze()` actually consumes — Chapter 19 already covered the
`system()`-and-`pdftotext` half of this in detail (Section 19.5: command
construction, the security caveat about untrusted input, and why this
program's actual usage context makes it acceptable). What's new here is
that the `.doc`/`.docx` branch chains **two** external tools with a
fallback:

```c
snprintf(cmd, sizeof(cmd), "antiword \"%s\" > \"%s\" 2>/dev/null", path, tmpfile);
if (system(cmd) != 0) {
    snprintf(cmd, sizeof(cmd),
        "libreoffice --headless --convert-to txt:Text \"%s\" --outdir /tmp/ 2>/dev/null", path);
    system(cmd);
    /* ... locate libreoffice's own output filename and rename() it to tmpfile ... */
}
```

`antiword` is tried first because it's smaller, faster, and purpose-built
for legacy `.doc` files; only if that specific command fails
(`system(cmd) != 0`) does the code fall back to invoking the much heavier
`libreoffice --headless` as a second attempt, capable of handling both
`.doc` and `.docx`. This is **graceful degradation**: rather than
requiring every user to have a specific tool installed, the code tries
the lightweight option first and only pays the cost of the heavyweight
fallback when it's actually needed — the same underlying judgment call as
Chapter 9's "try a fast specific path, fall back to a slower general one"
instinct, here applied to external process invocation instead of string
editing.

Every branch that converts a file funnels its output into a temporary
file (`/tmp/kin_val_<pid>.txt`, the process ID folded into the name so
two concurrent runs of this program never collide on the same temp
filename), and every branch sets `is_tmp = true` to record that fact.
Cleanup happens once, at the very end of the function, regardless of
which branch ran:

```c
validate_stream(fp, label);
fclose(fp);
if (is_tmp && tmpfile[0]) remove(tmpfile);
```

This project has no heap allocation to free (Chapter 21's defensive
discipline), but it still has a **resource** to manage here — a real file
on disk — and `is_tmp` is exactly the kind of explicit bookkeeping flag
that makes sure cleanup happens for the temp-file paths without
mistakenly trying to `remove()` a file the caller originally owned (the
plain-text branch never sets `is_tmp`, so its own input file is correctly
left untouched).

## 20.7 Case study: the report this file actually produces

Continuing Chapter 13's case-study sentence, here is what a caller running
`kin_validate_text("Yagiye ariko aragaruka.")` would see assembled from
every piece this chapter just covered:

```
  ----------------------------------------------------------------
  Ikibazo 1/1  [MISSING_COMMA  ·  Nta koma]
  ----------------------------------------------------------------
  Ijambo    : 'ariko' (indangiriro 2 / position 2)
  Context   : Yagiye >>ariko<< aragaruka.
  Ikibazo   : Icyungo 'ariko' gikeneye koma imbere yacyo.
  Problem   : Conjunction 'ariko' requires a comma before it.
  Gusubiza  : Shyira koma imbere ya 'ariko': '..., ariko ...'.
  Fix       : Insert a comma before 'ariko': '..., ariko ...'.
```

Tracing where each line comes from: `error_type_tag`/`error_type_rw`
(Section 20.1) produce the `[MISSING_COMMA · Nta koma]` header.
`build_context` (Section 20.3) produces the `>>ariko<<`-highlighted
sentence. `print_bilingual` (Section 20.4) splits `punctuation.c`'s
single `" / "`-joined message string into the separate `Ikibazo`/
`Problem` and `Gusubiza`/`Fix` lines. Every byte of this report traces
back to a `snprintf` call inside `punctuation.c`'s P1 rule (Chapter 13)
— `validator.c` never invents new text of its own; it only ever
reformats what the detection layer already wrote.

## Key takeaways

- Splitting detection (`syntax.c`, `punctuation.c`) from presentation
  (`validator.c`) means either side can change without touching the
  other — a new error type needs one new `case` here, never a change to
  the grammar logic; a new report layout never touches a grammar rule.
- One enum can be paired with more than one `kin_*_name()`-style lookup
  function when there's more than one audience for its labels — here, a
  machine-readable tag and a Kinyarwanda phrase, from the same `ErrorType`.
- A closed list of `||`-chained enum comparisons is a valid, simple
  membership test for a short, fixed set — the same shape as Chapter 7's
  table-driven membership tests, just without a backing array.
- `strncat`'s length argument must be recomputed at every call
  (`outsz - strlen(out) - 1`) because it appends rather than overwrites —
  forgetting to account for what's already in the buffer is a classic
  way to overflow it.
- `strstr` plus pointer arithmetic is how C splits a delimited string by
  hand, in the absence of a built-in `split()` — the inverse of building
  one up incrementally with repeated appends.
- The same "is this character a vowel / is this character uppercase"
  kind of check can be written with a standard-library call or a
  hand-rolled ASCII arithmetic expression — this project genuinely uses
  both styles in different files, with no real inconsistency in
  correctness, only in idiom.
- Trying a lightweight external tool first and falling back to a heavier
  one only on failure is graceful degradation — minimizing the common-case
  dependency footprint without making the rare case impossible.
- Even a project with zero heap allocation can still have a real resource
  to clean up (a temp file on disk) — an explicit boolean flag
  (`is_tmp`) tracking whether *this specific call* created something that
  needs removing is the same kind of bookkeeping discipline as any other
  manual resource-management pattern.

## Search YouTube for

- "strstr function in C explained"
- "strncat buffer overflow gotchas"
- "separation of concerns presentation vs business logic"
- "graceful degradation fallback design pattern"
- "temporary file handling and cleanup in C"

## Coming up in Chapter 21

Every source file has now been covered. Chapter 21 returns to the
Makefile from Chapter 1, this time going deep — object files, `.d`
dependency files, the difference between building `kinyarwanda_nlp` (the
CLI), `libkinyarwanda.a` (the static library), and `libkinyarwanda.so`
(the shared library) from the *exact same* source files, and why `-fPIC`
matters specifically for the shared-library build.
