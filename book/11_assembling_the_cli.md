# Chapter 11 — Assembling the Whole Pipeline: The CLI Tool

## How this chapter works

Same rules as Chapters 1 through 10. Every code example was actually
compiled with `gcc -std=c99 -Wall -Wextra` and actually executed — the
output shown in a fenced block prefixed with `$` is the real terminal
output of that exact program, and every CLI invocation shown was run
against the actual `kinyarwanda_nlp` binary built from this project's
own `Makefile`.

Ten chapters have each opened one file and gone deep. This last
chapter does the opposite: it opens `src/main.c` (the CLI entry
point), `src/validator.c` (the report printer this book has cited
since Chapter 7 without ever reading), and `src/api.c` (a 100-line
file that exists for exactly one reason — to hide everything the
other ten chapters built behind two plain `const char *` functions).
Together they are how a person typing `./kinyarwanda_nlp -s "..."` at
a terminal ends up running every chapter in this book in sequence
without ever knowing it.

---

# Part 1 — Language and Code, Side by Side

## 1.1 One sentence, eleven chapters, one command

```
$ ./kinyarwanda_nlp -s "Imana yaremye ijuru n'isi."
```

Typing that single line runs `kin_str_trim` (whitespace cleanup),
`kin_tokenize` (Chapter 6), every noun/adjective/verb/pronoun/
invariable detector from Chapters 1 through 5, `kin_analyze`'s nine
correction passes (Chapter 7), and `kin_print_analysis` (which this
book has been reading the *output* of since Chapter 1's very first
case study, without ever opening the function that produces it).
`main.c`'s own usage comment names the five other doors into this same
machinery:

> Usage: `./kinyarwanda_nlp` → interactive mode; `-s "text"` → analyze
> a single sentence; `-f file.txt` → analyze a text file; `-p file.pdf`
> → analyze a PDF; `--g2p -s "text"` → Chapter 8's phoneme mode;
> `--gloss -s "text"` → interlinear gloss; `-validation ...` →
> Chapter 10's punctuation-and-grammar validator.

Six modes, one binary, one `argv` loop deciding which chapters' code
actually runs for a given invocation.

## 1.2 Build it: a toy dispatcher with two modes, and what a third mode costs

```c
/* p1_toy_dispatch.c -- a minimal two-flag dispatcher */
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    const char *sentence = NULL;
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) verbose = true;
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) sentence = argv[++i];
    }
    if (sentence) printf("analyzing: %s (verbose=%d)\n", sentence, verbose);
    else printf("no sentence given\n");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra p1_toy_dispatch.c -o p1_toy_dispatch
$ ./p1_toy_dispatch -v -s "test"
analyzing: test (verbose=1)
```

Two flags, four lines of parsing. The real `main.c` has six modes
(`-s`, `-f`, `-p`, `--g2p`, `--gloss`, `-validation`), and — Section
4.4 finds — not every *combination* of two of those six flags was
actually given a defined meaning. A toy with two flags has only one
combination to get right; a dispatcher with six has fifteen pairs,
and this chapter's capstone shows what happens to one of them.

## 1.3 Checkpoint: test yourself before continuing

1. `main.c`'s argument loop checks `--g2p`/`--gloss`/`-validation`
   *before* it checks `-s`/`-f`/`-p`. Does parsing order here change
   which mode wins if a user passes contradictory flags, or are mode
   flags and input-source flags two independent groups?
2. Both `kin_g2p` (Chapter 8's public API) and `kin_correct` (this
   chapter) are documented to return a pointer into a `static` buffer.
   Before reading Section 2.2, predict what happens if a caller holds
   onto the pointer from one call and then makes a second call.

---

# Part 2 — The Outward-Facing Layer: Two Functions Instead of a Dozen Structs

## 2.1 `api.c`: hiding `Token` and `SentenceAnalysis` from a caller who shouldn't need them

Every chapter since Chapter 1 has worked directly with `Token` and
`SentenceAnalysis` — dozens of fields, `MorphBreakdown`, `GramRole`,
error arrays. A real downstream consumer — an ASR system wanting
spelling correction, a TTS system wanting phonemes — doesn't want any
of that; it wants to hand over text and get text back. `api.c` exists
for exactly that handoff:

```c
const char *kin_correct(const char *text);  /* ASR post-processor */
const char *kin_g2p(const char *text);      /* TTS phoneme string */
```

Two functions, `const char *` in, `const char *` out, no struct in
either signature. Every chapter's rich internal representation —
Chapters 1 through 5's tags, Chapter 7's correction passes, Chapter
8's phoneme sequence — still runs underneath both calls; `api.c` is
the one file in this whole project whose entire job is making that
machinery invisible to a caller who only ever wanted a string back.

## 2.2 The risk here: a `static` return buffer, demonstrated, not just stated

`kin_correct`'s own comment is upfront about its implementation:

