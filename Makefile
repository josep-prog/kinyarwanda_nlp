CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -Wpedantic -Iinclude -O2 -fPIC -MMD -MP
TARGET   = kinyarwanda_nlp
MANPAGE  = man/kinyarwanda_nlp.1
.DEFAULT_GOAL := all

# Library sources (everything except the CLI entry point)
LIB_SRCS = src/tokenizer.c \
           src/morphology.c \
           src/ortho.c \
           src/lexicon.c \
           src/pos_tagger.c \
           src/morph_dispatch.c \
           src/syntax.c \
           src/corrector.c \
           src/analysis.c \
           src/g2p.c \
           src/gloss.c \
           src/api.c \
           src/punctuation.c \
           src/validator.c

# All sources (library + CLI)
SRCS     = src/main.c $(LIB_SRCS)
OBJS     = $(SRCS:.c=.o)
LIB_OBJS = $(LIB_SRCS:.c=.o)

-include $(SRCS:.c=.d)

STATIC_LIB = libkinyarwanda.a
SHARED_LIB = libkinyarwanda.so

# ── Test suite ────────────────────────────────────────────────────────────────
TEST_SRCS = tests/run_tests.c \
            tests/test_morphology.c \
            tests/test_lexicon.c \
            tests/test_ortho.c \
            tests/test_pipeline.c

TEST_BIN  = tests/run_tests
-include $(TEST_SRCS:.c=.d)

PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin
LIBDIR   = $(PREFIX)/lib
INCDIR   = $(PREFIX)/include/kinyarwanda
MANDIR   = $(PREFIX)/share/man/man1

.PHONY: all clean test install install-lib uninstall help

all: $(TARGET) $(STATIC_LIB) $(SHARED_LIB)

# CLI binary
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Static library
$(STATIC_LIB): $(LIB_OBJS)
	ar rcs $@ $^

# Shared library
$(SHARED_LIB): $(LIB_OBJS)
	$(CC) -shared -o $@ $^

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

# Install library + public headers (requires sudo)
install-lib: $(STATIC_LIB) $(SHARED_LIB)
	install -d $(LIBDIR)
	install -m 644 $(STATIC_LIB) $(LIBDIR)/$(STATIC_LIB)
	install -m 755 $(SHARED_LIB) $(LIBDIR)/$(SHARED_LIB)
	ldconfig $(LIBDIR)
	install -d $(INCDIR)
	install -m 644 include/kinyarwanda_api.h $(INCDIR)/kinyarwanda_api.h
	install -m 644 include/kinyarwanda.h     $(INCDIR)/kinyarwanda.h
	install -m 644 include/g2p.h             $(INCDIR)/g2p.h
	@echo "Library installed to $(LIBDIR)"
	@echo "Headers installed to $(INCDIR)"
	@echo "Link with: -I$(INCDIR) -L$(LIBDIR) -lkinyarwanda"

# Uninstall
uninstall:
	rm -f $(BINDIR)/$(TARGET)
	rm -f $(MANDIR)/$(notdir $(MANPAGE))
	rm -f $(LIBDIR)/$(STATIC_LIB) $(LIBDIR)/$(SHARED_LIB)
	rm -rf $(INCDIR)

# View man page without installing
help:
	man ./$(MANPAGE)

# Automated test suite
test: $(TEST_BIN)
	@./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRCS) $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRCS) $(STATIC_LIB)

# Manual demo — runs binary against sample sentences and prints output
demo: $(TARGET)
	@echo "=== Demo 1: noun+adjective agreement (correct) ==="
	./$(TARGET) -s "Umuntu munini aragenda buhoro"
	@echo ""
	@echo "=== Demo 2: adjective agreement ERROR ==="
	./$(TARGET) -s "Umuntu binini aragenda"
	@echo ""
	@echo "=== Demo 3: possessive agreement (correct) ==="
	./$(TARGET) -s "Urugo rwacu rugenda"
	@echo ""
	@echo "=== Demo 4: possessive agreement ERROR ==="
	./$(TARGET) -s "Urugo wacu rugenda"
	@echo ""
	@echo "=== Demo 5: TENSE – past perfect ==="
	./$(TARGET) -s "Imana yaremye ijuru n'isi"
	@echo ""
	@echo "=== Demo 6: TENSE – future ==="
	./$(TARGET) -s "Azagenda vuba cyane"
	@echo ""
	@echo "=== Demo 7: G2P ==="
	./$(TARGET) --g2p -s "Imana yaremye ijuru"

clean:
	rm -f $(OBJS) $(SRCS:.c=.d) $(TARGET) $(TEST_BIN) $(TEST_SRCS:.c=.o) $(TEST_SRCS:.c=.d)
