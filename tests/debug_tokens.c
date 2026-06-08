#include "aether/tokenizer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void dump_tokens(const char *source, const char *name) {
    Tokenizer *t = tokenizer_create(source, strlen(source), name);
    printf("=== %s: <<<%s>>> ===\n", name, source);
    int i = 0;
    while (1) {
        Token tok = tokenizer_next(t);
        printf("%3d: [%-12s] '%.*s' L%dC%d", i, token_type_name(tok.type), 
               (int)tok.text.len, tok.text.data, tok.loc.line, tok.loc.col);
        if (tok.type == TOKEN_INT_LITERAL) printf(" val=%llu", (unsigned long long)tok.val.int_value);
        if (tok.type == TOKEN_FLOAT_LITERAL) printf(" val=%f", tok.val.float_value);
        if (tok.type == TOKEN_CHAR_LITERAL) printf(" val=%u(%c)", (unsigned)tok.val.int_value, (unsigned char)tok.val.int_value);
        printf("\n");
        i++;
        if (tok.type == TOKEN_EOF || tok.type == TOKEN_ERROR) break;
        if (i > 50) break;
    }
    printf("Total: %d\n\n", i);
    tokenizer_destroy(t);
}

int main() {
    dump_tokens("+ - * / % = == != < > <= >= && || -> .. ..= += -= << >> ?", "operators");
    dump_tokens("func let mut if elif else while for return true false none asm break continue struct enum class match case try throw catch and or not import const ref owned rc heap region pub static defer unsafe module sys pre post drop init self type trait impl pool protocol virtual dyn throws export entry layout test run", "keywords");
    dump_tokens("1_000_000\n0xFFFF_FFFF\n0b1010_0101\n", "underscores");
    dump_tokens("'a'\n'\\n'\n'\\x41'\n", "chars");
    dump_tokens("# comment\nfunc main() {\n    return 42\n}\n", "line comment");
    dump_tokens("#{ block #{}# }#\nfunc main() {\n    return 42\n}\n", "block comment");
    return 0;
}