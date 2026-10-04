"""Documented words page: words whose ubutinde and amasaku are written in REB books (with page).

Record a word; the engine predicts its marking from your standard-word recordings, and the
prediction is checked against the BOOK, not against anyone's ear. This is the fair test of the
engine. A word whose book marking the speaker confirms ("I say it this way") is CONFIRMED; when
the engine also lands on the book's marking, all three agree and the word is VERIFIED. The
standard words stay the school's 16-17 melodies: verified words are examples of those melodies,
not new standard words. Corpus: data/documented.tsv (built by scripts/build_documented.py); clips go to
data/documented/, results to data/documented_takes.tsv.
"""

from __future__ import annotations

import csv
import datetime as dt
from pathlib import Path

import gradio as gr
import soundfile as sf

from amasaku import anchors
from amasaku.marks import parse

ROOT = Path(__file__).resolve().parents[2]
CORPUS = ROOT / "data" / "documented.tsv"
CLIPS = ROOT / "data" / "documented"
TAKES = ROOT / "data" / "documented_takes.tsv"
TAKE_FIELDS = ["time", "id", "word", "recording", "vowel_by_vowel", "vowel_ok", "whole_word", "whole_ok", "speaker_agrees"]
COLS = ["#", "documented marking", "kind", "page", "recorded", "school method", "school (ubutinde first)",
        "whole word (old)", "status"]
SCHOOL = ROOT / "data" / "documented_school.tsv"
SCHOOL_FIELDS = ["id", "word", "ubutinde", "candidates", "closest", "marking", "ok",
                 "given_ubutinde", "given_closest", "given_marking", "given_ok"]
SHOWS = ["All", "Not recorded yet", "Minimal pairs", "Reference families"]


