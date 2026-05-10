/*
 * gloss.c — Morpheme English gloss system for the Kinyarwanda NLP engine.
 *
 * PURPOSE
 * ───────
 * Every akaramejambo (morpheme) identified by the analysis pipeline carries
 * structural labels (SP, TM, root, FV …) but no English meaning.  This module
 * fills the KinMorpheme.english_gloss field for each morpheme so that:
 *
 *   1. Kinyarwanda→English translation can work from the root up: each morpheme
 *      is a building block whose meaning is independently established before the
 *      translator combines them into a natural English equivalent.
 *
 *   2. Sentence accuracy checking can reference meaning, not just form.
 *
 *   3. ASR (speech-to-text) downstream work gets a lexically-grounded
 *      phoneme-to-meaning mapping from the G2P+gloss combination.
 *
 * GLOSSING STANDARD
 * ─────────────────
 * Abbreviations follow the Leipzig Glossing Rules (LGR, 2015 revision):
 *   1SG/2SG/1PL/2PL/3SG/3PL   — grammatical person + number
 *   HUM / DIM / ABSTR / LOC   — semantic class
 *   PRES / FUT / PRF / IMPF   — tense / aspect
 *   SUBJ / IMP / SEQ / OPT   — mood / mode
 *   NEG / COND                — polarity / conditionality
 *   PASS / CAUS / APPL / RECP / STAT / REV — derivational voice
 *   INF / IND / COP           — infinitive / indicative / copula
 *   CL{n}                     — Bantu noun class number
 *   Nt.{n}                    — indanganteko (class marker) number
 *   AGR.CL{n}                 — concordance agreement for class n
 *   OM.CL{n}                  — object-marker for class n
 *
 * AMATEGEKO Y'IGENAMAJWI (phonological rules) and word-connection rules:
 *   Phonological rules that joined morphemes are already documented in the
 *   KinMorpheme.rule field (e.g. "u→w §1.1").  This module adds MEANING on
 *   top of that structure, enabling translation from morpheme pairs outward.
 */

#include <string.h>
#include <stdio.h>
#include "../include/kinyarwanda.h"

