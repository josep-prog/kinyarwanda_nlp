"""Reading the scientific notation: doubled vowel = long (ubutinde), accent = high tone (isaku).

    inzáara -> plain "inzara"; vowels: i (short, L), a (long, H on mora 1 = falling), a (short, L)
    umwaámi -> plain "umwami";  vowels: u, a (long, H on mora 2 = rising), i
Both the acute (á) and the circumflex (â, older dictionaries) are read as High.
"""

from __future__ import annotations

import unicodedata
from dataclasses import dataclass

BASE = "aeiou"
HIGH_MARKS = {"́", "̂"}  # combining acute, circumflex


@dataclass
class Vowel:
    letter: str
    moras: list[str]  # "H" / "L" per mora; 2 moras = long

    @property
    def long(self) -> bool:
        return len(self.moras) == 2

    def written(self) -> str:
        return "".join(self.letter + ("́" if m == "H" else "") for m in self.moras)


def _chars(word: str) -> list[tuple[str, bool]]:
    """(base letter, high?) for each character, accents separated."""
    out = []
    for ch in unicodedata.normalize("NFD", word.lower()):
        if ch in HIGH_MARKS and out:
            out[-1] = (out[-1][0], True)
        elif unicodedata.category(ch) != "Mn":
            out.append((ch, False))
    return out


def parse(text: str) -> tuple[str, list[Vowel]]:
    """Marked text -> (plain text the voice reads, one Vowel per vowel of the plain text)."""
    plain_words, vowels = [], []
    for word in text.split():
        chars = _chars(word)
        plain, i = "", 0
        while i < len(chars):
            ch, high = chars[i]
            if ch in BASE:
                moras = ["H" if high else "L"]
                if i + 1 < len(chars) and chars[i + 1][0] == ch:  # doubled vowel = one long vowel
                    moras.append("H" if chars[i + 1][1] else "L")
                    i += 1
                vowels.append(Vowel(ch, moras))
            plain += ch
            i += 1
        plain_words.append(plain)
    return " ".join(plain_words), vowels


def write(plain: str, vowels: list[Vowel]) -> str:
    """Plain text + vowel specs -> marked text (NFC)."""
    out, k = "", 0
    for ch in plain:
        if ch in BASE and k < len(vowels):
            out += vowels[k].written()
            k += 1
        else:
            out += ch
    return unicodedata.normalize("NFC", out)


def variants(word: str, max_variants: int = 10) -> list[str]:
    """Candidate markings of a plain word, the way a student tries them out: first length
    (which vowel is long?), then tone (which mora is high?), on the stem (last two vowels)."""
    plain, base = parse(word)
    n = len(base)
    stem = list(range(max(0, n - 2), n))
    seen, out = set(), []

    def add(vs: list[Vowel]) -> None:
        w = write(plain, vs)
        if w not in seen:
            seen.add(w)
            out.append(w)

    add([Vowel(v.letter, ["L"]) for v in base])                      # no length, no high
    for i in stem:                                                     # one short high
        vs = [Vowel(v.letter, ["L"]) for v in base]
        vs[i].moras = ["H"]
        add(vs)
    for i in stem[:-1] or stem:                                        # long vowel: none / falling / rising
        for moras in (["L", "L"], ["H", "L"], ["L", "H"]):
            vs = [Vowel(v.letter, ["L"]) for v in base]
            vs[i].moras = list(moras)
            add(vs)
    return out[:max_variants]
