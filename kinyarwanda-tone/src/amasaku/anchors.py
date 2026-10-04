"""Reference words (amagambo fatizo): the school method of finding a word's marking by comparison.

A target "sounds like" a reference word when they carry the same melody counted from the END
of the word, so the reference's vowel patterns are copied onto the target's vowels from the
right: umwáana -> inzara = inzáara, umugoré -> isi = isí, umusóre -> matama = matáma. This is
what the REB families show (Y2 teacher guide p.116): right-edge copying reproduces 67 of the 67
family members, lining up stems from the left only 54. A hyphen in the target (gu-hiingiisha)
lines up stems instead, for comparing a root with a root. Then the length rules that need no comparison are applied
(Myers 2005): word-initial and word-final vowels are short, vowels before a nasal+consonant or
after a consonant+glide are long.

With audio, the target's pitch contour is compared with the speaker's own recordings of the
reference words (same voice, so no fixed thresholds), and the closest ones are suggested.
Each reference word has two recordings, as practised at school: said normally, and said slowly
"as a melody" so that every rise, fall and long vowel is easy to hear. A recording is compared
with the references of the same kind.
A suggestion is a comparison (evidence level 3), never a verified marking: the speaker confirms.
"""

from __future__ import annotations

import csv
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from amasaku.marks import BASE, Vowel, parse, write

ROOT = Path(__file__).resolve().parents[2]
ANCHORS = ROOT / "data" / "anchors.tsv"
ANCHOR_CLIPS = ROOT / "data" / "anchors"
FIELDS = ["id", "base", "example", "notes", "recording", "melody_recording"]
KINDS = {"normal": "recording", "melody": "melody_recording"}
NASALS = set("mn")
GLIDES = set("wy")


@dataclass
class Transfer:
    marking: str
    reference: str
    notes: list[str]  # length rules applied, conflicts to check


def load() -> list[dict]:
    with ANCHORS.open(encoding="utf-8") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    for r in rows:
        for field in KINDS.values():
            r[field] = r.get(field) or ""
    return rows


def store(rows: list[dict]) -> None:
    tmp = ANCHORS.with_suffix(".tmp")
    with tmp.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS, delimiter="\t", extrasaction="ignore")
        w.writeheader()
        w.writerows(rows)
    tmp.replace(ANCHORS)


def add(example: str, base: str = "", notes: str = "") -> dict:
    """Append a new standard word; returns its row."""
    rows = load()
    row = {"id": str(max((int(r["id"]) for r in rows), default=0) + 1), "base": base or parse(example)[0],
           "example": example, "notes": notes, "recording": "", "melody_recording": ""}
    store(rows + [row])
    return row


def same_marking(a: str, b: str) -> bool:
    """Same letters, same length and tone on every vowel (circumflex and acute both mean High)."""
    (pa, va), (pb, vb) = parse(a.replace("-", "")), parse(b.replace("-", ""))
    return pa == pb and [v.moras for v in va] == [v.moras for v in vb]


TAKES = ROOT / "data" / "anchor_takes.tsv"
TAKE_FIELDS = ["time", "id", "word", "kind", "recording", "predicted", "correct", "closest"]


