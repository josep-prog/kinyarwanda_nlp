# Chapter 18 — `main.c`: `argv`/`argc`, File I/O, and the REPL Loop

## The one file outside the library

Recall Chapter 1: `main.c` is deliberately excluded from `LIB_SRCS`,
because a library must never bring its own `main()` along for the ride.
This chapter is about everything that's true *because* of that exclusion
— `main.c` is the only file in the whole project that talks to `argv`, to
a terminal, or to the filesystem. Every other file you've studied takes
clean, already-extracted strings; this one is where those strings
*come from*.

## 18.1 Parsing `argv`: the consume-next-argument idiom

```c
for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
        print_help(argv[0]);
        return 0;
    } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
        sentence = argv[++i];
    } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
        filename = argv[++i];
    }
    /* ... */
}
```

`i` starts at `1`, not `0` — `argv[0]` is always the program's own
invocation path (e.g. `./kinyarwanda_nlp`), never a real argument, so
every argument-parsing loop in C skips it by convention.

`argv[++i]` is worth reading carefully, because it does two jobs in one
expression: `++i` is **pre-increment** — it increments `i` first, and
the *result* of the whole expression is the new, incremented value. So
when the loop sees `-s`, this single line both (a) advances `i` to point
at the *next* slot in `argv` (the actual sentence text that follows the
`-s` flag) and (b) uses that new value of `i` immediately to index into
`argv` and retrieve it — all in one statement. The outer `for` loop's own
`i++` then runs as normal on the next iteration, correctly resuming
*after* both the flag and its value, never re-examining the value string
as if it might itself be another flag.

The guard `i + 1 < argc` *before* every one of these comes from exactly
the same discipline Chapter 4 taught for scanning UTF-8 byte sequences:
**never read ahead in an array without first confirming the position
you're about to read actually exists.** If `-s` were the very last
argument on the command line with nothing after it, `i + 1 < argc` would
be false, and the `else if` simply wouldn't match — falling through
instead to the final `else` branch that reports an unknown/malformed
option, rather than reading `argv[argc]`, which is out of bounds (the
only guaranteed value there is the `NULL` sentinel from Chapter 4's
discussion of `argv`'s own null-terminated-array shape — reading *past*
that would be undefined behavior).

## 18.2 Mode selection: the priority chain, one final time, at the top level

```c
if (validation_mode) { /* ... */ return 0; }
if (g2p_mode && sentence) { /* ... */ return 0; }
if (gloss_mode && sentence) { /* ... */ return 0; }
if (sentence) { analyse_line(sentence, verbose); return 0; }
if (filename) { /* ... */ return 0; }
if (pdffile) { return analyse_pdf(pdffile, verbose); }
/* falls through to interactive mode */
```

After argument parsing finishes, `main()` decides *which entire mode of
operation* to run with exactly the same shape you've now seen at every
level of this codebase: a fixed, deliberately ordered sequence of checks,
each one ending in an early `return` the moment it matches, with
whatever's left over (no flags at all) falling all the way through to a
final default behavior (interactive mode). You met this shape choosing
between noun classes (Ch.6), choosing a token's grammar tree (Ch.8),
choosing which phonological rule fires at one buffer position (Ch.10),
and choosing whether two tokens agree (Ch.12) — and here it is again,
one more time, deciding something much coarser: not "what is this word,"
but "what should this entire program do right now." The technique scales
from a single character comparison all the way up to top-level program
behavior, completely unchanged in spirit. That consistency — the same
small idea, reused at every level of granularity, rather than a different
trick invented for each new problem — is a strong, true thing to be able
to say about this codebase's overall design.

## 18.3 `analyse_text`: a second, purpose-built scanner

```c
static void analyse_text(const char *text, bool verbose) {
    char seg[MAX_LINE];
    size_t si = 0;
    for (size_t i = 0; ; i++) {
        char c = text[i];
        if (c != '\0' && si < sizeof(seg) - 1) seg[si++] = c;

        bool end     = (c == '\0');
        bool is_term = (si > 0 && (seg[si-1]=='.' || seg[si-1]=='!' || seg[si-1]=='?'));
        bool next_ok = end || text[i+1]==' ' || text[i+1]=='\n' || text[i+1]=='\0';

        if ((is_term && next_ok) || end) {
            seg[si] = '\0';
            kin_str_trim(seg);
            if (seg[0]) { analyse_line(seg, verbose); putchar('\n'); }
            si = 0;
            while (text[i+1]==' ' || text[i+1]=='\n') i++;
        }
        if (end) break;
    }
    /* ... flush any remainder with no terminating punctuation ... */
}
```

