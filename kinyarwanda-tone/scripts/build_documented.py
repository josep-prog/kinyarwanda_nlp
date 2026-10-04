"""Build data/documented.tsv and data/documented_sentences.tsv: Kinyarwanda forms whose ubutinde
and amasaku are written out in REB teaching material, each with its source page.

Only forms read directly on the page are included (transcribed 2026-10-02). Exercise words whose
answer is not printed are kept out of the marked list (see GROUPS: same-pattern groups only).
"""
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
Y2 = "REB, Ikinyarwanda TTC Y2 SME/SSE, Igitabo cy'umwarimu"
Y2_URL = "https://elearning.reb.rw/pluginfile.php/23529/mod_resource/content/1/Kinyarwanda%20Y2%20SME%20%20SSE%20TG.pdf"
S5 = "REB, Ikinyarwanda S5, Igitabo cy'umunyeshuri (isomo rusange)"
S5_URL = "https://elearning.reb.rw/pluginfile.php/86224/mod_resource/content/1/IKINYARWANDA%20S5%20SB.pdf"

# Reference words (amagambo fatizo) and the words the book says share their amasaku. Y2 p.116 and p.128.
FAMILIES = {
    "umugabo": ["umugayo", "umugano", "umutavu", "ikigega", "ibiheri"],
    "umugaati": ["umugaanda", "urugeendo", "ubugiingo", "umuhoondo"],
    "kwaagaaza": ["kweegeera", "kuuvuura", "koondoora"],
    "umugorê": ["umukirê", "ubugomê", "urukizâ", "umugerî", "ihenê", "ishyarî", "isî", "indâ", "ugê", "kô", "sô", "ndê"],
    "umusôre": ["umukôro", "uruhâre", "igikênya", "gasôre", "musôni", "matâma", "shyôgwe", "shâmi", "isâro", "kôra"],
    "reerô": ["mbeesê", "geewê", "bwiizâ"],
    "umwaâmi": ["umwaâse", "umwaâri", "ubwoône", "urwiîri"],
    "umwâana": ["ubwâato", "icyâaha", "icyûuho", "urwêego"],
    "imbêehê": ["isâahâ", "inzêezâ"],
    "indôgobê": ["isâbunê", "ingâmiyâ", "imôdokâ"],
    "isâhaâne": ["ikâraâyi", "itâfaâri", "ingûfuûri"],
    "guhîingiisha": ["gukôondoora", "guhîinguura"],
    "umwiîgiisha": ["icyoôkeere", "isaânzuure", "umpiîngiishe"],
    "uzâansuûre": ["uzâambwiîre", "ibâambaâsi", "uzâabyoôtse"],
    "mwaârabyîize": ["baârashâaje", "byaârahîiye", "kaârabâaye"],
    "utwuûnguceênge": ["maâma", "sheênge", "icyaârahaâni"],
}
FAMILY_NOTES = {
    "umuhoondo": "p.116 only (not in the p.128 copy)",
    "kôra": "p.128 only",
    "isâro": "listed under umusôre on p.116 but under umugorê on p.128; its stem (H on the first stem vowel) fits umusôre",
    "maâma": "printed 'Maâma sheênge' (two words)", "sheênge": "printed 'Maâma sheênge' (two words)",
    "icyaârahaâni": "p.128 only",
}

