CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -Wpedantic -Iinclude -O2 -MMD -MP
-include $(SRCS:.c=.d)
TARGET   = kinyarwanda_nlp
MANPAGE  = man/kinyarwanda_nlp.1
SRCS     = src/main.c \
           src/tokenizer.c \
           src/morphology.c \
           src/lexicon.c \
           src/pos_tagger.c \
           src/syntax.c \
           src/corrector.c \
           src/analysis.c
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
	./$(TARGET) -s "Umuntu munini aragenda"
	@echo ""
	@echo "=== Test 2: adjective agreement ERROR ==="
	./$(TARGET) -s "Umuntu binini aragenda"
	@echo ""
	@echo "=== Test 3: possessive agreement (correct) ==="
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

clean:
	rm -f $(OBJS) $(SRCS:.c=.d) $(TARGET)
