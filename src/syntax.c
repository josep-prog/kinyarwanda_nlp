/*
 * syntax.c
 * Syntax checking for Kinyarwanda sentences.
 *
 * Rules applied (derived from the book):
 *
 * RULE 1 – Noun-adjective class agreement (p.65-68)
 *   An adjective (ntera) must bear the concordance prefix (indangasano)
 *   that matches the noun class it modifies.
 *   e.g.  umuntu munini  (nt.1 noun + nt.1 adjective prefix mu-)  ✓
 *         umuntu binini  (nt.1 noun + nt.8 adjective prefix bi-)  ✗
 *
 * RULE 2 – Possessive connector class agreement (p.91-92)
 *   A possessive pronoun (ikinyazina ngenera) must agree with the class
 *   of the noun it attaches to.
 *   e.g.  urugo rwacu  (nt.11: rwa + cu)  ✓
 *         urugo wacu   (nt.1 connector on nt.11 noun)  ✗
 *
 * RULE 3 – Sentence completeness
 *   A fully verbal sentence must contain at least one verb (inshinga).
 *   Exception: nominal predicates — noun + adjective (e.g. "umuntu mwiza") —
 *   are grammatically complete without a verb in Kinyarwanda and are NOT
 *   flagged.  The copula is implied.
 *   Note: the REB textbook uses "umuvugo" for a verb-bearing utterance, but
 *   in everyday Kinyarwanda "umuvugo" means oral poetry/performance.
 *   "interuro" is the unambiguous term for 'sentence' used here.
 *
 * RULE 4 – Unknown words
 *   Words tagged POS_FOREIGN that are not proper nouns are flagged.
 *
 * RULE 10 – Cross-word connector elision (ikata ry'inyajwi hagati y'amagambo)
 *   A connector/conjunction whose full form ends in a vowel MUST be written
 *   in its elided form (final vowel dropped + apostrophe) when immediately
 *   followed by a word beginning with a vowel.  RALC §12.1.
 *
 *   Affected connectors and their elided forms:
 *     ya→y'  wa→w'  ba→b'  rya→ry'  cya→cy'  bya→by'
 *     za→z'  rwa→rw' twa→tw' bwa→bw' kwa→kw'  hwa→hw'
 *     na→n'  nka→nk'
 *
 *   Exceptions (RALC §12.3–12.4 — these forms never elide):
 *     ku, mu   — prepositions: "ku ishuri" (not "k'ishuri")
 *     ni, si   — copula/negation: "ni inka" (not "n'inka" in copula context)
 *     nyiri    — possessive title: "nyiri inzu" (not "nyir'inzu")
 *
 *   e.g.  "inka y'undi yagiye."   ✓   "n'umugore we."   ✓
 *         "inka ya undi yagiye."  ✗   "na umugore we."  ✗
 */

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "../include/kinyarwanda.h"

static bool is_vowel_ch(char c) {
    return c=='a'||c=='e'||c=='i'||c=='o'||c=='u';
}

/* Expected concordance prefix for adjective, keyed by noun class */
static const char *adj_concordance[17] = {
    "",     /* 0 = unused     */
    "mu",   /* Nt.1           */
    "ba",   /* Nt.2           */
    "mu",   /* Nt.3           */
    "mi",   /* Nt.4           */
    "ri",   /* Nt.5           */
    "ma",   /* Nt.6           */
    "ki",   /* Nt.7           */
    "bi",   /* Nt.8           */
    "n",    /* Nt.9           */
    "zi",   /* Nt.10          */
    "ru",   /* Nt.11          */
    "ka",   /* Nt.12          */
    "tu",   /* Nt.13          */
    "bu",   /* Nt.14          */
    "ku",   /* Nt.15          */
    "ha",   /* Nt.16          */
};

/* Expected possessive connector for each noun class */
static const char *poss_connector[17] = {
    "",
    "wa",  "ba",  "wa",  "ya",  "rya", "ya",  "cya",
    "bya", "ya",  "za",  "rwa", "ka",  "twa", "bwa",
    "kwa", "ha",
};

/*
 * elided_connector_class()
 *
 * Maps an apostrophe-elided possessive connector fragment to its noun class.
 * e.g. "rw" (from "rw'iburasirazuba") → 11, because "rw" is the elision of
 * "rwa" which is the Nt.11 possessive connector.
 *
 * Only unambiguous cases are returned (connectors unique to one class).
 * Ambiguous single-letter fragments ("y","w","b") are omitted because they
 * are shared across multiple classes and cannot be resolved here.
 *
 * Used by RULE 2 to detect chain-head agreement in possessive phrases:
 *   "ruhande rw'iburasirazuba rwa Edeni"
 *   → both "rw" and "rwa" are Nt.11; "rwa" agrees with chain head "ruhande"
 *     not with the immediately preceding "iburasirazuba" (Nt.14).
 */
static int elided_connector_class(const char *s) {
    /* Unambiguous: each connector string belongs to exactly one class */
    if (strcmp(s, "rw") == 0) return 11; /* rwa → rw  (Nt.11)          */
    if (strcmp(s, "bw") == 0) return 14; /* bwa → bw  (Nt.14)          */
    if (strcmp(s, "cy") == 0) return  7; /* cya → cy  (Nt.7)           */
    if (strcmp(s, "ry") == 0) return  5; /* rya → ry  (Nt.5)           */
    if (strcmp(s, "by") == 0) return  8; /* bya → by  (Nt.8)           */
    if (strcmp(s, "tw") == 0) return 13; /* twa → tw  (Nt.13)          */
    /* Shared connector strings — return a representative class.
     * The chain-head check compares poss_connector[] STRINGS, not class
     * numbers, so returning any class that yields the same string is safe:
     *   "ya" is shared by Nt.4, Nt.6, Nt.9  → return 9 (poss_connector[9]="ya")
     *   "wa" is shared by Nt.1, Nt.3        → return 1 (poss_connector[1]="wa")
     *   "ba" is unique to Nt.2              → return 2 */
    if (strcmp(s, "y")  == 0) return  9; /* ya → y   (Nt.4/6/9 family) */
    if (strcmp(s, "w")  == 0) return  1; /* wa → w   (Nt.1/3 family)   */
    if (strcmp(s, "b")  == 0) return  2; /* ba → b   (Nt.2)            */
    return 0; /* unknown */
}

static void add_error(SentenceAnalysis *sa, ErrorType type, int idx,
                      const char *msg, const char *suggestion) {
    if (sa->error_count >= KIN_MAX_ERRORS) return;
    Error *e = &sa->errors[sa->error_count++];
    e->type         = type;
    e->token_index  = idx;
    strncpy(e->message,    msg,        KIN_MAX_MSG - 1);
    strncpy(e->suggestion, suggestion, KIN_MAX_MSG - 1);
    if (idx >= 0 && idx < sa->token_count)
        sa->tokens[idx].error_count++;
}

