"""A native speaker's own recording: measure it, suggest its marking, and copy its melody.

The same measurement used on Mwanafunzi (voice-model alignment + Praat/pYIN pitch) is run on
the recording, so both can be compared vowel by vowel.
"""

from __future__ import annotations

import numpy as np

from amasaku.marks import Vowel, parse, write

SR = 16000
_aligner = None


def aligner():
    global _aligner
    if _aligner is None:
        from amasaku.align import VitsAligner
        _aligner = VitsAligner()
    return _aligner


def prepare(audio: tuple[int, np.ndarray]) -> np.ndarray:
    """Gradio (sr, samples) -> 16 kHz mono float32, silence trimmed, peak-normalized."""
    import librosa

    sr, x = audio
    x = np.asarray(x, dtype=np.float32)
    if x.ndim > 1:
        x = x.mean(axis=1)
    if np.abs(x).max() > 1.5:  # int16 range
        x = x / 32768.0
    if sr != SR:
        x = librosa.resample(x, orig_sr=sr, target_sr=SR)
    x, _ = librosa.effects.trim(x, top_db=35)
    return (x / (np.abs(x).max() + 1e-9) * 0.9).astype(np.float32)


def measure_recording(wav: np.ndarray, text: str) -> list[dict]:
    """Per-vowel length (s) and pitch (semitones vs the recording's own middle), halves for 2 moras."""
    from amasaku.align import measure

    plain, _ = parse(text)
    letters = aligner().align(wav, plain)
    return measure(wav, letters, plain.split())


def suggest(text: str, measured: list[dict], long_ratio: float = 1.6, high_st: float = 1.5) -> str:
    """Marking heard in the recording: a vowel is long if it lasts > long_ratio x the median vowel
    (the last vowel, lengthened before a pause, is not counted), and a mora is High if its pitch is
    > high_st above the recording's falling trend line."""
    plain, _ = parse(text)
    if not measured:
        return plain
    durs = np.array([v["dur"] for v in measured])
    med = np.median(durs[:-1]) if len(durs) > 2 else np.median(durs)
    t = np.array([v["start"] for v in measured])
    p = np.array([np.nan if v["pitch"] is None else v["pitch"] for v in measured])
    ok = ~np.isnan(p)
    trend = np.polyval(np.polyfit(t[ok], p[ok], 1), t) if ok.sum() >= 3 else np.full_like(t, np.nanmean(p))
    out = []
    for k, v in enumerate(measured):
        last = k == len(measured) - 1
        is_long = not last and v["dur"] > long_ratio * med
        halves = [v["pitch_1"], v["pitch_2"]] if is_long else [v["pitch"]]
        moras = ["H" if h is not None and h - trend[k] > high_st else "L" for h in halves]
        out.append(Vowel(v["ch"], moras))
    return write(plain, out)


def melody_of(measured: list[dict]) -> tuple[list[float], list[list[tuple[float, float]]]]:
    """Relative vowel lengths and pitch targets of the recording, ready to impose on the voice."""
    durs = np.array([v["dur"] for v in measured])
    rel = durs / (np.median(durs) + 1e-9)
    targets = []
    for v in measured:
        pts = []
        if v["pitch_1"] is not None:
            pts.append((0.3, v["pitch_1"]))
        if v["pitch_2"] is not None:
            pts.append((0.75, v["pitch_2"]))
        if not pts and v["pitch"] is not None:
            pts.append((0.5, v["pitch"]))
        targets.append(pts)
    return list(rel), targets


def say_like(speaker, text: str, measured: list[dict], settings) -> np.ndarray:
    """Mwanafunzi's voice with the recording's vowel lengths and pitch movement."""
    plain, vowels = parse(text)
    if len(measured) != len(vowels):
        raise ValueError("recording and text do not have the same vowels")
    base, info = speaker.say(plain, settings)                    # the voice's own timing first
    own = np.array([b - a for a, b in info["spans"]])
    own_rel = own / (np.median(own) + 1e-9)
    rel, targets = melody_of(measured)
    factors = [float(np.clip(r / o, 0.5, 3.0)) for r, o in zip(rel, own_rel)]
    wav, _ = speaker.say(plain, settings, length_factors=factors, targets=targets)
    return wav


