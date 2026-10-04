"""Do the same words get the same melody and length every time the speaker says them?

    uv run python -m amasaku.consistency --clips 800

Runs the voice-model aligner + pitch on training clips (Mwanafunzi documentaries), then for
every word said >= 10 times (not phrase-final) compares each vowel's relative pitch and
relative duration across occurrences. High consistency = the measurement captures a real,
word-specific melody (lexical tone/length) rather than noise.
"""

from __future__ import annotations

import argparse
import csv
import io
import json
import zipfile
from collections import defaultdict
from pathlib import Path

import numpy as np
import soundfile as sf

from amasaku.align import ROOT, TTS, VitsAligner, measure

DATA = TTS / "data" / "prepared_v2"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--clips", type=int, default=800)
    args = ap.parse_args()
    rows = [r for r in csv.DictReader(open(DATA / "metadata.csv"), delimiter="|") if "MWANAFUNZI" in r["channel"]]
    rows = rows[: args.clips]
    z = zipfile.ZipFile(DATA / "prepared_dataset.zip")
    al = VitsAligner()
    occ = defaultdict(list)  # word -> list of (rel pitch per vowel, rel duration per vowel)
    out = []
    for k, r in enumerate(rows):
        wav, _ = sf.read(io.BytesIO(z.read(f"wavs/{r['id']}.wav")), dtype="float32")
        words = r["text"].split()
        vowels = measure(wav, al.align(wav, r["text"]), words)
        out.append({"id": r["id"], "text": r["text"], "vowels": vowels})
        rate = np.median([v["dur"] for v in vowels]) if vowels else 1
        last = len(words) - 1
        by_word = defaultdict(list)
        for v in vowels:
            by_word[v["word"]].append(v)
        for wi, vs in by_word.items():
            if wi == last or any(v["pitch"] is None for v in vs):
                continue
            p = np.array([v["pitch"] for v in vs])
            occ[words[wi]].append((p - p.mean(), np.array([v["dur"] for v in vs]) / rate))
        if k % 100 == 0:
            print(f"  {k}/{len(rows)} clips", flush=True)
    (ROOT / "out" / "train_aligned.json").write_text(json.dumps(out, ensure_ascii=False))

    def consistency(arrays):
        """Share of variance explained by vowel position within the word (0 = noise, 1 = fixed pattern)."""
        a = np.stack(arrays)
        total = a.var()
        within = a.var(axis=0).mean()
        return 1 - within / total if total > 0 else 0.0

    rng = np.random.default_rng(0)
    res = []
    for w, lst in occ.items():
        if len(lst) < 10 or len(lst[0][0]) < 2:
            continue
        lst = [x for x in lst if len(x[0]) == len(lst[0][0])]
        pitch_c = consistency([x[0] for x in lst])
        dur_c = consistency([x[1] for x in lst])
        # chance level: same arrays with vowel order shuffled per occurrence
        shuffled = [rng.permutation(x[0]) for x in lst]
        res.append((w, len(lst), pitch_c, consistency(shuffled), dur_c,
                    np.round(np.mean([x[0] for x in lst], 0), 1), np.round(np.mean([x[1] for x in lst], 0), 2)))
    res.sort(key=lambda r: -r[1])
    pc, ch, dc = np.mean([r[2] for r in res]), np.mean([r[3] for r in res]), np.mean([r[4] for r in res])
    print(f"\n{len(res)} words said >= 10 times. Pitch-pattern consistency {pc:.2f} (chance {ch:.2f}); "
          f"duration-pattern consistency {dc:.2f}")
    print(f"{'word':16s} {'n':>4s} {'pitch':>6s} {'chance':>7s} {'length':>7s}  mean pitch per vowel (st) | mean length per vowel (x)")
    for w, n, p, c, d, mp, md in res[:25]:
        print(f"{w:16s} {n:4d} {p:6.2f} {c:7.2f} {d:7.2f}  {mp} | {md}")


if __name__ == "__main__":
    main()