This is a *second* hand-written scanner, entirely separate from Chapter
4's `kin_tokenize()`, built to solve a different-*granularity* problem:
splitting a block of pasted, possibly multi-sentence text into
individual **sentences**, before each sentence is handed to
`kin_tokenize()` for **word**-level splitting. It reuses the named-boolean
technique from Chapter 12 (`end`, `is_term`, `next_ok` are each computed
and named separately, then combined in one readable `if`), and the same
"peek one character ahead before treating it as a boundary" discipline
from Chapter 4 (`text[i+1]` is checked, not blindly trusted, to confirm a
period is actually followed by whitespace or end-of-string and not, say,
a decimal number or abbreviation).

It's worth asking directly: why does this project have *two* separate
scanners instead of generalizing one to do both jobs? Because
sentence-splitting and word-splitting are genuinely different problems
with different boundary rules (a comma never ends a sentence but always
ends a word; a sentence-ending period needs to look at what follows it,
a word boundary mostly doesn't). Trying to force one function to handle
both would likely produce a more tangled, harder-to-verify piece of code
than two smaller, separately-readable scanners, each named for exactly
the granularity it operates at. Recognizing when *not* to unify two
similar-looking pieces of code — because their actual rules diverge
enough that forcing them together would cost more clarity than it saves —
is the same judgment call Chapter 15 raised about `scan_back_noun`,
applied here one more time.

## 18.4 `analyse_stream`: the canonical C line-by-line file-reading loop

```c
static void analyse_stream(FILE *fp, bool verbose) {
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        size_t l = strlen(line);
        if (l > 0 && line[l-1] == '\n') line[--l] = '\0';
        if (l > 0 && line[l-1] == '\r') line[--l] = '\0';
        kin_str_trim(line);
        /* ... */
        analyse_line(line, verbose);
    }
}
```

`while (fgets(line, sizeof(line), fp))` is *the* standard idiom for
reading a text file one line at a time in C. `fgets` reads up to
`sizeof(line) - 1` characters from `fp`, stopping early at a newline
(which it **keeps** in the buffer, unlike higher-level languages'
line-reading functions, which typically strip it for you), and returns
`NULL` once there's nothing left to read — which is exactly what makes it
usable directly as a `while` loop condition: the loop runs for every real
line, and stops naturally at end-of-file, with no separate
"have we reached the end" variable needed anywhere.

Because `fgets` keeps the newline, two almost-identical lines strip it
back off by hand: `if (l > 0 && line[l-1] == '\n') line[--l] = '\0';`
removes a Unix-style line ending, and the line right after it removes a
trailing `'\r'` as well — handling the case where the file uses
Windows-style `\r\n` line endings (a `\r` would otherwise be left sitting
just before where the `\n` used to be). This is `kin_str_trim`'s own
"shrink the string by writing `'\0'` earlier instead of moving bytes"
trick from Chapter 5, applied twice in a row by hand, once for each
possible line-ending convention.

## 18.5 `analyse_pdf`: the project's only call out to another program

```c
static int analyse_pdf(const char *pdfpath, bool verbose) {
    char tmpfile[512];
    snprintf(tmpfile, sizeof(tmpfile), "/tmp/kin_nlp_%d.txt", (int)getpid());

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "pdftotext -layout \"%s\" \"%s\" 2>/dev/null", pdfpath, tmpfile);

    int rc = system(cmd);
    /* ... open tmpfile, analyse_stream() it, then remove(tmpfile) ... */
}
```

This is the only place anywhere in this codebase that shells out to a
separate program (`pdftotext`, from the `poppler-utils` package) via
`system()`. Two specific things are worth naming precisely, both because
they're useful C knowledge and because being able to discuss them
honestly is a stronger defense position than not mentioning them at all.

**The temp filename uses `getpid()`** — the calling process's own process
ID — specifically so that two people running this tool against two
different PDFs *at the same time* don't both try to write to the exact
same `/tmp/kin_nlp_*.txt` file and corrupt each other's output. This is a
lightweight, practical collision-avoidance technique, not a
cryptographically airtight one — process IDs are reused over time by the
operating system, and on a shared multi-user system, files written to a
world-writable directory like `/tmp` always carry some inherent
filesystem-level risk (a different user predicting or pre-creating the
same filename) that a more security-conscious version of this function
might address with a proper unique-temp-file API instead of constructing
the name by hand. For a CLI tool a user runs against their own local
files, that risk is low — but if this function were ever wired up behind
a network-facing service accepting untrusted PDF paths, this exact code
is where you'd want to revisit that assumption.

**The command string is built with `snprintf` and `pdfpath` is
interpolated directly into it**, wrapped only in double quotes. If
`pdfpath` ever contained a double-quote character itself, or shell
metacharacters a quoted string doesn't neutralize, the resulting string
handed to `system()` (which executes it through `/bin/sh`) could behave
differently than intended — this is the general shape of a **command
injection** risk. In this specific program, `pdfpath` comes from `argv`
on the same machine the user is already running the program on — they
already have a shell, so there's no meaningful privilege boundary being
crossed by this particular path. But the *pattern* — building a shell
command string by directly concatenating external input into it — is
exactly the pattern that becomes a genuine vulnerability the moment the
input source changes (a filename uploaded through a web form, for
instance, rather than typed by the same person running the program).
Knowing precisely *why* a pattern is safe in its current, specific
context, and exactly what would make it stop being safe, is a much
stronger answer than either "this is fine" or "this is dangerous" stated
without the reasoning behind it.

## 18.6 The REPL: `fgets` against `stdin`, with bilingual exit commands

```c
char line[MAX_LINE];
while (1) {
    printf(">>> ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) break;
    /* ... strip \n, \r ... */
    if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0 ||
        strcmp(line, "urabeho") == 0) break;
    if (strcmp(line, "help") == 0 || strcmp(line, "ubufasha") == 0) {
        print_help(argv[0]); continue;
    }
    /* ... otherwise treat line as real input to analyse ... */
}
```

This is Section 18.4's `fgets`-loop idiom again, now reading from
`stdin` (the terminal) instead of a file — the *exact same* "loop while
`fgets` keeps succeeding, stop naturally when it returns `NULL`" shape,
which here doubles as the REPL's natural exit path on `Ctrl+D` (which
sends end-of-file on a Unix terminal, making `fgets` return `NULL` just
as it would at the end of a real file). `fflush(stdout)` before reading
input is necessary because `printf("> >> ")` doesn't guarantee its output
appears on screen immediately — output to a terminal is often
**line-buffered**, meaning it can sit in an internal buffer until a
newline is printed or the buffer is explicitly flushed; since the prompt
`">>> "` has no trailing newline, `fflush` forces it to appear before the
program then blocks waiting for the user's input. Bilingual command
matching (`"quit"`/`"exit"`/`"urabeho"`, `"help"`/`"ubufasha"`) is a small
but real usability decision — letting users exit in either English or
Kinyarwanda, an appropriate touch for a tool whose entire output is
already bilingual.

