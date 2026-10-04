"""Amasaku workbench: type a word with marks, hear it, compare the variations, save the right one.

    uv run amasaku-workbench        # http://127.0.0.1:7861

Tab 1 "Say it": any marked text (inzáara, umwaámi, ...) spoken exactly as written.
Tab 2 "Find the right marking": a plain word (optionally inside a sentence) is spoken in every
      candidate marking, length first, then tone; pick the right one or type the correction.
Tab 3 "Answer key": everything saved so far (data/gold.csv): the ground truth for training.
Tab 4 "Compare with my voice": record or upload yourself saying it; see both melodies vowel by
      vowel, get the marking heard in your voice, hear Mwanafunzi say it with your melody, save.
"""

from __future__ import annotations

import csv
import datetime as dt
from pathlib import Path

import numpy as np

from amasaku.marks import parse, variants
from amasaku.speak import Settings, Speaker

ROOT = Path(__file__).resolve().parents[2]
GOLD = ROOT / "data" / "gold.csv"
FIELDS = ["time", "word", "marking", "context", "source", "notes", "recording"]
RECORDINGS = ROOT / "data" / "recordings"
MAX_VARIANTS = 8
_speaker: Speaker | None = None


def speaker() -> Speaker:
    global _speaker
    if _speaker is None:
        _speaker = Speaker()
    return _speaker


def settings(long_factor, high_st, downstep_st, speed) -> Settings:
    return Settings(long_factor=long_factor, high_st=high_st, downstep_st=downstep_st, speed=speed)


def describe(marked: str) -> str:
    """Human-readable reading: each vowel, long or short, and which mora is high."""
    _, vowels = parse(marked)
    parts = []
    for v in vowels:
        if v.long:
            kind = {"HL": "long, falling (áa)", "LH": "long, rising (aá)", "HH": "long, high", "LL": "long, low"}
            parts.append(f"{v.letter}: {kind[''.join(v.moras)]}")
        else:
            parts.append(f"{v.letter}: short, {'high' if v.moras == ['H'] else 'low'}")
    return " · ".join(parts)


def say(text, long_factor, high_st, downstep_st, speed, keep_melody):
    if not text.strip():
        return None, ""
    sp = speaker()
    s = settings(long_factor, high_st, downstep_st, speed)
    s.reshape_pitch = not keep_melody
    wav, _ = sp.say(text.strip(), s)
    return (sp.sr, wav), describe(text.strip())


def find(word, context, long_factor, high_st, downstep_st, speed):
    word = word.strip().lower()
    if not word:
        return [None] * MAX_VARIANTS + [""] * MAX_VARIANTS + [None]
    sp = speaker()
    s = settings(long_factor, high_st, downstep_st, speed)
    cands = variants(word, MAX_VARIANTS)
    audios, labels = [], []
    for c in cands:
        if context and "{}" in context:
            sentence = context.replace("{}", c)
            idx = {context.split().index(next(w for w in context.split() if "{}" in w))}
            wav, _ = sp.say(sentence, s, control_words=idx)
        else:
            wav, _ = sp.say(c, s)
        audios.append((sp.sr, wav))
        labels.append(f"**{c}**  ·  {describe(c)}")
    pad = MAX_VARIANTS - len(cands)
    import gradio as gr
    return audios + [None] * pad + labels + [""] * pad + [gr.update(choices=cands, value=None)]


def append_gold(row: dict) -> None:
    GOLD.parent.mkdir(parents=True, exist_ok=True)
    new = not GOLD.exists()
    with GOLD.open("a", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS, extrasaction="ignore")
        if new:
            w.writeheader()
        w.writerow({k: row.get(k, "") for k in FIELDS})


def analyse(audio, text, long_factor, high_st, downstep_st, speed):
    """Measure the speaker's recording, suggest its marking, and make the voice copy it."""
    from amasaku import record

    if audio is None or not text.strip():
        return "Record or upload yourself, and type what you said.", "", None, None
    sp = speaker()
    s = settings(long_factor, high_st, downstep_st, speed)
    mine = record.prepare(audio)
    try:
        measured = record.measure_recording(mine, text.strip())
        suggestion, reasons = record.contour_suggest(mine, text.strip(), measured)
        copied = record.say_like(sp, text.strip(), measured, s)
    except Exception as e:  # e.g. the recording does not match the text
        return f"Could not analyse the recording: {e}", "", None, None
    from amasaku.align import measure

    engine_wav, info = sp.say(suggestion, s)
    engine_letters = record.aligner().align(engine_wav, parse(suggestion)[0])
    engine_measured = measure(engine_wav, engine_letters, parse(suggestion)[0].split())
    svg = record.bars_svg([("You", measured, "#c0392b"), ("Engine", engine_measured, "#2e6f9e")])
    text_out = (f"**Heard in your voice:** {suggestion}  \n{describe(suggestion)}  \n"
                + "  \n".join(f"- {r}" for r in reasons)
                + "  \nNote: a word at the very end of a statement is lowered (no high tone utterance-finally), "
                  "so its rise may be flattened.")
    return text_out, svg, (sp.sr, copied), (sp.sr, engine_wav)