/* ══════════════════════════════════════════════════════════════════════════
 * 1. VERB STEM GLOSS TABLE
 * Maps igicumbi (verb stem) to English gloss.
 * Covers the 200+ most frequent stems from REB textbooks + Bible corpus.
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct { const char *stem; const char *gloss; } VerbGloss;

static const VerbGloss VERB_GLOSS_TABLE[] = {
    /* ── Core textbook verbs ─────────────────────────────────────────────── */
    { "som",       "read"              }, { "bon",       "see/get"            },
    { "ig",        "learn/study"       }, { "andik",     "write"              },
    { "kund",      "love"              }, { "ang",       "hate"               },
    { "end",       "go/walk"           }, { "reb",       "look at"            },
    { "tur",       "live/reside"       }, { "fat",       "take/hold"          },
    { "vug",       "speak/say"         }, { "baz",       "ask"                },
    { "kor",       "work/do"           }, { "mar",       "finish"             },
    { "gir",       "have/be"           }, { "seng",      "pray/worship"       },
    { "kiz",       "save/heal"         }, { "bak",       "build/construct"    },
    { "ger",       "reach/arrive"      }, { "zuk",       "rise/resurrect"     },
    { "injir",     "enter/go in"       }, { "twar",      "carry/lead"         },
    { "mvir",      "obey/listen"       }, { "zan",       "bring"              },
    { "vuk",       "be born"           }, { "byuk",      "get up/wake up"     },
    { "tiny",      "fear/be afraid"    }, { "kir",       "heal/recover"       },
    { "rem",       "create/form"       }, { "it",        "call/name"          },
    { "shyir",     "put/place"         }, { "fash",      "help"               },
    { "ganir",     "talk/converse"     }, { "tek",       "cook"               },
    { "nywa",      "drink"             }, { "rug",       "cook/prepare"       },
    { "jy",        "go to"             }, { "gend",      "go/travel"          },
    { "shob",      "be able/can"       }, { "yumv",      "hear/understand"    },
    { "tah",       "go home"           }, { "tum",       "send/cause"         },
    { "tor",       "choose/vote"       }, { "mer",       "grow/sprout"        },
    { "ban",       "live together"     }, { "bar",       "count"              },
    { "imb",       "dig"               }, { "shak",      "want/seek"          },
    { "tang",      "give/offer"        }, { "ic",        "kill"               },
    /* ── High-frequency auxiliary / copula ───────────────────────────────── */
    { "b",         "be/exist"          }, { "z",         "come/arrive"        },
    { "h",         "give"              }, { "v",         "come from/leave"    },
    { "j",         "come"              }, { "jy",        "go to"              },
    /* ── Common action verbs ─────────────────────────────────────────────── */
    { "ter",       "plant/strike"      }, { "men",       "break/shatter"      },
    { "rek",       "leave/let go"      }, { "sab",       "ask/request"        },
    { "gur",       "buy/purchase"      }, { "sig",       "leave/anoint"       },
    { "rot",       "dream"             }, { "rang",      "lead/precede"       },
    { "tsind",     "conquer/win"       }, { "hung",      "flee/escape"        },
    { "zirik",     "meditate"          }, { "bwir",      "tell"               },
    { "izer",      "believe/trust"     }, { "meny",      "know/understand"    },
    { "emez",      "confirm"           }, { "erek",      "show/point"         },
    { "ririmb",    "sing"              }, { "byar",      "give birth"         },
    { "onger",     "add/do again"      }, { "tesh",      "lose"               },
    { "ish",       "finish/end"        }, { "kubit",     "strike/hit/beat"    },
    { "pfa",       "die"               }, { "pf",        "die"                },
    { "cungur",    "redeem/ransom"     }, { "himbaz",    "celebrate/praise"   },
    { "humur",     "comfort/console"   }, { "hinduk",    "change/turn"        },
    { "tekerez",   "think/reflect"     }, { "komez",     "continue/strengthen"},
    { "emer",      "believe/accept"    }, { "eger",      "approach/come near" },
    { "ron",       "find/discover"     }, { "vom",       "draw/fetch water"   },
    { "teg",       "prepare/trap"      }, { "koherez",   "send on errand"     },
    { "geragez",   "try/attempt"       }, { "hagarar",   "stop/stand still"   },
    { "hindur",    "change/transform"  }, { "hish",      "hide/conceal"       },
    { "tangir",    "begin/start"       }, { "tabur",     "rescue/save"        },
    { "rir",       "cry"               }, { "sek",       "laugh"              },
    { "shemez",    "praise/glorify"    }, { "barik",     "bless"              },
    { "kang",      "wake/be alert"     }, { "pang",      "plan/arrange"       },
    { "tegur",     "prepare/get ready" }, { "hiriy",     "bless/prosper"      },
    { "fung",      "open"              }, { "twik",      "burn/set fire"      },
    { "rimbuk",    "perish/be destroyed"}, { "tak",      "shout/cry out"      },
    { "zam",       "lift/raise up"     }, { "iruk",      "run"                },
    { "tung",      "possess/maintain"  }, { "gaban",     "share"              },
    { "er",        "shine/be pure"     }, { "komer",     "be strong"          },
    { "komerek",   "be wounded/injured"}, { "bur",       "lack/lose"          },
    { "ihangan",   "endure/be patient" }, { "ihut",      "hurry/be quick"     },
    { "oroh",      "be easy/gentle"    }, { "zamuk",     "go up/ascend"       },
    { "sohok",     "go out/exit"       }, { "ifuz",      "desire/wish"        },
    { "ragir",     "shepherd/guide"    }, { "rog",       "bewitch/poison"     },
    { "giz",       "improve/fix"       }, { "zor",       "be full/satisfied"  },
    { "fot",       "photograph"        }, { "shon",      "set (sun)/sink"     },
    { "kub",       "fold/multiply"     }, { "nyag",      "steal/plunder"      },
    { "hit",       "pass through"      }, { "rahir",     "swear/take oath"    },
    { "cecek",     "be quiet/silent"   }, { "hembur",    "console/wipe tears" },
    { "hugur",     "train/instruct"    }, { "hungur",    "liberate/free"      },
    { "cur",       "forge/shape"       }, { "bat",       "press/oppress"      },
    { "rw",        "fight"             }, { "rwan",      "fight/wage war"     },
    { "rut",       "surpass/exceed"    }, { "humek",     "breathe"            },
    { "gor",       "be difficult"      }, { "tonder",    "be careful"         },
    { "sur",       "visit"             }, { "himb",      "compose/invent"     },
    { "ur",        "wake up/rise"      }, { "rangam",    "be upright/straight"},
    { "geran",     "compare/equate"    }, { "tsimb",     "seize/grab"         },
    { "reber",     "watch/look after"  }, { "rumir",     "swallow"            },
    { "rumb",      "excel/be first"    }, { "sukum",     "push"               },
    { "tuhuk",     "rest/break"        }, { "jyanir",    "take for/bring to"  },
    { "simbur",    "replace/substitute"}, { "rob",       "sink/drown"         },
    { "boh",       "tie/bind"          }, { "tontoy",    "be naive/careless"  },
    { "tondek",    "arrange/organize"  }, { "tonderez",  "be careful"         },
    { "herur",     "stretch/extend"    }, { "nyur",      "pass through"       },
    { "hengam",    "exceed/go beyond"  }, { "vangur",    "mix/stir/blend"     },
    { "konger",    "add more/give more"}, { "ror",       "look at/examine"    },
    { "sezer",     "say goodbye"       }, { "curang",    "play instrument"    },
    { "er",        "shine/be pure"     }, { "rez",       "clean/purify"       },
    { "wez",       "be clean/holy"     }, { "um",        "harden/stay firm"   },
    { "ram",       "grow crops"        }, { "bik",       "store"              },
    { "hind",      "plant"             }, { "ambuk",     "cross"              },
    { "hem",       "breathe with difficulty"}, { "sib",  "erase/miss"        },
    { "vuz",       "play (instrument)" }, { "keb",       "cut by slicing"     },
    { "tem",       "cut by striking"   }, { "kat",       "cut through"        },
    { "subiz",     "answer/respond"    }, { "gez",       "finish/reach"       },
    { "kiranuk",   "be righteous"      }, { "kiraniirw", "be saved/redeemed"  },
    { "hagarar",   "stop/stand still"  }, { "haguruk",   "stand up/rise"      },
    { "manuk",     "go down/descend"   }, { "tandukany", "separate/distinguish"},
    { "jyen",      "walk along"        }, { "irukan",    "chase away/fire"    },
    { "inginga",   "beg/plead"         }, { "rush",      "surpass/be more"    },
    { "jyan",      "go together"       }, { "giy",       "go (past stem)"     },
    { "subit",     "return/go back"    }, { "ton",       "drip/trickle"       },
    { "hug",       "be busy/occupied"  }, { "senyur",    "demolish"           },
    { "ting",      "forbid/prevent"    }, { "kwib",      "remember/recall"    },
    { "umvikan",   "be understood"     }, { "sangir",    "pray for"           },
    { "fungurir",  "open for"          }, { "van",       "part from"          },
    { "tan",       "part ways"         }, { "vunik",     "break/fracture"     },
    { "hur",       "meet/encounter"    }, { "rongor",    "marry"              },
    { "fung",      "open (door)"       }, { "shirom",    "be ashamed"         },
    { "nyw",       "drink (form)"      }, { "nywer",     "give to drink"      },
    { "nywish",    "make drink"        }, { "yimb",      "swell/inflate"      },
    { "nyuran",    "cross each other"  }, { "rakar",     "be angry"           },
    { "rakaz",     "enrage/make angry" }, { "vaner",     "separate from"      },
    { "nywer",     "water/make drink"  }, { "zaner",     "bring for"          },
    { "us",        "resemble/look like"}, { "gus",       "resemble/be similar"},
    { "boher",     "tighten/bind up"   }, { "gob",       "surround/encircle"  },
    { "honger",    "add/congratulate"  }, { "heng",      "migrate/go beyond"  },
    { "hindir",    "pour out/shed"     }, { "hishimir",  "rejoice for"        },
    { "cecek",     "be quiet"          }, { "cebur",     "humiliate/abase"    },
    { "humuk",     "be freed/released" }, { "humat",     "stick/adhere"       },
    { "gendar",    "live/walk/behave"  }, { "komez",     "continue"           },
    { "hongez",    "calm/pacify"       }, { "sim",       "replace/substitute" },
    { "shumbush",  "give replacement"  }, { "simbuz",    "cause to replace"   },
    { NULL,        NULL                }
};

