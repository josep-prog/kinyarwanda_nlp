"""Melody viewer: hear the real sentence and see each vowel's measured pitch and length.

    uv run python -m amasaku.view            # out/aligned.json -> out/view.html

Each vowel is a bar: higher = higher pitch (semitones vs the speaker's middle), wider = longer.
A dashed line shows the sentence's downward drift (downstep/declination). Vowels clearly above
that line are candidate High tones (shown as á); clearly long vowels (not phrase-final) are
shown doubled (aa). These are first-pass guesses for a native speaker to judge.
"""

from __future__ import annotations

import html
import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
ACUTE = dict(a="á", e="é", i="í", o="ó", u="ú")


def tentative(vowels: list[dict]) -> list[str]:
    """First-pass marking: H if above the sentence's drift line by >1.5 st; long if > 1.6x the median."""
    t = np.array([v["start"] for v in vowels])
    p = np.array([np.nan if v["pitch"] is None else v["pitch"] for v in vowels])
    ok = ~np.isnan(p)
    line = np.polyval(np.polyfit(t[ok], p[ok], 1), t) if ok.sum() > 2 else np.zeros_like(t)
    med = np.median([v["dur"] for v in vowels])
    marks = []
    for v, pi, li in zip(vowels, p, line):
        ch = v["ch"]
        long_ = v["dur"] > 1.6 * med and "phrase-final" not in v["context"]
        high = ok.any() and not np.isnan(pi) and pi - li > 1.5
        s = (ch + ch) if long_ else ch
        if high:
            s = ACUTE[ch] + (ch if long_ else "")
        marks.append(s)
    return marks, line


def svg(vowels: list[dict], line: np.ndarray, marks: list[str]) -> str:
    if not vowels:
        return ""
    t0, t1 = vowels[0]["start"], vowels[-1]["start"] + vowels[-1]["dur"]
    W, H, lo, hi = 760, 180, -8, 8
    x = lambda t: 10 + (t - t0) / max(t1 - t0, 1e-3) * (W - 20)
    y = lambda st: H - 20 - (min(max(st, lo), hi) - lo) / (hi - lo) * (H - 40)
    parts = [f'<svg viewBox="0 0 {W} {H}" role="img" aria-label="vowel pitch and length">',
             f'<line x1="10" x2="{W-10}" y1="{y(0)}" y2="{y(0)}" class="zero"/>']
    pts = " ".join(f"{x(v['start']):.1f},{y(l):.1f}" for v, l in zip(vowels, line))
    parts.append(f'<polyline points="{pts}" class="drift"/>')
    for v, m in zip(vowels, marks):
        if v["pitch"] is None:
            continue
        cls = "hi" if any(c in m for c in "áéíóú") else "lo"
        x0, x1 = x(v["start"]), x(v["start"] + v["dur"])
        parts.append(f'<rect x="{x0:.1f}" y="{y(v["pitch"]) - 4:.1f}" width="{max(x1 - x0 - 1, 2):.1f}" height="8" '
                     f'rx="2" class="{cls}"><title>{html.escape(v["w"])}: {html.escape(m)} '
                     f'{v["dur"]*1000:.0f} ms, {v["pitch"]:+.1f} st</title></rect>')
        parts.append(f'<text x="{(x0 + x1) / 2:.1f}" y="{H - 4}" class="lbl">{html.escape(m)}</text>')
    parts.append("</svg>")
    return "".join(parts)


def main() -> None:
    data = json.loads((ROOT / "out" / "aligned.json").read_text())
    cards = []
    for r in data:
        vowels = r["vowels"]
        marks, line = tentative(vowels)
        # rebuild the sentence with marked vowels
        words, k = r["text"].split(), 0
        marked = []
        for wi, w in enumerate(words):
            out = ""
            for ch in w:
                if ch in "aeiou" and k < len(vowels) and vowels[k]["word"] == wi:
                    out += marks[k]
                    k += 1
                else:
                    out += ch
            marked.append(out)
        cards.append(f'<section><p class="plain">{html.escape(r["text"])}</p>'
                     f'<p class="marked">{html.escape(" ".join(marked))}</p>'
                     f'<audio controls preload="none" src="file://{html.escape(r["wav"])}"></audio>'
                     f'{svg(vowels, line, marks)}</section>')
    page = f"""<!doctype html><html lang="rw"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1"><title>Amasaku Viewer</title>
<style>
:root {{ --bg:#fafaf7; --fg:#1d1d1b; --muted:#6b6b66; --card:#fff; --line:#e3e2dc; --hi:#c0392b; --lo:#2e6f9e; }}
@media (prefers-color-scheme: dark) {{ :root {{ --bg:#161615; --fg:#ecebe6; --muted:#9a9993; --card:#1f1f1d; --line:#33332f; --hi:#f07a6a; --lo:#7fb6dc; }} }}
body {{ margin:0; background:var(--bg); color:var(--fg); font:16px/1.5 system-ui, sans-serif; }}
main {{ max-width:820px; margin:0 auto; padding:20px 16px 60px; }}
section {{ background:var(--card); border:1px solid var(--line); border-radius:10px; padding:14px; margin:14px 0; }}
.plain {{ color:var(--muted); margin:0 0 4px; }} .marked {{ font-size:1.15rem; margin:0 0 8px; }}
audio {{ width:100%; height:34px; }} svg {{ width:100%; height:auto; }}
.zero {{ stroke:var(--line); }} .drift {{ fill:none; stroke:var(--muted); stroke-dasharray:4 4; }}
.hi {{ fill:var(--hi); }} .lo {{ fill:var(--lo); }} .lbl {{ font-size:11px; fill:var(--muted); text-anchor:middle; }}
</style></head><body><main>
<h1>Amasaku n'ubutinde: measured on Mwanafunzi</h1>
<p>First-pass guesses: <span style="color:var(--hi)">red = above the sentence's falling line (candidate High, á)</span>,
<span style="color:var(--lo)">blue = not high</span>, wider bar = longer vowel (doubled, aa). Hover a bar for numbers.
Please tell us which words are right and which are wrong.</p>
{''.join(cards)}</main></body></html>"""
    (ROOT / "out" / "view.html").write_text(page, encoding="utf-8")
    print(f"{len(cards)} sentences -> {ROOT / 'out' / 'view.html'}")


if __name__ == "__main__":
    main()
