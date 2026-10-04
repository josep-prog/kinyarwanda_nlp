"""Reference-word transfer, checked only against sourced markings (see the linguistic spec, section M)."""

from amasaku.anchors import forced_length, transfer


def test_length_rules():  # Myers 2005: initial/final short, before NC and after CG long
    assert forced_length("inzara") == [1, None, 1]  # the long aa follows nz: lexical, not by rule
    assert forced_length("umwana") == [1, 2, 1]
    assert forced_length("guhinga") == [None, 2, 1]
    assert forced_length("inyoni") == [1, None, 1]  # ny is one consonant


def test_guhinga_like_umwana():  # TTC Book 2: guhîinga
    assert transfer("umwâana", "guhinga").marking == "guhíinga"


def test_abana_like_umwana():  # TTC Book 2: abâana (same stem, other class)
    assert transfer("umwâana", "abana").marking == "abáana"


def test_umwami_is_rising():  # Jarnow 2020 ex. 1a
    assert transfer("umwaâmi", "umwami").marking == "umwaámi"


def test_own_length_is_kept():  # school order: ubutinde first, then amasaku; hyphen = line up roots
    assert transfer("umwâana", "gu-hiingiisha").marking == "guhíingiisha"


def test_shy_is_one_consonant():  # REB Y2 p.116: ishyarî, shyôgwe are short
    assert forced_length("ishyari") == [1, None, 1]


def test_reb_families():  # REB Y2 teacher guide p.116: every member has its reference word's amasaku
    import csv
    from pathlib import Path

    from amasaku.anchors import same_marking
    from amasaku.marks import Vowel, parse, write

    rows = [r for r in csv.DictReader(open(Path(__file__).parents[1] / "data" / "documented.tsv"), delimiter="\t")
            if r["category"].startswith("same")]
    wrong = []
    for r in rows:
        plain, vowels = parse(r["marked"])
        unmarked = write(plain, [Vowel(v.letter, ["L"] * len(v.moras)) for v in vowels])
        if not same_marking(transfer(r["family"], unmarked).marking, r["marked"]):
            wrong.append(r["marked"])
    assert not wrong, wrong


def test_hyphen_marks_the_stem():
    assert transfer("umwâana", "nda-bona").marking.startswith("nda")