void kin_check_syntax(SentenceAnalysis *sa) {
    /* RULE 3: Must have a verb
     * Exception: a sentence composed entirely of interjections, particles,
     * or adverbs is a valid elliptical utterance (e.g. greetings: "Muraho",
     * "Amakuru", "Murakoze", "Yego", "Oya").  Do not flag these as errors. */
    if (!sa->has_verb && sa->token_count > 0) {
        bool all_interj = true;
        int  content_count = 0;   /* non-punctuation, non-number tokens */
        for (int ii = 0; ii < sa->token_count; ii++) {
            POS p = sa->tokens[ii].pos;
            if (p != POS_PUNCTUATION && p != POS_NUMBER)
                content_count++;
            if (p != POS_INTERJECTION && p != POS_ADVERB &&
                p != POS_ADVERB_TIME &&
                p != POS_VERB_PARTICLE && p != POS_CONJUNCTION &&
                p != POS_PUNCTUATION) {
                all_interj = false;
            }
        }
        /* Suppress the error for fragments: a single-word input (noun,
         * number-label, quoted word…) without a verb is a list item or
         * continuation fragment, not a grammatical mistake.                 */
        bool is_fragment = (content_count <= 1);

        /* Nominal predicate exception: noun + adjective forms a complete
         * sentence in Kinyarwanda — the copula is implicit.
         * e.g. "umuntu mwiza" (the/a good person), "inzu nziza" (a nice house).
         * Allow pronouns, adverbs, prepositions, and locatives alongside
         * the noun+adjective core without triggering ERR_NO_VERB.         */
        bool is_nominal_pred = false;
        {
            bool has_noun = false, has_adj = false, all_nominal = true;
            for (int ii = 0; ii < sa->token_count; ii++) {
                POS p = sa->tokens[ii].pos;
                if (p == POS_PUNCTUATION || p == POS_NUMBER) continue;
                if (p == POS_NOUN || p == POS_RELATIVE_NOUN) has_noun = true;
                else if (p == POS_ADJECTIVE)                  has_adj  = true;
                else if (p == POS_PRONOUN   || p == POS_ADVERB     ||
                         p == POS_ADVERB_TIME || p == POS_LOCATIVE  ||
                         p == POS_PREPOSITION) { /* compatible modifier */ }
                else all_nominal = false;
            }
            is_nominal_pred = has_noun && has_adj && all_nominal;
        }

        if (!all_interj && !is_fragment && !is_nominal_pred) {
            add_error(sa, ERR_NO_VERB, -1,
                "Iyi nteruro ntifite inshinga / "
                "This sentence has no verb.",
                "Ongeraho inshinga (Add a verb).");
        }
    }

    /* RULE 1 & 2: Agreement between adjacent noun → adjective / possessive */
    for (int i = 0; i < sa->token_count - 1; i++) {
        Token *cur  = &sa->tokens[i];
        Token *next = &sa->tokens[i + 1];
        if (cur->pos  == POS_PUNCTUATION) continue;
        if (next->pos == POS_PUNCTUATION) continue;

        /* RULE 1: Noun followed immediately by adjective */
        if (cur->pos == POS_NOUN && cur->noun_class > 0 &&
            next->pos == POS_ADJECTIVE && next->noun_class > 0) {
            if (cur->noun_class != next->noun_class) {
                char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
                const char *expected_pfx = adj_concordance[cur->noun_class];
                snprintf(msg, sizeof(msg),
                    "Gushyira hamwe nabi: '%s' (inteko %d) na '%s' (inteko %d). "
                    "Agreement error: '%s' (class %d) with '%s' (class %d).",
                    cur->surface, cur->noun_class,
                    next->surface, next->noun_class,
                    cur->surface, cur->noun_class,
                    next->surface, next->noun_class);
                /* Generate the actual corrected adjective surface form */
                char corrected_adj[KIN_MAX_WORD] = {0};
                const char *adj_core = NULL;
                for (int m = 0; m < next->morph.n; m++) {
                    if (strcmp(next->morph.m[m].label, "C") == 0) {
                        adj_core = next->morph.m[m].form;
                        break;
                    }
                }
                if (adj_core && adj_core[0])
                    kin_vv_join(expected_pfx, adj_core,
                                corrected_adj, sizeof(corrected_adj));

                snprintf(sug, sizeof(sug),
                    "Hindura '%s' ugashyira '%s'. "
                    "Indangasano ya ntera igomba kuba '%s' (inteko %d). / "
                    "Replace '%s' with '%s'. "
                    "Concordance prefix for class %d must be '%s'.",
                    next->surface,
                    corrected_adj[0] ? corrected_adj : expected_pfx,
                    expected_pfx, cur->noun_class,
                    next->surface,
                    corrected_adj[0] ? corrected_adj : expected_pfx,
                    cur->noun_class, expected_pfx);
                add_error(sa, ERR_ADJ_AGREEMENT, i + 1, msg, sug);
            }
        }

        /* RULE 2: Noun followed by possessive or reflexive possessive pronoun.
         * Covers both ikinyazina ngenera (wa/ya/rya...) and
         * ikinyazina ngenera ngenga (wange/wacu/wabo...).        p.91-96 */
        if (cur->pos == POS_NOUN && cur->noun_class > 0 &&
            next->pos == POS_PRONOUN &&
            (next->pron_type == PRON_POSSESSIVE ||
             next->pron_type == PRON_REFLEXIVE) &&
            next->noun_class > 0) {
            /* Two classes may share the same connector (e.g. Nt.1 and Nt.3
             * both use "wa").  Compare connector strings, not class numbers,
             * to avoid false positives like "umunsi wa mbere".           */
            /* Nt.9 nouns are surface-invariant (sg = pl); their plural uses
             * Nt.10 agreement. Allow either Nt.9↔Nt.10 direction.          */
            bool nt9_10_ok = (cur->noun_class == 9 && next->noun_class == 10)
                          || (cur->noun_class == 10 && next->noun_class == 9);
            if (!nt9_10_ok && strcmp(poss_connector[cur->noun_class],
                                     poss_connector[next->noun_class]) != 0) {
                /*
                 * Chain-head agreement check
                 *
                 * In Kinyarwanda a possessive chain can propagate agreement
                 * from the structural head noun rather than the immediately
                 * preceding noun.  The pattern is:
                 *
                 *   N1(NCa)  CONN1(NCa)  N2(any)  CONN2(NCa)  N3
                 *
                 * where CONN1 and CONN2 share the same class NCa (the class
                 * of N1, the chain head).  Example:
                 *
                 *   ruhande(Nt.11) rw'iburasirazuba(Nt.14) rwa Edeni
                 *   ↑ head (Nt.11)  ↑ CONN1 elided (Nt.11)   ↑ CONN2 (Nt.11)
                 *
                 * Here "rwa" before "Edeni" agrees with "ruhande" (Nt.11),
                 * NOT with "iburasirazuba" (Nt.14), because "Edeni" specifies
                 * the whole "eastern side" entity, not just the direction.
                 *
                 * Detection: when CONN2 mismatches the immediate noun (cur),
                 * look at the token immediately before cur:
                 *   • If that token is a connector (full PRON_POSSESSIVE or an
                 *     elided POS_CONJUNCTION fragment) with the SAME class as
                 *     CONN2, AND
                 *   • The noun immediately before that connector also has the
                 *     same class as CONN2
                 * then CONN2 is a valid chain-head agreement and is not an error.
                 */
                bool chain_head_ok = false;
                if (i >= 2) {
                    const Token *prev_conn = &sa->tokens[i - 1];
                    const Token *prev_noun = &sa->tokens[i - 2];

                    /* Determine the possessive class of the preceding connector.
                     * Handle both full forms (PRON_POSSESSIVE) and elided
                     * fragments stored as POS_CONJUNCTION (rw, cy, ry, bw…). */
                    int prev_conn_class = 0;
                    if (prev_conn->pos == POS_PRONOUN &&
                        (prev_conn->pron_type == PRON_POSSESSIVE ||
                         prev_conn->pron_type == PRON_REFLEXIVE)) {
                        prev_conn_class = prev_conn->noun_class;
                    } else if (prev_conn->pos == POS_CONJUNCTION) {
                        prev_conn_class = elided_connector_class(prev_conn->lower);
                    }

                    /* Chain head agreement holds when:
                     *  (a) CONN1 and CONN2 share the same connector string
                     *  (b) the noun before CONN1 also uses that connector */
                    if (prev_conn_class > 0 &&
                        strcmp(poss_connector[prev_conn_class],
                               poss_connector[next->noun_class]) == 0 &&
                        prev_noun->pos == POS_NOUN &&
                        prev_noun->noun_class > 0 &&
                        strcmp(poss_connector[prev_noun->noun_class],
                               poss_connector[next->noun_class]) == 0) {
                        chain_head_ok = true;
                    }
                }

                if (!chain_head_ok) {
                    char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
                    const char *expected = poss_connector[cur->noun_class];
                    snprintf(msg, sizeof(msg),
                        "Ikinyazina ngenera '%s' ntigishyikira izina '%s' (inteko %d). "
                        "Possessive '%s' does not agree with noun '%s' (class %d).",
                        next->surface, cur->surface, cur->noun_class,
                        next->surface, cur->surface, cur->noun_class);
                    snprintf(sug, sizeof(sug),
                        "Hindura '%s' ugakoresheje '%s...' (ikinyazina ngenera cy'inteko %d). "
                        "Replace '%s' with '%s...' (possessive for class %d).",
                        next->surface, expected, cur->noun_class,
                        next->surface, expected, cur->noun_class);
                    add_error(sa, ERR_POSS_AGREEMENT, i + 1, msg, sug);
                }
            }
        }
    }

    /* RULE 4: Flag unrecognised non-proper-noun words */
    for (int i = 0; i < sa->token_count; i++) {
        Token *t = &sa->tokens[i];
        if (t->pos == POS_PUNCTUATION) continue;  /* punctuation is not a word */
        if (t->pos == POS_FOREIGN && !t->is_proper_noun) {
            char msg[KIN_MAX_MSG];
            snprintf(msg, sizeof(msg),
                "Ijambo '%s' ntirizwi mu Kinyarwanda. "
                "Word '%s' is not recognised as Kinyarwanda.",
                t->surface, t->surface);
            add_error(sa, ERR_UNKNOWN_WORD, i, msg,
                "Suzuma imyandikire cyangwa ijambo ry'amahanga. "
                "Check spelling or this may be a foreign word.");
        }
    }

    /* RULE 5: Subject-verb agreement (indangasubizi y'inshinga na izina)  *
     * A conjugated verb's subject prefix (SP) must match the class of the  *
     * subject noun that precedes it.  Book p.88 + year-4 p.102+.           *
     * We only check adjacent noun → verb pairs where both classes are known */
    for (int i = 0; i < sa->token_count - 1; i++) {
        Token *noun = &sa->tokens[i];
        /* Skip punctuation in the outer scan */
        if (noun->pos == POS_PUNCTUATION) continue;
        /* Find the next non-punctuation token as the verb candidate */
        int vi = i + 1;
        while (vi < sa->token_count && sa->tokens[vi].pos == POS_PUNCTUATION) vi++;
        if (vi >= sa->token_count) continue;
        Token *verb = &sa->tokens[vi];

        if (noun->pos != POS_NOUN)      continue;
        if (verb->pos != POS_VERB_CONJ) continue;
        /* Do not check agreement across a clause/sentence boundary.
         * A comma or period between noun and verb means they belong to
         * different clauses — subject-verb agreement does not apply.       */
        {
            bool boundary_between = false;
            for (int k = i + 1; k < vi; k++) {
                if (sa->tokens[k].pos == POS_PUNCTUATION &&
                    (sa->tokens[k].is_clause_boundary ||
                     sa->tokens[k].is_sent_boundary)) {
                    boundary_between = true;
                    break;
                }
            }
            if (boundary_between) continue;
        }
        /* Verbal nouns (izina ryaturutse ku nshinga) are syntactically nouns
         * even though they carry POS_VERB_CONJ morphology.  They do not
         * agree with a preceding subject noun — they ARE the subject noun. */
        if (verb->gram_role == GRAM_ROLE_VERBAL_NOUN) continue;
        /* Skip when either side has an unknown or ambiguous class */
        if (noun->noun_class == 0 || verb->noun_class == 0) continue;
        /* Nt.1 and Nt.3 both use the same SP "a/u"; treat as compatible */
        int nc = noun->noun_class;
        int vc = verb->noun_class;
        if ((nc == 1 || nc == 3) && (vc == 1 || vc == 3)) continue;
        /* Nt.4 and Nt.9 both use the same SP "i"; treat as compatible.
         * e.g. "Imana iravuga" — Imana is Nt.9 but iravuga's SP "i" is
         * stored as Nt.4 (first-match in the SP table).              */
        if ((nc == 4 || nc == 9) && (vc == 4 || vc == 9)) continue;
        /* Nt.9 (singular) and Nt.10 (plural) are a paired noun class.
         * Class 9 nouns used with plural reference take class 10 agreement
         * e.g. "imbuto zikwiriye" — imbuto is Nt.9 but plural agreement
         * uses SP "zi" (Nt.10). These are grammatically correct forms.  */
        if ((nc == 9 || nc == 10) && (vc == 9 || vc == 10)) continue;
        /* "ya" SP (stored as cls 6) is ambiguous: it is ALSO the Nt.1 past
         * tense form (a-subject + past 'a' marker → "ya").  Do not flag   *
         * agreement errors when verb SP class is 6 and noun is Nt.1/3/9.  *
         * e.g.  "Imana yaremye ijuru" – Nt.9 noun + ya (Nt.1-past) verb.  */
        if (vc == 6 && (nc == 1 || nc == 3 || nc == 9)) continue;
        /* Copula + locative forms (TENSE_COPULA_PAST/PRES) are existential   *
         * constructions and do not follow strict subject-verb agreement.      *
         * "yariho ubusa busa" = "there was emptiness" – yariho is impersonal.*
         * "hari" (existential) similarly carries no agreement obligation.     *
         * Skip agreement checking for all copula locative tenses.            */
        if (verb->verb_tense == TENSE_COPULA_PAST ||
            verb->verb_tense == TENSE_COPULA_PRES) continue;

        if (nc != vc) {
            /* Before flagging, scan further back: if an earlier noun in the
             * sentence has a class that matches the verb's SP (vc), then the
             * immediate predecessor is an object, not the subject.  Suppress
             * the error — the true subject is that earlier noun.
             * Also treat Nt.4/9 as equivalent (same SP "i") when scanning. */
            bool has_remote_subject = false;
            bool crossed_conj = false;
            for (int j = i - 1; j >= 0; j--) {
                const Token *t = &sa->tokens[j];
                /* Stop scanning back at sentence boundaries               */
                if (t->pos == POS_PUNCTUATION && t->is_sent_boundary) break;
                int tc = 0;
                if (t->pos == POS_NOUN && t->noun_class > 0) {
                    tc = t->noun_class;
                } else if (t->pos == POS_VERB_CONJ && t->noun_class > 0
                           && t->gram_role != GRAM_ROLE_VERBAL_NOUN) {
                    /* A preceding conjugated verb with the same SP class means
                     * this is a coordinate clause sharing the same implicit
                     * subject.  e.g. "bitegeke ... , bitandukanye ..." — both
                     * verbs are class 8; the subject (lights) is established
                     * by the earlier verb's SP, not by any intervening noun.  */
                    tc = t->noun_class;
                } else {
                    if (t->pos == POS_CONJUNCTION) crossed_conj = true;
                    continue;
                }
                if (tc == vc) { has_remote_subject = true; break; }
                if ((tc == 4 || tc == 9) && (vc == 4 || vc == 9))
                    { has_remote_subject = true; break; }
                if ((tc == 1 || tc == 3) && (vc == 1 || vc == 3))
                    { has_remote_subject = true; break; }
                /* Nt.9 (singular) / Nt.10 (plural) are a paired class.
                 * imbuto (Nt.9) is a valid remote subject for a Nt.10 verb. */
                if ((tc == 9 || tc == 10) && (vc == 9 || vc == 10))
                    { has_remote_subject = true; break; }
                /* "ya" SP (stored as class 6) is also the Nt.1/3/9 past-tense
                 * form (a + past-TM-a → ya).  A remote Nt.1/3/9 noun or verb
                 * with the same implicit subject is a valid match for ya- verbs.
                 * e.g. "Imana irangiza imirimo yakoze" — imirimo is the object
                 * of the relative clause; yakoze's subject is Imana (Nt.9).    */
                if (vc == 6 && (tc == 1 || tc == 3 || tc == 9))
                    { has_remote_subject = true; break; }
                /* Coordinate subject: [Nt.1/3 noun] kandi [Nt.1/3 noun] + ba-(Nt.2) verb.
                 * Two human nouns joined by a conjunction collectively take
                 * class-2 plural agreement (ba-).
                 * e.g. "umugabo kandi umugore barakora" is correct grammar.    */
                if (crossed_conj && vc == 2 && (tc == 1 || tc == 3) && (nc == 1 || nc == 3))
                    { has_remote_subject = true; break; }
            }
            if (has_remote_subject) continue;

            /* Sentence-initial capitalised words that the prefix heuristic
             * assigned a non-human class may be proper names of people.
             * In Kinyarwanda all human proper nouns take class-1 agreement
             * (SP "a") regardless of their phonological shape (e.g. Kayini,
             * Kagome, Rukesha, Butera).  When the noun is sentence-initial,
             * capitalised, not already confirmed as a proper noun, and the
             * verb uses human SP (class 1 or 3), suppress the error.      */
            if (isupper((unsigned char)noun->surface[0]) &&
                !noun->is_proper_noun && !noun->is_kinyarwanda && (vc == 1 || vc == 3)) {
                bool sent_initial = (i == 0);
                if (!sent_initial) {
                    for (int k = i - 1; k >= 0; k--) {
                        if (sa->tokens[k].pos == POS_PUNCTUATION) {
                            if (sa->tokens[k].is_sent_boundary)
                                { sent_initial = true; break; }
                            continue;
                        }
                        break;
                    }
                }
                if (sent_initial) continue;
            }

            char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
            snprintf(msg, sizeof(msg),
                "Inshinga '%s' ntishyikira izina '%s': "
                "inteko y'inshinga=%d ariko inteko y'izina=%d. "
                "Verb '%s' subject prefix (class %d) doesn't agree "
                "with noun '%s' (class %d).",
                verb->surface, noun->surface, vc, nc,
                verb->surface, vc, noun->surface, nc);
            const NounClass *ncls = kin_get_noun_class(nc);
            snprintf(sug, sizeof(sug),
                "Indangasubizi igomba kuba '%s' (inteko %d). "
                "The subject prefix for class %d should be '%s'.",
                ncls ? ncls->subj_prefix : "?", nc,
                nc, ncls ? ncls->subj_prefix : "?");
            add_error(sa, ERR_SUBJ_VERB_AGREEMENT, i + 1, msg, sug);
        }
    }

    /* RULE 6: Vowel contact (iranya ry'impanvu) – Amategeko y'igenamajwi    *
     * No two vowels may appear adjacent in a Kinyarwanda word.             *
     * When VV contact occurs at a morpheme boundary one of these must apply:*
     *   u→w  (mweza ← mu+iza),  i→y (cyeza ← ki+iza),                    *
     *   a→Ø  elision (beza ← ba+iza),  a+i→e fusion (benja ← ba+inja).   *
     * We flag words that contain VV hiatus and are tagged as Kinyarwanda.  */
    for (int i = 0; i < sa->token_count; i++) {
        Token *t = &sa->tokens[i];
        /* Only check words we believe are Kinyarwanda and not interjections */
        if (t->pos == POS_PUNCTUATION) continue;
        if (!t->is_kinyarwanda) continue;
        if (t->pos == POS_INTERJECTION) continue;
        if (t->is_proper_noun) continue;
        if (!kin_has_vowel_hiatus(t->lower)) continue;

        /* Determine which corrective rule should apply based on the pair    */
        char pair[3] = {0};
        const char *w = t->lower;
        for (int j = 0; w[j] && w[j+1]; j++) {
            if (is_vowel_ch(w[j]) && is_vowel_ch(w[j+1])) {
                pair[0] = w[j]; pair[1] = w[j+1]; break;
            }
        }
        char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
        const char *rule = "a→Ø (elision)";
        if (pair[0] == 'u') rule = "u→w (glide: uX → wX)";
        else if (pair[0] == 'i') rule = "i→y (glide: iX → yX)";
        else if (pair[0] == 'a' && pair[1] == 'i') rule = "a+i→e (ubumwe)";
        snprintf(msg, sizeof(msg),
            "Iranya ry'impanvu: '%s' irimo impanvu ebyiri zisubiranya ('%c%c'). "
            "Vowel hiatus in '%s': vowels '%c' and '%c' are adjacent.",
            t->surface, pair[0], pair[1],
            t->surface, pair[0], pair[1]);
        snprintf(sug, sizeof(sug),
            "Amategeko y'igenamajwi agomba gukurikizwa: %s. "
            "Apply phonological rule: %s.",
            rule, rule);
        add_error(sa, ERR_VOWEL_HIATUS, i, msg, sug);
    }

    /* RULE 6b: Letter 'l' in native Kinyarwanda words
     * Official Orthography Rules §2.3: 'l' exists only in proper names
     * (Kigali, Repubulika, Leta) and foreign words.  Any 'l' in a recognised
     * Kinyarwanda word that is NOT a proper noun is an orthographic error.  */
    for (int i = 0; i < sa->token_count; i++) {
        Token *t = &sa->tokens[i];
        if (t->pos == POS_PUNCTUATION) continue;
        if (!t->is_kinyarwanda) continue;
        if (t->is_proper_noun) continue;
        if (t->pos == POS_FOREIGN) continue;
        if (!strchr(t->lower, 'l')) continue;
        char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
        snprintf(msg, sizeof(msg),
            "Inyuguti §2.3: '%s' irimo inyuguti 'l' itavugwa mu Kinyarwanda. "
            "Letter 'l' in '%s' is not a native Kinyarwanda letter.",
            t->surface, t->surface);
        snprintf(sug, sizeof(sug),
            "Suzuma niba ari ijambo ry'amahanga cyangwa izina bwite. "
            "Check if this is a foreign word or proper name.");
        add_error(sa, ERR_SPELLING, i, msg, sug);
    }

    /* RULE 7: kugenda vs kujya — directional-motion verb selection
     *
     * In standard Kinyarwanda there are two distinct "to go" verbs:
     *   kugenda  = manner/non-directional: "to walk / to travel / to move"
     *              Takes manner adverbs:  "aragenda buhoro" (walks slowly) ✓
     *              Takes path phrases:   "aragenda ku muhanda" (on the road) ✓
     *              Does NOT take bare destination nouns.
     *   kujya    = directional:          "to go TO (a destination)"
     *              Takes destination NP: "ajya ku ishuri"  (goes to school) ✓
     *                                    "azajya i Kigali" (will go to Kigali) ✓
     *              Requires locative connector before common nouns:
     *                ku  = to/at a place from outside (ku ishuri, ku ibitaro)
     *                mu  = into an enclosed space     (mu nzu, mu cyumba, mu gari)
     *                i   = to a proper place name     (i Kigali, i Musanze)
     *
     * When a "gend"-stem verb is immediately followed by a bare destination
     * noun (no intervening preposition), flag the error and suggest kujya.
     *
     * Source: native-speaker correction (2026-03-21).
     */
    for (int i = 0; i < sa->token_count - 1; i++) {
        Token *verb = &sa->tokens[i];
        if (verb->pos == POS_PUNCTUATION) continue;
        /* Find next non-punctuation token */
        int ni = i + 1;
        while (ni < sa->token_count && sa->tokens[ni].pos == POS_PUNCTUATION) ni++;
        if (ni >= sa->token_count) continue;
        Token *next = &sa->tokens[ni];

        if (verb->pos != POS_VERB_CONJ) continue;
        bool is_kugenda = (strcmp(verb->stem, "gend") == 0 || strcmp(verb->stem, "end") == 0);
        bool is_kujya   = (strcmp(verb->stem, "jy")   == 0 || strcmp(verb->stem, "giy") == 0);
        if (!is_kugenda && !is_kujya) continue;

        /* Case A: kugenda + bare common noun (no preposition)
         * e.g. "aragenda ishuri" — should be "ajya ku ishuri"               */
        if (is_kugenda && next->pos == POS_NOUN && !next->is_proper_noun) {
            char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
            snprintf(msg, sizeof(msg),
                "Guhitamo inshinga nabi: '%s' (kugenda) ikurikirwa n'izina '%s' "
                "nta mugereka wo hagati. "
                "Wrong verb: 'kugenda' (manner of movement) followed directly by "
                "destination noun '%s' without a locative connector. "
                "Use 'kujya' + locative (ku/mu) for going TO a destination.",
                verb->surface, next->surface, next->surface);
            snprintf(sug, sizeof(sug),
                "Jyana 'kugenda' na 'kujya' + mugereka w'ahantu: "
                "'ajya ku %s' (ahantu ho hanze) cyangwa 'ajya mu %s' (aho binjiramo). "
                "Use 'ku' for institutions/destinations, 'mu' for enclosed spaces.",
                next->surface, next->surface);
            add_error(sa, ERR_VERB_SELECTION, i, msg, sug);
        }

        /* Case B: kugenda + locative "i" + destination name
         * e.g. "Uragenda i Kigali" — should be "Ujya i Kigali"
         * "i" (locative) marks a destination; only kujya takes a destination. */
        if (is_kugenda && next->pos == POS_LOCATIVE && strcmp(next->lower, "i") == 0) {
            int ni2 = ni + 1;
            while (ni2 < sa->token_count && sa->tokens[ni2].pos == POS_PUNCTUATION) ni2++;
            if (ni2 < sa->token_count) {
                Token *dest = &sa->tokens[ni2];
                if (dest->is_proper_noun || dest->pos == POS_NOUN) {
                    char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
                    snprintf(msg, sizeof(msg),
                        "Guhitamo inshinga nabi: '%s' (kugenda) ikurikirwa n'indangahantu "
                        "'i %s'. Inshinga 'kugenda' ntiyemera intego y'ahantu. "
                        "Wrong verb: 'kugenda' cannot take a locative destination 'i %s'. "
                        "Use 'kujya' for going TO a place.",
                        verb->surface, dest->surface, dest->surface);
                    snprintf(sug, sizeof(sug),
                        "Jyana inshinga 'kugenda' n'inshinga 'kujya' iyo ushaka kuvuga "
                        "kujya ahantu. Urugero: 'urajya i Kigali' (not 'uragenda i Kigali'). "
                        "Replace 'kugenda' with 'kujya'. "
                        "E.g. 'urajya i Kigali' / 'uzajya i Kigali' / 'wagiye i Kigali'.");
                    add_error(sa, ERR_VERB_SELECTION, i, msg, sug);
                }
            }
        }

        /* Case C: kujya + bare common noun (missing locative connector)
         * e.g. "Ajya ishuri" — correct form is "Ajya ku ishuri"
         * kujya requires ku (to/at a place) or mu (into a space) before the noun.
         * Spoken: "ku ishuri" → "kwishuri"; written standard: "ku ishuri".
         * Case B above handles proper nouns with "i"; this catches common nouns. */
        if (is_kujya && next->pos == POS_NOUN && !next->is_proper_noun) {
            char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
            snprintf(msg, sizeof(msg),
                "Inshinga 'kujya' isaba mugereka w'ahantu (ku cyangwa mu) "
                "imbere y'izina '%s'. "
                "Missing locative connector before '%s': "
                "'kujya' requires 'ku' or 'mu' before a destination noun.",
                next->surface, next->surface);
            snprintf(sug, sizeof(sug),
                "Shyira 'ku' imbere y'izina (ahantu ho hanze): 'ajya ku %s'. "
                "Shyira 'mu' (aho binjiramo): 'ajya mu %s'. "
                "Bavuga 'kw-/mw-' imbere y'inzwi; inyandiko: 'ku/mu + izina'. "
                "Use 'ku' for destinations, 'mu' for enclosed spaces.",
                next->surface, next->surface);
            add_error(sa, ERR_MISSING_LOCATIVE, i, msg, sug);
        }
    }

    /* RULE 8: Conditional clause marking (Inziganyo)                          *
     *                                                                           *
     * When a conditional conjunction ("niba", "nibyo") precedes a conjugated   *
     * verb, that verb is inside a conditional clause.  We set the               *
     * is_conditional_clause flag so the output labels the clause correctly      *
     * WITHOUT overriding the verb's actual tense (past/present/future).        *
     * The verb keeps its own tense; the clause type is shown separately.       *
     *                                                                           *
     * Verbs already detected as TENSE_CONDITIONAL by the morphology engine     *
     * (SP+a+[ku]+stem+a pattern) keep their tense tag — only the flag is set.  *
     *                                                                           *
     * The verb is not necessarily the immediately next token — a subject noun  *
     * may intervene ("niba umwana aragenda"). We look up to 3 tokens ahead.    *
     *                                                                           *
     * "iyo" as conditional connector (sentence-initial): handled when it       *
     * appears as the first token or immediately after a conjunction.  In all   *
     * other positions it is a demonstrative/relative pronoun and is left alone.*/
    /* "nibyo" = ni+byo = "that's right / indeed" (Nt.8 copula) — NOT conditional */
    static const char *COND_MARKERS[] = { "niba", NULL };

    for (int i = 0; i < sa->token_count; i++) {
        Token *t = &sa->tokens[i];
        bool is_cond = false;

        /* Lexically-marked conditional conjunctions */
        if (t->pos == POS_CONJUNCTION) {
            for (int k = 0; COND_MARKERS[k]; k++) {
                if (strcmp(t->lower, COND_MARKERS[k]) == 0) { is_cond = true; break; }
            }
        }

        /* "iyo" as conditional: only sentence-initial or after a conjunction   *
         * (e.g. "...ariko iyo...").  Elsewhere it is a demonstrative pronoun.  */
        if (!is_cond && strcmp(t->lower, "iyo") == 0) {
            if (i == 0)                                          is_cond = true;
            else if (sa->tokens[i-1].pos == POS_CONJUNCTION)    is_cond = true;
        }

        if (!is_cond) continue;

        /* Mark the first conjugated verb within the next 3 tokens as being
         * inside a conditional clause.  Preserve its actual verb_tense. */
        for (int j = i + 1; j < sa->token_count && j <= i + 3; j++) {
            if (sa->tokens[j].pos == POS_VERB_CONJ) {
                sa->tokens[j].is_conditional_clause = true;
                break;
            }
        }
    }

    /* RULE 9: Vowel harmony on verb extensions (-ish-/-esh-, -ik-/-ek-)
     *
     * Kinyarwanda vowel harmony (Zorc & Nibagwire §2.5.13):
     *   Root last vowel is mid (e/o)     → extension must be -esh- or -ek-
     *   Root last vowel is non-mid (a/i/u) → extension must be -ish- or -ik-
     *
     * gukorisha  → root "kor" has /o/ (mid) → -esh- required → flag gukorisha
     * gusomisha  → root "som" has /o/ (mid) → -esh- required → flag gusomisha
     *
     * Note: verb_ext is not set for POS_VERB_INF tokens (pos_tagger.c limitation),
     * so we inspect the morpheme breakdown directly instead of filtering by verb_ext.
     */
    for (int i = 0; i < sa->token_count; i++) {
        Token *t = &sa->tokens[i];
        if (t->pos != POS_VERB_INF && t->pos != POS_VERB_CONJ) continue;

        /* Find the EXT morpheme and the root morpheme in the breakdown */
        const char *ext_surf = NULL;
        const char *root_form = NULL;
        for (int m = 0; m < t->morph.n; m++) {
            if (strcmp(t->morph.m[m].label, "EXT")  == 0) ext_surf  = t->morph.m[m].surface;
            if (strcmp(t->morph.m[m].label, "root") == 0) root_form = t->morph.m[m].form;
        }
        if (!ext_surf || !root_form) continue;
        /* Only check harmony-sensitive allomorphs */
        if (strcmp(ext_surf,"ish") != 0 && strcmp(ext_surf,"esh") != 0 &&
            strcmp(ext_surf,"ik")  != 0 && strcmp(ext_surf,"ek")  != 0) continue;

        /* Determine if root's last vowel is mid (e/o) */
        bool root_mid = false;
        bool found_vowel = false;
        for (int k = (int)strlen(root_form) - 1; k >= 0 && !found_vowel; k--) {
            char c = root_form[k];
            if (c == 'e' || c == 'o') { root_mid = true;  found_vowel = true; }
            else if (c == 'a' || c == 'i' || c == 'u') { root_mid = false; found_vowel = true; }
        }
        if (!found_vowel) continue;

        /* Check: mid root + -ish-/-ik- = wrong; non-mid root + -esh-/-ek- = wrong */
        bool wrong_ish = root_mid  && (strcmp(ext_surf, "ish") == 0 || strcmp(ext_surf, "ik") == 0);
        bool wrong_esh = !root_mid && (strcmp(ext_surf, "esh") == 0 || strcmp(ext_surf, "ek") == 0);
        if (!wrong_ish && !wrong_esh) continue;

        /* Build the correct form: replace the wrong allomorph with the right one */
        const char *wrong_ext  = wrong_ish ? ext_surf : ext_surf; /* "ish"/"ik" or "esh"/"ek" */
        const char *right_ext  = wrong_ish
            ? (strcmp(ext_surf, "ish") == 0 ? "esh" : "ek")
            : (strcmp(ext_surf, "esh") == 0 ? "ish" : "ik");

        /* Build corrected surface word: find the wrong ext in the surface and replace */
        char corrected[KIN_MAX_WORD] = {0};
        const char *pos = strstr(t->surface, wrong_ext);
        if (pos) {
            size_t prefix_len = (size_t)(pos - t->surface);
            strncpy(corrected, t->surface, prefix_len);
            corrected[prefix_len] = '\0';
            strncat(corrected, right_ext, sizeof(corrected) - strlen(corrected) - 1);
            strncat(corrected, pos + strlen(wrong_ext),
                    sizeof(corrected) - strlen(corrected) - 1);
        } else {
            strncpy(corrected, t->surface, sizeof(corrected) - 1);
        }

        char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
        snprintf(msg, sizeof(msg),
            "Uvuguruye amazina (-ish-/-esh-, -ik-/-ek-): '%s' — igicumbi '%s' "
            "gifite inshuro y'icyembe (%s), bityo ikinyabiziga kigomba kuba '-%s-', "
            "si '-%s-'. "
            "Vowel harmony: '%s' has mid vowel (%s), so extension must be '-%s-', "
            "not '-%s-'.",
            t->surface, root_form,
            root_mid ? "e/o" : "a/i/u",
            right_ext, wrong_ext,
            root_form, root_mid ? "e/o" : "a/i/u",
            right_ext, wrong_ext);
        snprintf(sug, sizeof(sug), "Hindura '%s' ugakoresheje '%s'."
            " / Replace '%s' with '%s'.",
            t->surface, corrected, t->surface, corrected);
        add_error(sa, ERR_SPELLING, i, msg, sug);
    }

    /* RULE 10: Cross-word connector elision (ikata ry'inyajwi hagati y'amagambo)
     * Connectors/conjunctions in their full vowel-final form must be written
     * as elided (drop final vowel + apostrophe) before a vowel-initial word.
     * Source: RALC Orthography Rules §12.1 (Ikata ry'inyajwi).
     *
     * Exceptions that NEVER elide (§12.3-12.4):
     *   ku, mu  — prepositions
     *   ni, si  — copula / negative copula
     *   nyiri   — possessive title
     * Numbers (POS_NUMBER) and punctuation tokens are also skipped.         */
    static const struct { const char *full; const char *elided; }
    CONN_ELISION[] = {
        /* Possessive connectors (ibinyazina ngenera) */
        {"ya",  "y'"},   /* Nt.4, Nt.6, Nt.9  */
        {"wa",  "w'"},   /* Nt.1, Nt.3         */
        {"ba",  "b'"},   /* Nt.2               */
        {"rya", "ry'"},  /* Nt.5               */
        {"cya", "cy'"},  /* Nt.7               */
        {"bya", "by'"},  /* Nt.8               */
        {"za",  "z'"},   /* Nt.10, Nt.12       */
        {"rwa", "rw'"},  /* Nt.11              */
        {"twa", "tw'"},  /* Nt.13              */
        {"bwa", "bw'"},  /* Nt.14              */
        {"kwa", "kw'"},  /* Nt.15              */
        {"hwa", "hw'"},  /* Nt.16              */
        /* Conjunctions */
        {"na",  "n'"},
        {"nka", "nk'"},
        {NULL,  NULL}
    };

    for (int i = 0; i + 1 < sa->token_count; i++) {
        const Token *tc = &sa->tokens[i];      /* connector candidate  */
        const Token *tn = &sa->tokens[i + 1];  /* following word       */

        /* Skip punctuation tokens on either side */
        if (tc->pos == POS_PUNCTUATION || tn->pos == POS_PUNCTUATION) continue;
        /* Skip number tokens as the next word */
        if (tn->pos == POS_NUMBER) continue;

        /* Find this token's lower form in the elision table */
        const char *elided = NULL;
        for (int k = 0; CONN_ELISION[k].full; k++) {
            if (strcmp(tc->lower, CONN_ELISION[k].full) == 0) {
                elided = CONN_ELISION[k].elided;
                break;
            }
        }
        if (!elided) continue;

        /* Check if the following word starts with a vowel */
        if (!tn->lower[0] || !is_vowel_ch(tn->lower[0])) continue;

        char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
        snprintf(msg, sizeof(msg),
            "Ijambo '%s' riri imbere y'ijambo ritangira indomo ('%s') — "
            "inyajwi isoza '%s' igomba gukatwa (kaswe), wandika '%s%s'. / "
            "'%s' precedes a vowel-initial word ('%s') — final vowel must be "
            "elided; write '%s%s' instead.",
            tc->surface, tn->surface, tc->surface,
            elided, tn->surface,
            tc->surface, tn->surface,
            elided, tn->surface);
        snprintf(sug, sizeof(sug),
            "Hindura '%s %s' ugakoresheje '%s%s'. / "
            "Replace '%s %s' with '%s%s'.",
            tc->surface, tn->surface, elided, tn->surface,
            tc->surface, tn->surface, elided, tn->surface);
        add_error(sa, ERR_VOWEL_HIATUS, i, msg, sug);
    }

    /* RULE 11: Locative form selection — mu/muri and ku/kuri             *
     *                                                                     *
     * "-ri" is the locative copula (verb "to be" in locative form).      *
     * It is REQUIRED when the complement is:                             *
     *   • a personal pronoun  (muri mwe, muri bo, kuri we, kuri mwe…)   *
     *   • a proper name       (muri Egiputa, muri Kristo — handled below)*
     *                                                                     *
     * It is WRONG when the complement is a plain common noun:            *
     *   muri nzu → mu nzu  /  muri gihugu → mu gihugu                   *
     *                                                                     *
     * Source: corpus analysis of Bibiliya Yera (30,984 sentences):       *
     *   muri + personal pronoun: 1,156 instances (zero exceptions)       *
     *   mu  + common noun:       13,626 instances (zero muri+common_noun)*
     *   kuri + personal pronoun: 200+ instances                          *
     *   ku  + common noun:       6,197 instances                         *
     *                                                                     *
     * 11a: mu  + PRON_PERSONAL  → should be "muri"                      *
     * 11b: ku  + PRON_PERSONAL  → should be "kuri"                      *
     * 11c: muri + common noun   → should be "mu"                        */
    for (int i = 0; i < sa->token_count - 1; i++) {
        const Token *loc = &sa->tokens[i];
        if (loc->pos != POS_LOCATIVE) continue;

        /* Find next non-punctuation token */
        int ni = i + 1;
        while (ni < sa->token_count && sa->tokens[ni].pos == POS_PUNCTUATION) ni++;
        if (ni >= sa->token_count) continue;
        const Token *nxt = &sa->tokens[ni];

        char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];

        /* 11a: mu + personal pronoun */
        if (strcmp(loc->lower, "mu") == 0 &&
            nxt->pos == POS_PRONOUN && nxt->pron_type == PRON_PERSONAL) {
            snprintf(msg, sizeof(msg),
                "'mu' ntishyiranye n'ikinyazina '%s'. "
                "Indangahantu 'mu' isaba '-ri' imbere y'ibinyazina ngenga. "
                "'mu' cannot precede a personal pronoun '%s'; "
                "the locative copula '-ri' is required.",
                nxt->surface, nxt->surface);
            snprintf(sug, sizeof(sug),
                "Hindura 'mu %s' ugakoresheje 'muri %s'. "
                "Replace 'mu %s' with 'muri %s'.",
                nxt->surface, nxt->surface, nxt->surface, nxt->surface);
            add_error(sa, ERR_WRONG_LOCATIVE, i, msg, sug);
        }

        /* 11b: ku + personal pronoun */
        if (strcmp(loc->lower, "ku") == 0 &&
            nxt->pos == POS_PRONOUN && nxt->pron_type == PRON_PERSONAL) {
            snprintf(msg, sizeof(msg),
                "'ku' ntishyiranye n'ikinyazina '%s'. "
                "Indangahantu 'ku' isaba '-ri' imbere y'ibinyazina ngenga. "
                "'ku' cannot precede a personal pronoun '%s'; "
                "use 'kuri' instead.",
                nxt->surface, nxt->surface);
            snprintf(sug, sizeof(sug),
                "Hindura 'ku %s' ugakoresheje 'kuri %s'. "
                "Replace 'ku %s' with 'kuri %s'.",
                nxt->surface, nxt->surface, nxt->surface, nxt->surface);
            add_error(sa, ERR_WRONG_LOCATIVE, i, msg, sug);
        }

        /* 11c: muri + common noun (no demonstrative) → should be mu
         * Exception: muri + proper noun (muri Egiputa) is CORRECT.
         * Exception: when the next token is POS_CONJUNCTION (demonstrative
         * like icyo/iyo/uwo tagged as CONJ), "muri icyo gihe" is correct. */
        if (strcmp(loc->lower, "muri") == 0 &&
            nxt->pos == POS_NOUN && !nxt->is_proper_noun) {
            snprintf(msg, sizeof(msg),
                "'muri' ntishyiranye n'izina mbonera '%s'. "
                "Indangahantu 'muri' ikoreshwa imbere y'ibinyazina ngenga "
                "n'amazina bwite (akarere/igihugu), si imbere y'amazina mbonera. "
                "'muri' is used before pronouns and proper names, "
                "not before common nouns like '%s'.",
                nxt->surface, nxt->surface);
            snprintf(sug, sizeof(sug),
                "Hindura 'muri %s' ugakoresheje 'mu %s'. "
                "Replace 'muri %s' with 'mu %s'.",
                nxt->surface, nxt->surface, nxt->surface, nxt->surface);
            add_error(sa, ERR_WRONG_LOCATIVE, i, msg, sug);
        }
    }

    /* RULE 12: Purpose particle "ngo" / "kugira ngo" requires subjunctive mood
     *
     * In Kinyarwanda the purpose conjunction "ngo" ("in order to / so that")
     * and the compound "kugira ngo" ("so that / in order that") MUST be
     * followed by a verb in the subjunctive (ikigombero: SP + stem + -e).
     *
     * Corpus evidence (Bibiliya Yera, 30 984 sentences):
     *   All 1 158 instances of "kugira ngo" are followed by a subjunctive or
     *   negative-relative verb: abe, babone, mumenye, ubone, mubone, mube,
     *   menye (TENSE_SUBJUNCTIVE) and hatagira, itagira (TENSE_NEG_RELATIVE).
     *   Zero occurrences of an indicative verb after "kugira ngo".
     *
     * Detection:
     *   "ngo" tagged POS_VERB_PARTICLE used as a PURPOSE particle
     *   (not hearsay) is identified by context:
     *     • Preceded by "kugira" (POS_VERB_INF, stem=="gir") — definite purpose
     *     • Not sentence-initial AND not immediately after a clause/sentence
     *       boundary mark — mid-sentence ngo is purpose in formal/written text
     *
     * Only clear indicative tenses are flagged:
     *   TENSE_PRESENT / TENSE_PRESENT_NORA / TENSE_PAST_PERF /
     *   TENSE_PAST_IMPF / TENSE_FUTURE
     *
     * Source: Bibiliya Yera corpus + Zorc & Nibagwire (1990) §7.4;
     *         REB S4 textbook (ikigombero section).
     */
    for (int i = 0; i < sa->token_count; i++) {
        Token *t = &sa->tokens[i];
        if (t->pos != POS_VERB_PARTICLE) continue;
        if (strcmp(t->lower, "ngo") != 0) continue;

        /* ── Distinguish purpose ngo from hearsay ngo ───────────────────── */
        bool is_purpose = false;

        /* Case A: immediately preceded by kugira (verb inf, stem "gir") */
        for (int k = i - 1; k >= 0; k--) {
            Token *prev = &sa->tokens[k];
            if (prev->pos == POS_PUNCTUATION) continue;
            if (prev->pos == POS_VERB_INF &&
                strcmp(prev->stem, "gir") == 0)
                is_purpose = true;
            break;
        }

        /* Case B: mid-sentence ngo — not sentence-initial and not right after
         * a clause or sentence boundary (comma, semicolon, period…).       */
        if (!is_purpose && i > 0) {
            Token *prev = &sa->tokens[i - 1];
            bool after_boundary = (prev->pos == POS_PUNCTUATION &&
                                   (prev->is_clause_boundary ||
                                    prev->is_sent_boundary));
            if (!after_boundary) is_purpose = true;
        }

        if (!is_purpose) continue;

        /* ── Find the next conjugated verb (up to 4 tokens ahead) ──────── *
         * Only nouns and pronouns may intervene between "ngo" and its    *
         * complement verb (they act as the subject of the purpose clause). *
         * Any other non-verb token (conjunction, locative, adverb, etc.) *
         * signals the end of the complement clause scope — stop scanning.  *
         * Note: some short verb forms (e.g. "abe", "mbe") are stored in   *
         * INVARIABLES as POS_CONJUNCTION; they end the scope here, which  *
         * is correct: if the complement verb is already "abe" (subj.), no *
         * further verb needs to be checked.                                */
        for (int j = i + 1; j < sa->token_count && j <= i + 4; j++) {
            Token *vt = &sa->tokens[j];
            if (vt->pos == POS_PUNCTUATION) {
                if (vt->is_clause_boundary || vt->is_sent_boundary) break;
                continue;
            }
            /* Subject noun/pronoun of the purpose clause — skip over */
            if (vt->pos == POS_NOUN || vt->pos == POS_PRONOUN) continue;
            /* Anything other than a conjugated verb ends the scope */
            if (vt->pos != POS_VERB_CONJ) break;

            /* Acceptable: subjunctive, neg-relative (hatagira), or unknown */
            if (vt->verb_tense == TENSE_SUBJUNCTIVE     ||
                vt->verb_tense == TENSE_SUBJUNCTIVE_LOC ||
                vt->verb_tense == TENSE_NEG_RELATIVE    ||
                vt->verb_tense == TENSE_NONE) break;

            /* Flag only unambiguous indicative tenses */
            if (vt->verb_tense == TENSE_PRESENT      ||
                vt->verb_tense == TENSE_PRESENT_NORA ||
                vt->verb_tense == TENSE_PAST_PERF    ||
                vt->verb_tense == TENSE_PAST_IMPF    ||
                vt->verb_tense == TENSE_FUTURE) {
                char msg[KIN_MAX_MSG], sug[KIN_MAX_MSG];
                snprintf(msg, sizeof(msg),
                    "Inshinga '%s' ikurikira '%s' igomba kuba mu ikigombero "
                    "(SP + igicumbi + -e). "
                    "'%s' follows purpose particle '%s' and must be in the "
                    "subjunctive (ikigombero: SP + root + e).",
                    vt->surface, t->surface,
                    vt->surface, t->surface);
                /* Build the corrected form: replace final -a with -e */
                char corrected[KIN_MAX_WORD];
                strncpy(corrected, vt->surface, sizeof(corrected) - 1);
                corrected[sizeof(corrected) - 1] = '\0';
                size_t clen = strlen(corrected);
                if (clen > 0 && corrected[clen - 1] == 'a')
                    corrected[clen - 1] = 'e';
                snprintf(sug, sizeof(sug),
                    "Hindura '%s' ugakoresheje ikigombero: '%s'. "
                    "Replace '%s' with its subjunctive form: '%s' "
                    "(final vowel -a → -e).",
                    vt->surface, corrected,
                    vt->surface, corrected);
                add_error(sa, ERR_WRONG_VERB_MOOD, j, msg, sug);
            }
            break;
        }
    }

    sa->is_complete = sa->has_verb && (sa->error_count == 0);
}
