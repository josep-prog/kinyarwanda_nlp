"""Standard words page (amagambo fatizo): record them, re-record them, test the engine on them.

Each standard word has two reference recordings (what other words are compared with): said
normally, and said slowly "as a melody", the school way of hearing where the voice rises, falls
and lingers. It can also have any number of test takes. A test take is predicted from the OTHER standard words only and checked
against the word's known marking, so the score shows how well the engine would do on a word
it has never heard. Takes go to data/anchors/takes/ and data/anchor_takes.tsv.
"""

from __future__ import annotations

import datetime as dt

import gradio as gr
import soundfile as sf

from amasaku import anchors
from amasaku.marks import parse

COLS = ["#", "standard word", "normal", "melody", "extra takes", "radio examples", "tests", "correct", "last prediction"]


def table() -> list[list]:
    takes = anchors.load_takes()
    out = []
    for r in anchors.load():
        mine = [t for t in takes if t["id"] == r["id"]]
        right = sum(t["correct"] == "yes" for t in mine)
        last = mine[-1] if mine else None
        extra = [x for x in anchors.load_refs() if x["id"] == r["id"]]
        out.append([r["id"], r["example"], "✅" if r["recording"] else "", "✅" if r["melody_recording"] else "",
                    sum(x["kind"] != "radio" for x in extra) or "", sum(x["kind"] == "radio" for x in extra) or "",
                    len(mine) or "",
                    f"{right} of {len(mine)}" if mine else "",
                    (("✅ " if last["correct"] == "yes" else "❌ ") + last["predicted"]) if last else ""])
    return out


def score() -> str:
    takes = anchors.load_takes()
    rows = anchors.load()
    normal = sum(bool(r["recording"]) for r in rows)
    melody = sum(bool(r["melody_recording"]) for r in rows)
    head = f"Reference recordings: {normal} of {len(rows)} normal, {melody} of {len(rows)} melody"
    if not takes:
        return head + ". No tests yet."
    parts = []
    for kind in anchors.KINDS:
        mine = [t for t in takes if (t.get("kind") or "normal") == kind]
        if mine:
            right = sum(t["correct"] == "yes" for t in mine)
            parts.append(f"{kind}: **{right} of {len(mine)}** ({100 * right // len(mine)}%)")
    return head + " · engine correct on test takes, " + ", ".join(parts)


def card(r: dict | None) -> str:
    if r is None:
        return "Click a word in the table."
    lines = [f"### #{r['id']} · **{r['example']}**"]
    if r["base"] and r["base"] != parse(r["example"])[0]:
        lines.append(f"Base form in your list: {r['base']}")
    if r["notes"]:
        lines.append(f"⚠️ {r['notes']}")
    lines.append(("Normal ✅" if r["recording"] else "Normal: not recorded") + " · "
                 + ("Melody ✅" if r["melody_recording"] else "Melody: not recorded"))
    return "  \n".join(lines)


def find(wid: str) -> tuple[list[dict], dict | None]:
    rows = anchors.load()
    return rows, next((r for r in rows if r["id"] == str(wid)), None)


def clip(r: dict | None, field: str = "recording") -> str | None:
    return str(anchors.ROOT / r[field]) if r and r[field] else None