/* ══════════════════════════════════════════════════════════════════════════
 * 2. NOUN STEM GLOSS TABLE
 * Maps igicumbi (noun stem / C morpheme) to English gloss.
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct { const char *igicumbi; const char *gloss; } NounGloss;

static const NounGloss NOUN_GLOSS_TABLE[] = {
    { "ntu",       "person"            }, { "gore",      "woman/wife"         },
    { "gabo",      "man/husband"       }, { "mwana",     "child"              },
    { "nka",       "cow"               }, { "nzu",       "house"              },
    { "tabo",      "book"              }, { "go",        "home/family"        },
    { "isi",       "earth/world"       }, { "mana",      "God"                },
    { "juru",      "sky/heaven"        }, { "mwuka",     "spirit"             },
    { "mucyo",     "light"             }, { "mwijima",   "darkness"           },
    { "mazi",      "water"             }, { "muntu",     "person"             },
    { "nsi",       "earth/ground"      }, { "mbuto",     "seed/fruit"         },
    { "shuti",     "friend"            }, { "tima",      "heart"              },
    { "rezi",      "fire"              }, { "joro",      "night"              },
    { "ku",        "day"               }, { "mpamvu",    "reason/cause"       },
    { "sura",      "chapter/face"      }, { "jagwe",     "lion"               },
    { "nyana",     "son/child"         }, { "baho",      "life"               },
    { "ganza",     "hand/authority"    }, { "gingo",     "arm/branch"         },
    { "kono",      "wing/shoulder"     }, { "sha",       "owner/lord"         },
    { "mwami",     "king/lord"         }, { "bwami",     "kingdom/kingship"   },
    { "hoza",      "peace/comfort"     }, { "kora",      "work/deed"          },
    { "migisha",   "blessings"         }, { "tembera",   "journey/walk"       },
    { "ruhame",    "grace"             }, { "rugwiro",   "holiness"           },
    { "kunda",     "love"              }, { "butegetsi", "authority/power"    },
    { "tema",      "courage"           }, { "rugo",      "homestead/home"     },
    { "muryango",  "door/family"       }, { "ryo",       "river"              },
    { "si",        "earth/land"        }, { "taka",      "cry/shout"          },
    { "sato",      "snake"             }, { "dogo",      "dog"                },
    { "nyoni",     "bird"              }, { "ntama",     "sheep"              },
    { "mbuzi",     "goat"              }, { "inka",      "cow"                },
    { "masa",      "bull/ox"           }, { "hari",      "exists"             },
    { "ri",        "day"               }, { "ye",        "him/her"            },
    { "zo",        "them (Nt.10)"      }, { "byo",       "them (Nt.8)"        },
    { "cyo",       "it (Nt.7)"         }, { "byo",       "them (Nt.8)"        },
    { "yo",        "it (Nt.4/6/9)"     }, { "ryo",       "it (Nt.5)"          },
    { "rwo",       "it (Nt.11)"        }, { "two",       "them (Nt.13)"       },
    { "bwo",       "it (Nt.14)"        }, { "kwo",       "it (Nt.15)"         },
    { "ha",        "here/there"        },
    { NULL,        NULL                }
};

/* ══════════════════════════════════════════════════════════════════════════
 * 3. ADJECTIVE STEM GLOSS
 * ══════════════════════════════════════════════════════════════════════════ */