def load() -> list[dict]:
    with CORPUS.open(encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def load_takes() -> list[dict]:
    if not TAKES.exists():
        return []
    with TAKES.open(encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def last_takes() -> dict[str, dict]:
    return {t["id"]: t for t in load_takes()}


def load_school() -> dict[str, dict]:
    if not SCHOOL.exists():
        return {}
    with SCHOOL.open(encoding="utf-8") as f:
        return {r["id"]: r for r in csv.DictReader(f, delimiter="\t")}


def run_school(r: dict, wav) -> dict:
    """The school method on one recording, twice: ubutinde found from the recording, and ubutinde
    done first and right (the book's lengths, tones removed), as a student does by counting beats."""
    from amasaku.marks import Vowel, write

    own = anchors.anchor_id_of(r["plain"])           # never compare a standard word with itself
    a = anchors.school_match(wav, r["plain"], exclude=own)
    plain, vs = parse(r["marked"])
    lengths_only = write(plain, [Vowel(v.letter, ["L"] * len(v.moras)) for v in vs])
    b = anchors.school_match(wav, lengths_only, exclude=own)
    return {"id": r["id"], "word": r["marked"], "ubutinde": a.ubutinde, "candidates": ", ".join(a.candidates),
            "closest": a.ranked[0]["reference"] if a.ranked else "", "marking": a.marking,
            "ok": "yes" if anchors.same_marking(a.marking, r["marked"]) else "no",
            "given_ubutinde": b.ubutinde, "given_closest": b.ranked[0]["reference"] if b.ranked else "",
            "given_marking": b.marking, "given_ok": "yes" if anchors.same_marking(b.marking, r["marked"]) else "no"}


def save_school(results: dict[str, dict]) -> None:
    with SCHOOL.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=SCHOOL_FIELDS, delimiter="\t")
        w.writeheader()
        w.writerows(results.values())


def retest_all() -> str:
    results = load_school()
    n = 0
    for r in load():
        if clip_of(r).exists():
            wav, _ = sf.read(clip_of(r), dtype="float32")
            results[r["id"]] = run_school(r, wav)
            n += 1
    save_school(results)
    return f"Re-tested {n} recordings with the school method."


def kind(r: dict) -> str:
    if r["pair"]:
        return f"pair with {r['pair']}"
    if r["family"]:
        return "reference word" if r["family"] == r["marked"] else f"like {r['family']}"
    return r["category"]


def clip_of(r: dict) -> Path:
    return CLIPS / f"{int(r['id']):03d}_{r['plain']}.wav"


def table(show: str) -> list[list]:
    last = last_takes()
    school = load_school()
    out = []
    for r in load():
        done = clip_of(r).exists()
        if (show == "Not recorded yet" and done) or (show == "Minimal pairs" and not r["pair"]) \
                or (show == "Reference families" and not r["family"]):
            continue
        t = last.get(r["id"])
        sc = school.get(r["id"])
        mark = lambda ok, m: ("✅ " if ok == "yes" else "❌ ") + m
        out.append([r["id"], r["marked"], kind(r), r["page"], "✅" if done else "",
                    mark(sc["ok"], f"{sc['marking']} (like {sc['closest']})") if sc else "",
                    mark(sc["given_ok"], f"{sc['given_marking']} (like {sc['given_closest']})") if sc else "",
                    mark(t["whole_ok"], t["whole_word"]) if t else "", status(r, t)])
    return out or [[""] * len(COLS)]


def is_standard(r: dict) -> bool:
    return anchors.anchor_id_of(r["plain"]) is not None and any(
        anchors.same_marking(a["example"], r["marked"]) for a in anchors.load())


def status(r: dict, t: dict | None) -> str:
    if is_standard(r):
        return "⭐ standard word"
    if t and t.get("speaker_agrees") == "yes":
        engine_ok = t.get("vowel_ok") == "yes" or t.get("whole_ok") == "yes"
        return "✔ verified (book + you + engine)" if engine_ok else "confirmed (book + you)"
    return ""


def score() -> str:
    last = list(last_takes().values())
    if not last:
        return f"{len(load())} documented words. Nothing recorded yet."
    w = sum(t["whole_ok"] == "yes" for t in last)
    sc = list(load_school().values())
    line = f"Engine against the book, {len(last)} recorded words: old whole word **{w}**"
    if sc:
        a = sum(x["ok"] == "yes" for x in sc)
        b = sum(x["given_ok"] == "yes" for x in sc)
        line += (f" · school method **{a} of {len(sc)}** · school method with ubutinde done first **{b} of "
                 f"{len(sc)}**")
    return line


def card(r: dict | None) -> str:
    if r is None:
        return "Click a word in the table."
    lines = [f"### #{r['id']} · **{r['marked']}**" + (f" — {r['meaning']}" if r["meaning"] else ""),
             f"{kind(r)} · {r['category']}",
             f"Source: [{r['source']}, p. {r['page']}]({r['url']})"]
    if r["note"]:
        lines.append(f"Note: {r['note']}")
    lines.append("Say it **alone, naturally**, then press Save & test.")
    return "  \n".join(lines)


def find(wid: str) -> dict | None:
    return next((r for r in load() if r["id"] == str(wid)), None)


def build_tab(prepare, sr: int):
    gr.Markdown("## Documented words (from REB books)\nEvery word here has its ubutinde and amasaku printed in "
                "an REB book (page given). Record a word and the engine's answer is checked against the "
                "**book**, so a mistake is the engine's, not yours. Click a row to pick a word.")
    sel = gr.State("")
    summary = gr.Markdown(score())
    with gr.Row():
        with gr.Column(scale=2):
            info = gr.Markdown(card(None))
            voice = gr.Audio(sources=["microphone", "upload"], type="numpy", label="Your voice")
            agree = gr.Checkbox(label="I say it the way the book marks it", value=False)
            go_b = gr.Button("Save & test", variant="primary")
            all_b = gr.Button("Re-test all recordings with the school method")
            result = gr.Markdown()
        with gr.Column(scale=3):
            show = gr.Radio(SHOWS, value="All", label="Show")
            grid = gr.Dataframe(value=table("All"), headers=COLS, interactive=False, wrap=True)

    def pick(evt: gr.SelectData, show_):
        wid = table(show_)[evt.index[0]][0]
        r = find(wid)
        return wid, card(r), str(clip_of(r)) if r and clip_of(r).exists() else None, "", False

    def test(wid, audio, agrees, show_):
        r = find(wid)
        if r is None or audio is None:
            return "Pick a word and record it first.", table(show_), score()
        wav = prepare(audio)
        CLIPS.mkdir(parents=True, exist_ok=True)
        sf.write(clip_of(r), wav, sr)
        own = anchors.anchor_id_of(r["plain"])          # never compare a standard word with itself
        try:
            merged, why = anchors.judge_vowels(wav, r["plain"], exclude=own)
        except Exception as e:
            merged, why = "", [f"could not judge the vowels: {e}"]
        best = anchors.closest(wav, r["plain"], exclude=own)
        whole = best[0]["transfer"].marking if best else ""
        v_ok, w_ok = anchors.same_marking(merged, r["marked"]), anchors.same_marking(whole, r["marked"])
        sc = run_school(r, wav)
        results = load_school()
        results[r["id"]] = sc
        save_school(results)
        new = not TAKES.exists()
        with TAKES.open("a", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=TAKE_FIELDS, delimiter="\t")
            if new:
                w.writeheader()
            w.writerow({"time": dt.datetime.now().isoformat(timespec="seconds"), "id": r["id"],
                        "word": r["marked"], "recording": str(clip_of(r).relative_to(ROOT)),
                        "vowel_by_vowel": merged, "vowel_ok": "yes" if v_ok else "no",
                        "whole_word": whole, "whole_ok": "yes" if w_ok else "no",
                        "speaker_agrees": "yes" if agrees else "no"})
        tick = lambda ok: "✅" if ok else "❌"
        lines = [f"Book: **{r['marked']}**",
                 f"{tick(sc['ok'] == 'yes')} School method: ubutinde **{sc['ubutinde']}** → standard words with "
                 f"the same long vowels: {sc['candidates']} → closest melody **{sc['closest']}** → "
                 f"**{sc['marking']}**",
                 f"{tick(sc['given_ok'] == 'yes')} With ubutinde done first ({sc['given_ubutinde']}): like "
                 f"**{sc['given_closest']}** → **{sc['given_marking']}**",
                 f"{tick(v_ok)} Vowel by vowel: **{merged or '-'}**"] + [f"- {x}" for x in why] + [
                 f"{tick(w_ok)} Whole word: **{whole or '-'}**"
                 + (" (sounds like " + ", ".join(b["reference"] for b in best) + ")" if best else "")]
        if is_standard(r):
            lines.append("⭐ Already a standard word.")
        elif agrees and (v_ok or w_ok):
            lines.append("✔ Book, you and the engine agree: **verified**.")
        elif agrees:
            lines.append("Confirmed by the book and you, but the engine missed it: not promoted. "
                         "This word shows the engine what it does not know yet.")
        else:
            lines.append("Not confirmed: tick “I say it the way the book marks it” if you do, and save again.")
        return "  \n".join(lines), table(show_), score()

    grid.select(pick, [show], [sel, info, voice, result, agree])
    go_b.click(test, [sel, voice, agree, show], [result, grid, summary])
    show.change(table, [show], [grid])
    all_b.click(lambda s: (retest_all(), table(s), score()), [show], [result, grid, summary])