def save_recording(audio, text, marking, notes):
    from amasaku import record
    import soundfile as sf

    if audio is None or not text.strip() or not marking.strip():
        return "Need the recording, the text and the confirmed marking.", load_gold()
    if parse(marking.strip())[0] != parse(text.strip())[0]:
        return f"'{marking}' does not spell '{text}' (only the marks may differ).", load_gold()
    RECORDINGS.mkdir(parents=True, exist_ok=True)
    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    path = RECORDINGS / f"{stamp}_{parse(text.strip())[0].replace(' ', '_')[:40]}.wav"
    sf.write(path, record.prepare(audio), record.SR)
    append_gold({"time": dt.datetime.now().isoformat(timespec="seconds"), "word": parse(marking)[0],
                 "marking": marking.strip(), "context": "", "source": "native speaker (recorded)",
                 "notes": notes or "", "recording": str(path.relative_to(ROOT))})
    return f"Saved: {marking.strip()} with your recording.", load_gold()


def save(word, choice, typed, context, notes):
    marking = (typed or "").strip() or choice
    if not word.strip() or not marking:
        return "Pick a variation or type the correct marking first.", load_gold()
    plain, _ = parse(marking)
    if plain.replace(" ", "") != parse(word.strip().lower())[0].replace(" ", ""):
        return f"'{marking}' does not spell the word '{word}' (letters must match, only marks differ).", load_gold()
    append_gold({"time": dt.datetime.now().isoformat(timespec="seconds"), "word": parse(marking)[0],
                 "marking": marking, "context": context or "", "source": "native speaker", "notes": notes or ""})
    return f"Saved: {marking}", load_gold()


def load_gold():
    if not GOLD.exists():
        return [[""] * len(FIELDS)]
    with GOLD.open(encoding="utf-8") as f:
        return [[r.get(k, "") for k in FIELDS] for r in csv.DictReader(f)] or [[""] * len(FIELDS)]


def build():
    import gradio as gr

    with gr.Blocks(title="Amasaku Workbench") as ui:
        gr.Markdown("## Amasaku n'ubutinde workbench\nDoubled vowel = long (ubutinde). Accent = high tone "
                    "(isaku): **áa** falls, **aá** rises. Mwanafunzi v3 voice; pitch reshaped with Praat.")
        with gr.Accordion("Sound settings", open=False):
            with gr.Row():
                long_factor = gr.Slider(1.3, 2.6, value=1.9, step=0.1, label="Long vowel length (x)")
                high_st = gr.Slider(0.5, 6, value=2, step=0.5, label="High tone height (semitones)")
                downstep_st = gr.Slider(0, 2, value=0.6, step=0.1, label="Downstep per high tone")
                speed = gr.Slider(0.6, 1.2, value=0.85, step=0.05, label="Speed")
        knobs = [long_factor, high_st, downstep_st, speed]

        with gr.Tab("1. Say it"):
            text = gr.Textbox(label="Marked text", value="umwáana w'umwaámi")
            keep = gr.Checkbox(False, label="Ignore tone marks (hear the voice's own melody, length still applied)")
            go = gr.Button("Say it", variant="primary")
            audio = gr.Audio(label="Result", autoplay=True)
            reading = gr.Markdown()
            go.click(say, [text] + knobs + [keep], [audio, reading])
            text.submit(say, [text] + knobs + [keep], [audio, reading])

        with gr.Tab("2. Find the right marking"):
            with gr.Row():
                word = gr.Textbox(label="Word (plain, no marks)", value="inzara")
                context = gr.Textbox(label="Optional sentence, {} = the word",
                                     placeholder="abantu benshi bari bafite {} cyane")
            run = gr.Button("Generate the variations", variant="primary")
            slots = []
            for i in range(MAX_VARIANTS):
                with gr.Row():
                    a = gr.Audio(label=f"Variation {i + 1}", scale=2)
                    l = gr.Markdown()
                slots.append((a, l))
            choice = gr.Radio([], label="Which one is right?")
            typed = gr.Textbox(label="...or type the correct marking (e.g. inzáara)")
            notes = gr.Textbox(label="Notes (meaning, dialect, why)")
            keep_btn = gr.Button("Save to the answer key")
            status = gr.Markdown()
            run.click(find, [word, context] + knobs, [a for a, _ in slots] + [l for _, l in slots] + [choice])

        with gr.Tab("4. Compare with my voice"):
            gr.Markdown("Record yourself (or upload a file) saying a word or short sentence, and type exactly "
                        "what you said. Then compare your melody with the engine's, vowel by vowel.")
            with gr.Row():
                my_audio = gr.Audio(sources=["microphone", "upload"], type="numpy", label="Your voice")
                my_text = gr.Textbox(label="What you said (plain or marked)", value="umwana")
            check = gr.Button("Analyse my recording", variant="primary")
            found = gr.Markdown()
            chart = gr.HTML()
            with gr.Row():
                copied = gr.Audio(label="Mwanafunzi's voice with YOUR melody")
                engine = gr.Audio(label="Mwanafunzi's voice with the marking heard in your voice")
            confirmed = gr.Textbox(label="Correct marking (edit the suggestion if needed)")
            rec_notes = gr.Textbox(label="Notes")
            rec_save = gr.Button("Save recording + marking to the answer key")
            rec_status = gr.Markdown()
            check.click(analyse, [my_audio, my_text] + knobs, [found, chart, copied, engine])

        with gr.Tab("3. Answer key"):
            table = gr.Dataframe(value=load_gold, headers=FIELDS, interactive=False, wrap=True)
            gr.Markdown(f"Saved in `{GOLD}`.")
        keep_btn.click(save, [word, choice, typed, context, notes], [status, table])
        rec_save.click(save_recording, [my_audio, my_text, confirmed, rec_notes], [rec_status, table])
    return ui


def main() -> None:
    build().launch(server_name="127.0.0.1", server_port=7861)


if __name__ == "__main__":
    main()