static const char *adj_stem_gloss(const char *stem) {
    if (!stem || !stem[0]) return "";
    if (strcmp(stem, "nini")  == 0)                          return "big/adult";
    if (strcmp(stem, "to")    == 0 || strcmp(stem, "toto") == 0 ||
        strcmp(stem, "toya")  == 0 || strcmp(stem, "to-to") == 0)
                                                             return "small";
    if (strcmp(stem, "bi")    == 0)                          return "bad";
    if (strcmp(stem, "iza")   == 0)                          return "good/beautiful";
    if (strcmp(stem, "re")    == 0 || strcmp(stem, "re-re") == 0)
                                                             return "long/tall";
    if (strcmp(stem, "gufi")  == 0 || strcmp(stem, "gufiya") == 0)
                                                             return "short";
    if (strcmp(stem, "zima")  == 0)                          return "whole/healthy";
    if (strcmp(stem, "inshi") == 0)                          return "many";
    if (strcmp(stem, "ke")    == 0 || strcmp(stem, "keya") == 0 ||
        strcmp(stem, "ke-ke") == 0)                          return "few";
    if (strcmp(stem, "kuru")  == 0)                          return "old/big";
    if (strcmp(stem, "bisi")  == 0)                          return "raw/unripe";
    if (strcmp(stem, "shya")  == 0 || strcmp(stem, "shyashya") == 0)
                                                             return "new/fresh";
    if (strcmp(stem, "gari")  == 0)                          return "wide";
    if (strcmp(stem, "tindi") == 0)                          return "different/other";
    if (strcmp(stem, "sa")    == 0 || strcmp(stem, "sa-sa") == 0)
                                                             return "like/similar";
    if (strcmp(stem, "eri")   == 0)                          return "two";
    if (strcmp(stem, "atu")   == 0)                          return "three";
    if (strcmp(stem, "ne")    == 0)                          return "four";
    if (strcmp(stem, "anu")   == 0)                          return "five";
    return "";
}

/* ══════════════════════════════════════════════════════════════════════════
 * 4. INVARIABLE WORD GLOSS
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct { const char *word; const char *gloss; } InvGloss;

static const InvGloss INV_GLOSS_TABLE[] = {
    /* Conjunctions */
    { "n",        "and/with"       }, { "na",       "and/with"       },
    { "ariko",    "but"            },
    { "kandi",    "and/also"       }, { "rero",     "so/then"        },
    { "ngo",      "that/saying"    }, { "ko",       "that"           },
    { "nuko",     "so/therefore"   }, { "ahubwo",   "but rather"     },
    { "kuko",     "because"        }, { "kugira",   "so that/in order"},
    { "kuti",     "saying that"    }, { "none",     "now/well then"  },
    { "cyangwa",  "or"             }, { "mbese",    "perhaps/whether"},
    /* Prepositions */
    { "mu",       "in/into"        }, { "ku",       "on/at"          },
    { "i",        "at (place)"     }, { "nka",      "like/as"        },
    { "kuri",     "on/about"       }, { "muri",     "in/within"      },
    { "kwa",      "at (person's)"  }, { "iya",      "of/belonging to"},
    { "ya",       "of"             }, { "rya",      "of (Nt.5)"      },
    { "bya",      "of (Nt.8)"      }, { "cya",      "of (Nt.7)"      },
    /* Adverbs */
    { "neza",     "well/nicely"    }, { "cyane",    "very/a lot"     },
    { "gato",     "a little"       }, { "buhoro",   "slowly"         },
    { "vuba",     "quickly/soon"   }, { "rimwe",    "once/together"  },
    { "kenshi",   "often"          }, { "ntaryo",   "seldom"         },
    { "hanze",    "outside"        }, { "imbere",   "forward/ahead"  },
    { "nyuma",    "after/behind"   }, { "mbere",    "before/first"   },
    { "ubu",      "now"            }, { "kera",     "long ago"       },
    { "none",     "now/today"      }, { "ejo",      "yesterday/tomorrow"},
    { "buri",     "every/each"     },
    /* Locatives */
    { "hano",     "here"           }, { "hariya",   "there"          },
    { "hejuru",   "above/up"       }, { "hasi",     "below/down"     },
    { "hanze",    "outside"        }, { "imbere",   "in front"       },
    { "inyuma",   "behind"         }, { "hagati",   "between/middle" },
    /* Conditionals */
    { "niba",     "if"             }, { "iyo",      "if/when"        },
    /* Quotative */
    { "ati",      "[saying]"       }, { "iti",      "[saying]"       },
    { "bati",     "[they said]"    }, { "ruti",     "[it/he said]"   },
    /* Affirmative / negative */
    { "yego",     "yes"            }, { "oya",      "no"             },
    { "ni",       "is/are (COP)"   }, { "siko",     "it is not"      },
    { "ntabwo",   "not at all"     }, { "nta",      "not/no (NEG)"   },
    /* Verb particles */
    { "dore",     "look!/behold"   }, { "ko",       "that"           },
    /* Interjections */
    { "muraho",   "hello"          }, { "murakoze", "thank you"      },
    { "mwiriwe",  "good evening"   }, { "mwaramutse","good morning"  },
    { "amakuru",  "how are you"    }, { "yee",      "[exclamation]"  },
    { "asyi",     "[exclamation]"  }, { "ahaa",     "[surprise]"     },
    { NULL, NULL }
};

