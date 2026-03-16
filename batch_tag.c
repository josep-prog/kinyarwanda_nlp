/*
 * batch_tag.c - batch POS tagger for Bible word-list analysis
 * Usage: cat wordlist.txt | ./batch_tag
 * Output: word TAB POS TAB class TAB stem
 */
#include <stdio.h>
#include <string.h>
#include "include/kinyarwanda.h"

int main(void) {
    char word[KIN_MAX_WORD];
    while (fgets(word, sizeof(word), stdin)) {
        size_t len = strlen(word);
        while (len > 0 && (word[len-1] == '\n' || word[len-1] == '\r' || word[len-1] == ' '))
            word[--len] = '\0';
        if (len < 2) continue;

        Token t = {0};
        strncpy(t.surface, word, KIN_MAX_WORD - 1);
        kin_strlower(word, t.lower, KIN_MAX_WORD);   /* normalize diacritics */
        t.is_proper_noun = false;

        kin_tag_token(&t);

        const char *pos_name;
        switch (t.pos) {
            case POS_NOUN:          pos_name = "NOUN";      break;
            case POS_VERB_INF:      pos_name = "VERB_INF";  break;
            case POS_VERB_CONJ:     pos_name = "VERB_CONJ"; break;
            case POS_ADJECTIVE:     pos_name = "ADJ";       break;
            case POS_PRONOUN:       pos_name = "PRONOUN";   break;
            case POS_ADVERB:        pos_name = "ADVERB";    break;
            case POS_CONJUNCTION:   pos_name = "CONJ";      break;
            case POS_PREPOSITION:   pos_name = "PREP";      break;
            case POS_INTERJECTION:  pos_name = "INTERJ";    break;
            case POS_VERB_PARTICLE: pos_name = "PARTICLE";  break;
            case POS_LOCATIVE:      pos_name = "LOCATIVE";  break;
            case POS_FOREIGN:       pos_name = "FOREIGN";   break;
            default:                pos_name = "UNKNOWN";   break;
        }
        printf("%s\t%s\t%d\t%s\n", word, pos_name, t.noun_class, t.stem);
    }
    return 0;
}
