"""Word recording table: go through the list, record or upload each word, mark it, save.

    uv run amasaku-studio        # http://127.0.0.1:7862

Each saved word: the clip goes to data/words/, and words.tsv gets the recording, your marking,
what the tool heard from your voice, and notes. Marked words also go to the answer key (gold.csv).

Reference words (amagambo fatizo, data/anchors.tsv): record the 16 school words once. Then for
any word, pick the reference it "sounds like" to copy its stem melody into the marking box, and
every new recording is compared with your own reference recordings to suggest the closest ones.
"""

from __future__ import annotations

import csv
import datetime as dt
from pathlib import Path

import gradio as gr  # module level: the row-click handler's gr.SelectData annotation must resolve
import soundfile as sf

from amasaku import anchors, documented, standard
from amasaku.marks import parse
from amasaku.wordlist import FIELDS, WORDS

ROOT = Path(__file__).resolve().parents[2]
CLIPS = ROOT / "data" / "words"
TABLE_COLS = ["#", "word", "meaning", "book marking", "recorded", "your marking", "engine suggests"]
REVIEW = "To review (recorded, not marked)"
SHOWS = ["All", REVIEW, "Not recorded yet", "Recorded"]


def load() -> list[dict]:
    with WORDS.open(encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def store(rows: list[dict]) -> None:
    tmp = WORDS.with_suffix(".tmp")
    with tmp.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS, delimiter="\t")
        w.writeheader()
        w.writerows(rows)
    tmp.replace(WORDS)


def shown(r: dict, show: str) -> bool:
    done = bool(r["recording"])
    return {"Not recorded yet": not done, "Recorded": done,
            REVIEW: done and not r["your_marking"]}.get(show, True)


def table(rows: list[dict], show: str) -> list[list]:
    out = []
    for r in rows:
        if not shown(r, show):
            continue
        out.append([r["id"], r["plain"], r["meaning"], r["book_marking"], "✅" if r["recording"] else "",
                    r["your_marking"], r["heard"]])
    return out or [["", "", "", "", "", "", ""]]


def card(rows: list[dict], i: int) -> str:
    r = rows[i]
    done = sum(bool(x["recording"]) for x in rows)
    lines = [f"### #{r['id']} · **{r['plain']}**" + (f" — {r['meaning']}" if r["meaning"] else ""),
             f"Source: {r['source']}"]
    if r["book_marking"]:
        lines.append(f"Book writes it: **{r['book_marking']}** (in its sentence; may differ said alone)")
    if r["recording"]:
        lines.append(f"Already recorded · your marking: **{r['your_marking'] or '-'}**")
    marked = sum(bool(x["recording"] and x["your_marking"]) for x in rows)
    lines.append(f"\nProgress: {done} of {len(rows)} recorded · {marked} of {done} marked by you")
    return "  \n".join(lines)


def clip_name(r: dict) -> str:
    """0031_yigeze.wav for a word, 0031-2_yigeze.wav for its second way of saying it."""
    base, _, way = r["id"].partition("-")
    return f"{int(base):04d}" + (f"-{way}" if way else "") + f"_{r['plain']}.wav"


def add_way(rows: list[dict], i: int) -> int:
    """Insert another way of saying rows[i]'s word right after its last variant; return its index."""
    base = rows[i]["id"].split("-")[0]
    family = [k for k, r in enumerate(rows) if r["id"].split("-")[0] == base]
    n = max(int(rows[k]["id"].partition("-")[2] or 1) for k in family) + 1
    first = rows[family[0]]
    new = {k: "" for k in FIELDS} | {"id": f"{base}-{n}", "plain": first["plain"], "meaning": first["meaning"],
                                    "book_marking": first["book_marking"], "source": first["source"]}
    rows.insert(family[-1] + 1, new)
    return family[-1] + 1


def step(rows: list[dict], start: int, show: str, direction: int = 1) -> int:
    """The next (or previous) word after `start` that the current filter shows."""
    n = len(rows)
    for d in range(1, n + 1):
        k = (start + direction * d) % n
        if shown(rows[k], show):
            return k
    return start


