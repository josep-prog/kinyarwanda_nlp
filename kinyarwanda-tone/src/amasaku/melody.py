"""Read a word's marking from its melody, the way REB describes how a marking is SAID.

The written marking is not the heard melody. REB (Y2 TTC teacher guide, p.114, p.127):
  - integuza: the syllable before a high is also said high, but not written:
    umusôre is said [ù-mû-sô-rè], umugorê [ù-mù-gô-rê];
  - a long vowel is said falling (âa), rising (aâ), low (aa) or high (âa before another high);
and a word said alone ends low (Jarnow 2020: no high utterance-finally), so a final high is heard
only through the integuza on the vowel before it.

So instead of guessing marks from pitch, every possible marking of the word is turned into the
melody it predicts (analysis by synthesis), and the marking whose predicted melody best fits the
recording wins. Length rules (word-initial/final short, long before NC and after CG) limit the
candidates. The few settings (how much duration counts, how unlikely each extra high is) are
fitted on the standard words only; the documented words are kept for testing.
"""

from __future__ import annotations

import itertools
from dataclasses import dataclass

import numpy as np

from amasaku.anchors import forced_length
from amasaku.marks import Vowel, parse, write

SHORT = [["L"], ["H"]]
LONG = [["L", "L"], ["H", "L"], ["L", "H"], ["H", "H"]]


@dataclass
class Settings:
    w_dur: float = 1.0     # weight of the duration fit against the pitch fit
    high_st: float = 3.0   # a high is said this many semitones above the speaker's low (fixed per speaker)
    long_log: float = 0.5  # a long vowel lasts exp(long_log) times a short one (fixed per speaker)
    integuza: float = 0.5  # how high the anticipation high is said, as a share of a real high
    h_cost: float = 0.15   # prior cost of each written high (fewer highs unless the melody needs them)
    max_highs: int = 3


def candidates(plain: str, max_highs: int = 3) -> list[list[list[str]]]:
    """Every marking allowed by the length rules, with at most `max_highs` high vowels."""
    rules = [n for word in plain.split() for n in forced_length(word)]
    options = [SHORT if n == 1 else LONG if n == 2 else SHORT + LONG for n in rules]
    out = []
    for combo in itertools.product(*options):
        if sum("H" in m for m in combo) <= max_highs:
            out.append([list(m) for m in combo])
    return out


def surface(marking: list[list[str]], integuza: float = 0.5) -> list[list[float]]:
    """The heard level of each mora of a written marking, said alone: 1 high, 0 low, and the
    anticipation high in between (it is a smaller rise than a real high in the recordings)."""
    lv = [[1.0 if m == "H" else 0.0 for m in v] for v in marking]
    for i, v in enumerate(marking):
        if v[0] == "H" and i > 0:                      # integuza on the vowel before
            lv[i - 1] = [max(x, integuza) for x in lv[i - 1]]
    lv[-1][-1] = 0.0                                   # a word said alone ends low
    return lv


def _observed(measured: list[dict]) -> tuple[list[list[float]], np.ndarray]:
    """Per vowel its two halves' pitch (semitones), and log duration."""
    halves = []
    for x in measured:
        a = x["pitch_1"] if x["pitch_1"] is not None else x["pitch"]
        b = x["pitch_2"] if x["pitch_2"] is not None else x["pitch"]
        halves.append([np.nan if a is None else float(np.clip(a, -8, 8)),   # clip octave errors
                       np.nan if b is None else float(np.clip(b, -8, 8))])
    durs = np.log(np.array([max(x["dur"], 0.01) for x in measured]))
    return halves, durs


