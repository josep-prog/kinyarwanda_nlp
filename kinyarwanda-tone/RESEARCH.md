# Amasaku n'ubutinde: research notes

Goal: recover the tone (amasaku) and vowel length (ubutinde) that written Kinyarwanda
leaves out, first by measuring them in real speech (Ismaël Mwanafunzi), then by predicting
them from text in context, so that a TTS voice can say every word with native melody.

## Notation (the scientific orthography)
- High tone: acute accent on the vowel (mora) that carries it: *usomá*
- Long vowel: doubled vowel: *ikiréera*, *umuryaango*
- Long vowel with falling tone = H on the first mora: *áa*; rising = H on the second: *aá*
- Unmarked = low (toneless) and short

This is the notation of the Coupez, Kamanzi, Bizimana et al. dictionary (2005), which marks
tone and length on every entry ([Glottolog](https://glottolog.org/resource/reference/id/76159);
[Kinyarwanda, Wikipedia](https://en.wikipedia.org/wiki/Kinyarwanda)).

## What the literature says, and what it means for automatic labelling

| Fact | Source | Consequence for us |
|---|---|---|
| One phonemic tone (H). "Low" is the absence of H. H surfaces as high, low, rising or falling | Kimenyi 2002, cited in [Jarnow, PLSA](https://journals.linguisticsociety.org/proceedings/index.php/PLSA/article/download/4699/4341) | Label each mora **H or not**, not a three-way scale |
| The **mora** is the tone-bearing unit. A long vowel = 2 moras; H on mora 1 = falling, on mora 2 = rising | Jarnow; [Myers 2003, *Phonetica* 60:71-97](https://doi.org/10.1159/000071448) | Measure pitch on each **half** of a long vowel |
| Minimal pairs differ only by tone/length: *umuryáango* "door" vs *umuryaango* "family" | Jarnow | Test set for the labeller |
| **Downstep** in nearly all utterances: each successive H is lower than the previous | Jarnow | H/L must be judged **relative to the running H level**, not absolute pitch. A late H can be lower than an early L |
| Tone spreads/anticipates only **leftward** | Jarnow (Kimenyi 2002: "tone anticipation") | Rising pitch before an H is expected, not a separate H |
| Right edges of prosodic words get a **boundary H**; **no H utterance-finally** (statements end low) | Jarnow | Separate lexical H from word-edge H; expect the final fall we measured (−3.2 st) |
| Polar questions: **downstep suspended** on the last lexical H, word-final prosodic H deleted | Jarnow | Questions need their own marker (later) |
| F0 peak timing depends on mora position, phrase/word position and a following H: **peaks can be delayed** | [Myers 2003](https://doi.org/10.1159/000071448) | Look for an H's peak on its mora **or just after it** |
| Vowel length is **neutralized**: word-initial and word-final vowels are short; vowels **before NC** (mb, nd, ng, nz, nt, nk...) and **after CG** (Cw, Cy) are long | [Myers 2005, *J. Phonetics* 33](https://www.sciencedirect.com/science/article/abs/pii/S0095447005000148) | Length there is **predictable by rule**: only measure length where it is contrastive. Word-initial beats pre-NC: the *i* of *inkoko* stays short (corrected 2026-10-02) |
| Verb tone has no single rule: the root's H, a suffixal H (Hpost) and the **H of the negative prefix *nti-*** each behave differently, with left-to-right metrical (trochaic) structure | [Goldsmith & Mpiranya 2011](https://www.degruyterbrill.com/document/doi/10.1515/9783110246223.25/html?lang=en) | Predicting verb tone needs **morphology** (tense, negation, object markers): this is where kinyarwanda-nlp comes in. *nti-* → "ni" errors drop a tone-bearing, meaning-changing prefix |

## Plan
1. **Align** (now): Kinyarwanda CTC model + forced alignment → start/end of every letter, so
   every vowel gets a duration and a pitch track.
2. **Label**: per vowel, length relative to the local speech rate (excluding phrase-final and
   neutralized positions), and H/L per mora relative to a downstep-aware running H line.
   Output text in the notation above. Native-speaker check on a sample + minimal pairs.
3. **Train** the voice on labelled text (tone and length as explicit input symbols).
4. **Predict** labels from plain text: first the most frequent pattern per word, then a
   context + morphology model (kinyarwanda-nlp) for homographs (*inkoko*/*inkoko*).

## Reference works (authority, in this order)
1. **Simon Bizimana (1998), *Imiterere y'Ikinyarwanda*, Igitabo cya 1: Amajwi, amagambo n'interuro** (Pelloti, Kigali): tone-marked grammar by one of the main experts on amasaku ([source](https://link.springer.com/content/pdf/10.1007%2F978-3-642-14684-8_6.pdf); [obituary](https://umunota.com/en/rwanda-mourns-renowned-kinyarwanda-linguist-simon-bizimana/))
2. Coupez, Kamanzi, Bizimana et al. (2005), tone- and length-marked dictionary ([Glottolog](https://glottolog.org/resource/reference/id/76159))
3. RALC, *Amategeko y'igenantego ry'Ikinyarwanda* (official phonology rules, incl. igenantego saku) ([academia.edu](https://www.academia.edu/40344584/AMATEGEKO_YIGENANTEGO_RYIKINYARWANDA_R_E_P_U_B_U_L_I_K_A_Y_U_R_W_A_N_D_A_INTEKO_NYARWANDA_YURURIMI_NUMUCO_RALC)); also in ~/Documents/kinyarwanda_nlp/data (Git LFS, not yet pulled)
4. Kimenyi (2002), *A Tonal Grammar of Kinyarwanda* ([Semantic Scholar](https://www.semanticscholar.org/paper/A-tonal-grammar-of-Kinyarwanda-:-an-autosegmental-Kimenyi/e1d93fa149994ef56af91ff693e17f3dbfa0bce3))
5. School textbooks (S6 Kinyarwanda, TTC Book 2): teach grammatical tone rules, e.g. relative pronoun (ikinyazina mbanziriza) always high, demonstrative (nyereka) low ([TTC book 2](https://www.cur.ac.rw/mis/main/library/documents/book_file/digital-63f8c1c33fdc87.65223974.pdf))
6. Habumuremyi & Uwamahoro (2006) dictionary: ~181 tone-marked forms in its phonology section, circumflex for H (*abâanzi* vs *abaânzi*), copyrighted ([Webonary](https://www.webonary.org/kinyarwanda/))

## More tone rules
- **Meeussen's rule**: two adjacent H inside a stem → H L; across compound words a **downstep** is inserted instead ([Wikipedia](https://en.wikipedia.org/wiki/Meeussen%27s_rule))
- **Tone anticipation**: an H raises the syllable before it (Kimenyi 2002)
- Grammatical tone: relative vs demonstrative pronouns (*úwo* vs *uwo*), negative *nti-* (Goldsmith & Mpiranya)

## Evidence from other tonal languages
- Yoruba TTS with tone-marked input: 88.5% clarity, 80% naturalness ([Festival study](https://www.researchgate.net/publication/379899651_Development_of_a_Yoruba_Text-to-Speech_System_Using_Festival))
- Restoring tone marks to plain text is a solved-in-part NLP task for Yoruba ([YAD, T5](https://arxiv.org/pdf/2412.20218); [restoration model](https://www.researchgate.net/publication/318851806_Restoring_tone-marks_in_standard_Yoruba_electronic_text_Improved_model)): the model for our step 4
- Evaluation: reference-free lexical-tone metric for TTS ([Tone on a Budget, 2026](https://arxiv.org/html/2609.14817)); syllable-tone annotated dataset format ([YembaTones](https://www.sciencedirect.com/science/article/pii/S2352340923009216)); F0 at 10 ms with Parselmouth/Praat is the common practice ([Bantu ASR](https://arxiv.org/html/2606.31642))

## Findings so far (our own measurements)
- CTC speech recognizers give one-frame letter spikes: unusable for length. The voice model's own alignment gives real letter durations (16 ms).
- Phrase-final lengthening: 160 ms vs ~52 ms (3x), clearly measured.
- Word-internal length rules (pre-NC, post-Cw): only 4-10% effects in fast narration. Not reliable yet.
- Pitch patterns are word-specific for content words (consistency 0.35 vs 0.06 chance; *ndetse* 0.93, *buryo* 0.81); function words depend on context.
- Method (from the native speaker's school practice): decide **ubutinde first** (count beats), then **amasaku** (which mora is high), by comparing with **anchor words** of known marking.

## Native speaker recordings (2026-10-01)
Measured on the speaker's own recordings (umwana, umwami, umwana w'umwami):
- The áa / aá contrast is **where the pitch peaks inside the long vowel**: *umwana* peaks early in
  the *a* then falls 3.7 st (áa); *umwami* rises 1.5 st to a late peak (aá). Matches Myers 2003.
- The voice-model aligner (trained on Mwanafunzi) cuts another speaker's vowels badly, and CTC
  spikes are unusable; peak timing inside the word's loudest stretch works on any voice:
  2/2 isolated words correct.
- In the phrase *umwana w'umwami*, the final word came out falling: possibly utterance-final
  lowering (no H utterance-finally), to be confirmed by the speaker.

## Anchor words (native speaker, 2026-10-01)
umwáana, umwaámi, umugabo, umugoré (school list and TTC book 2; was wrongly noted as umugóre), inká, umugaanda (all low: long aa before nd, no high; native speaker 2026-10-02, was wrongly noted as umugánda)

Full school list (16 words) in data/anchors.tsv; full linguistic spec: "Kinyarwanda Ubutinde and Amasaku Engine — Linguistic Specification" (Claude doc, 2026-10-02).

## Open questions for the native speaker
- Minimal pairs with sentences (target: 20+), marked in this notation
- Which dialect/norm to follow when speakers differ (Mwanafunzi's speech is the reference for now)

## Findings 2026-10-02: why the engine fails on the REB documented words
Test: the speaker recorded 112 REB-documented words (data/documented.tsv); the engine is scored against the book.
- Exact marking: whole-word comparison with the standard words 33/96 (non-standard words), vowel-by-vowel 15/96.
- Not caused by slow speech: 0.24 s/vowel, between the speaker's normal (0.21) and melody (0.29) standard recordings.
- The speaker's pronunciation agrees with the book: falling âa drops 1.2 st (rising 13%), rising aâ +0.2 st (rising 59%).
- Integuza is real in the recordings: in words with one high, the pitch peak is on the written high 28x and on the vowel BEFORE it 23x (REB Y2 TG p.114). A final high is not heard as pitch (word said alone ends low); only the integuza before it shows.
- The 17 standard melodies are enough: with the right standard word chosen, right-edge transfer gives 98/104 documented words (the 3 misses of the mwaârabyîize family disappear with the book's marking of #15). The weak step is recognising which standard word a recording sounds like (~35-40%).
- Tried and rejected on a held-out half (no gain on test): rule-based melody reader (analysis by synthesis with integuza), loudness-peak vowel segmentation, normal+melody references, end-aligned per-vowel matching, family recordings as extra class examples (+6/62 class accuracy, not significant).
- Other speakers (scripts/check_contours_across_speakers.py, non-final words): Radio Rwanda umwâana -1.0 st (0% rising, n=40), abâana -1.1, imyâaka -0.6; umwaâmi -0.1 (38% rising, n=39). Mwanafunzi: falling words -1.1, umwaâmi -0.3 (23% rising). The book's falling/rising contrast holds across speakers; "rising" is level-or-up relative to falling, not a large rise.

## Findings 2026-10-02 (later): school method and the TTS
- School method (ubutinde first -> standard words with the same long vowels from the end -> melody comparison -> copy): 40/112 documented words exact with ubutinde done first (27 with ubutinde from the recording; old whole word 34). Ubutinde narrows 17 standard words to 7.6 on average; the right one is among them for 47/63 family words.
- Length from audio is weak: duration ratio gets 27/39 contrastive vowels in the standard words (all-short baseline 25).
- All local speech data is Mwanafunzi (prepared data filtered at speaker similarity >= 0.60; "Radio Rwanda" = his "Wari uzi ko" programme). His words cut from connected speech made matching worse (right standard word 26 vs 29 with the speaker's own recordings), so they are stored (data/anchors/radio, kind "radio") but not used by default. Other speakers: Mozilla Common Voice Kinyarwanda (~1,181 speakers) needs an account.
- TTS (scripts/tts_vs_native.py, data/tts_vs_native.tsv): pitch-contour distance to the native speaker's recording of each documented word: plain 0.308, book marking 0.233, book marking + integuza + low final 0.186. Marked+REB closer than plain for 70/105 (sign test p<0.001); vs marking without the REB rules: closer for only 47/105 but much closer on average (Wilcoxon p<0.001): promising, to be judged by ear (out/tts_listen/).