# (marked, meaning, category, page, note); Y2 unless the page starts with "S5"
WORDS = [
    ("umugabo", "", "short, nyesi", "113", ""), ("umuneke", "", "short, nyesi", "113", ""),
    ("umusôre", "", "short, nyejuru", "113", "the anticipation high on the syllable before is not written (p.114)"),
    ("umugorê", "", "short, nyejuru", "113", "the anticipation high on the syllable before is not written (p.114)"),
    ("umugaati", "", "long (u-mu-gaa-ti)", "113", ""), ("kugeenda", "", "long (ku-gee-nda)", "113", ""),
    ("umutaako", "", "long, nyesi nyesi", "114", ""), ("umugaanda", "", "long, nyesi nyesi", "114", ""),
    ("kuvooma", "", "long, nyesi nyesi", "114", ""),
    ("umwaâmi", "", "long, nyesi nyejuru (rising)", "114", ""), ("umwaâri", "", "long, nyesi nyejuru (rising)", "114", ""),
    ("umwâana", "", "long, nyejuru nyesi (falling)", "114", ""), ("umwâaka", "", "long, nyejuru nyesi (falling)", "114", ""),
    ("ikâawâ", "", "nyejuru after nyejuru nyejuru", "115", ""), ("umusaâvê", "", "nyejuru after nyejuru nyejuru", "115", ""),
    ("kaawâ", "", "without augment: tones change", "115", "compare ikâawâ"),
    ("saavê", "", "without augment: tones change", "115", "compare umusaâvê"),
    ("kutâzâajyayô", "", "negative ta: always nyejuru", "115", ""), ("kutâvugâ", "", "negative ta: always nyejuru", "115", ""),
    ("mudâsobwâ", "", "negative ta: always nyejuru", "115", ""),
    ("igitî", "", "example word", "111", ""), ("umukoôbwa", "", "example word", "111", ""), ("imbâahô", "", "example word", "111", ""),
    ("umudûri", "", "exercise answer", "117", ""), ("amabaati", "", "exercise answer", "117", ""),
    ("imyâaka", "", "exercise answer", "117", ""), ("ibyaâtsi", "", "exercise answer", "117", ""),
    ("gateêra", "", "length example", "127", ""), ("guhaaha", "", "length example", "127", ""),
    ("umwîishywa", "child of one's sister", "word list", "S5 27", ""),
]
# Minimal pairs: same spelling, different ubutinde/amasaku and meaning
PAIRS = [
    ("inkokô", "", "inkooko", "", "116"),
    ("umusaâmbi", "igisiga (bird)", "umusaambi", "bicaraho (seat)", "116, 128"),
    ("gutaka", "ububabare (cry out in pain)", "gutaaka", "inzu (decorate a house)", "128"),
    ("ibiyagâ", "amazi (lakes)", "ibiyaga", "serwakira (storms)", "125"),
    ("ikirêerê", "isanzure (sky, atmosphere)", "ikireere", "insina (banana-plant part)", "125"),
    ("kurêengera", "kwita ku bidukikije (protect)", "kurêengeera", "kurenga amategeko (transgress)", "125"),
    ("amafû", "ubuhehere (moisture)", "amafu", "y'amasaka (sorghum flour)", "125"),
    ("umwêerâ", "ukweruruka k'umubiri (paleness)", "umweêra", "umuntu ufite uruhu rwera (white person)", "S5 97, 141"),
]
# Exercise answer key without marks: words the book puts in the same ubutinde/amasaku group (p.117)
GROUPS = [["ukuri", "ubumwe", "imbogo", "ingoro", "ugutwi"],
          ["ubutuna", "ubuzima", "umugabo", "inyandiko", "igitovu"],
          ["umusore", "igikumwe", "umutima"],
          ["urukuta", "umuco", "umugore"],
          ["urukwavu", "umugongo"]]
