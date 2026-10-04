"""Step 1: align speech to letters and measure every vowel's duration and pitch.

    uv run amasaku-align                      # Mwanafunzi held-out clips -> out/aligned.json + out/view.html
    uv run amasaku-align --check              # also test against known length rules (Myers & Hansen 2005)

Durations come from the Mwanafunzi voice model itself (monotonic alignment search, as in
its training), which assigns every 16 ms of real audio to a letter. (The CTC speech
recognizer, --aligner ctc, only gives one-frame spikes: unusable for length.) Pitch (pYIN,
10 ms) is summarized per vowel, and per half for long vowels (H on the first mora =
falling, on the second = rising).
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

import numpy as np
import soundfile as sf
import torch

ROOT = Path(__file__).resolve().parents[2]
TTS = ROOT.parent / "kinyarwanda-tts"
SR = 16000
VOWELS = set("aeiou")


class Aligner:
    def __init__(self):
        from kintts.data.prepare import ASR

        self.asr = ASR()
        self.tok = self.asr.proc.tokenizer
        self.blank = self.tok.pad_token_id
        self.delim = self.tok.convert_tokens_to_ids(self.tok.word_delimiter_token)

    def emissions(self, wav: np.ndarray) -> torch.Tensor:
        inputs = self.asr.proc(wav, sampling_rate=SR, return_tensors="pt").to(self.asr.dev)
        with torch.no_grad():
            return self.asr.model(**inputs).logits.log_softmax(-1).cpu()

    def align(self, wav: np.ndarray, text: str) -> list[dict]:
        """Letters of `text` with start/end times (s). Words separated by spaces."""
        import torchaudio.functional as F

        words = [w for w in re.sub(r"[^a-z' ]", " ", text.lower()).split() if w]
        vocab = self.tok.get_vocab()
        targets, owner = [], []  # owner[i] = (word index, letter) for each target token
        for wi, w in enumerate(words):
            if wi:
                targets.append(self.delim)
                owner.append(None)
            for ch in w:
                if ch in vocab:
                    targets.append(vocab[ch])
                    owner.append((wi, ch))
        em = self.emissions(wav)
        frame_s = len(wav) / SR / em.shape[1]
        ali, scores = F.forced_align(em, torch.tensor([targets], dtype=torch.int32), blank=self.blank)
        spans = F.merge_tokens(ali[0], scores[0].exp())
        letters = []
        for k, (span, own) in enumerate(zip(spans, owner)):
            if own is None:
                continue
            nxt = spans[k + 1].start if k + 1 < len(spans) else span.end
            letters.append({"word": own[0], "ch": own[1], "start": span.start * frame_s,
                            "end": max(nxt, span.end) * frame_s, "conf": float(span.score)})
        return letters


class VitsAligner:
    """Letter durations from the voice model itself (monotonic alignment search, as in VITS
    training): every 16 ms frame of real audio is assigned to the letter whose learned sound
    it matches best. Unlike CTC (one-frame spikes), this gives each letter its true extent."""

    def __init__(self, model_dir: Path = TTS / "checkpoints" / "mwanafunzi-v3"):
        from transformers import AutoTokenizer, VitsModel

        self.model = VitsModel.from_pretrained(model_dir).eval()
        self.tok = AutoTokenizer.from_pretrained(model_dir)
        self.hop = int(np.prod(self.model.config.upsample_rates))

    def spectrogram(self, wav: np.ndarray) -> torch.Tensor:
        x = torch.from_numpy(wav).float()[None]
        pad = (1024 - self.hop) // 2
        x = torch.nn.functional.pad(x[:, None], (pad, pad), mode="reflect")[:, 0]
        spec = torch.stft(x, 1024, hop_length=self.hop, win_length=1024, window=torch.hann_window(1024),
                          center=False, return_complex=True)
        return torch.sqrt(spec.real ** 2 + spec.imag ** 2 + 1e-6)  # (1, 513, frames)

    @staticmethod
    def monotonic_path(score: np.ndarray) -> np.ndarray:
        """Best monotonic assignment of frames (columns) to tokens (rows): token index per frame."""
        n_tok, n_fr = score.shape
        value = np.full(n_tok, -np.inf)
        value[0] = score[0, 0]
        stay = np.zeros((n_fr, n_tok), dtype=bool)
        for y in range(1, n_fr):
            moved = np.concatenate([[-np.inf], value[:-1]])
            stay[y] = value >= moved
            value = np.maximum(value, moved) + score[:, y]
            value[np.arange(n_tok) > y] = -np.inf
        path, x = np.zeros(n_fr, dtype=int), n_tok - 1
        for y in range(n_fr - 1, -1, -1):
            path[y] = x
            if y and not stay[y, x]:
                x -= 1
        return path

    def align(self, wav: np.ndarray, text: str) -> list[dict]:
        m = self.model
        ids = self.tok(text.lower(), return_tensors="pt")["input_ids"]
        spec = self.spectrogram(wav)
        with torch.no_grad():
            enc = m.text_encoder(input_ids=ids, padding_mask=torch.ones(1, ids.shape[1], 1))
            mean_p, logs_p = enc.prior_means.transpose(1, 2), enc.prior_log_variances.transpose(1, 2)
            mask = torch.ones(1, 1, spec.shape[-1])
            _, mean_q, _ = m.posterior_encoder(spec, mask)
            z_p = m.flow(mean_q, mask, reverse=False)
            inv = torch.exp(-2 * logs_p)                                   # (1, C, T_text)
            score = (torch.sum(-0.5 * np.log(2 * np.pi) - logs_p, 1, keepdim=True).transpose(1, 2)
                     + torch.matmul(-0.5 * (z_p ** 2).transpose(1, 2), inv).transpose(1, 2)
                     + torch.matmul((mean_p * inv).transpose(1, 2), z_p)
                     + torch.sum(-0.5 * mean_p ** 2 * inv, 1, keepdim=True).transpose(1, 2))[0]
        path = self.monotonic_path(score.numpy())
        frames = np.bincount(path, minlength=ids.shape[1])
        tokens = self.tok.convert_ids_to_tokens(ids[0])
        frame_s = self.hop / SR
        starts = np.concatenate([[0], np.cumsum(frames)[:-1]])
        letters, word = [], 0
        for k, t in enumerate(tokens):
            if t == self.tok.pad_token:  # blank between letters: its time goes to the letter before it
                continue
            if t == " ":
                word += 1
                continue
            n = frames[k] + (frames[k + 1] if k + 1 < len(tokens) and tokens[k + 1] == self.tok.pad_token else 0)
            letters.append({"word": word, "ch": t, "start": starts[k] * frame_s,
                            "end": (starts[k] + n) * frame_s, "conf": 1.0})
        return letters


def pitch_track(wav: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    import librosa

    f0, voiced, _ = librosa.pyin(wav, fmin=60, fmax=400, sr=SR, frame_length=1024, hop_length=160)
    times = np.arange(len(f0)) * 0.01
    return times, np.where(voiced, f0, np.nan)


def measure(wav: np.ndarray, letters: list[dict], words: list[str]) -> list[dict]:
    """Vowel records: duration, median pitch (semitones vs the clip's median), per-half pitch, context."""
    times, f0 = pitch_track(wav)
    ref = np.nanmedian(f0)
    st = 12 * np.log2(f0 / ref)
    out = []
    for i, l in enumerate(letters):
        if l["ch"] not in VOWELS:
            continue
        sel = (times >= l["start"]) & (times < l["end"])
        mid = (l["start"] + l["end"]) / 2
        first = st[sel & (times < mid)]
        second = st[sel & (times >= mid)]
        word = words[l["word"]]
        same_word = [x for x in letters if x["word"] == l["word"]]
        pos = same_word.index(l)
        nxt = same_word[pos + 1]["ch"] if pos + 1 < len(same_word) else ""
        nxt2 = same_word[pos + 2]["ch"] if pos + 2 < len(same_word) else ""
        prv = same_word[pos - 1]["ch"] if pos > 0 else ""
        prv2 = same_word[pos - 2]["ch"] if pos > 1 else ""
        if pos == 0:
            context = "word-initial"
        elif pos == len(same_word) - 1:
            context = "word-final"
        elif nxt in "mn" and nxt2 and nxt2 not in VOWELS and nxt2 not in "y":
            context = "before NC"          # long by rule
        elif prv in "wy" and prv2 and prv2 not in VOWELS:
            context = "after CG"           # long by rule
        else:
            context = "contrastive"        # length depends on the word
        out.append({"word": l["word"], "w": word, "i": i, "ch": l["ch"], "start": round(l["start"], 3),
                    "dur": round(l["end"] - l["start"], 3),
                    "pitch": None if np.all(np.isnan(st[sel])) else round(float(np.nanmedian(st[sel])), 2),
                    "pitch_1": None if np.all(np.isnan(first)) else round(float(np.nanmedian(first)), 2),
                    "pitch_2": None if np.all(np.isnan(second)) else round(float(np.nanmedian(second)), 2),
                    "context": context, "conf": round(l["conf"], 2)})
    return out


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--set", type=Path, default=TTS / "eval" / "heldout" / "set.json")
    ap.add_argument("--out", type=Path, default=ROOT / "out")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--aligner", choices=["vits", "ctc"], default="vits")
    args = ap.parse_args()

    items = json.loads(args.set.read_text())
    aligner = VitsAligner() if args.aligner == "vits" else Aligner()
    args.out.mkdir(parents=True, exist_ok=True)
    results = []
    for it in items:
        wav, _ = sf.read(args.set.parent / f"{it['id']}.wav", dtype="float32")
        words = it["text"].split()
        letters = aligner.align(wav, it["text"])
        vowels = measure(wav, letters, words)
        # phrase-final lengthening: the last 2 vowels of a clip are excluded from length statistics
        for v in vowels[-2:]:
            v["context"] += " (phrase-final)"
        results.append({"id": it["id"], "text": it["text"], "wav": str(args.set.parent / f"{it['id']}.wav"),
                        "vowels": vowels})
    (args.out / "aligned.json").write_text(json.dumps(results, ensure_ascii=False, indent=1))
    print(f"{len(results)} clips, {sum(len(r['vowels']) for r in results)} vowels -> {args.out / 'aligned.json'}")

    if args.check:
        groups: dict[str, list[float]] = {}
        for r in results:
            rate = np.median([v["dur"] for v in r["vowels"]])  # normalize by this clip's speaking rate
            for v in r["vowels"]:
                groups.setdefault(v["context"], []).append(v["dur"] / rate)
        print("\nVowel duration relative to the clip's median vowel (literature: before NC / after CG long,"
              " word-initial / word-final short):")
        for k in ["word-initial", "word-final", "contrastive", "before NC", "after CG"]:
            d = np.array(groups.get(k, []))
            if len(d):
                print(f"  {k:14s} n={len(d):4d}  median {np.median(d):.2f}  mean {d.mean():.2f}")


if __name__ == "__main__":
    main()
