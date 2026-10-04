"""Build the recording list (data/words.tsv), in priority order.

    uv run python -m amasaku.wordlist

1. Impuzashusho: written alike, said differently (REB S4 Kinyarwanda, Umutwe wa 3; research)
2. Words tone-marked in the TTC Ikinyarwanda student book 2 (book marking kept as reference)
3. Words the voice struggles with: misheard in v1/v3 speech (speech-recognizer check on the
   held-out test), plus categories the tests exposed (greetings never heard in training,
   nti- negatives, numbers, question words, politeness)
4. The native speaker's anchor words
5. Mwanafunzi's most frequent words, kept only if they also appear in the S6 school textbooks
   (drops names, foreign words and transcription errors)
Existing recordings and markings in words.tsv are preserved when the list is rebuilt.
"""

from __future__ import annotations

import csv
import json
import re
from collections import Counter
from pathlib import Path

from amasaku.marks import parse

ROOT = Path(__file__).resolve().parents[2]
WORDS = ROOT / "data" / "words.tsv"
FIELDS = ["id", "plain", "meaning", "book_marking", "source", "your_marking", "heard", "recording", "notes"]
TTS = ROOT.parent / "kinyarwanda-tts"
NLP = ROOT.parent / "kinyarwanda_nlp" / "data"

S4 = "REB S4 Kinyarwanda, Umutwe wa 3 (impuzashusho)"
HOMOGRAPHS = [
    ("inkoko", "itungo (chicken)", S4), ("inkoko", "igikoresho bagosoza imyaka (winnowing basket)", S4),
    ("imyenda", "imyambaro (clothes)", S4), ("imyenda", "amadeni (debts)", S4),
    ("inda", "igice cy'umubiri (belly)", S4), ("inda", "udusimba tujya mu myenda (lice)", S4),
    ("gutara", "gushyira ibitoki mu rwina (ripen bananas in a pit)", S4),
    ("gutara", "gukusanya ibintu bitatanye (collect)", S4),
    ("ikiraro", "iteme bambukiraho (bridge)", S4), ("ikiraro", "inzu y'inka cyangwa andi matungo (stable)", S4),
    ("gutaka", "gusakuza bitewe n'ikikubayeho (cry out)", S4), ("gutaka", "gushyira imitako ku kintu (decorate)", S4),
    ("guhuma", "kurwara amaso (go blind)", S4), ("guhuma", "kuvuga kw'impyisi (hyena's cry)", S4),
    ("kuvura", "meaning 1 (please fill)", S4 + ", exercise"), ("kuvura", "meaning 2 (please fill)", S4 + ", exercise"),
    ("guhisha", "meaning 1 (please fill)", S4 + ", exercise"), ("guhisha", "meaning 2 (please fill)", S4 + ", exercise"),
    ("gushima", "meaning 1 (please fill)", S4 + ", exercise"), ("gushima", "meaning 2 (please fill)", S4 + ", exercise"),
    ("inzara", "hunger", "native speaker"), ("inzara", "fingernails / claws", "native speaker"),
    ("umuryango", "door (umuryáango)", "Jarnow, PLSA"), ("umuryango", "family (umuryaango)", "Jarnow, PLSA"),
]
STRUGGLES = {
    "greetings (never heard in the training audio)": ["muraho", "mwaramutse", "mwiriwe", "murakoze", "amakuru",
        "bite", "ni", "meza", "neza", "mwirirwe", "uramutse", "wiriwe", "ijoro", "ryiza", "murabeho", "yego", "oya",
        "mbabarira", "ndagukunda", "turabashimiye", "ikaze", "murakaza"],
    "nti- negatives (high tone on the negative; 'nti' heard as 'ni')": ["ntabwo", "nta", "ntiyaje", "ntibazaza",
        "ntiyavuze", "ntibishoboka", "ntawe", "ntacyo", "ntihazagire", "sinzi", "sinabimenye", "ntibyari",
        "ntidushobora", "ntibakunda", "ntiyigeze"],
    "numbers": ["rimwe", "kabiri", "gatatu", "kane", "gatanu", "gatandatu", "karindwi", "umunani", "icyenda",
        "icumi", "makumyabiri", "mirongo", "ijana", "magana", "igihumbi", "ibihumbi", "miliyoni", "miliyari",
        "abiri", "atatu", "ane", "atanu", "bibiri", "bitatu", "cyenda", "cumi"],
    "question words": ["iki", "nde", "he", "ryari", "gute", "kuki", "ese", "angahe", "bangahe", "uwuhe", "ikihe"],
}
ANCHORS = [("umwana", "child", "umwáana"), ("umwami", "king", "umwaámi"), ("umugabo", "man", ""),
           ("umugore", "woman", "umugoré"), ("inka", "cow", "inká"), ("umuganda", "community work", "umugaanda")]


