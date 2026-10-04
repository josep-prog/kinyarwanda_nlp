"""Import recordings for documented words from files (same steps as the studio's Save & test).

    uv run python scripts/import_documented.py ~/Downloads/amafu.mp3=118 ~/Downloads/amafu_1.mp3=119
"""
import csv, datetime as dt, sys, warnings
import librosa, soundfile as sf
warnings.filterwarnings("ignore")
from amasaku import anchors, documented, record

for arg in sys.argv[1:]:
    path, wid = arg.rsplit("=", 1)
    r = documented.find(wid)
    x, sr = librosa.load(path, sr=None, mono=True)
    wav = record.prepare((sr, x))
    documented.CLIPS.mkdir(parents=True, exist_ok=True)
    sf.write(documented.clip_of(r), wav, record.SR)
    own = anchors.anchor_id_of(r["plain"])
    best = anchors.closest(wav, r["plain"], exclude=own)
    whole = best[0]["transfer"].marking if best else ""
    try:
        merged, _ = anchors.judge_vowels(wav, r["plain"], exclude=own)
    except Exception:
        merged = ""
    new = not documented.TAKES.exists()
    with documented.TAKES.open("a", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=documented.TAKE_FIELDS, delimiter="\t")
        if new:
            w.writeheader()
        w.writerow({"time": dt.datetime.now().isoformat(timespec="seconds"), "id": r["id"], "word": r["marked"],
                    "recording": str(documented.clip_of(r).relative_to(documented.ROOT)),
                    "vowel_by_vowel": merged, "vowel_ok": "yes" if anchors.same_marking(merged, r["marked"]) else "no",
                    "whole_word": whole, "whole_ok": "yes" if anchors.same_marking(whole, r["marked"]) else "no",
                    "speaker_agrees": "no"})
    sc = documented.run_school(r, wav)
    res = documented.load_school(); res[r["id"]] = sc; documented.save_school(res)
    print(f"#{r['id']} {r['marked']:12s} <- {path.split('/')[-1]:16s} school (ubutinde first): {sc['given_marking']} "
          f"({'right' if sc['given_ok'] == 'yes' else 'wrong'}, like {sc['given_closest']})")
print(documented.score())