def fit_cost(marking: list[list[str]], halves, durs, s: Settings) -> float:
    lv = surface(marking, s.integuza)
    pred, obs, idx = [], [], []
    for i, (levels, (a, b)) in enumerate(zip(lv, halves)):
        pts = [levels[0], levels[-1]]                  # short vowel: one level over both halves
        for p, o in zip(pts, (a, b)):
            if not np.isnan(o):
                pred.append(p), obs.append(o), idx.append(i)
    if len(obs) < 3:
        return np.inf
    pred, obs, idx = np.array(pred), np.array(obs), np.array(idx, float)
    last = (idx == len(marking) - 1).astype(float)
    # the high step is the speaker's own (fixed); per word only level, declination and final drop are fitted
    resid = obs - s.high_st * pred
    X = np.column_stack([np.ones_like(pred), idx, last])
    coef, *_ = np.linalg.lstsq(X, resid, rcond=None)
    pitch_err = float(np.mean((resid - X @ coef) ** 2))
    # duration: long vowels last exp(long_log) times longer. First and last vowels are left out:
    # both are short by rule, the first often has no consonant and the last is lengthened.
    longs = np.array([len(m) == 2 for m in marking], float)[1:-1]
    d = durs[1:-1] - s.long_log * longs
    dur_err = float(np.var(d)) if len(d) >= 2 else 0.0
    highs = sum("H" in m for m in marking)
    return pitch_err + s.w_dur * dur_err * 10 + s.h_cost * highs


def decode(plain: str, measured: list[dict], s: Settings = Settings(), top: int = 3) -> list[tuple[str, float]]:
    """The best markings for a recording, best first, with their cost."""
    _, target = parse(plain)
    if len(measured) != len(target):
        return []
    halves, durs = _observed(measured)
    scored = []
    for m in candidates(plain, s.max_highs):
        c = fit_cost(m, halves, durs, s)
        if np.isfinite(c):
            scored.append((c, m))
    scored.sort(key=lambda x: x[0])
    return [(write(plain, [Vowel(v.letter, mo) for v, mo in zip(target, m)]), round(c, 3)) for c, m in scored[:top]]


def measure_nuclei(wav: np.ndarray, plain: str, sr: int = 16000) -> list[dict]:
    """Vowels found as loudness peaks (syllable nuclei), for any speaker, no aligner needed.

    A word with n vowels has n loudness peaks; the n most prominent are taken (at least 60 ms
    apart), the word is cut at the dips between them, and each vowel's pitch is read over the
    loud part of its stretch (within 6 dB of its peak), in two halves. Same fields as
    record.measure_recording, so the decoder can use either."""
    import parselmouth

    _, target = parse(plain)
    n = len(target)
    snd = parselmouth.Sound(wav.astype(np.float64), sampling_frequency=sr)
    inten = snd.to_intensity(100, 0.005)
    t = inten.xs()
    db = inten.values[0]
    db = np.convolve(db, np.ones(5) / 5, mode="same")
    pitch = snd.to_pitch(0.005, 60, 400)
    f0 = np.array([pitch.get_value_at_time(x) for x in t])
    voiced = ~np.isnan(f0)
    if voiced.sum() < 5:
        return []
    # candidate peaks: local maxima in voiced frames, ranked by prominence
    peaks = [i for i in range(1, len(db) - 1) if db[i] >= db[i - 1] and db[i] > db[i + 1] and voiced[i]]
    def prominence(i):
        left = db[max(0, i - 40):i + 1].min()
        right = db[i:i + 41].min()
        return db[i] - max(left, right)
    chosen = []
    for i in sorted(peaks, key=prominence, reverse=True):
        if all(abs(t[i] - t[j]) >= 0.06 for j in chosen):
            chosen.append(i)
        if len(chosen) == n:
            break
    if len(chosen) != n:
        return []
    chosen.sort()
    vt = np.where(voiced)[0]
    bounds = [vt[0]] + [chosen[k] + int(np.argmin(db[chosen[k]:chosen[k + 1] + 1])) for k in range(n - 1)] + [vt[-1]]
    ref = np.nanmedian(f0[voiced])
    st = 12 * np.log2(f0 / ref)
    out = []
    for k in range(n):
        a, b = bounds[k], bounds[k + 1]
        seg = np.arange(a, b + 1)
        loud = seg[(db[seg] >= db[chosen[k]] - 6) & voiced[seg]]
        if len(loud) == 0:
            loud = seg[voiced[seg]]
        vals = st[loud] if len(loud) else np.array([np.nan])
        half = max(1, len(vals) // 2)
        med = lambda x: None if len(x) == 0 or np.all(np.isnan(x)) else round(float(np.nanmedian(x)), 2)
        out.append({"word": 0, "ch": target[k].letter, "start": round(float(t[a]), 3),
                    "dur": round(float(t[b] - t[a]), 3), "pitch": med(vals), "pitch_1": med(vals[:half]),
                    "pitch_2": med(vals[half:])})
    return out
