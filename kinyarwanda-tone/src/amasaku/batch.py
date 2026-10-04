"""Batch: one recording of a whole group of words -> per-word detection vs the native marking.

    uv run amasaku-batch ~/Downloads/A.mp3 --group A
    uv run amasaku-batch ~/Downloads/*.mp3            # group = file name (A.mp3 -> A)

The recording is cut at the pauses into exactly as many pieces as the group has words (read in
list order). Each word is measured and its marking detected from its own pitch; where the list
has the speaker's marking, the two are compared (length, tone, exact). Results:
data/round1_results.tsv; clips: data/recordings/; agreed/native markings: data/gold.csv.
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
from pathlib import Path

import numpy as np
import soundfile as sf

from amasaku import record
from amasaku.marks import parse

ROOT = Path(__file__).resolve().parents[2]
LIST = ROOT / "data" / "wordlist_round1.tsv"
RESULTS = ROOT / "data" / "round1_results.tsv"


def read_list(path: Path) -> list[dict]:
    rows = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        cols = (line.split("\t") + ["", "", "", ""])[:4]
        rows.append({"plain": cols[0].strip(), "marking": cols[1].strip(), "meaning": cols[2].strip(),
                     "group": cols[3].strip()})
    return rows


def split_words(wav: np.ndarray, n: int, sr: int = record.SR) -> list[tuple[int, int]]:
    """Cut at pauses into exactly n pieces (tries several silence thresholds)."""
    import librosa

    best = None
    for top_db in range(18, 50, 2):
        segs = librosa.effects.split(wav, top_db=top_db, frame_length=1024, hop_length=160)
        merged = []
        for s, e in segs:  # pieces closer than 0.25 s belong to the same word
            if merged and s - merged[-1][1] < int(0.25 * sr):
                merged[-1] = (merged[-1][0], e)
            else:
                merged.append((s, e))
        merged = [(s, e) for s, e in merged if e - s > int(0.15 * sr)]
        if len(merged) == n:
            return merged
        if best is None or abs(len(merged) - n) < abs(len(best) - n):
            best = merged
    raise ValueError(f"found {len(best)} words in the recording but the group has {n}: "
                     f"please leave a clear pause (~1 s) between words")


def compare(gold: str, heard: str) -> tuple[bool, bool]:
    """(length agrees, tone agrees) per vowel."""
    _, g = parse(gold)
    _, h = parse(heard)
    if len(g) != len(h):
        return False, False
    length = all(a.long == b.long for a, b in zip(g, h))
    tone = all(("H" in a.moras) == ("H" in b.moras) and (not a.long or a.moras == b.moras) for a, b in zip(g, h))
    return length, tone


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("audio", nargs="+", type=Path)
    ap.add_argument("--group", help="group letter (default: the file name, e.g. A.mp3 -> A)")
    ap.add_argument("--list", type=Path, default=LIST)
    args = ap.parse_args()

    words = read_list(args.list)
    out_rows = []
    for path in args.audio:
        group = args.group or path.name.split(".")[0].upper()
        items = [w for w in words if w["group"] == group]
        if not items:
            print(f"{path.name}: no group '{group}' in {args.list}")
            continue
        x, sr = sf.read(path, dtype="float32")
        wav = record.prepare((sr, x))
        try:
            pieces = split_words(wav, len(items))
        except ValueError as e:
            print(f"{path.name}: {e}")
            continue
        clip_dir = ROOT / "data" / "recordings"
        clip_dir.mkdir(parents=True, exist_ok=True)
        print(f"\nGroup {group} ({path.name}): {len(items)} words")
        for k, (it, (s, e)) in enumerate(zip(items, pieces)):
            pad = int(0.05 * record.SR)
            clip = wav[max(0, s - pad):e + pad]
            clip_path = clip_dir / f"{group}_{k + 1:02d}_{it['plain']}.wav"
            sf.write(clip_path, clip, record.SR)
            try:
                measured = record.measure_recording(clip, it["plain"])
                heard, reasons = record.contour_suggest(clip, it["plain"], measured)
            except Exception as err:
                heard, reasons = "?", [str(err)]
            gold = it["marking"]
            length_ok, tone_ok = compare(gold, heard) if gold else (None, None)
            verdict = "" if not gold else ("✅" if heard == gold else ("~ length ok" if length_ok else "❌"))
            print(f"  {k + 1:2d}. {it['plain']:12s} {it['meaning'][:22]:22s} heard {heard:14s} "
                  f"yours {gold or '(not filled)':14s} {verdict}")
            out_rows.append({"group": group, "n": k + 1, "plain": it["plain"], "meaning": it["meaning"],
                             "yours": gold, "heard": heard, "length_agrees": length_ok, "tone_agrees": tone_ok,
                             "clip": str(clip_path.relative_to(ROOT)), "why": " | ".join(reasons)})
            if gold:
                from amasaku.workbench import append_gold
                append_gold({"time": dt.datetime.now().isoformat(timespec="seconds"), "word": it["plain"],
                             "marking": gold, "context": it["meaning"], "source": "native speaker (batch)",
                             "notes": f"tool heard {heard}", "recording": str(clip_path.relative_to(ROOT))})

    if out_rows:
        new = not RESULTS.exists()
        with RESULTS.open("a", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=list(out_rows[0]), delimiter="\t")
            if new:
                w.writeheader()
            w.writerows(out_rows)
        scored = [r for r in out_rows if r["yours"]]
        if scored:
            exact = sum(r["yours"] == r["heard"] for r in scored)
            length = sum(bool(r["length_agrees"]) for r in scored)
            tone = sum(bool(r["tone_agrees"]) for r in scored)
            print(f"\nAgreement with your markings on {len(scored)} words: exact {exact}/{len(scored)}, "
                  f"length {length}/{len(scored)}, tone {tone}/{len(scored)}")
        print(f"Details: {RESULTS}")


if __name__ == "__main__":
    main()
