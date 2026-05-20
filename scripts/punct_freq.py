#!/usr/bin/env python3
"""
punct_freq.py — Corpus-driven punctuation trigger analysis for Kinyarwanda.

Usage:
  python3 scripts/punct_freq.py data/kinyarwanda_biblia.txt
  python3 scripts/punct_freq.py data/kinyarwanda_biblia.txt --mark !
  python3 scripts/punct_freq.py data/kinyarwanda_biblia.txt --mark , --top 30

What it does:
  For each sentence-ending mark (?, !, .), it finds which words appear
  significantly more often in sentences ending with that mark than in other
  sentences.  These are the "trigger" words you should add to the
  is_interrogative() / is_exclamative() functions in punctuation.c.

Output columns:
  word        — lowercased word form
  count       — occurrences in sentences ending with --mark
  ratio       — how many times more frequent vs. other sentences
                (ratio > 10x = strong trigger, 3–10x = moderate)
  coverage%   — what % of all --mark sentences contain this word
"""

import re
import sys
import argparse
from collections import Counter


def clean_tokens(s: str) -> list[str]:
    s = re.sub(r"[«»\"\"''""]+", " ", s)
    s = re.sub(r"(?<!\w)\d+(?!\w)", " ", s)
    return [
        w.lower().strip(".,;:!?() \t")
        for w in s.split()
        if len(w) > 1 and not w.strip(".,;:!?() \t").isdigit()
    ]


def split_sentences(text: str) -> list[str]:
    text = re.sub(r"(?<!\w)\d+(?!\w)", " ", text)
    return [s.strip() for s in re.split(r"(?<=[.?!])\s+", text) if s.strip()]


def analyse(corpus_path: str, mark: str, top: int, min_count: int) -> None:
    with open(corpus_path, encoding="utf-8") as f:
        text = f.read()

    sentences = split_sentences(text)
    target = [s for s in sentences if s.endswith(mark)]
    other  = [s for s in sentences if not s.endswith(mark) and len(s) > 8]

    print(f"\nCorpus : {corpus_path}")
    print(f"Mark   : '{mark}'")
    print(f"Target sentences : {len(target):,}")
    print(f"Other  sentences : {len(other):,}")

    t_words = Counter(w for s in target for w in clean_tokens(s))
    o_words = Counter(w for s in other  for w in clean_tokens(s))
    t_total = max(sum(t_words.values()), 1)
    o_total = max(sum(o_words.values()), 1)

    scored = []
    for w, tc in t_words.items():
        if tc < min_count or len(w) < 2:
            continue
        oc = o_words.get(w, 0)
        ratio = (tc / t_total) / ((oc + 0.5) / o_total)
        cov   = tc / len(target) * 100
        scored.append((ratio, tc, cov, w))
    scored.sort(reverse=True)

    print(f"\n{'word':22s} {'count':>7s} {'ratio':>8s} {'coverage':>9s}")
    print("-" * 52)
    shown = 0
    for ratio, tc, cov, w in scored:
        if any(c.isdigit() for c in w):
            continue
        print(f"  {w:20s} {tc:7d} {ratio:7.1f}x  {cov:7.2f}%")
        shown += 1
        if shown >= top:
            break

    # Coverage sweep for a candidate list the user can verify
    print(f"\nSingle-word coverage sweep (how many '{mark}' sentences contain each):")
    candidates = [w for _, _, _, w in scored[:top] if not any(c.isdigit() for c in w)]
    if candidates:
        cumulative = set()
        print(f"  {'word':20s}  {'alone':>6s}  {'cumulative':>12s}")
        print("  " + "-" * 44)
        for w in candidates[:20]:
            alone = sum(1 for s in target if w in clean_tokens(s))
            cumulative.update(
                i for i, s in enumerate(target) if w in clean_tokens(s)
            )
            pct_alone = alone / len(target) * 100
            pct_cum   = len(cumulative) / len(target) * 100
            print(f"  {w:20s}  {pct_alone:5.1f}%  {pct_cum:11.1f}%")

    print()


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("corpus", help="Path to plain-text corpus file")
    ap.add_argument("--mark", default="?",
                    choices=["?", "!", "."],
                    help="Terminal punctuation to analyse (default: ?)")
    ap.add_argument("--top",  type=int, default=40,
                    help="Number of top trigger words to show (default: 40)")
    ap.add_argument("--min",  type=int, default=4,
                    help="Minimum occurrences to include a word (default: 4)")
    args = ap.parse_args()
    analyse(args.corpus, args.mark, args.top, args.min)


if __name__ == "__main__":
    main()