const char *kin_word_gloss(const char *word, POS pos) {
    if (!word) return "";
    for (int i = 0; INV_GLOSS_TABLE[i].word; i++)
        if (strcmp(word, INV_GLOSS_TABLE[i].word) == 0)
            return INV_GLOSS_TABLE[i].gloss;
    (void)pos;
    return "";
}

/* ══════════════════════════════════════════════════════════════════════════
 * 5. GRAMMATICAL FEATURE GLOSSES
 * ══════════════════════════════════════════════════════════════════════════ */

/* SP class → pronoun gloss */
static const char *sp_class_gloss(int cls) {
    switch (cls) {
        case 1:  return "3SG.HUM";     /* umuntu (human singular)  */
        case 2:  return "3PL.HUM";     /* abantu (human plural)    */
        case 3:  return "3SG.CL3";     /* umuti (tree/thing sg)    */
        case 4:  return "3PL.CL4";     /* imiti (plural)           */
        case 5:  return "3SG.CL5";     /* ibuye (Nt.5 sg)          */
        case 6:  return "3PL.CL6";     /* amabuye (mass/plural)    */
        case 7:  return "3SG.CL7";     /* ikigo (Nt.7 sg)          */
        case 8:  return "3PL.CL8";     /* ibigo (Nt.8 pl)          */
        case 9:  return "3SG.CL9";     /* inka (animal/thing sg)   */
        case 10: return "3PL.CL10";    /* inka (pl agreement)      */
        case 11: return "3SG.CL11";    /* urugo (long/thin)        */
        case 12: return "3SG.DIM";     /* akana (diminutive sg)    */
        case 13: return "3PL.DIM";     /* utugabo (diminutive pl)  */
        case 14: return "3SG.ABSTR";   /* uburezi (abstract)       */
        case 15: return "3SG.INF";     /* ukugenda (infinitive)    */
        case 16: return "3SG.LOC";     /* ahantu (locative)        */
        default: return "PERS";
    }
}

/* SP surface form → personal pronoun gloss (1sg/2sg/1pl/2pl) */
static const char *sp_personal_gloss(const char *form) {
    if (!form) return NULL;
    if (strcmp(form, "nd") == 0 || strcmp(form, "n") == 0)  return "1SG";
    if (strcmp(form, "tu") == 0 || strcmp(form, "tw") == 0) return "1PL";
    if (strcmp(form, "du") == 0 || strcmp(form, "dw") == 0) return "1PL.COND";
    if (strcmp(form, "mu") == 0 || strcmp(form, "mw") == 0) return "2PL";
    if (strcmp(form, "nu") == 0)                             return "2SG.COND";
    if (strcmp(form, "ba") == 0)                             return "3PL.HUM";
    if (strcmp(form, "ha") == 0)                             return "3SG.LOC";
    return NULL;
}

/* TM → tense/mood gloss */
static const char *tm_gloss(VerbTense t) {
    switch (t) {
        case TENSE_PRESENT:           return "PRES";
        case TENSE_FUTURE:
        case TENSE_FUTURE_SUBJ:
        case TENSE_FUTURE_SUBJ_LOC:  return "FUT";
        case TENSE_NARRATIVE:
        case TENSE_NARRATIVE_SUBJ:   return "SEQ";
        case TENSE_OPTATIVE:         return "OPT";
        case TENSE_NEG_RELATIVE:     return "NEG";
        case TENSE_NEG_ANTERIOR:     return "NEG.PRES";
        case TENSE_NEG_DA_2SG:       return "NEG.2SG";
        default:                     return "\xe2\x88\x85"; /* UTF-8 ∅ */
    }
}

/* FV → aspect/mood gloss */
static const char *fv_gloss(VerbTense t) {
    switch (t) {
        case TENSE_PRESENT:
        case TENSE_PRESENT_NORA:
        case TENSE_FUTURE:
        case TENSE_NARRATIVE:
        case TENSE_OPTATIVE:
        case TENSE_CONDITIONAL:
        case TENSE_NEG_RELATIVE:
        case TENSE_NEG_ANTERIOR:     return "IND";
        case TENSE_PAST_PERF:
        case TENSE_PAST_PERF_LOC:   return "PRF";
        case TENSE_PAST_IMPF:        return "IMPF";
        case TENSE_SUBJUNCTIVE:
        case TENSE_SUBJUNCTIVE_LOC:
        case TENSE_FUTURE_SUBJ:
        case TENSE_FUTURE_SUBJ_LOC:
        case TENSE_NARRATIVE_SUBJ:   return "SUBJ";
        case TENSE_IMPERATIVE:       return "IMP";
        case TENSE_COPULA_PAST:
        case TENSE_COPULA_PRES:     return "COP";
        case TENSE_STATIVE_POSS:     return "STAT";
        default:                     return "IND";
    }
}