def bars_svg(rows: list[tuple[str, list[dict], str]]) -> str:
    """Vowel-by-vowel comparison: each row is (label, measured vowels, colour)."""
    W, rowh = 720, 120
    parts = [f'<svg viewBox="0 0 {W} {rowh * len(rows) + 10}" style="width:100%;height:auto" '
             f'role="img" aria-label="vowel length and pitch comparison">']
    for r, (label, vs, colour) in enumerate(rows):
        if not vs:
            continue
        y0 = r * rowh + 10
        t0 = vs[0]["start"]
        t1 = vs[-1]["start"] + vs[-1]["dur"]
        x = lambda t: 110 + (t - t0) / max(t1 - t0, 1e-3) * (W - 120)
        y = lambda st: y0 + 60 - max(-8, min(8, st)) * 6
        parts.append(f'<text x="4" y="{y0 + 64}" font-size="13" fill="currentColor">{label}</text>')
        parts.append(f'<line x1="110" x2="{W - 10}" y1="{y0 + 60}" y2="{y0 + 60}" stroke="currentColor" opacity=".2"/>')
        for v in vs:
            if v["pitch"] is None:
                continue
            x0, x1 = x(v["start"]), x(v["start"] + v["dur"])
            p1 = v["pitch_1"] if v["pitch_1"] is not None else v["pitch"]
            p2 = v["pitch_2"] if v["pitch_2"] is not None else v["pitch"]
            parts.append(f'<line x1="{x0:.1f}" y1="{y(p1):.1f}" x2="{max(x1 - 2, x0 + 2):.1f}" y2="{y(p2):.1f}" '
                         f'stroke="{colour}" stroke-width="7" stroke-linecap="round">'
                         f'<title>{v["ch"]}: {v["dur"] * 1000:.0f} ms, {p1:+.1f} -> {p2:+.1f} st</title></line>')
            parts.append(f'<text x="{(x0 + x1) / 2:.1f}" y="{y0 + 112}" font-size="12" text-anchor="middle" '
                         f'fill="currentColor" opacity=".7">{v["ch"]}</text>')
    parts.append("</svg>")
    return "".join(parts)


def contour_suggest(wav: np.ndarray, text: str, measured: list[dict], rise_st: float = 1.0) -> tuple[str, list[str]]:
    """Marking from where the pitch peaks inside each word's loudest stretch (its main vowel).

    Long vowels carry contours (Jarnow; Myers 2003): a peak early then a fall = H on the first
    mora (áa); a rise to a late peak = H on the second (aá). A level nucleus clearly above the
    word's other vowels = short High. Works on any voice: it needs loudness and pitch only, the
    aligner just says which vowel the loud stretch belongs to. Returns (marking, explanations)."""
    import parselmouth

    plain, _ = parse(text)
    snd = parselmouth.Sound(wav.astype(np.float64), sampling_frequency=SR)
    pitch, inten = snd.to_pitch(0.01, 60, 400), snd.to_intensity(75, 0.01)
    times = np.arange(0, snd.duration, 0.01)
    f0 = np.array([pitch.get_value_at_time(t) for t in times])
    db = np.array([inten.get_value(t) for t in times])
    vowels = [Vowel(v["ch"], ["L"]) for v in measured]
    notes = []
    words = sorted({v["word"] for v in measured})
    for wi in words:
        idx = [k for k, v in enumerate(measured) if v["word"] == wi]
        a = measured[idx[0]]["start"]
        b = measured[idx[-1]]["start"] + measured[idx[-1]]["dur"]
        sel = (times >= a) & (times <= b) & ~np.isnan(db)
        if sel.sum() < 5:
            continue
        loud = sel & (db >= np.nanmax(db[sel]) - 3)               # the word's loudest stretch
        voiced = loud & ~np.isnan(f0)
        if voiced.sum() < 3:
            continue
        t_loud, f_loud = times[voiced], f0[voiced]
        center = t_loud.mean()
        k = min(idx, key=lambda j: abs(measured[j]["start"] + measured[j]["dur"] / 2 - center))
        st = 12 * np.log2(f_loud / f_loud.min())
        peak = (t_loud[np.argmax(f_loud)] - t_loud[0]) / max(t_loud[-1] - t_loud[0], 1e-3)
        fall_after = st.max() - st[-1]
        rise_before = st.max() - st[0]
        word = plain.split()[wi]
        if peak <= 0.35 and fall_after >= rise_st:
            vowels[k] = Vowel(measured[k]["ch"], ["H", "L"])
            notes.append(f"{word}: pitch peaks early in the '{measured[k]['ch']}' then falls "
                         f"{fall_after:.1f} st -> long, falling (áa)")
        elif peak >= 0.65 and rise_before >= rise_st:
            vowels[k] = Vowel(measured[k]["ch"], ["L", "H"])
            notes.append(f"{word}: pitch rises {rise_before:.1f} st to a late peak in the '{measured[k]['ch']}' "
                         f"-> long, rising (aá)")
        else:
            others = [measured[j]["pitch"] for j in idx if j != k and measured[j]["pitch"] is not None]
            mine = measured[k]["pitch"]
            if mine is not None and others and mine - max(others) > 1.5:
                vowels[k] = Vowel(measured[k]["ch"], ["H"])
                notes.append(f"{word}: '{measured[k]['ch']}' level and higher than the rest -> short high")
            else:
                notes.append(f"{word}: no clear peak in its main vowel (peak at {peak:.0%}) -> left low")
    return write(plain, vowels), notes
