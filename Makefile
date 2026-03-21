CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -Wpedantic -Iinclude -O2 -MMD -MP
-include $(SRCS:.c=.d)
TARGET   = kinyarwanda_nlp
MANPAGE  = man/kinyarwanda_nlp.1
SRCS     = src/main.c \
           src/tokenizer.c \
           src/morphology.c \
           src/ortho.c \
           src/lexicon.c \
           src/pos_tagger.c \
           src/syntax.c \
           src/corrector.c \
           src/analysis.c \
           src/g2p.c
OBJS     = $(SRCS:.c=.o)

PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin
MANDIR   = $(PREFIX)/share/man/man1

.PHONY: all clean test install uninstall help

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Install binary + man page (requires sudo)
install: $(TARGET)
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	install -d $(MANDIR)
	install -m 644 $(MANPAGE) $(MANDIR)/$(notdir $(MANPAGE))
	@echo "Installed to $(BINDIR)/$(TARGET)"
	@echo "Man page at $(MANDIR)/$(notdir $(MANPAGE))"
	@echo "Run: man kinyarwanda_nlp"

# Uninstall
uninstall:
	rm -f $(BINDIR)/$(TARGET)
	rm -f $(MANDIR)/$(notdir $(MANPAGE))

# View man page without installing
help:
	man ./$(MANPAGE)

# Functional tests
test: $(TARGET)
	@echo "=== Test 1: noun+adjective agreement (correct) ==="
	./$(TARGET) -s "Umuntu munini aragenda buhoro"
	@echo ""
	@echo "=== Test 2: adjective agreement ERROR ==="
	./$(TARGET) -s "Umuntu binini aragenda"
	@echo ""
	@echo "=== Test 3: possessive agreement (correct – rurabaho now verb) ==="
	./$(TARGET) -s "Urugo rwacu rurabaho"
	@echo ""
	@echo "=== Test 4: possessive agreement ERROR ==="
	./$(TARGET) -s "Urugo wacu rurabaho"
	@echo ""
	@echo "=== Test 5: sentence without verb ==="
	./$(TARGET) -s "Umuntu munini"
	@echo ""
	@echo "=== Test 6: verb infinitive ==="
	./$(TARGET) -s "Gusoma igitabo ni byiza"
	@echo ""
	@echo "=== Test 7: pronouns (verbose) ==="
	./$(TARGET) -v -s "Uyu mwana arakunda wabo"
	@echo ""
	@echo "=== Test 8: foreign word ==="
	./$(TARGET) -s "Umwana computer aragenda"
	@echo ""
	@echo "=== Test 9: TENSE – present (ara+stem+a) ==="
	./$(TARGET) -s "Imana iravuga iti habeho umucyo"
	@echo ""
	@echo "=== Test 10: TENSE – past perfect (stem+ye) ==="
	./$(TARGET) -s "Imana yaremye ijuru n'isi"
	@echo ""
	@echo "=== Test 11: TENSE – future (za+stem+a) ==="
	./$(TARGET) -s "Azagenda vuba cyane"
	@echo ""
	@echo "=== Test 12: TENSE – past imperfect (stem+aga) ==="
	./$(TARGET) -s "Yagendaga buri munsi"
	@echo ""
	@echo "=== Test 13: version ==="
	./$(TARGET) --version
	@echo ""
	@echo "=== Test 14: kugenda/kujya RULE – ERROR (gend + dest. noun) ==="
	./$(TARGET) -s "Umwana aragenda ishuri buri munsi"
	@echo ""
	@echo "=== Test 15: kugenda/kujya RULE – correct (kujya + dest. noun) ==="
	./$(TARGET) -s "Umwana ajya ishuri buri munsi"
	@echo ""
	@echo "=== Test 16: nuko + ahubwo correctly tagged ==="
	./$(TARGET) -s "Nuko Imana ibona ko byari byiza ahubwo ibihimba"
	@echo ""
	@echo "=== Test 17: Uwiteka now recognized as noun (not verb) ==="
	./$(TARGET) -s "Uwiteka ni Imana"
	@echo ""
	@echo "=== Test 18: reduplicated adjectives (barebare, muremure) ==="
	./$(TARGET) -s "Abantu barebare barakora kandi umuntu muremure aragenda"
	@echo ""
	@echo "=== Test 19: CONDITIONAL tense – niba + intervening noun ==="
	./$(TARGET) -v -s "Niba umwana aragenda azagaruka vuba"
	@echo ""
	@echo "=== Test 20: CONDITIONAL tense – nibyo + direct verb ==="
	./$(TARGET) -v -s "Nibyo azagenda azabona byiza"
	@echo ""
	@echo "=== Test 21: G2P – simple word (genda) ==="
	./$(TARGET) --g2p -s "genda"
	@echo ""
	@echo "=== Test 22: G2P – digraphs (ishuri, nyuma, shyiga) ==="
	./$(TARGET) --g2p -s "ishuri nyuma shyiga"
	@echo ""
	@echo "=== Test 23: G2P – nasal clusters (mbere, ntabwo, nshuti) ==="
	./$(TARGET) --g2p -s "mbere ntabwo nshuti"
	@echo ""
	@echo "=== Test 24: G2P – labialized (bwana, rwacu, kwitonda) ==="
	./$(TARGET) --g2p -s "bwana rwacu kwitonda"
	@echo ""
	@echo "=== Test 25: G2P – number normalization (3 km) ==="
	./$(TARGET) --g2p -s "Azagenda 3 km"
	@echo ""
	@echo "=== Test 26: G2P – full sentence (Imana yaremye ijuru) ==="
	./$(TARGET) --g2p -s "Imana yaremye ijuru"
	@echo ""
	@echo "=== Test 27: greeting – Murakoze (PAST_PERF, no ERR_NO_VERB) ==="
	./$(TARGET) -s "Murakoze cyane"
	@echo ""
	@echo "=== Test 28: greeting – Muraho (interjection, no ERR_NO_VERB) ==="
	./$(TARGET) -s "Muraho"
	@echo ""
	@echo "=== Test 29: greeting – Mwaramutse (PAST_PERF detected) ==="
	./$(TARGET) -s "Mwaramutse"
	@echo ""
	@echo "=== Test 30: greeting – Mwiriwe (interjection, no ERR_NO_VERB) ==="
	./$(TARGET) -s "Mwiriwe"
	@echo ""
	@echo "=== Test 31: stative – Afite inzu (has_verb via INVARIABLES) ==="
	./$(TARGET) -s "Afite inzu nziza"
	@echo ""
	@echo "=== Test 32: 1sg knowledge – Ndabizi neza ==="
	./$(TARGET) -s "Ndabizi neza"
	@echo ""
	@echo "=== Test 33: PAST_PERF ra+stem+e – Arakoze ==="
	./$(TARGET) -s "Arakoze"
	@echo ""
	@echo "=== Test 34: personal pronoun – We uragenda hehe ==="
	./$(TARGET) -v -s "We uragenda hehe"
	@echo ""
	@echo "=== Test 35: possessive label – wanjye (should show Possessive) ==="
	./$(TARGET) -v -s "Inzu yanjye ni nziza"
	@echo ""
	@echo "=== Test 36: adverb – nabi tagwa neza ==="
	./$(TARGET) -s "Ibikorwa bye byagaragaye nabi"

clean:
	rm -f $(OBJS) $(SRCS:.c=.d) $(TARGET)