/* VerbExtension → derivational gloss */
static const char *ext_gloss(VerbExtension e) {
    switch (e) {
        case VEXT_PASSIVE:              return "PASS";
        case VEXT_CAUSATIVE:
        case VEXT_CAUSATIVE_Y:
        case VEXT_CAUSATIVE_IZ:         return "CAUS";
        case VEXT_CAUSATIVE_PASSIVE:    return "CAUS.PASS";
        case VEXT_APPLICATIVE:          return "APPL";
        case VEXT_RECIPROCAL:           return "RECP";
        case VEXT_REFLEXIVE:            return "REFL";
        case VEXT_STATIVE:              return "STAT";
        case VEXT_REVERSIVE:            return "REV";
        case VEXT_DOUBLE_APPLICATIVE:   return "APPL2";
        case VEXT_APPLIC_CAUSATIVE:     return "APPL.CAUS";
        case VEXT_APPLIC_PASSIVE:       return "APPL.PASS";
        case VEXT_CAUS_Y_PASSIVE:       return "CAUS.PASS";
        case VEXT_CAUS_NEUTER:          return "STAT.CAUS";
        default:                        return "";
    }
}

/* Noun class D morpheme gloss */
static const char *noun_d_gloss(int cls) {
    switch (cls) {
        case 1:  return "CL1.SG";
        case 2:  return "CL2.PL";
        case 3:  return "CL3.SG";
        case 4:  return "CL4.PL";
        case 5:  return "CL5.SG";
        case 6:  return "CL6.PL";
        case 7:  return "CL7.SG";
        case 8:  return "CL8.PL";
        case 9:  return "CL9.SG";
        case 10: return "CL10.PL";
        case 11: return "CL11.SG";
        case 12: return "CL12.DIM";
        case 13: return "CL13.DIM.PL";
        case 14: return "CL14.ABSTR";
        case 15: return "CL15.INF";
        case 16: return "CL16.LOC";
        default: return "D";
    }
}

/* Noun class RT morpheme gloss */
static const char *noun_rt_gloss(int cls) {
    if (cls < 1 || cls > 16) return "RT";
    /* Return "Nt.N" string — use a static array indexed by class */
    static const char *tbl[] = {
        "?", "Nt.1","Nt.2","Nt.3","Nt.4","Nt.5","Nt.6","Nt.7","Nt.8",
        "Nt.9","Nt.10","Nt.11","Nt.12","Nt.13","Nt.14","Nt.15","Nt.16"
    };
    return tbl[cls];
}

/* ══════════════════════════════════════════════════════════════════════════
 * 6. PUBLIC LOOKUP FUNCTIONS
 * ══════════════════════════════════════════════════════════════════════════ */

bool kin_get_verb_gloss(const char *stem, char *out, size_t outsize) {
    if (!stem || !out || outsize == 0) return false;
    out[0] = '\0';
    for (int i = 0; VERB_GLOSS_TABLE[i].stem; i++) {
        if (strcmp(stem, VERB_GLOSS_TABLE[i].stem) == 0) {
            strncpy(out, VERB_GLOSS_TABLE[i].gloss, outsize - 1);
            out[outsize - 1] = '\0';
            return true;
        }
    }
    return false;
}