def load_takes() -> list[dict]:
    if not TAKES.exists():
        return []
    with TAKES.open(encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def append_take(take: dict) -> None:
    new = not TAKES.exists()
    with TAKES.open("a", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=TAKE_FIELDS, delimiter="\t")
        if new:
            w.writeheader()
        w.writerow(take)


def predict_held_out(wav: np.ndarray, anchor_id: str, kind: str = "normal") -> tuple[str, list[dict]]:
    """Predict a standard word's marking from a recording using only the OTHER standard words.

    The word's own reference recording is left out, otherwise the test would just find itself.
    The engine can only predict a pattern that some other standard word carries."""
    rows = load()
    me = next(r for r in rows if r["id"] == anchor_id)
    others = [r for r in rows if r["id"] != anchor_id]
    best = closest(wav, parse(me["example"])[0], others, kind=kind)
    return (best[0]["transfer"].marking if best else ""), best


def anchor_id_of(plain: str) -> str | None:
    """The standard word spelled `plain`, if any (so a test never compares a word with itself)."""
    return next((r["id"] for r in load() if parse(r["example"])[0] == plain), None)


def label(r: dict) -> str:
    return f"{r['id']}. {r['example']}"


def forced_length(plain_word: str) -> list[int | None]:
    """Per vowel of one word: 1 or 2 moras when a rule fixes the length, None when it is contrastive."""
    out = []
    idx = [k for k, ch in enumerate(plain_word) if ch in BASE]
    for n, k in enumerate(idx):
        if n == 0 and k == 0:
            out.append(1)                                   # word-initial vowel: short
        elif n == len(idx) - 1 and k == len(plain_word) - 1:
            out.append(1)                                   # word-final vowel: short
        elif (k + 2 < len(plain_word) and plain_word[k + 1] in NASALS
              and plain_word[k + 2] not in BASE and plain_word[k + 2] not in GLIDES):
            out.append(2)                                   # before nasal + consonant: long
        elif (k >= 2 and plain_word[k - 1] in GLIDES and plain_word[k - 2] not in BASE
              and plain_word[k - 2:k] not in ("ny", "hy")):    # ny, shy are one consonant (REB: ishyarî, shyôgwe)
            out.append(2)                                   # after consonant + glide: long
        else:
            out.append(None)
    return out


def apply_length_rules(plain: str, vowels: list[Vowel]) -> tuple[list[Vowel], list[str]]:
    """Make vowel lengths obey the rules; report every change so the speaker can check it."""
    rules = [n for word in plain.split() for n in forced_length(word)]
    out, notes = [], []
    for v, n in zip(vowels, rules):
        moras = list(v.moras)
        if n == 2 and len(moras) == 1:
            if moras == ["H"]:
                notes.append(f"'{v.letter}' must be long here; the high was put on its first mora (check)")
            moras = moras + ["L"]
        elif n == 1 and len(moras) == 2:
            moras = ["H"] if "H" in moras else ["L"]
        out.append(Vowel(v.letter, moras))
    return out, notes


# Augment + class prefixes and infinitive prefixes, longest first. A guess only: the speaker can
# mark the stem start with a hyphen (nda-bona, umu-gore) when it is wrong.
PREFIXES = ["umu", "umw", "aba", "abw", "imi", "imy", "iki", "icy", "ibi", "iby", "ama", "amw", "uru", "urw",
            "aka", "akw", "utu", "utw", "ubu", "ubw", "uku", "ukw", "aha", "ab", "am", "in", "im", "ku", "kw", "gu",
            "i"]


def splits(text: str) -> list[tuple[str, str]]:
    """Possible (prefix, stem) splits of a word, plain letters, longest prefix first.

    A hyphen marks the stem start. Otherwise every matching prefix is a guess: a-ba-ana is
    written abana, so both aba-na and ab-ana must be tried."""
    word = parse(text)[0]
    if "-" in word:
        i = word.index("-")
        return [(word[:i], word[i + 1:])]
    out = [(p, word[len(p):]) for p in PREFIXES
           if word.startswith(p) and word[len(p):] and not (p in ("in", "im") and word[len(p)] == "y")]
    return out or [("", word)]


def split(text: str, like: str | None = None) -> tuple[str, str]:
    """The split of `text`; with `like`, the one whose stem has as many vowels as `like`'s stem."""
    options = splits(text)
    if like is not None:
        n = len(parse(split(like)[1])[1])
        options = [o for o in options if len(parse(o[1])[1]) == n] or options
    return options[0]


def _fit(moras: list[str], n: int) -> tuple[list[str], str]:
    """The reference vowel's tone on a target vowel of n moras (the target's own length wins)."""
    if len(moras) == n:
        return list(moras), ""
    if n == 1:
        return (["H"] if "H" in moras else ["L"]), ""
    if moras == ["H"]:
        return ["H", "L"], "high put on the first mora of a long vowel (check: could be rising)"
    return ["L", "L"], ""


def transfer(reference: str, target: str) -> Transfer:
    """Copy the reference's vowel patterns onto the target, then apply length rules.

    Words are lined up at their last vowel (REB families: umugorê ~ isî, umusôre ~ matâma). With a
    hyphen in the target, stems are lined up at their first vowel instead (root with root:
    umw|áana -> gu|hiingiisha); prefixes then stay low. `target` may already show ubutinde
    (doubled vowels: guhiingiisha): then its lengths are kept and only the tone is taken from the
    reference, as in the school order, length first, then tone. Without doubled vowels,
    contrastive lengths are taken from the reference too."""
    _, ref = parse(reference.replace("-", ""))
    plain, tgt = parse(target.replace("-", ""))
    if "-" in target:
        (r_pre, r_stem), (t_pre, t_stem) = split(reference), split(target, like=reference)
        r0, t0 = len(parse(r_pre)[1]), len(parse(t_pre)[1])
    else:  # right edge: as many vowels from the end as both words have
        k = min(len(ref), len(tgt))
        r0, t0 = len(ref) - k, len(tgt) - k
    own_length = any(v.long for v in tgt)
    vowels = [Vowel(v.letter, ["L"] * len(v.moras)) for v in tgt]
    notes = []
    n_ref, n_tgt = len(ref) - r0, len(tgt) - t0
    for j in range(min(n_ref, n_tgt)):
        a, b = ref[r0 + j], tgt[t0 + j]
        if own_length:
            moras, note = _fit(a.moras, len(b.moras))
            if note:
                notes.append(f"'{b.letter}': {note}")
        else:
            moras = list(a.moras)
        vowels[t0 + j] = Vowel(b.letter, moras)
    vowels, rule_notes = apply_length_rules(plain, vowels)
    notes += rule_notes
    if "-" in target:
        notes.insert(0, f"stems lined up: {r_pre}-{r_stem} with {t_pre}-{t_stem}")
    else:
        notes.insert(0, "lined up from the end of the word, as in the REB families "
                        "(write a hyphen before the root to line up roots instead: gu-hiinga)")
    if t0 > 0:
        notes.append(f"the first {t0} vowel(s) have no counterpart in the reference: left low")
    if n_tgt > n_ref:
        notes.append(f"the last {n_tgt - n_ref} vowel(s) have no counterpart in the reference: left low")
    if not own_length:
        notes.append("vowel lengths copied from the reference where no rule fixes them: "
                     "type the word with its long vowels first (ubutinde) to keep your own")
    return Transfer(write(plain, vowels), reference, notes)


def vowel_features(measured: list[dict]) -> np.ndarray:
    """Per vowel: pitch vs the word's other vowels, vs the previous vowel, the rise/fall inside
    it, and its position (0 initial, 1 medial, 2 final: statements end low, so a final vowel is
    only compared with other final vowels)."""
    def pitch(x, key):
        v = x[key] if x[key] is not None else x["pitch"]
        return np.nan if v is None else v

    p1 = [pitch(x, "pitch_1") for x in measured]
    p2 = [pitch(x, "pitch_2") for x in measured]
    mid = [np.nan if np.isnan(a) and np.isnan(b) else np.nanmean([a, b]) for a, b in zip(p1, p2)]
    out = []
    for i in range(len(measured)):
        others = [m for j, m in enumerate(mid) if j != i and not np.isnan(m)]
        rel = mid[i] - np.mean(others) if others and not np.isnan(mid[i]) else 0.0
        prev = mid[i] - mid[i - 1] if i > 0 and not np.isnan(mid[i]) and not np.isnan(mid[i - 1]) else 0.0
        slope = p2[i] - p1[i] if not np.isnan(p1[i]) and not np.isnan(p2[i]) else 0.0
        pos = 0 if i == 0 else (2 if i == len(measured) - 1 else 1)
        out.append([rel, prev, slope, pos])
    return np.array(out, dtype=float)


FEATURE_WEIGHTS = np.array([1.0, 1.0, 1.0, 4.0])
_examples: dict = {}


def vowel_examples(kind: str = "normal", exclude: str | None = None) -> list[dict]:
    """Every vowel of every recorded standard word: its features, its known pattern, its word."""
    from amasaku import record

    import soundfile as sf

    field = KINDS[kind]
    out = []
    for r in load():
        if r["id"] == exclude or not r[field] or not (ROOT / r[field]).exists():
            continue
        key = (r[field], r["example"], (ROOT / r[field]).stat().st_mtime)
        if key not in _examples:
            wav, _ = sf.read(ROOT / r[field], dtype="float32")
            plain, vowels = parse(r["example"])
            measured = record.measure_recording(wav, plain)
            _examples[key] = [] if len(measured) != len(vowels) else [
                {"features": f, "moras": v.moras, "letter": v.letter, "word": r["example"], "n": n}
                for n, (f, v) in enumerate(zip(vowel_features(measured), vowels))]
        out += _examples[key]
    return out


def judge_vowels(wav: np.ndarray, plain: str, kind: str = "normal", exclude: str | None = None,
                 k: int = 3) -> tuple[str, list[str]]:
    """The school's merging, done by ear on the recording: each vowel of the word gets the
    pattern of the standard-word vowels that sound most like it (k nearest), within what the
    length rules allow. Returns (marking, one explanation per vowel)."""
    from amasaku import record

    examples = vowel_examples(kind, exclude)
    if not examples:
        return "", []
    measured = record.measure_recording(wav, plain)
    _, target = parse(plain)
    if len(measured) != len(target):
        return "", ["could not find every vowel in the recording"]
    X = np.array([e["features"] for e in examples])
    rules = [n for word in plain.split() for n in forced_length(word)]
    vowels, notes = [], []
    for f, v, n in zip(vowel_features(measured), target, rules):
        dist = np.abs((X - f) * FEATURE_WEIGHTS).sum(axis=1)
        if n is not None:
            dist[[len(e["moras"]) != n for e in examples]] = np.inf
        near = [examples[i] for i in np.argsort(dist)[:k] if np.isfinite(dist[i])]
        votes: dict[str, int] = {}
        for e in near:
            votes["".join(e["moras"])] = votes.get("".join(e["moras"]), 0) + 1
        moras = list(max(votes, key=votes.get)) if votes else ["L"] * (n or 1)
        vowels.append(Vowel(v.letter, moras))
        shown = Vowel(v.letter, moras).written()
        like = ", ".join(f"{e['word']} ('{Vowel(e['letter'], e['moras']).written()}')" for e in near)
        notes.append(f"'{shown}' like {like}" + (f" (length fixed by rule)" if n else ""))
    return write(plain, vowels), notes


def contour(wav: np.ndarray, points: int = 40) -> np.ndarray | None:
    """The word's pitch melody: voiced F0 in semitones around its median, resampled to `points`."""
    from amasaku.align import pitch_track

    _, f0 = pitch_track(wav)
    voiced = f0[~np.isnan(f0)]
    if len(voiced) < 5:
        return None
    st = 12 * np.log2(voiced / np.median(voiced))
    return np.interp(np.linspace(0, 1, points), np.linspace(0, 1, len(st)), st)


def _dtw(a: np.ndarray, b: np.ndarray) -> float:
    n, m = len(a), len(b)
    d = np.full((n + 1, m + 1), np.inf)
    d[0, 0] = 0.0
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            d[i, j] = abs(a[i - 1] - b[j - 1]) + min(d[i - 1, j], d[i, j - 1], d[i - 1, j - 1])
    return float(d[n, m] / (n + m))


REFS = ROOT / "data" / "anchor_refs.tsv"
REF_FIELDS = ["id", "kind", "recording", "speaker", "note"]
ALL_KINDS = ("normal", "melody", "radio")


def load_refs() -> list[dict]:
    if not REFS.exists():
        return []
    with REFS.open(encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def add_ref(anchor_id: str, kind: str, recording: str, speaker: str = "S1", note: str = "") -> None:
    """One more recording of a standard word (another take, or another speaker's)."""
    new = not REFS.exists()
    with REFS.open("a", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=REF_FIELDS, delimiter="\t")
        if new:
            w.writeheader()
        w.writerow({"id": anchor_id, "kind": kind, "recording": recording, "speaker": speaker, "note": note})


def recordings_of(r: dict, kinds=ALL_KINDS) -> list[tuple[str, str]]:
    """Every recording of standard word `r` of the given kinds: [(path, kind)]."""
    out = [(r[KINDS[k]], k) for k in ("normal", "melody") if k in kinds and r.get(KINDS[k])]
    out += [(x["recording"], x["kind"]) for x in load_refs() if x["id"] == r["id"] and x["kind"] in kinds]
    return [(p, k) for p, k in out if (ROOT / p).exists()]


_contours: dict = {}


def contour_of(path: str) -> np.ndarray | None:
    import soundfile as sf

    key = (path, (ROOT / path).stat().st_mtime)
    if key not in _contours:
        wav, _ = sf.read(ROOT / path, dtype="float32")
        _contours[key] = contour(wav)
    return _contours[key]


def closest(wav: np.ndarray, target_plain: str, rows: list[dict] | None = None, top: int = 3,
            kind: str = "normal", exclude: str | None = None, kinds=None) -> list[dict]:
    """Standard words ranked by how close their melody is to the target recording.

    Every recording of a standard word counts (all takes, and other speakers'); a word's
    distance is that of its closest recording. References with the same number of vowels as
    the target are preferred. `kinds` (default: just `kind`) picks normal / slow melody / radio
    recordings. Distance is in semitones (smaller = closer)."""
    rows = load() if rows is None else rows
    kinds = kinds or (kind,)
    mine = contour(wav)
    if mine is None:
        return []
    n_target = len(parse(target_plain)[1])
    ranked = []
    for r in rows:
        if r["id"] == exclude:
            continue
        dists = []
        for path, k in recordings_of(r, kinds):
            theirs = contour_of(path)
            if theirs is not None:
                dists.append(_dtw(mine, theirs))
        if not dists:
            continue
        n_ref = len(parse(r["example"])[1])
        dist = min(dists) + 0.5 * abs(n_ref - n_target)
        ranked.append({"reference": r["example"], "id": r["id"], "distance": round(dist, 2),
                       "takes": len(dists), "transfer": transfer(r["example"], target_plain)})
    return sorted(ranked, key=lambda x: x["distance"])[:top]


def length_signature(marked: str) -> list[int]:
    """Moras per vowel, e.g. inzaara -> [1, 2, 1]."""
    return [len(v.moras) for v in parse(marked)[1]]


def same_ending(a: list[int], b: list[int]) -> bool:
    """The same long vowels counted from the end of the word (over the vowels both have)."""
    k = min(len(a), len(b))
    return a[-k:] == b[-k:]


@dataclass
class SchoolMatch:
    marking: str
    ubutinde: str            # the target with its long vowels, before amasaku
    candidates: list[str]    # standard words with the same long vowels from the end
    ranked: list[dict]       # their melody distances, closest first
    notes: list[str]


# Mwanafunzi's words cut from connected speech made matching worse (school method on the 112
# documented recordings: right standard word 26 vs 29 with the speaker's own recordings only).
SCHOOL_KINDS = ("normal", "melody")


def school_match(wav: np.ndarray, target: str, exclude: str | None = None,
                 kinds=SCHOOL_KINDS) -> SchoolMatch:
    """The school method: 1. ubutinde first; 2. keep the standard words with long vowels in the
    same places (from the end); 3. compare melodies with those only; 4. mark the target like the
    closest one (its tones copied from the end, the target keeping its own lengths).

    `target` may show ubutinde already (inzaara); otherwise lengths come from the rules plus the
    recording (a contrastive vowel is long if it lasts clearly longer than the word's others)."""
    plain, tv = parse(target)
    notes = []
    if any(v.long for v in tv):
        lengths = [len(v.moras) for v in tv]
        notes.append("ubutinde as given")
    else:
        lengths = estimate_lengths(wav, plain)
        notes.append("ubutinde from the length rules and the recording")
    shaped = write(plain, [Vowel(v.letter, ["L"] * n) for v, n in zip(tv, lengths)])
    rows = [r for r in load() if r["id"] != exclude]
    cands = [r for r in rows if same_ending(length_signature(r["example"]), lengths)]
    if not cands:
        cands = rows
        notes.append("no standard word has these long vowels: all were compared")
    ranked = closest(wav, plain, cands, top=len(cands), kinds=kinds)
    if not ranked:
        return SchoolMatch("", shaped, [r["example"] for r in cands], [], notes + ["no melody found"])
    best = ranked[0]["reference"]
    t = transfer(best, shaped)
    return SchoolMatch(t.marking, shaped, [r["example"] for r in cands], ranked, notes + t.notes)


LONG_RATIO = 1.2  # fitted on the standard words: 27 of 39 contrastive vowels (all-short gets 25): weak


def estimate_lengths(wav: np.ndarray, plain: str) -> list[int]:
    """Moras per vowel: the rules where they decide, else long if the vowel lasts LONG_RATIO times
    the word's typical medial vowel (loudness-peak segmentation, so it works on any voice)."""
    from amasaku.melody import measure_nuclei

    rules = [n for word in plain.split() for n in forced_length(word)]
    m = measure_nuclei(wav, plain)
    if len(m) != len(rules):
        return [n or 1 for n in rules]
    durs = np.array([x["dur"] for x in m])
    medial = durs[1:-1] if len(durs) > 2 else durs
    base = np.median(medial)
    return [n if n is not None else (2 if durs[i] > LONG_RATIO * base else 1) for i, n in enumerate(rules)]
