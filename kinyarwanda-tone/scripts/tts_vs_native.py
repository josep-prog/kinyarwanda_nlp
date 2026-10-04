"""Does writing ubutinde/amasaku into the TTS input bring its melody closer to a native speaker?

For every REB-documented word the speaker recorded, the TTS (Mwanafunzi v3) says it four ways:
  plain     : the word as normally written; the voice's own melody
  marked    : the book's marking through speak.py as it was before today
  marked+REB: the book's marking plus integuza and a low final (REB Y2 TG p.114)
  REB-only-length: book lengths, voice's own melody (pitch not reshaped)
and each output's pitch contour is compared (DTW, semitones around its median) with the
speaker's own recording of that word. Smaller = closer to the native melody.
"""
import csv, warnings, numpy as np, soundfile as sf
warnings.filterwarnings("ignore")
from amasaku import anchors, documented
from amasaku.speak import Speaker, Settings
import librosa

spk = Speaker()
def contour16(wav, sr):
    w = librosa.resample(wav.astype(np.float32), orig_sr=sr, target_sr=16000) if sr != 16000 else wav
    return anchors.contour(w.astype(np.float32))
def z(c): return (c - c.mean()) / (c.std() + 1e-6)
ways = {
    "plain": lambda r: spk.say(r["plain"], Settings(reshape_pitch=False))[0],
    "length only": lambda r: spk.say(r["marked"], Settings(reshape_pitch=False))[0],
    "marked (before)": lambda r: spk.say(r["marked"], Settings())[0],
    "marked + REB": lambda r: spk.say(r["marked"], Settings(integuza=0.5, final_low=True))[0],
}
rows = [r for r in documented.load() if documented.clip_of(r).exists()]
dist = {k: [] for k in ways}
out = []
for r in rows:
    nat, _ = sf.read(documented.clip_of(r), dtype="float32")
    cn = anchors.contour(nat)
    if cn is None:
        continue
    rec = {"word": r["marked"]}
    for k, f in ways.items():
        c = contour16(f(r), spk.sr)
        d = np.nan if c is None else anchors._dtw(z(cn), z(c))
        dist[k].append(d); rec[k] = round(float(d), 3)
    out.append(rec)
D = {k: np.array(v) for k, v in dist.items()}
ok = ~np.any(np.isnan(np.stack(list(D.values()))), axis=0)
print(f"{ok.sum()} words; distance to the native melody (smaller = closer):")
for k, v in D.items():
    print(f"  {k:16s} mean {v[ok].mean():.3f}   closest of the four for {np.sum(np.argmin(np.stack([D[x][ok] for x in D]), axis=0) == list(D).index(k))} words")
base = D["plain"][ok]
for k in list(D)[1:]:
    print(f"  {k:16s} closer than plain for {np.sum(D[k][ok] < base)} of {ok.sum()} words")
with open(anchors.ROOT / "data" / "tts_vs_native.tsv", "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=list(out[0]), delimiter="\t"); w.writeheader(); w.writerows(out)