def build_tab(prepare, sr: int):
    """The Standard words tab; `prepare` turns Gradio audio into 16 kHz mono at rate `sr`."""
    gr.Markdown("## Standard words (amagambo fatizo)\nGive each word two **reference** recordings: said "
                "**normally**, and said **slowly as a melody**. Record or upload either one. Then record it again "
                "any time as a **test**: the engine predicts it from the *other* standard words only (normal "
                "against normal, melody against melody), and the table keeps its score. Click a row to pick a word.")
    sel = gr.State("")
    summary = gr.Markdown(score())
    with gr.Row():
        with gr.Column(scale=2):
            info = gr.Markdown(card(None))
            with gr.Row():
                normal = gr.Audio(sources=["microphone", "upload"], type="numpy", label="Normal way")
                melody = gr.Audio(sources=["microphone", "upload"], type="numpy", label="Melody way (slow)")
            with gr.Row():
                ref_b = gr.Button("Save as reference recordings")
                more_b = gr.Button("➕ Add as extra takes")
            with gr.Group():
                kind = gr.Radio(list(anchors.KINDS), value="normal", label="Test with which recording above?")
                test_b = gr.Button("Test: can the engine predict it?", variant="primary")
            mark = gr.Textbox(label="Marking (circumflex or acute = high, doubled vowel = long)")
            mark_b = gr.Button("Save marking")
            status = gr.Markdown()
            with gr.Accordion("Add a standard word", open=False):
                new_mark = gr.Textbox(label="Marking, e.g. umugaanda")
                new_notes = gr.Textbox(label="Notes (optional)")
                add_b = gr.Button("Add")
        with gr.Column(scale=3):
            grid = gr.Dataframe(value=table(), headers=COLS, interactive=False, wrap=True)

    def pick(evt: gr.SelectData):
        wid = table()[evt.index[0]][0]
        _, r = find(wid)
        return wid, card(r), clip(r), clip(r, "melody_recording"), r["example"] if r else "", ""

    def save_reference(wid, normal_, melody_):
        rows, r = find(wid)
        if r is None or (normal_ is None and melody_ is None):
            return card(r), "Pick a word and record or upload at least one way first.", table(), score()
        anchors.ANCHOR_CLIPS.mkdir(parents=True, exist_ok=True)
        saved = []
        for audio, k, suffix in ((normal_, "normal", ""), (melody_, "melody", "_melody")):
            if audio is not None:
                path = anchors.ANCHOR_CLIPS / f"{int(r['id']):02d}_{parse(r['example'])[0]}{suffix}.wav"
                sf.write(path, prepare(audio), sr)
                r[anchors.KINDS[k]] = str(path.relative_to(anchors.ROOT))
                saved.append(k)
        anchors.store(rows)
        return card(r), f"Saved the {' and '.join(saved)} reference for **{r['example']}**.", table(), score()

    def extra_takes(wid, normal_, melody_):
        """Keep the main references; store these recordings as more examples of the same melody."""
        _, r = find(wid)
        if r is None or (normal_ is None and melody_ is None):
            return card(r), "Pick a word and record or upload at least one way first.", table(), score()
        folder = anchors.ANCHOR_CLIPS / "takes_ref"
        folder.mkdir(parents=True, exist_ok=True)
        saved = []
        for audio, k in ((normal_, "normal"), (melody_, "melody")):
            if audio is None:
                continue
            n = sum(x["id"] == r["id"] and x["kind"] == k for x in anchors.load_refs()) + 2
            path = folder / f"{int(r['id']):02d}_{parse(r['example'])[0]}_{k}_{n}.wav"
            sf.write(path, prepare(audio), sr)
            anchors.add_ref(r["id"], k, str(path.relative_to(anchors.ROOT)))
            saved.append(f"{k} take {n}")
        return card(r), f"Added {', '.join(saved)} for **{r['example']}**.", table(), score()

    def test(wid, normal_, melody_, kind_):
        _, r = find(wid)
        audio = normal_ if kind_ == "normal" else melody_
        if r is None or audio is None:
            return f"Pick a word and record the {kind_} way above first.", table(), score()
        wav = prepare(audio)
        predicted, best = anchors.predict_held_out(wav, r["id"], kind_)
        if not best:
            return f"Save {kind_} reference recordings for some other standard words first.", table(), score()
        n = sum(t["id"] == r["id"] for t in anchors.load_takes()) + 1
        takes = anchors.ANCHOR_CLIPS / "takes"
        takes.mkdir(parents=True, exist_ok=True)
        path = takes / f"{int(r['id']):02d}_{parse(r['example'])[0]}_{kind_}_{n}.wav"
        sf.write(path, wav, sr)
        ok = anchors.same_marking(predicted, r["example"])
        near = " · ".join(f"{b['reference']} → {b['transfer'].marking} ({b['distance']})" for b in best)
        anchors.append_take({"time": dt.datetime.now().isoformat(timespec="seconds"), "id": r["id"],
                             "word": r["example"], "kind": kind_, "recording": str(path.relative_to(anchors.ROOT)),
                             "predicted": predicted, "correct": "yes" if ok else "no", "closest": near})
        verdict = "✅ correct" if ok else f"❌ wrong (should be **{r['example']}**)"
        why = "" if ok else ("  \nIf no other standard word has this word's pattern, the engine cannot "
                             "find it: that is a sign the list needs another word with this pattern.")
        return f"Engine predicts **{predicted}**: {verdict}  \nClosest: {near}{why}", table(), score()

    def save_mark(wid, text):
        rows, r = find(wid)
        text = (text or "").strip()
        if r is None or not text:
            return card(r), "Pick a word first.", table()
        if parse(text)[0] != parse(r["example"])[0]:
            return card(r), f"'{text}' does not spell '{parse(r['example'])[0]}': only the marks may differ.", table()
        r["example"], r["notes"] = text, ""
        anchors.store(rows)
        return card(r), f"Marking saved: **{text}**", table()

    def add_word(text, notes):
        text = (text or "").strip()
        if not text:
            return "Type the word with its marking.", table(), score(), "", ""
        r = anchors.add(text, notes=notes or "")
        return f"Added #{r['id']} **{r['example']}**: click it in the table and save a reference recording.", \
            table(), score(), "", ""

    grid.select(pick, [], [sel, info, normal, melody, mark, status])
    ref_b.click(save_reference, [sel, normal, melody], [info, status, grid, summary])
    more_b.click(extra_takes, [sel, normal, melody], [info, status, grid, summary])
    test_b.click(test, [sel, normal, melody, kind], [status, grid, summary])
    mark_b.click(save_mark, [sel, mark], [info, status, grid])
    add_b.click(add_word, [new_mark, new_notes], [status, grid, summary, new_mark, new_notes])