# Sentence-level forms (amasaku mbonezanteruro): never use as a word's own melody
SENTENCES = [
    ("Abo nî Kamaâri na Rûgaâmba baâteye ibitî.", "118", ""),
    ("Iby'uûwo mukôro wô kuriinda ikirêerê birashiimiishije.", "118", ""),
    ("Ikirêerê n'ûmwuûka duhuumêeka byaangiizwa n'îibyôotsi.", "119", ""),
    ("Umugorê n'ûmugabo barafâtanya mu kurêengera ibidûkikije.", "119", ""),
    ("Karaangwâ yahûguuye abatûuranyi bê kuu ngârukâ z'aâko kaânya cyâangwâ zizigûye zikomôoka ku kwâangiiza amashyaamba.", "119", ""),
    ("Muu nzêego z'ûbuyobozi biitoondera ibyaâkwaanduza umwuûka mwiizâ.", "119", ""),
    ("Ishyaamba ryiitaabwaho na Mutamu.", "119", "rule a: word without H after na/nka/-a: unchanged"),
    ("Kanyâna na Kagabo bafatanya kuriinda ibihûmaanya ikirêerê", "119", "rule a"),
    ("Umukôro wa Mugabo", "120", "rule a"),
    ("Abâana baa Nkûbito biitabiiriye umugaanda wô gutêera ibitî.", "120", "rule b: next word H on syllable 1: the connective lengthens"),
    ("Ageendana na Cyûuma.", "120", "rule b"), ("Mukuungwâ na Ntâruka bireegeranye.", "120", "rule b"),
    ("Saavê ituuwe nka Kîbuungo.", "120", "rule c: H on syllable 2 moves to syllable 1"),
    ("Umukôro wa Mûtesi", "120", "rule c"),
    ("Kiizâ na Mûgorê baravûukana.", "120", "rule d: H on syllable 3: syllable 1 also gets H"),
    ("Umujyî wa Kîgalî urasukuuye.", "120", "rule d"),
    ("Inzu ya Kâriîsa sî iy'îbyaâtsi.", "120", "rule e: rising on syllable 2 stays, syllable 1 gets H"),
    ("Kamaâri yiigiisha nka Mûhiîre kubûungabuunga ibidûkiikije.", "120", "rule e"),
    ("Umugorê n'ûmugabo", "120", "rule f: elided connective before augment: augment gets H"),
    ("Abâana b'âbakoôbwa", "120", "rule f"),
    ("Afatwa nk'îintwâari.", "121", "rule g: augment becomes falling"),
    ("Miniisîtiri w'îintêbe yasuuye/yasûuye Icyaânya cy'Âkagêra.", "121", "rule g"),
    ("Abatô bageendana n'iîki gihe.", "121", "rule h: demonstrative gets rising on its first vowel"),
    ("Yiitwaara nk'aâba babyêeyi bê.", "121", "rule h"),
    ("Guhumaanya ikirêerê biteeza ingârukâ z'aâko kaânya.", "121", "rule h"),
    ("Gutêera ibitî biraanga umutûurage w'îbikorwî by'împuhwe n'îinêezâ.", "121", "exercise answer"),
    ("Iteerambere riraambyê turigezwahô nô kuriinda ikirêerê ibigîhumaanya.", "121", "exercise answer"),
    ("Ni ngoombwâ kugabanya ibyôotsi bivâ muu môdokâ n'îkoreeshwa ry'îinkwî.", "121", "exercise answer"),
    ("Kagabo na Mûtoni bahaawe/baâhaawe igihêembo kukô baâfashe nêezâ ibidûkiikije.", "121", "exercise answer"),
    ("Nyirî amahîirwê amenya iby'îmihiîndagurikire y'îbihe.", "121", "exercise answer"),
    ("Yavuuyeyô uno muunsi.", "115", "locatives mo/ho/yo and so/ko: always H"),
    ("Uzi ko Kamana yageendaniyekô!", "115", ""), ("Yiinjiyemô nonaha.", "115", ""),
    ("Kare wa mwana yamwiihomyehô biba ubusa.", "115", ""),
    ("Uwô mvugâ yaaje.", "S5 49", "relative pronoun: always H"), ("Ibyô akorâ birakwîiye.", "S5 49", ""),
    ("Si kô bavuzê", "S5 50", ""), ("Yagiiyeyô", "S5 50", ""), ("Namuboonyemô/ Namûboonyemô", "S5 50", ""),
    ("Amasuunzu sî amasakâ.", "S5 50", "copula ni/si: H, lost at sentence start"),
    ("Uwô nshâakâ nî uwo.", "S5 50", ""), ("Ni umwâana nk'âbaândi.", "S5 50", ""), ("Si nge ujyayô.", "S5 50", ""),
    ("Avuuka i Sâavê.", "S5 50", "locative i changes tone: compare Saavê"),
    ("Kunywâ nô kuryâ birajyaana.", "S5 50", "no/nko and -o connectives: H"),
    ("Umurimâ wô guhîinga nî uwo.", "S5 50", ""), ("Iyo nyâna yô gukwâ nî iyi.", "S5 50", ""),
    ("Akarimâ k'îgikoôni.", "S5 48", ""), ("Abâana b'âbakoôbwa bafatanya na bâsaaza bâabo.", "S5 48", ""),
    ("Mugeenzi na Mugabo barakûundana.", "S5 48", ""), ("Muu nzêego z'ûbuyobozi bakora nêezâ nk'întoôre.", "S5 48", ""),
    ("Kiizâ na Mûganwâ", "S5 48", "rule d"), ("Umugî wa Kîgalî", "S5 48", "rule d"),
    ("Inzu ya Kâliîsa", "S5 49", "rule e"), ("Kamaâli agenda nka Mûhiîre.", "S5 49", "rule e"),
    ("Mutôni na Gâsaro", "S5 49", "rule c"), ("Inkoni ya Gâsore", "S5 49", "rule c"),
]