static bool get_noun_gloss(const char *igicumbi, char *out, size_t outsize) {
    if (!igicumbi || !out || outsize == 0) return false;
    out[0] = '\0';
    for (int i = 0; NOUN_GLOSS_TABLE[i].igicumbi; i++) {
        if (strcmp(igicumbi, NOUN_GLOSS_TABLE[i].igicumbi) == 0) {
            strncpy(out, NOUN_GLOSS_TABLE[i].gloss, outsize - 1);
            out[outsize - 1] = '\0';
            return true;
        }
    }
    return false;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 7. kin_fill_morpheme_glosses()
 *
 * Post-process a single token's morpheme breakdown, filling english_gloss
 * for each morpheme slot.  Called after kin_morpheme_analyze().
 * ══════════════════════════════════════════════════════════════════════════ */

void kin_fill_morpheme_glosses(Token *tok) {
    MorphBreakdown *mb = &tok->morph;
    if (mb->n == 0) return;

    for (int i = 0; i < mb->n; i++) {
        KinMorpheme *m = &mb->m[i];
        char gloss[KIN_MORPH_GLOSS_LEN];
        gloss[0] = '\0';

        if (strcmp(m->label, "SP") == 0) {
            /* Try personal-pronoun forms first (nd, tu, mu …), then class */
            const char *pg = sp_personal_gloss(m->form);
            if (!pg) pg = sp_class_gloss(tok->noun_class);
            strncpy(gloss, pg, sizeof(gloss) - 1);
        }
        else if (strcmp(m->label, "TM") == 0) {
            const char *tg = tm_gloss(tok->verb_tense);
            strncpy(gloss, tg, sizeof(gloss) - 1);
        }
        else if (strcmp(m->label, "OM") == 0) {
            if (tok->obj_class > 0)
                snprintf(gloss, sizeof(gloss), "OM.CL%d", tok->obj_class);
            else
                strncpy(gloss, "OM", sizeof(gloss) - 1);
        }
        else if (strcmp(m->label, "root") == 0) {
            /* Primary form; fall back to surface if surface differs */
            if (!kin_get_verb_gloss(m->form, gloss, sizeof(gloss)))
                kin_get_verb_gloss(m->surface, gloss, sizeof(gloss));
        }
        else if (strcmp(m->label, "EXT") == 0) {
            const char *eg = ext_gloss(tok->verb_ext);
            strncpy(gloss, eg, sizeof(gloss) - 1);
        }
        else if (strcmp(m->label, "FV") == 0) {
            const char *fg = fv_gloss(tok->verb_tense);
            strncpy(gloss, fg, sizeof(gloss) - 1);
        }
        else if (strcmp(m->label, "PREF") == 0) {
            strncpy(gloss, "INF", sizeof(gloss) - 1);
        }
        else if (strcmp(m->label, "D") == 0) {
            strncpy(gloss, noun_d_gloss(tok->noun_class), sizeof(gloss) - 1);
        }
        else if (strcmp(m->label, "RT") == 0) {
            strncpy(gloss, noun_rt_gloss(tok->noun_class), sizeof(gloss) - 1);
        }
        else if (strcmp(m->label, "C") == 0) {
            if (tok->pos == POS_ADJECTIVE) {
                const char *ag = adj_stem_gloss(m->form);
                strncpy(gloss, ag, sizeof(gloss) - 1);
            } else {
                /* Noun/relative-noun: look up igicumbi */
                get_noun_gloss(m->form, gloss, sizeof(gloss));
            }
        }
        else if (strcmp(m->label, "RS") == 0) {
            if (tok->noun_class > 0)
                snprintf(gloss, sizeof(gloss), "AGR.CL%d", tok->noun_class);
            else
                strncpy(gloss, "AGR", sizeof(gloss) - 1);
        }

        gloss[sizeof(gloss) - 1] = '\0';
        /* copy at most GLOSS_LEN-1 bytes so null-terminator always fits */
        size_t glen = strlen(gloss);
        if (glen >= KIN_MORPH_GLOSS_LEN) glen = KIN_MORPH_GLOSS_LEN - 1;
        memcpy(m->english_gloss, gloss, glen);
        m->english_gloss[glen] = '\0';
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 * 8. ROUGH TRANSLATION BUILDER
 *
 * Assembles a rough English translation by chaining per-token contributions.
 * This is an approximation useful for quick reference — not a true MT output.
 *
 * Strategy (SVO, matching Kinyarwanda surface order which is also SVO):
 *   Noun       → stem gloss (or surface if unknown)
 *   Verb conj  → [root gloss] (tense label if non-present)
 *   Verb inf   → "to [root gloss]"
 *   Adjective  → stem gloss
 *   Conjunction→ English equivalent
 *   Adverb     → English equivalent or surface form
 *   Pronoun    → use surface (short enough to be self-explanatory)
 *   Punctuation→ same punctuation mark
 * ══════════════════════════════════════════════════════════════════════════ */

static void build_rough_translation(const SentenceAnalysis *sa,
                                    char *out, size_t outsize) {
    out[0] = '\0';
    size_t pos = 0;

    for (int i = 0; i < sa->token_count; i++) {
        const Token *t = &sa->tokens[i];
        char piece[128] = "";

        if (t->pos == POS_PUNCTUATION) {
            snprintf(piece, sizeof(piece), "%s", t->surface);
        }
        else if (t->pos == POS_NOUN || t->pos == POS_RELATIVE_NOUN) {
            /* Find C morpheme gloss */
            const char *cg = "";
            for (int m = 0; m < t->morph.n; m++) {
                if (strcmp(t->morph.m[m].label, "C") == 0 &&
                    t->morph.m[m].english_gloss[0]) {
                    cg = t->morph.m[m].english_gloss; break;
                }
            }
            snprintf(piece, sizeof(piece), "%s", cg[0] ? cg : t->surface);
        }
        else if (t->pos == POS_VERB_CONJ) {
            const char *rg = "";
            for (int m = 0; m < t->morph.n; m++) {
                if (strcmp(t->morph.m[m].label, "root") == 0 &&
                    t->morph.m[m].english_gloss[0]) {
                    rg = t->morph.m[m].english_gloss; break;
                }
            }
            /* Add tense marker for non-present tenses */
            const char *tense_str = "";
            switch (t->verb_tense) {
                case TENSE_PAST_PERF:
                case TENSE_PAST_PERF_LOC:  tense_str = "(past) ";    break;
                case TENSE_PAST_IMPF:       tense_str = "(was) ";     break;
                case TENSE_FUTURE:          tense_str = "(will) ";    break;
                case TENSE_SUBJUNCTIVE:     tense_str = "(should) ";  break;
                default:                    tense_str = "";            break;
            }
            if (rg[0])
                snprintf(piece, sizeof(piece), "%s%s", tense_str, rg);
            else
                snprintf(piece, sizeof(piece), "%s", t->surface);
        }
        else if (t->pos == POS_VERB_INF) {
            const char *rg = "";
            for (int m = 0; m < t->morph.n; m++) {
                if (strcmp(t->morph.m[m].label, "root") == 0 &&
                    t->morph.m[m].english_gloss[0]) {
                    rg = t->morph.m[m].english_gloss; break;
                }
            }
            if (rg[0])
                snprintf(piece, sizeof(piece), "to %s", rg);
            else
                snprintf(piece, sizeof(piece), "%s", t->surface);
        }
        else if (t->pos == POS_ADJECTIVE) {
            const char *cg = "";
            for (int m = 0; m < t->morph.n; m++) {
                if (strcmp(t->morph.m[m].label, "C") == 0 &&
                    t->morph.m[m].english_gloss[0]) {
                    cg = t->morph.m[m].english_gloss; break;
                }
            }
            snprintf(piece, sizeof(piece), "[%s]", cg[0] ? cg : t->stem);
        }
        else {
            /* Pronouns, prepositions, conjunctions, adverbs */
            const char *wg = kin_word_gloss(t->lower, t->pos);
            /* For conjunctions/prepositions, prefer gloss; for pronouns/adverbs
             * surface is often short and self-explanatory */
            if (wg[0])
                snprintf(piece, sizeof(piece), "%s", wg);
            else if (t->pos == POS_PRONOUN)
                snprintf(piece, sizeof(piece), "%s", t->surface);
            else
                snprintf(piece, sizeof(piece), "[%s]", t->surface);
        }

        /* Append piece to output with a space separator */
        size_t plen = strlen(piece);
        if (plen == 0) continue;
        bool is_punct = (t->pos == POS_PUNCTUATION);
        if (pos > 0 && !is_punct && out[pos-1] != ' ')
            out[pos++] = ' ';
        if (pos + plen + 1 >= outsize) break;
        memcpy(out + pos, piece, plen + 1);
        pos += plen;
    }
    out[outsize - 1] = '\0';
}

/* ══════════════════════════════════════════════════════════════════════════
 * 9. kin_print_interlinear()
 *
 * Displays a Leipzig-style interlinear gloss for the analysed sentence.
 * Format (per token):
 *
 *   [surface]    morph1 - morph2 - morph3
 *                gloss1 - gloss2 - gloss3
 *
 * Invariable words show a single word-level gloss.
 * A rough translation line follows the full token list.
 * ══════════════════════════════════════════════════════════════════════════ */

void kin_print_interlinear(const SentenceAnalysis *sa) {
    printf("\n");
    printf("  \xe2\x95\x90\xe2\x95\x90\xe2\x95\x90 Interlinear Gloss (Amategeko y'Igenamajwi) "
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    printf("  Morpheme chain and English gloss for each word / akaramejambo\n");
    printf("  Abbreviations: CL=class, Nt=inteko, SP=subject-prefix, TM=tense-marker\n");
    printf("                 OM=object-marker, EXT=extension, FV=final-vowel\n");
    printf("                 AGR=concordance, INF=infinitive, PASS/CAUS/APPL/RECP=voice\n");
    printf("  \xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    for (int i = 0; i < sa->token_count; i++) {
        const Token *t = &sa->tokens[i];

        if (t->pos == POS_PUNCTUATION) {
            printf("  %-18s [%s]\n", t->surface, t->surface);
            continue;
        }

        printf("  %-18s ", t->surface);

        if (t->morph.n > 0) {
            /* Morpheme surface chain */
            for (int m = 0; m < t->morph.n; m++) {
                if (m > 0) printf(" \xe2\x80\x93 "); /* – */
                const char *surf = t->morph.m[m].surface[0]
                                   ? t->morph.m[m].surface
                                   : t->morph.m[m].form;
                printf("%-8s", surf);
            }
            printf("\n");
            printf("  %-18s ", "");
            /* Gloss chain */
            for (int m = 0; m < t->morph.n; m++) {
                if (m > 0) printf(" \xe2\x80\x93 ");
                const char *gl = t->morph.m[m].english_gloss[0]
                                 ? t->morph.m[m].english_gloss
                                 : t->morph.m[m].label;
                printf("%-8s", gl);
            }
            printf("\n");
        } else {
            /* No morpheme breakdown: show word-level gloss or POS */
            const char *wg = kin_word_gloss(t->lower, t->pos);
            if (wg && wg[0])
                printf("%s\n", wg);
            else
                printf("(%s)\n", kin_pos_name(t->pos));
        }
        printf("\n");
    }

    /* Rough translation */
    char trans[1024];
    build_rough_translation(sa, trans, sizeof(trans));
    if (trans[0]) {
        printf("  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 Translation hint: %s\n", trans);
    }

    printf("  \xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n\n");
}