## Key takeaways

- `argv[++i]` advances the loop index and retrieves the next argument in
  one expression — always guarded by an explicit `i + 1 < argc` bounds
  check first, the same "never read ahead without confirming the
  position exists" discipline from Chapter 4.
- The ordered-priority-chain pattern you've now seen choosing noun
  classes, grammar trees, phonological rules, and token agreement also
  governs which *entire program mode* runs — the same small idea, reused
  at every level of granularity in this codebase.
- Two scanners with a similar *style* but different *granularity*
  (sentence-splitting here, word-splitting in Ch.4) are kept separate
  deliberately, because unifying them would cost more clarity than it
  would save.
- `while (fgets(buf, size, fp))` is the standard C idiom for reading a
  file (or `stdin`) line by line, terminating naturally on end-of-file;
  remember `fgets` keeps the newline, so stripping it (and any preceding
  `\r`) is a manual, explicit step.
- `system()` building a shell command from interpolated external input is
  a real, named risk pattern (command injection) — safe or not depends
  entirely on whether that input could ever come from an untrusted
  source, not on whether the code "looks fine" in isolation.
- Terminal output is commonly line-buffered; `fflush(stdout)` is needed
  to force a prompt without a trailing newline to actually appear before
  the program blocks waiting for input.

## Search YouTube for

- "argv argc command line parsing in C"
- "fgets explained reading lines in C"
- "stdout buffering and fflush explained"
- "command injection vulnerability explained"
- "REPL loop design pattern"

## Coming up in Chapter 19

Parts I–IV of this book covered every source file. Part III now turns to
the tools around the code: Chapter 19 returns to the Makefile from
Chapter 1, this time going deep — object files, `.d` dependency files,
the difference between building `kinyarwanda_nlp` (the CLI),
`libkinyarwanda.a` (the static library), and `libkinyarwanda.so` (the
shared library) from the *exact same* source files, and why `-fPIC`
matters specifically for the shared-library build.