def src(page: str) -> tuple[str, str, str]:
    return (S5, page[3:], S5_URL) if page.startswith("S5") else (Y2, page, Y2_URL)


def plain(marked: str) -> str:
    from amasaku.marks import parse
    return parse(marked.lower())[0]


rows = []
def add(marked, meaning, category, page, note, family="", pair=""):
    s, p, url = src(page)
    rows.append({"id": str(len(rows) + 1), "plain": plain(marked), "marked": marked, "meaning": meaning,
                 "category": category, "family": family, "pair": pair, "source": s, "page": p, "url": url,
                 "note": note})

for ref, members in FAMILIES.items():
    add(ref, "", "reference word (ijambo fatizo)", "116", "", family=ref)
    for m in members:
        add(m, "", "same amasaku as its reference word", "116", FAMILY_NOTES.get(m, ""), family=ref)
seen = {r["marked"] for r in rows}
for marked, meaning, cat, page, note in WORDS:
    if marked not in seen:
        add(marked, meaning, cat, page, note)
        seen.add(marked)
for a, ga, b, gb, page in PAIRS:
    pages = page.split(", ")
    for m, g, other in ((a, ga, b), (b, gb, a)):
        add(m, g, "minimal pair", pages[0] if page.startswith("S5") else pages[0], "", pair=other)
        if page.startswith("S5"):
            rows[-1]["source"], rows[-1]["page"], rows[-1]["url"] = S5, page[3:], S5_URL

fields = ["id", "plain", "marked", "meaning", "category", "family", "pair", "source", "page", "url", "note"]
with open(ROOT / "data" / "documented.tsv", "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=fields, delimiter="\t")
    w.writeheader()
    w.writerows(rows)
with open(ROOT / "data" / "documented_sentences.tsv", "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f, delimiter="\t")
    w.writerow(["id", "sentence", "rule_or_note", "source", "page", "url"])
    for k, (s, page, note) in enumerate(SENTENCES, 1):
        so, p, url = src(page)
        w.writerow([k, s, note, so, p, url])
with open(ROOT / "data" / "documented_groups.tsv", "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f, delimiter="\t")
    w.writerow(["group", "words_with_the_same_ubutinde_and_amasaku", "source", "page"])
    for k, g in enumerate(GROUPS, 1):
        w.writerow([k, ", ".join(g), Y2, "117"])
print(len(rows), "words,", len(SENTENCES), "sentences,", len(GROUPS), "groups")
