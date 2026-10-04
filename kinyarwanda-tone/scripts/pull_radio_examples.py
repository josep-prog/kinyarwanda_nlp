"""Cut other speakers' examples of the standard words out of the TTS corpus (Radio Rwanda,
Ismaël Mwanafunzi) and register them as extra reference recordings (kind "radio").

Only occurrences where the word keeps its own melody are used (REB Y2 TG p.119-121, S5 p.48-50):
not sentence-final (statements end low), not right after the connectives na/nka or a possessive
in -a (rules a-h change the melody there).
"""
import csv, io, sys, zipfile, collections, warnings
import numpy as np, soundfile as sf
warnings.filterwarnings("ignore")
from amasaku import anchors
from amasaku.align import VitsAligner
from amasaku.marks import parse

D = anchors.ROOT.parent / "kinyarwanda-tts" / "data" / "prepared_v2"
PER = int(sys.argv[1]) if len(sys.argv) > 1 else 15
CONNECTIVES = {"na", "nka", "wa", "ya", "ba", "za", "cya", "bya", "rya", "ka", "twa", "bwa", "kwa", "ha", "rwa",
               "gwa", "ga", "mwa", "ra", "y'", "w'"}
std = {parse(r["example"])[0]: r for r in anchors.load()}
done = collections.Counter((x["id"], x["speaker"]) for x in anchors.load_refs() if x["kind"] == "radio")
rows = list(csv.DictReader(open(D / "metadata.csv"), delimiter="|"))
z = zipfile.ZipFile(D / "prepared_dataset.zip")
al = VitsAligner()
out_dir = anchors.ANCHOR_CLIPS / "radio"
out_dir.mkdir(parents=True, exist_ok=True)
added = collections.Counter()
for r in rows:
    words = r["text"].split()
    hits = [(i, w) for i, w in enumerate(words) if w in std and i < len(words) - 1
            and (i == 0 or words[i - 1] not in CONNECTIVES)
            and done[(std[w]["id"], r["channel"])] < PER]
    if not hits:
        continue
    wav, _ = sf.read(io.BytesIO(z.read(f"wavs/{r['id']}.wav")), dtype="float32")
    try:
        letters = al.align(wav, r["text"])
    except Exception:
        continue
    for i, w in hits:
        mine = [l for l in letters if l["word"] == i]
        if not mine:
            continue
        a, b = max(0, mine[0]["start"] - 0.03), mine[-1]["end"] + 0.03
        clip = wav[int(a * 16000):int(b * 16000)]
        if len(clip) < 0.2 * 16000 or anchors.contour(clip) is None:
            continue
        a_id = std[w]["id"]
        k = done[(a_id, r["channel"])] + 1
        tag = "radio" if "Radio" in r["channel"] else "mwanafunzi"
        path = out_dir / f"{int(a_id):02d}_{w}_{tag}_{k}.wav"
        sf.write(path, clip, 16000)
        anchors.add_ref(a_id, "radio", str(path.relative_to(anchors.ROOT)), r["channel"], r["id"])
        done[(a_id, r["channel"])] += 1
        added[w] += 1
print("added:", dict(added))