def suggestions(r: dict) -> tuple[list[tuple[str, str]], str]:
    """The engine's candidate markings for a recorded word, from the standard words, and why.

    Two ways, as at school: the whole word against the standard words it sounds most like, and
    each vowel against the standard-word vowels it sounds most like (merging). Returns radio
    choices (label, marking) and an explanation."""
    if not r["recording"] or not (ROOT / r["recording"]).exists():
        return [], ""
    wav, _ = sf.read(ROOT / r["recording"], dtype="float32")
    choices, lines = [], []
    try:
        merged, why = anchors.judge_vowels(wav, r["plain"])
    except Exception as e:  # alignment can fail on a bad clip
        merged, why = "", [f"could not judge the vowels: {e}"]
    if merged:
        choices.append((f"{merged}  (vowel by vowel)", merged))
        lines.append(f"**Vowel by vowel → {merged}**  \n" + "  \n".join(f"- {w}" for w in why))
    best = anchors.closest(wav, r["plain"])
    if best:
        lines.append("**Whole word sounds like:** " + " · ".join(
            f"{b['reference']} → {b['transfer'].marking}" for b in best))
    for b in best:
        m = b["transfer"].marking
        if all(not anchors.same_marking(m, c[1]) for c in choices):
            choices.append((f"{m}  (like {b['reference']})", m))
    return choices, "\n\n".join(lines)


def next_unrecorded(rows: list[dict], start: int) -> int:
    for k in list(range(start + 1, len(rows))) + list(range(0, start + 1)):
        if not rows[k]["recording"]:
            return k
    return start


def anchor_choices() -> list[str]:
    return [anchors.label(r) for r in anchors.load()]


def anchor_by_label(lab: str) -> tuple[list[dict], int]:
    refs = anchors.load()
    return refs, next(k for k, r in enumerate(refs) if anchors.label(r) == lab)


def anchor_clip(lab: str) -> str | None:
    if not lab:
        return None
    refs, k = anchor_by_label(lab)
    return str(ROOT / refs[k]["recording"]) if refs[k]["recording"] else None


def compare_with_anchors(wav, plain: str) -> tuple[str, str]:
    """(suggested marking, explanation) from the closest of your reference recordings, or ('', '') if none."""
    best = anchors.closest(wav, plain)
    if not best:
        return "", ""
    parts = [f"{b['reference']} → {b['transfer'].marking} (distance {b['distance']})" for b in best]
    return best[0]["transfer"].marking, "Closest reference words by your voice: " + " · ".join(parts)