def school_vocabulary() -> set[str]:
    words = set()
    for f in ["Kinyarwanda_Core_S6_SB.txt", "Kinyarwanda_Languages_S6.txt"]:
        p = NLP / f
        if p.exists():
            words |= set(re.findall(r"[a-z']+", p.read_text(encoding="utf-8", errors="ignore").lower().replace("’", "'")))
    return words


def main() -> None:
    old = {}
    ways: dict[tuple[str, str], list[dict]] = {}  # extra ways of saying a word (ids like 31-2), added in the studio
    if WORDS.exists():
        with WORDS.open(encoding="utf-8") as f:
            for r in csv.DictReader(f, delimiter="\t"):
                if "-" in r["id"]:
                    ways.setdefault((r["plain"], r["meaning"]), []).append(r)
                else:
                    old[(r["plain"], r["meaning"])] = r
    rows: list[dict] = []
    seen: set[tuple[str, str]] = set()

    def add(plain: str, meaning: str = "", book: str = "", source: str = "") -> None:
        key = (plain, meaning)
        if key in seen or not plain:
            return
        seen.add(key)
        rows.append({"plain": plain, "meaning": meaning, "book_marking": book, "source": source})

    for plain, meaning, src in HOMOGRAPHS:
        add(plain, meaning, "", src)
    school = school_vocabulary()
    misheard = ROOT / "data" / "misheard_words.json"
    if misheard.exists():
        for w in json.loads(misheard.read_text()):
            if "'" not in w and w in school:
                add(w, "", "", "voice struggles: misheard in v1/v3 speech")
    for category, words in STRUGGLES.items():
        for w in words:
            add(w, "", "", f"voice struggles: {category}")
    book_json = ROOT / "data" / "book_ttc2_marked.json"
    if book_json.exists():
        for plain, forms in sorted(json.loads(book_json.read_text()).items()):
            if plain in school or plain in {"ikirere", "intego", "umugore", "umugabo"}:
                add(plain, "", " / ".join(forms), "TTC Ikinyarwanda student book 2 (tone-marked passages)")
    for plain, meaning, mark in ANCHORS:
        add(plain, meaning, "", "native speaker anchor" + (f" ({mark})" if mark else ""))
    counts: Counter = Counter()
    meta = TTS / "data" / "prepared_v2" / "metadata.csv"
    if meta.exists():
        with meta.open(encoding="utf-8") as f:
            for r in csv.DictReader(f, delimiter="|"):
                if "MWANAFUNZI" in r["channel"]:
                    counts.update(r["text"].split())
    for w, n in counts.most_common():
        if n < 5 or len(rows) >= 1000:
            break
        if len(w) >= 2 and w in school and not re.search(r"[qx]", w):
            add(w, "", "", f"Mwanafunzi documentaries ({n} times)")

    WORDS.parent.mkdir(parents=True, exist_ok=True)
    with WORDS.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS, delimiter="\t")
        w.writeheader()
        for i, r in enumerate(rows, 1):
            prev = old.get((r["plain"], r["meaning"]), {})
            w.writerow({**{k: prev.get(k, "") for k in FIELDS}, **r, "id": i,
                        **{k: prev.get(k, "") for k in ["your_marking", "heard", "recording", "notes"]}})
            for n, extra in enumerate(ways.get((r["plain"], r["meaning"]), []), 2):
                w.writerow({**extra, "id": f"{i}-{n}"})
    by = Counter(r["source"].split(" (")[0].split(",")[0] for r in rows)
    print(f"{len(rows)} words -> {WORDS}")
    for k, v in by.most_common():
        print(f"  {v:4d}  {k}")


if __name__ == "__main__":
    main()
