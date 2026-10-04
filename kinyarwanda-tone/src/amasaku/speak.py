"""Speak marked text: the voice says exactly the ubutinde and amasaku written.

1. Length: the v3 voice decides how long each letter lasts; for a long vowel (aa) that
   duration is multiplied by `long_factor` before the audio is generated.
2. Tone: Praat (PSOLA) reshapes the pitch so every mora sits on a target relative to the
   sentence's own falling line: High = line + `high_st` semitones (each later H a little lower:
   downstep), Low = line - `low_st`. A long vowel gets two targets: áa = falls, aá = rises.
   Consonants are left to smooth interpolation between vowel targets.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import numpy as np

from amasaku.marks import Vowel, parse

TTS = Path(__file__).resolve().parents[3] / "kinyarwanda-tts"


@dataclass
class Settings:
    long_factor: float = 1.9   # long vowel = ~2 beats
    high_st: float = 2.0       # H above the falling line, semitones (4 sounded too high to the native speaker)
    low_st: float = 1.0        # L below the falling line
    downstep_st: float = 0.6   # each later H in the sentence is this much lower
    fall_st: float = 3.0       # how much the line falls over the sentence (measured ~3-4 st)
    speed: float = 0.85
    reshape_pitch: bool = True
    initial_min_frames: int = 0  # minimum length (16 ms frames) of word-initial vowels (lengthening did not help)
    integuza: float = 0.0      # REB Y2 TG p.114: the vowel before a high is also said high (share of high_st;
                               # 0 = off). Seen in native recordings (peak on the vowel before the high 23/54)
    final_low: bool = False    # a high on the last vowel of an utterance is said low (Jarnow 2020; REB p.114),
                               # only its integuza shows
    initial_gap_db: float = 3.0  # word-initial vowels quieter than the next vowel by more than this are raised:
                                 # the voice whispers them ("umwáana" heard as "mwana", 18 dB quieter)


class Speaker:
    def __init__(self, model: str = "mwanafunzi-v3"):
        from kintts.compare import HFVits

        self.tts = HFVits(str(TTS / "checkpoints" / model), use_normalizer=False)
        self.tts.model.noise_scale, self.tts.model.noise_scale_duration = 0.3, 0.6
        self.tts.min_letter_frames, self.tts.denoise = 1, 0.0
        self.sr = self.tts.sr
        import torch
        self.torch = torch
        self.hop = int(np.prod(self.tts.model.config.upsample_rates))
        self._scale = None  # per-token log-duration additions for the current utterance
        self._floor = None  # per-token minimum log-duration (-inf = none)
        self.tts.model.duration_predictor.register_forward_hook(self._lengthen)

    def _lengthen(self, _m, _i, log_duration):
        if self._scale is None:
            return None
        out = log_duration + self._scale.view(1, 1, -1)
        return out if self._floor is None else self.torch.maximum(out, self._floor.view(1, 1, -1))

    def say(self, marked: str, s: Settings = Settings(), control_words: set[int] | None = None,
            length_factors: list[float] | None = None, targets: list[list[tuple[float, float]]] | None = None
            ) -> tuple[np.ndarray, dict]:
        """control_words: indices of words whose marks are imposed (None = every word). Other
        words keep the voice's own melody, so a test word can be heard inside a real sentence.
        length_factors / targets: per-vowel length multipliers and pitch targets
        [(position 0-1 in the vowel, semitones vs the voice's middle)], e.g. copied from a
        native speaker's recording; they replace the ones derived from the marks."""
        import torch

        plain, vowels = parse(marked)
        text = plain + (f" {self._end_mark()}" if self.tts.markers else "")
        ids = self.tts.tok(text, return_tensors="pt")["input_ids"][0]
        tokens = self.tts.tok.convert_ids_to_tokens(ids)
        vowel_tok = [k for k, t in enumerate(tokens) if t in "aeiou"][: len(vowels)]

        scale = torch.zeros(len(ids))
        blank = self.tts.tok.pad_token
        for i, (k, v) in enumerate(zip(vowel_tok, vowels)):
            factor = length_factors[i] if length_factors else (s.long_factor if v.long else 1.0)
            if factor != 1.0:  # the vowel's sound is shared by its letter and the blank after it: stretch both
                scale[k] = float(np.log(factor))
                if k + 1 < len(tokens) and tokens[k + 1] == blank:
                    scale[k + 1] = float(np.log(factor))
        self._scale = scale
        floor = torch.full((len(ids),), -float("inf"))
        if s.initial_min_frames:
            space = self.tts.tok.convert_tokens_to_ids(" ")
            for k in vowel_tok:  # vowel at the start of a word (after a space or at the very start)
                prev = [t for t in ids[:k].tolist() if t != self.tts.tok.pad_token_id]
                if not prev or prev[-1] == space:
                    floor[k] = float(np.log(s.initial_min_frames * s.speed * 0.999))
        self._floor = floor
        captured = {}
        hook = self.tts.model.duration_predictor.register_forward_hook(
            lambda _m, _i, out: captured.__setitem__("d", out))
        self.tts.model.speaking_rate = s.speed
        try:
            wav = self.tts._generate(text).astype(np.float64)
        finally:
            hook.remove()
            self._scale = self._floor = None
        dur = torch.ceil(torch.exp(captured["d"]) / s.speed).flatten().numpy()
        ends = np.cumsum(dur) * self.hop / self.sr
        starts = ends - dur * self.hop / self.sr
        spans = [(starts[k], ends[k] + (dur[k + 1] * self.hop / self.sr if k + 1 < len(dur) else 0))
                 for k in vowel_tok]
        word_of = [i for i, w in enumerate(plain.split()) for ch in w if ch in "aeiou"]
        controlled = [control_words is None or word_of[k] in control_words for k in range(len(vowels))]
        info = {"plain": plain, "vowels": vowels, "spans": spans, "controlled": controlled}
        if s.reshape_pitch and vowels:
            wav = self._reshape(wav, vowels, spans, s, controlled, targets)
        if s.initial_gap_db:
            wav = self._lift_initial_vowels(wav, vowels, spans, word_of, s.initial_gap_db)
        return wav.astype(np.float32), info

    def _lift_initial_vowels(self, wav: np.ndarray, vowels: list[Vowel], spans: list, word_of: list[int],
                             gap_db: float, max_gain_db: float = 15.0) -> np.ndarray:
        """Bring each word-initial vowel to within `gap_db` of the next vowel of the same word."""
        def level(a, b):
            seg = wav[int(a * self.sr):int(b * self.sr)]
            return 20 * np.log10(np.sqrt(np.mean(seg ** 2)) + 1e-9) if len(seg) else None

        gain = np.ones(len(wav))
        ramp = int(0.02 * self.sr)
        for k in range(len(vowels) - 1):
            first_of_word = k == 0 or word_of[k - 1] != word_of[k]
            if not first_of_word or word_of[k + 1] != word_of[k]:
                continue
            (a, b), (c, d) = spans[k], spans[k + 1]
            mine, nxt = level(a, b), level(c, d)
            if mine is None or nxt is None or nxt - mine <= gap_db:
                continue
            g = 10 ** (min(nxt - gap_db - mine, max_gain_db) / 20)
            i0, i1 = int(a * self.sr), int(b * self.sr)
            gain[i0:i1] = np.maximum(gain[i0:i1], g)
            lo = max(0, i0 - ramp)
            gain[lo:i0] = np.maximum(gain[lo:i0], np.linspace(1, g, i0 - lo))
            gain[i1:i1 + ramp] = np.maximum(gain[i1:i1 + ramp], np.linspace(g, 1, len(gain[i1:i1 + ramp])))
        return wav * gain

    def _end_mark(self) -> str:
        from kintts.data.prepare import ASR
        return ASR.END_MARK

    def _reshape(self, wav: np.ndarray, vowels: list[Vowel], spans: list, s: Settings,
                 controlled: list[bool], targets: list | None = None) -> np.ndarray:
        import parselmouth
        from parselmouth.praat import call

        snd = parselmouth.Sound(wav, sampling_frequency=self.sr)
        pitch = snd.to_pitch(0.01, 60, 400)
        f0 = pitch.selected_array["frequency"]
        voiced = f0[f0 > 0]
        if len(voiced) < 5:
            return wav
        mid_hz = float(np.median(voiced))
        total = snd.duration
        manip = call(snd, "To Manipulation", 0.01, 60, 400)
        tier = call(manip, "Extract pitch tier")
        call(tier, "Remove points between", 0, total)

        def line(t: float) -> float:  # semitones relative to the speaker's middle, falling over the sentence
            return s.fall_st / 2 - s.fall_st * (t / max(total, 1e-3))

        # keep the voice's own melody everywhere except inside the vowels we control
        # (every 50 ms, median-smoothed: raw 20 ms points made the melody jumpy, "breathless")
        busy = [(a - 0.03, b + 0.03) for (a, b), c in zip(spans, controlled) if c]
        xs = pitch.xs()
        for j in range(0, len(f0), 5):
            window = f0[max(0, j - 3):j + 4]
            window = window[window > 0]
            t = xs[j]
            if len(window) >= 3 and not any(a <= t <= b for a, b in busy):
                call(tier, "Add point", float(t), float(np.median(window)))
        n_high = 0
        raised = [s.integuza > 0 and i + 1 < len(vowels) and vowels[i + 1].moras[0] == "H"
                  for i in range(len(vowels))]
        for i, (v, (a, b), c) in enumerate(zip(vowels, spans, controlled)):
            if not c:
                continue
            if targets is not None:
                for pos, st in targets[i]:
                    call(tier, "Add point", float(a + (b - a) * pos), float(mid_hz * 2 ** (st / 12)))
                continue
            centers = [(a + b) / 2] if not v.long else [a + (b - a) * 0.3, a + (b - a) * 0.75]
            last = i == len(vowels) - 1
            for k, (mora, t) in enumerate(zip(v.moras, centers)):
                if mora == "H" and not (s.final_low and last):
                    st = line(t) + s.high_st - s.downstep_st * n_high
                    n_high += 1
                elif raised[i] and (k == len(v.moras) - 1 or not v.long):
                    st = line(t) - s.low_st + s.integuza * (s.high_st + s.low_st)  # integuza before a high
                else:
                    st = line(t) - s.low_st
                call(tier, "Add point", float(t), float(mid_hz * 2 ** (st / 12)))
        call([tier, manip], "Replace pitch tier")
        out = call(manip, "Get resynthesis (overlap-add)")
        y = out.values[0]
        return y[: len(wav)] if len(y) >= len(wav) else np.pad(y, (0, len(wav) - len(y)))