def build():
    from amasaku import record

    rows0 = load()
    with gr.Blocks(title="Word Recording Table") as ui:
        with gr.Tab("Words") as words_tab:
            gr.Markdown("## Word recording table\nSay each word **alone, naturally**. Record with the microphone or "
                        "upload a file, write the correct marking (doubled vowel = long, accent = high), save. "
                        "Click any row in the table to jump to that word.")
            idx = gr.State(next_unrecorded(rows0, -1))
            with gr.Row():
                with gr.Column(scale=2):
                    info = gr.Markdown(card(rows0, next_unrecorded(rows0, -1)))
                    mine = gr.Audio(label="Your recording of this word", interactive=False)
                    with gr.Group():
                        sugg = gr.Radio([], label="Engine suggestions from the standard words: click one to use it")
                        sugg_why = gr.Markdown()
                    audio = gr.Audio(sources=["microphone", "upload"], type="numpy", label="Record again (optional)")
                    marking = gr.Textbox(label="Correct marking (e.g. inkóko)")
                    with gr.Group():
                        with gr.Row():
                            like = gr.Dropdown(anchor_choices(), label="Sounds like (reference word)", scale=3)
                            like_b = gr.Button("Use its melody", scale=1)
                        like_audio = gr.Audio(label="Reference word, your voice", interactive=False)
                    notes = gr.Textbox(label="Notes / meaning (optional)")
                    with gr.Row():
                        prev_b = gr.Button("← Previous")
                        skip_b = gr.Button("Skip")
                        save_b = gr.Button("Save & next →", variant="primary")
                    way_b = gr.Button("➕ Add another way to say this word")
                    status = gr.Markdown()
                with gr.Column(scale=3):
                    show = gr.Radio(SHOWS, value="All", label="Show")
                    grid = gr.Dataframe(value=table(rows0, "All"), headers=TABLE_COLS, interactive=False, wrap=True)
            def go(i, show_):
                rows = load()
                i = max(0, min(i, len(rows) - 1))
                r = rows[i]
                choices, why = suggestions(r)
                return (i, card(rows, i), None, r["your_marking"], r["notes"], table(rows, show_),
                        str(ROOT / r["recording"]) if r["recording"] else None,
                        gr.update(choices=choices, value=None), why)

            def save(i, audio_, mark, note, show_):
                rows = load()
                r = rows[i]
                msg = ""
                if audio_ is not None:
                    CLIPS.mkdir(parents=True, exist_ok=True)
                    wav = record.prepare(audio_)
                    path = CLIPS / clip_name(r)
                    sf.write(path, wav, record.SR)
                    r["recording"] = str(path.relative_to(ROOT))
                    try:
                        r["heard"], msg = compare_with_anchors(wav, r["plain"])
                        msg = f"Engine suggests **{r['heard']}** · {msg}" if r["heard"] else ""
                    except Exception as e:
                        msg = f"Saved, but the engine could not measure it: {e}"
                mark = (mark or "").replace("-", "")
                if mark.strip():
                    if parse(mark.strip())[0] != r["plain"]:
                        return (i, card(rows, i), audio_, mark, note, table(rows, show_), gr.update(), gr.update(),
                                gr.update(), f"'{mark}' does not spell '{r['plain']}': only the marks may differ.")
                    r["your_marking"] = mark.strip()
                r["notes"] = note or r["notes"]
                store(rows)
                if r["your_marking"] and r["recording"]:
                    from amasaku.workbench import append_gold
                    append_gold({"time": dt.datetime.now().isoformat(timespec="seconds"), "word": r["plain"],
                                 "marking": r["your_marking"], "context": r["meaning"], "source": "native speaker (studio)",
                                 "notes": f"tool heard {r['heard']}; {r['notes']}", "recording": r["recording"]})
                agree = ""
                if r["your_marking"] and r["heard"]:
                    agree = (" ✅ engine had it right" if anchors.same_marking(r["your_marking"], r["heard"])
                             else f" ❌ engine said {r['heard']}")
                j = step(rows, i, show_) if show_ in (REVIEW, "Recorded") else next_unrecorded(rows, i)
                return go(j, show_) + (f"Saved #{r['id']} {r['plain']}. {msg}{agree}",)

            def pick(evt: gr.SelectData, show_):
                rows = load()
                wid = table(rows, show_)[evt.index[0]][0]
                i = next((k for k, r in enumerate(rows) if r["id"] == str(wid)), 0)
                return go(i, show_)

            def another(i, show_):
                rows = load()
                j = add_way(rows, i)
                store(rows)
                out = go(j, show_)
                return out[:4] + (f"another way to say #{rows[i]['id']}",) + out[5:] + (
                    f"Added #{rows[j]['id']} {rows[j]['plain']}: record this way of saying it and mark it. "
                    "Use the notes to say when/where it is said this way.",)

            def use_melody(i, lab, mark):
                if not lab:
                    return mark, None, "Pick a reference word first."
                refs, k = anchor_by_label(lab)
                ref = refs[k]
                plain = load()[i]["plain"]
                target = mark.strip() if mark and parse(mark.strip().replace("-", ""))[0] == plain else plain
                t = anchors.transfer(ref["example"], target)
                return t.marking, anchor_clip(lab), f"Like **{ref['example']}** → **{t.marking}**  \n" + "  \n".join(
                    f"- {n}" for n in t.notes) + "\n\nListen to both; correct the marking if it is not right, then save."

            like_b.click(use_melody, [idx, like, marking], [marking, like_audio, status])
            like.change(anchor_clip, [like], [like_audio])
            words_tab.select(lambda: gr.update(choices=anchor_choices()), [], [like])

            outs = [idx, info, audio, marking, notes, grid, mine, sugg, sugg_why]
            save_b.click(save, [idx, audio, marking, notes, show], outs + [status])
            skip_b.click(lambda i, s: go(step(load(), i, s), s), [idx, show], outs)
            way_b.click(another, [idx, show], outs + [status])
            prev_b.click(lambda i, s: go(step(load(), i, s, -1), s), [idx, show], outs)
            show.change(lambda s: go(step(load(), -1, s), s), [show], outs)
            grid.select(pick, [show], outs)
            sugg.change(lambda m, cur: m or cur, [sugg, marking], [marking])
            ui.load(lambda i, s: go(i, s), [idx, show], outs)
        with gr.Tab("Standard words"):
            standard.build_tab(record.prepare, record.SR)
        with gr.Tab("Documented words"):
            documented.build_tab(record.prepare, record.SR)
    return ui


def main() -> None:
    build().launch(server_name="127.0.0.1", server_port=7862)


if __name__ == "__main__":
    main()
