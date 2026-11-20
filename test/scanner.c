// Direct scanner tests: exercise token combinations independently of LR states.
#include <stdio.h>

#include "../src/scanner.c"

typedef struct {
    TSLexer lexer;
    const char *input;
    size_t position;
} TestLexer;

static void test_advance(TSLexer *lexer, bool skip) {
    (void)skip;
    TestLexer *test = (TestLexer *)lexer;
    if (test->input[test->position]) {
        test->position++;
    }
    lexer->lookahead = (unsigned char)test->input[test->position];
}

static void test_mark_end(TSLexer *lexer) { (void)lexer; }

static bool test_eof(const TSLexer *lexer) { return lexer->lookahead == 0; }

static unsigned failures;
static unsigned checks;

static void check_scan(const char *name, const char *input, const bool *symbols, bool expected_found,
                       TSSymbol expected_symbol) {
    TestLexer test = {
        .lexer = {
            .lookahead = (unsigned char)input[0],
            .advance = test_advance,
            .mark_end = test_mark_end,
            .eof = test_eof,
        },
        .input = input,
    };
    void *scanner = tree_sitter_bash_external_scanner_create();
    bool found = tree_sitter_bash_external_scanner_scan(scanner, &test.lexer, symbols);
    checks++;
    if (found != expected_found || (found && test.lexer.result_symbol != expected_symbol)) {
        fprintf(stderr, "FAIL: %s (found=%d, symbol=%u)\n", name, found, test.lexer.result_symbol);
        failures++;
    }
    tree_sitter_bash_external_scanner_destroy(scanner);
}

int main(void) {
    bool symbols[ERROR_RECOVERY + 1] = {[CONCAT] = true, [CLOSING_BRACE] = true};
    const char *whitespace[] = {" ", "\t", "\r", "\n"};
    const char *names[] = {"space", "tab", "carriage return", "newline"};

    for (size_t i = 0; i < sizeof(whitespace) / sizeof(whitespace[0]); i++) {
        char name[96];
        snprintf(name, sizeof(name), "%s can concatenate when NEWLINE is invalid", names[i]);
        symbols[NEWLINE] = false;
        check_scan(name, whitespace[i], symbols, true, CONCAT);

        // Defensive contract: whitespace must not manufacture a zero-width
        // CONCAT when the parser can accept a newline. Current generated LR
        // states do not request this exact combination of valid symbols.
        snprintf(name, sizeof(name), "%s cannot concatenate when NEWLINE is valid", names[i]);
        symbols[NEWLINE] = true;
        check_scan(name, whitespace[i], symbols, false, 0);
    }

    check_scan("adjacent non-whitespace still concatenates", "x", symbols, true, CONCAT);
    check_scan("EOF does not concatenate", "", symbols, false, 0);
    check_scan("closing brace does not concatenate", "}", symbols, false, 0);

    symbols[NEWLINE] = false;
    symbols[CLOSING_BRACE] = false;
    check_scan("whitespace without a closing brace does not concatenate", " ", symbols, false, 0);

    symbols[CLOSING_BRACE] = true;
    symbols[EXPANSION_WORD] = true;
    check_scan("expansion whitespace remains an expansion word", " }", symbols, true, EXPANSION_WORD);

    symbols[EXPANSION_WORD] = false;
    symbols[CONCAT] = false;
    check_scan("disabled CONCAT is not emitted", " ", symbols, false, 0);

    symbols[CONCAT] = true;
    symbols[ERROR_RECOVERY] = true;
    check_scan("error recovery does not concatenate whitespace", " ", symbols, false, 0);

    printf("Scanner checks: %u; failures: %u\n", checks, failures);
    return failures ? 1 : 0;
}