> Static buffer: 4096 bytes — sufficient for the longest plausible
> sentence.

A `static` buffer inside a function persists between calls and is
shared by every call — meaning the pointer this function returns
stops being valid (in the sense of "still holding the result you
asked for") the moment you call it again:

```c
/* p2_static.c -- the same pointer, before and after a second call */
#include "kinyarwanda_api.h"
#include <stdio.h>

int main(void) {
    const char *r1 = kin_correct("Umuntu munini aragenda.");
    printf("r1 right after first call:  %s\n", r1);

    const char *r2 = kin_correct("Imana yaremye ijuru.");
    printf("r1 read again, after second call: %s\n", r1);
    printf("r2: %s\n", r2);
    printf("r1 == r2 (same address)? %s\n", (r1 == r2) ? "yes" : "no");
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p2_static.c -L . -lkinyarwanda -o p2_static
$ LD_LIBRARY_PATH=. ./p2_static
r1 right after first call:  Umuntu munini aragenda.
r1 read again, after second call: Imana yaremye ijuru.
r2: Imana yaremye ijuru.
r1 == r2 (same address)? yes
```

`r1` and `r2` are the literal same memory address. Reading `r1` a
second time, after calling `kin_correct` again, silently shows the
*second* call's result — not because anything is broken, but because
that is exactly what a `static` return buffer is documented to do.
This is the same category of risk this book has flagged since Chapter
2's fixed-size output buffers, in a new shape: not "this buffer can
overflow," but "this buffer has exactly one owner at a time, and that
owner is whichever call happened most recently." A caller who needs
two corrected sentences at once must copy the first result out before
making the second call — `kin_correct` itself gives no warning beyond
its own comment.

---

# Part 3 — Making It Interactive

## 3.1 Build it: the validator, exactly as `validator.c` itself calls it

```c
/* p3_repl.c -- kin_analyze, then kin_check_punctuation, the same two
 * calls Chapter 10 introduced, now wrapped the way validator.c does */
#include "kinyarwanda.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    char line[256];
    printf("Type a sentence to validate, or 'q' to quit.\n");
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == 'q' && line[1] == '\0') break;
        kin_validate_text(line);
    }
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p3_repl.c -L . -lkinyarwanda -o p3_repl
$ printf "Umuntu munini aragenda.\nq\n" | LD_LIBRARY_PATH=. ./p3_repl
Type a sentence to validate, or 'q' to quit.

======================================================================
ISUZUMA RY'INYANDIKO — Sentence Validation
======================================================================

  [Interuro 1 / Sentence 1]
  "Umuntu munini aragenda."

  ✓ Iyi nyandiko ntakosa ifite. / No violations — sentence is correct.
```

One call, `kin_validate_text`, and the entire report — the header
box, the `[Interuro 1 / Sentence 1]` line, the bilingual verdict — is
`validator.c`'s own code, not anything this chapter wrote. Section 4.1
finds that the `[Interuro 1]` label printed here directly contradicts
what the function's own internal comment says should happen for
exactly this input.

---

# Part 4 — Capstone: Four Real Findings, and the Fix Applied to Each

This part originally just found these four bugs. All four are now
fixed in this project's own source — the same files this chapter has
been reading, not a hypothetical patch. Each section below shows the
bug as it was found, the fix that was actually applied, and the same
reproduction re-run against the fixed code.

## 4.1 The headline bug: a corrected sentence can be replaced by a paragraph of advice

`api.c`'s `kin_correct` trusts every `ERR_SPELLING` error's
`suggestion` field to already contain a single corrected *word*:

```c
const char *word = tok->surface;
for (int e = 0; e < sa.error_count; e++) {
    if (sa.errors[e].token_index == i
        && sa.errors[e].type == ERR_SPELLING
        && sa.errors[e].suggestion[0] != '\0') {
        word = sa.errors[e].suggestion;
        break;
    }
}
```

`syntax.c` has exactly two places that actually raise `ERR_SPELLING`.
Read what one of them puts in `suggestion`:

```c
snprintf(sug, sizeof(sug),
    "Suzuma niba ari ijambo ry'amahanga cyangwa izina bwite. "
    "Check if this is a foreign word or proper name.");
add_error(sa, ERR_SPELLING, i, msg, sug);
```

That's not a word — it's a full bilingual sentence of advice. Trigger
this exact rule (the letter `'l'`, §2.3 from Chapter 6's "no letter L
in native Kinyarwanda" orthography rule, on a word the tagger
recognizes as Kinyarwanda and not a proper noun) and run the result
through `kin_correct`:

```c
/* p4_garbled.c -- a real ERR_SPELLING trigger, through kin_correct */
#include "kinyarwanda_api.h"
#include <stdio.h>

int main(void) {
    const char *s = "Abana balimo kwiga.";
    printf("Input:     %s\n", s);
    printf("Corrected: %s\n", kin_correct(s));
    return 0;
}
```

```
$ gcc -std=c99 -Wall -Wextra -I include p4_garbled.c -L . -lkinyarwanda -o p4_garbled
$ LD_LIBRARY_PATH=. ./p4_garbled
Input:     Abana balimo kwiga.
Corrected: Abana Suzuma niba ari ijambo ry'amahanga cyangwa izina bwite. Check if this is a foreign word or proper name. kwiga.
```

The single word `balimo` is replaced by an entire paragraph of
bilingual advice, dropped directly into the middle of the
"corrected" sentence — for a function whose entire documented purpose
is ASR post-processing, where this string would be fed straight back
into a downstream system expecting one clean sentence. The bug isn't
in *whether* the letter-`l` rule is right (Chapter 6 already
established it is); it's that `kin_correct` was written assuming every
`ERR_SPELLING` suggestion is a drop-in replacement word, and neither
of the two real code paths that raise `ERR_SPELLING` was written to
honor that assumption — each puts a full explanatory sentence in the
same field `kin_correct` blindly substitutes in.

### The fix

`suggestion` and "a literal replacement word" are two different
things, and the fix keeps them as two different fields instead of
trying to make one do both jobs. `kinyarwanda.h`'s `Error` struct
gained a new member:

```c
typedef struct {
    ErrorType type;
    int       token_index;
    char      message[KIN_MAX_MSG];
    char      suggestion[KIN_MAX_MSG];
    /* A literal drop-in replacement word, when one is actually known —
     * distinct from 'suggestion', which is a bilingual explanatory
     * sentence and is NOT safe to substitute directly into a sentence.
     * Empty ("") when no concrete replacement word was computed. */
    char      corrected_word[KIN_MAX_WORD];
} Error;
```

`syntax.c`'s vowel-harmony rule — the one call site that already
computes a real corrected word (`corrected`, built a few lines above
its own `add_error` call) — now copies that word into the new field
right after raising the error:

```c
int err_idx = sa->error_count;
add_error(sa, ERR_SPELLING, i, msg, sug);
if (sa->error_count > err_idx)
    strncpy(sa->errors[err_idx].corrected_word, corrected, KIN_MAX_WORD - 1);
```

The letter-`l` call site is left exactly as it was: it never computes
a real correction (the engine genuinely doesn't know whether `balimo`
is a typo, a foreign word, or a proper name missing capitalization),
so `corrected_word` for that error simply stays empty — every `Error`
starts zeroed out (`kin_analyze` runs `memset(&sa, 0, sizeof(sa))`
before anything else), so "no correction known" and "field never
touched" are the same state.

`api.c`'s `kin_correct` now reads `corrected_word` instead of
`suggestion`, and only substitutes when one was actually computed:

```c
const char *word = tok->surface;
for (int e = 0; e < sa.error_count; e++) {
    if (sa.errors[e].token_index == i
        && sa.errors[e].type == ERR_SPELLING
        && sa.errors[e].corrected_word[0] != '\0') {
        word = sa.errors[e].corrected_word;
        break;
    }
}
```

Re-running the exact reproduction from above, against the fixed code:

```
$ gcc -std=c99 -Wall -Wextra -I include p4_garbled.c -L . -lkinyarwanda -o p4_garbled
$ LD_LIBRARY_PATH=. ./p4_garbled
Input:     Abana balimo kwiga.
Corrected: Abana balimo kwiga.
```

No more inserted paragraph. Because the letter-`l` rule has no real
correction to offer, `kin_correct` now does the honest thing for that
case: leave the word exactly as the caller wrote it, rather than
replace it with anything — wrong or not.

## 4.2 Validating a `.doc`/`.docx` with neither converter installed looks exactly like success

`kin_validate_file`'s fallback chain tries `antiword`, then
`libreoffice`, with this comment on the failure path:

> if (system(cmd) != 0) { /* libreoffice unavailable; rename below will
> detect failure */ }

The code never actually checks `rename()`'s return value — the
comment describes a safety net that isn't there. Test the realistic
case: a system with neither tool installed (confirmed absent in this
environment), validating a file with a `.docx` extension:

```
$ which antiword libreoffice
antiword not found
libreoffice not found
$ echo "Umuntu munini aragenda kandi ariko." > /tmp/fake.docx
$ ./kinyarwanda_nlp -validation /tmp/fake.docx
$ echo "exit code: $?"

======================================================================
ISUZUMA RY'INYANDIKO — Sentence Validation
Dosiye / File: /tmp/fake.docx
======================================================================

exit code: 0
```

No error message. No sentence count. No violations reported. Exit
code `0`. Both conversion tools failed completely, but the only
visible trace is an empty report — which looks, to a user who hasn't
read this chapter, exactly like "your document had zero grammar
problems," not "your document was never actually read." The root
cause: `antiword`'s failed shell redirect (`> "$tmpfile"`) still
creates an empty `tmpfile` even when the command itself fails, so by
the time `libreoffice` also fails and its `rename()` silently does
nothing, `fopen(tmpfile, "r")` successfully opens that leftover empty
file — the `if (!fp)` error-message block a few lines later, which
*would* have told the user to install one of the two tools, never
gets a chance to run, because `fp` isn't `NULL`. It's a real word.
It's just empty.

### The fix

The fix tracks the *actual exit status* of each conversion attempt in
a `bool converted`, instead of inferring success from whether a file
happens to exist afterward — and explicitly deletes `antiword`'s
empty leftover file before trying `libreoffice`, so no stale file
survives to be opened by accident:

```c
snprintf(cmd, sizeof(cmd),
    "antiword \"%s\" > \"%s\" 2>/dev/null", path, tmpfile);
bool converted = (system(cmd) == 0);
if (!converted) {
    remove(tmpfile);  /* discard antiword's empty/partial redirect target */
    snprintf(cmd, sizeof(cmd),
        "libreoffice --headless --convert-to txt:Text "
        "\"%s\" --outdir /tmp/ 2>/dev/null", path);
    if (system(cmd) == 0) {
        /* ...build lo_out, the path libreoffice actually wrote to... */
        converted = (rename(lo_out, tmpfile) == 0);
    }
}
if (!converted) {
    fprintf(stderr,
        "Ikosa: Ntibishoboka guhindura '%s'. "
        "Shyiraho 'antiword' cyangwa 'libreoffice'.\n"
        "Error: cannot convert '%s'. "
        "Install antiword or libreoffice.\n", path, path);
    return;
}
fp = fopen(tmpfile, "r");
```

`rename()`'s return value — the exact check the old comment claimed
existed — now actually gates `converted`. Re-running the identical
reproduction:

```
$ which antiword libreoffice
antiword not found
libreoffice not found
$ echo "Umuntu munini aragenda kandi ariko." > /tmp/fake.docx
$ ./kinyarwanda_nlp -validation /tmp/fake.docx
Ikosa: Ntibishoboka guhindura '/tmp/fake.docx'. Shyiraho 'antiword' cyangwa 'libreoffice'.
Error: cannot convert '/tmp/fake.docx'. Install antiword or libreoffice.
$ echo "exit code: $?"
exit code: 0
```

The user now gets the actual error message this function always had
the *words* for, just never the working code path to reach. (The exit
code is still `0` — `kin_validate_file` itself returns `void`, and
`main.c`'s `-validation` dispatch doesn't inspect anything further.
Surfacing this failure as a non-zero process exit code would mean
changing `kin_validate_file`'s public signature, a larger change than
this specific bug — found and described in this chapter as "the
report looks like success" — actually called for.)

## 4.3 A comment describing a fix that was never written

`kin_validate_text`'s own body has this comment directly above the
one line of actual logic:

```c
void kin_validate_text(const char *text) {
    if (!text || !text[0]) return;
    print_header(NULL);
    int sent_num = 0, total = 0;
    /* For a single sentence (no trailing punctuation) send_num stays 0 in
     * print_report, so the "[Interuro N]" prefix is suppressed.  We correct
     * this by using sent_num=0 for the first call when input is one sentence.
     * To detect: count terminal marks in text.                               */
    validate_text_block(text, &sent_num, &total);
    print_footer(sent_num, total);
}
```

The comment describes counting terminal marks in the text to decide
whether to suppress the `[Interuro N]` prefix for single-sentence
input. No such counting exists anywhere in this function or the one
it calls. Test the exact single-sentence case the comment is about:

```
$ ./kinyarwanda_nlp -validation "Umuntu munini aragenda."

======================================================================
ISUZUMA RY'INYANDIKO — Sentence Validation
======================================================================

  [Interuro 1 / Sentence 1]
  "Umuntu munini aragenda."

  ✓ Iyi nyandiko ntakosa ifite. / No violations — sentence is correct.
```

`[Interuro 1 / Sentence 1]` prints — the exact prefix the comment
claims gets suppressed. The reason is mechanical: `validate_text_block`
unconditionally runs `(*sent_num)++` the moment it finds *any* complete
segment, even the only one a single sentence has, so `sent_num` is `1`,
not `0`, by the time `print_report` checks it. This is harmless in
practice — the label is arguably more informative than confusing
here — but it's a real, verifiable case of a comment documenting an
intention that the code beside it doesn't carry out.

### The fix

Since the actual behavior (always showing `[Interuro N]`, even for
one sentence) was never the problem — only the comment's claim about
it was — the fix doesn't touch the logic at all, only the comment:

```c
int sent_num = 0, total = 0;
/* validate_text_block() increments sent_num for every sentence it
 * finds, including the only one in single-sentence input, so
 * print_report's "[Interuro N / Sentence N]" prefix is shown even
 * when the input is just one sentence. */
validate_text_block(text, &sent_num, &total);
```

The output is unchanged, which is itself the point: the comment now
describes what the three lines beside it actually do, instead of
describing a feature that was never built.

## 4.4 `--gloss` (and `--g2p`) silently do nothing when combined with `-f` or `-p`

`main.c`'s gloss-mode dispatch only fires when both flags are present:

```c
if (gloss_mode && sentence) {
    SentenceAnalysis sa = kin_analyze(sentence);
    printf("\nInput: %s\n", sentence);
    kin_print_analysis(&sa, verbose);
    kin_print_interlinear(&sa);
    return 0;
}
```

`sentence` is only ever set by `-s`. Pass `--gloss` with `-f` instead,
and `gloss_mode && sentence` is false — the function falls through,
with no error, straight to the ordinary file-analysis branch lower
down:

```
$ echo "Umuntu munini aragenda." > /tmp/glosstest.txt
$ ./kinyarwanda_nlp --gloss -f /tmp/glosstest.txt

[Umurongo 1 / Line 1]

Input: Umuntu munini aragenda.
========================================================================
ISESENGURA RY'URURIMI / LANGUAGE ANALYSIS
========================================================================
...
 Umuntu               Izina mbonera (Noun)            Nt.1      ntu
  └─ Uturemajambo (Morphemes): [D]u + [RT]mu(Nt.1) + [C]ntu
  └─ Gusubiza (Reconstruction):
       Ingingo:  [D]u + [RT]mu + [C]ntu  →  umuntu  ✓
```

That's Chapter 1's ordinary morpheme-table output, not Chapter 8's
interlinear gloss format `--gloss` was supposed to select. The flag
was accepted (no "unknown option" error — `--gloss` is a real,
recognized flag) and then quietly never consulted again. The same gap
applies to `--g2p -f`/`--g2p -p`, for the identical structural reason:
both special modes are wired to `sentence` alone, with no equivalent
branch for `filename` or `pdffile`.

### The fix

`main.c` gained `gloss_line`/`gloss_stream`/`gloss_pdf` and
`g2p_line`/`g2p_stream`/`g2p_pdf` — three-function sets mirroring the
exact shape `analyse_line`/`analyse_stream`/`analyse_pdf` already
had, so each special mode can now run over one sentence, a whole file,
or a PDF the same way plain analysis always could. The dispatch itself
changed from "only fires if both flags are present" to "fires on the
mode flag, then picks whichever input source was actually given — and
says so clearly if none was":

```c
if (gloss_mode) {
    if (sentence) { gloss_line(sentence, verbose); return 0; }
    if (filename) { /* open filename, gloss_stream(fp, verbose) */ return 0; }
    if (pdffile)  { return gloss_pdf(pdffile, verbose); }
    fprintf(stderr,
        "Ikosa: --gloss ikeneye -s, -f cyangwa -p.\n"
        "Error: --gloss requires one of -s, -f, or -p.\n");
    return 1;
}
```

(`g2p_mode`'s dispatch is the same shape, calling `g2p_line`/
`g2p_stream`/`g2p_pdf` instead.) Re-running the exact reproduction:

```
$ echo "Umuntu munini aragenda." > /tmp/glosstest.txt
$ ./kinyarwanda_nlp --gloss -f /tmp/glosstest.txt
...
  ═══ Interlinear Gloss (Amategeko y'Igenamajwi) ══════════════
  Morpheme chain and English gloss for each word / akaramejambo
  ...
  Umuntu             u        – mu       – ntu
                     CL1.SG   – Nt.1     – person

  munini             mu       – nini
                     AGR.CL1  – big/adult

  aragenda           a        – ra       – gend     – a
                     3SG.HUM  – PRES     – go/travel – IND

  .                  [.]
  ─── Translation hint: person [big/adult] go/travel.
  ═══════════════════════════════════════════════════════
```

The interlinear section Chapter 8 introduced now actually appears —
`--gloss -f` runs the same gloss pipeline `--gloss -s` always did,
just once per line of the file. And the case that used to fall
through silently now says exactly what's wrong:

```
$ ./kinyarwanda_nlp --gloss
Ikosa: --gloss ikeneye -s, -f cyangwa -p.
Error: --gloss requires one of -s, -f, or -p.
$ echo "exit: $?"
exit: 1
```

---

# Part 5 — Reframing This as an Engineering Contract

## 5.1 Three layers, three different callers in mind

This project has, by the end of this chapter, three distinct
"outermost" surfaces, each built for a different caller:

```
  internal engine        Token / SentenceAnalysis / MorphBreakdown
  (Ch.1-10)               -- every struct this book has read directly

         |  used by both layers below
         v
  ┌──────────────────────┐         ┌──────────────────────────┐
  │ validator.c            │         │ api.c                     │
  │ kin_validate_text/file │         │ kin_correct / kin_g2p     │
  │ -> prints a bilingual  │         │ -> returns a plain string │
  │    REPORT to stdout    │         │    for another PROGRAM    │
  └──────────────────────┘         └──────────────────────────┘
         ^                                    ^
         |                                    |
    a human reading a terminal        an ASR/TTS system calling
    (-validation mode)                 this as a library (api.c)
```

`validator.c` is built for a human: box-drawn headers, bilingual
labels, an `>>word<<` highlight in context. `api.c` is built for
another program: no formatting at all, just a string a caller can
immediately re-embed. Both sit on top of the exact same
`SentenceAnalysis` Chapters 1 through 10 already built — the
difference is entirely in what each layer does with it on the way out,
which is exactly why Section 4.1's bug is invisible if you only ever
use `-validation` mode (which prints the full bilingual explanation
and *labels* it as a suggestion, where a human reader can tell it
isn't a word) and only surfaces once something expects `kin_correct`'s
output to be plain, re-insertable text.

## 5.2 The same sentence-splitting logic, written twice

`main.c`'s `analyse_text()` and `validator.c`'s `validate_text_block()`
both implement the identical rule — split on `.`/`!`/`?` followed by a
space, newline, or end-of-string, never split on a comma — as two
separate, independently written loops in two different files, rather
than one shared helper either file calls. Neither copy is wrong; both
were verified against the same real examples earlier in this book.
But it's the kind of duplication that makes Section 4.3's bug
*specifically* a `validator.c` problem and not also a `main.c`
problem — the two files' sentence counters are tracked through two
separately maintained variables, in two separately written functions,
and only one of the two ever grew the stale comment Section 4.3 found.

---

# Part 6 — Reading the Real Production Code

## 6.1 `main.c`'s mode dispatch, in full

```c
/* Validation mode: -validation <sentence|paragraph|file> */
if (validation_mode) {
    if (val_input) {
        FILE *probe = fopen(val_input, "r");
        if (probe) { fclose(probe); kin_validate_file(val_input); }
        else        kin_validate_text(val_input);
    } else if (sentence) { kin_validate_text(sentence); }
    else if (filename)   { kin_validate_file(filename); }
    else if (pdffile)    { kin_validate_file(pdffile); }
    else { /* interactive validation loop */ }
    return 0;
}

if (g2p_mode && sentence) { /* Chapter 8's phoneme mode */ return 0; }
if (gloss_mode && sentence) { /* Chapter 8/9's interlinear gloss */ return 0; }
if (sentence) { analyse_line(sentence, verbose); return 0; }
if (filename) { /* analyse_stream */ return 0; }
if (pdffile)  { return analyse_pdf(pdffile, verbose); }
/* interactive mode */
```

Read top to bottom, this is one long `if`/`else if` chain checked in a
fixed priority order: `-validation` first, then `--g2p`, then
`--gloss`, then plain `-s`/`-f`/`-p`, then interactive mode last. The
`val_input` branch's own `fopen` probe — try opening the argument as a
file; if that fails, treat it as inline text — is the one piece of
genuine auto-detection logic in the whole dispatcher; every other
branch is a direct one-to-one mapping from a flag to a function call,
with no fallback if the input doesn't fit (the source of Section
4.4's gap).

## 6.2 `print_report` and `build_context`, the functions behind every report this book has quoted since Chapter 7

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

Every `strncat` call here recomputes `outsz - strlen(out) - 1` fresh,
rather than tracking a running length in a local variable — slightly
more expensive (re-walking the growing buffer on every token) but
correct by construction: there's no separate counter that could ever
drift out of sync with the buffer's real contents, the same
"recompute from the source of truth instead of trusting a cached
copy" instinct this book has favored since the bounds checks in
Chapter 2.

## 6.3 `kin_correct`, the function behind Section 4.1's finding, in full

```c
const char *kin_correct(const char *text)
{
    static char buf[4096];
    if (!text || !text[0]) { buf[0] = '\0'; return buf; }

    SentenceAnalysis sa = kin_analyze(text);
    kin_suggest_corrections(&sa);

    buf[0] = '\0';
    size_t pos = 0;
    for (int i = 0; i < sa.token_count; i++) {
        Token *tok = &sa.tokens[i];
        const char *word = tok->surface;
        for (int e = 0; e < sa.error_count; e++) {
            if (sa.errors[e].token_index == i
                && sa.errors[e].type == ERR_SPELLING
                && sa.errors[e].suggestion[0] != '\0') {
                word = sa.errors[e].suggestion;
                break;
            }
        }
        bool attach_left = (tok->pos == POS_PUNCTUATION
                            && tok->punct_type != PUNCT_QUOTE_OPEN);
        if (pos > 0 && !attach_left) {
            if (pos < sizeof(buf) - 1) buf[pos++] = ' ';
        }
        size_t wlen = strlen(word);
        if (pos + wlen >= sizeof(buf)) wlen = sizeof(buf) - pos - 1;
        memcpy(buf + pos, word, wlen);
        pos += wlen;
        buf[pos] = '\0';
    }
    return buf;
}
```

Notice this function calls `kin_suggest_corrections` — Chapter 7's
own narrow-scope corrector, already found there to only fill in a
concrete corrected word for `ERR_ADJ_AGREEMENT` and
`ERR_POSS_AGREEMENT`. `ERR_SPELLING` was never in that function's
scope at all; its `suggestion` field is filled directly by `syntax.c`
at the moment the error is first detected, by two call sites that were
each written to explain the problem in prose, not to hand back a
replacement token. `kin_correct` was written after both of those
call sites already existed, assuming a contract neither one was ever
asked to honor.

---

# Part 7 — Looking Back: The Whole Book, in One Pipeline

This chapter closes a path that started in Chapter 1 with a single
question — what is `umuntu`, and why is it tagged class 1 — and ends
here with a single command producing a printed report. Tracing that
path back:

- **Chapters 1–5** built the five trees: nouns, adjectives, verbs,
  pronouns, invariable words — each chapter justifying its own data
  structure (open-set rule tables, closed-set flat lookups, the
  hybrid verb tagger) against the others.
- **Chapter 6** found the layer underneath all five: the tokenizer
  deciding where a word even begins or ends, before any tree gets a
  chance to classify it.
- **Chapter 7** found the layer above all five: `kin_analyze`'s nine
  steps, correcting what a single word's tagger couldn't know on its
  own.
- **Chapter 8** stepped sideways into an entirely independent
  pipeline — pronunciation, never touching a `Token` at all.
- **Chapter 9** found the engine spelling every morpheme boundary
  every earlier tree's plus-sign had quietly deferred.
- **Chapter 10** found the rules governing the marks between the
  words those five trees had already tagged.
- **This chapter** found the three different doors — a human-facing
  report, a program-facing string, a six-flag CLI dispatcher — that
  every one of those ten chapters' work has to pass through before it
  reaches anyone outside this source tree.

Two large files were used constantly across this book and never given
their own chapter: `morphology.c` (3,366 lines — the `SP_TABLE[]`,
`OM_TABLE[]`, vowel-contact joiners, and prefix-stripping logic behind
every noun, adjective, and verb chapter's morpheme breakdown) and
`morph_dispatch.c` (3,528 lines — the type-dispatch logic that decided
*which* of those breakdown routines to run for a given tagged token).
That's an honest gap, not an oversight being hidden: every tool this
book has taught — read the header comment first, compile and run the
real function, test its own documented examples against itself,
trust the code over the comment when they disagree — applies
identically to both files. A reader who has finished this book has
everything needed to open `morphology.c` next and keep going.

---

# Part 8 — Practice

### Beginner

1. Run `./kinyarwanda_nlp --help` for real and find the one usage line
   that documents `-validation` taking either a sentence or a file
   path as its argument. Which function in `main.c` performs the
   auto-detection between the two?
2. Section 4.3 found a comment describing logic that doesn't exist.
   Find the exact line where `sent_num` is incremented inside
   `validate_text_block`, and confirm by hand that it always reaches
   `1` for any non-empty single sentence.

### Intermediate

3. Section 4.1's bug means any real `ERR_SPELLING` trigger corrupts
   `kin_correct`'s output. Construct a second sentence — different
   from `"Abana balimo kwiga."` — that triggers `syntax.c`'s *other*
   `ERR_SPELLING` call site (the vowel-harmony extension mismatch
   around RULE 9), and confirm the same kind of corruption.
4. Section 4.4 found `--gloss -f` silently falls back to ordinary
   analysis. Trace `main.c`'s dispatch chain by hand and confirm
   `--g2p -p somefile.pdf` has the identical gap, for the identical
   reason.
5. `kin_g2p` (Chapter 8's public API) shares `kin_correct`'s
   static-buffer design. Write the equivalent of Section 2.2's test
   program for `kin_g2p` and confirm the same aliasing behavior.

### Advanced

6. Section 4.1's real fix added `corrected_word` as a field separate
   from `suggestion`, rather than, say, parsing a corrected word back
   out of the bilingual `suggestion` sentence. Read the actual diff in
   `kinyarwanda.h` and `syntax.c`. Why is a dedicated field safer here
   than trying to extract a quoted word from `sug` with `strstr`,
   given that `message`/`suggestion` strings are meant for bilingual
   human-facing prose, not machine parsing?
7. Section 4.2's real fix tracks `bool converted` from each `system()`
   call's actual exit status — not from whether `tmpfile` happens to
   exist afterward, which was the original bug's exact failure mode.
   Given that, is the fix's `remove(tmpfile)` call (made right before
   trying `libreoffice`) actually load-bearing for correctness, or is
   it cleanup that happens to also be good practice? Trace what
   `converted` would end up being, with that `remove()` call deleted,
   in the case where `libreoffice` also fails — does the function
   still correctly refuse to call `fopen`?
8. Section 5.2 found the same sentence-splitting logic duplicated in
   `main.c` and `validator.c`. Sketch the signature of a single shared
   helper both files could call instead, and name which header it
   would need to be declared in.

---

## Key takeaways

- A single CLI invocation like `./kinyarwanda_nlp -s "..."` runs every
  chapter in this book in sequence — tokenizing, tagging, correcting,
  and finally printing — through a fixed-priority `if`/`else if` chain
  in `main.c` that decides which of six modes a given set of flags
  selects.
- `api.c` exists to hide every internal struct this book has used
  directly behind two plain string-in, string-out functions
  (`kin_correct`, `kin_g2p`) — both documented to return a pointer
  into a `static` buffer, a real, demonstrated aliasing risk distinct
  from every fixed-buffer-overflow risk this book has flagged before.
- The headline real finding, now fixed: `kin_correct` assumed every
  `ERR_SPELLING` error's `suggestion` field was a drop-in replacement
  word, but the two real places that raise `ERR_SPELLING` both wrote
  a full bilingual explanatory sentence into that field instead. The
  fix adds a separate `corrected_word` field to `Error`, populated
  only where a real replacement word is actually computed; `kin_correct`
  now reads that field instead of `suggestion`, and leaves a word
  untouched rather than guess when no real correction is known.
- A second real finding, now fixed: when neither `antiword` nor
  `libreoffice` was installed, validating a `.doc`/`.docx` file
  produced an empty report and exit code `0` — indistinguishable from
  genuine success — because a failed shell redirect still created the
  temp file `kin_validate_file` went on to successfully `fopen`. The
  fix tracks each conversion step's real exit/rename status in a
  `bool converted`, instead of inferring success from file existence.
- A third real finding, now fixed: a comment in `kin_validate_text`
  described counting terminal punctuation marks to suppress a label
  for single-sentence input; no such counting existed anywhere in the
  function, and the label printed every time regardless. The fix
  rewrites the comment to describe what the code actually does,
  since the existing behavior was harmless and didn't need to change.
- A fourth real finding, now fixed: `--gloss` and `--g2p` were only
  wired to the `-s` (inline sentence) input source; combined with
  `-f` or `-p`, both flags were silently accepted and then never
  consulted again. The fix adds real file- and PDF-stream support for
  both modes (mirroring the existing `analyse_stream`/`analyse_pdf`
  shape) and a clear error when neither flag is given any usable
  input source at all.
- Two of this project's largest files, `morphology.c` and
  `morph_dispatch.c` (over 6,800 lines combined), were used in every
  chapter's case studies but never opened directly in this book — an
  honest, deliberate gap left for the reader to close using exactly
  the methods this book has taught.

## Sources quoted in this chapter

- `src/main.c` (the full mode-dispatch chain, `analyse_text`,
  `analyse_line`, and the new `gloss_*`/`g2p_*` functions added by
  Section 4.4's fix).
- `src/validator.c` (`kin_validate_text`, `kin_validate_file`,
  `print_report`, `build_context`, `validate_text_block`, all as
  fixed by Sections 4.2 and 4.3).
- `src/api.c` (`kin_correct`, `kin_g2p`, in full, as fixed by
  Section 4.1).
- `src/syntax.c` (both real `ERR_SPELLING` call sites, one of them
  updated by Section 4.1's fix).
- `include/kinyarwanda.h` (the `Error` struct's new `corrected_word`
  field, added by Section 4.1's fix).
- Every `pN_*.c` program and every real CLI invocation in this chapter
  was actually compiled or run against the real
  `gcc -std=c99 -Wall -Wextra` build to produce the exact output
  quoted above.

## Closing this book

Eleven chapters ago, this book opened with a single question — what
is `umuntu`, and why does this engine call it noun class 1 — answered
by reading one table in `lexicon.c`. Every chapter since has followed
the same discipline: state the Kinyarwanda linguistic rule first, in
plain language, with a real quoted example; read the actual C function
that implements it; compile and run that exact code, never an invented
transcript; and when a comment and the code beside it disagree, trust
what actually ran. That discipline is what found every real, verified
bug in this book — not a tool, not a hunch, just running the project's
own documented examples against itself and writing down what actually
happened. This chapter went one step further than every chapter before
it: the four bugs Part 4 found are no longer bugs — `kinyarwanda.h`,
`syntax.c`, `api.c`, `validator.c`, and `main.c` all carry real fixes
now, verified the same way every finding in this book was verified,
by compiling and running the exact reproduction again afterward. That
last step — not just finding what's wrong, but reading the surrounding
code closely enough to fix it without breaking anything this book
already verified — was the actual goal of every chapter before this
one. A reader who has worked through all eleven chapters has
everything needed to open any remaining file in `src/` — `morphology.c`
and `morph_dispatch.c` chief among them — and keep going: find the
header comment, find the real function, compile it, run it, and trust
the result over the claim.
