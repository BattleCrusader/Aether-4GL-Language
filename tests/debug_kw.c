#include "aether/tokenizer.h"
#include <stdio.h>
#include <string.h>

int main() {
    const char *src =
        "func let mut if elif else while for in "
        "return true false none asm break continue "
        "struct enum class match case try throw catch "
        "and or not import const ref owned rc heap "
        "region pub static defer unsafe module sys "
        "pre post drop init self type trait impl "
        "pool protocol virtual dyn throws export entry "
        "layout test run\n";

    printf("Source length: %zu\n", strlen(src));
    
    Tokenizer *t = tokenizer_create(src, strlen(src), "test");
    int kw = 0;
    char unexpected[128];
    
    while (1) {
        Token tok = tokenizer_next(t);
        if (tok.type == TOKEN_EOF) break;
        if (tok.type == TOKEN_NEWLINE) continue;
        if (tok.type == TOKEN_IDENT) {
            snprintf(unexpected, sizeof(unexpected), 
                     "IDENT for '%.*s' at L%dC%d", 
                     (int)tok.text.len, tok.text.data, tok.loc.line, tok.loc.col);
            printf("FAIL: %s\n", unexpected);
            tokenizer_destroy(t);
            return 1;
        }
        if (!(tok.type >= TOKEN_KW_FUNC && tok.type <= TOKEN_KW_RUN)) {
            printf("FAIL: unexpected type %s for '%.*s' at L%dC%d\n", 
                   token_type_name(tok.type), (int)tok.text.len, tok.text.data, 
                   tok.loc.line, tok.loc.col);
            tokenizer_destroy(t);
            return 1;
        }
        kw++;
    }
    tokenizer_destroy(t);
    printf("PASS: %d keywords recognized\n", kw);
    return 0;
}