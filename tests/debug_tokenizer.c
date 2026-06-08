/* debug_tokenizer.c — dump token stream for analysis */

#include "aether/tokenizer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *source;
    const char *name = "<inline>";

    if (argc > 1) {
        /* Read file */
        FILE *f = fopen(argv[1], "rb");
        if (!f) { perror("fopen"); return 1; }
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *buf = (char *)malloc((size_t)len + 1);
        fread(buf, 1, (size_t)len, f);
        buf[len] = '\0';
        fclose(f);
        source = buf;
        name = argv[1];
    } else {
        source = "func main() {\n    return 42\n}\n";
    }

    Tokenizer *t = tokenizer_create(source, strlen(source), name);
    printf("=== Tokens for: %s ===\n", name);
    printf("Source: <<<\n%s>>>\n\n", source);
    
    int i = 0;
    while (1) {
        Token tok = tokenizer_next(t);
        printf("%3d: [%-12s] '%.*s'", i, token_type_name(tok.type), 
               (int)tok.text.len, tok.text.data);
        if (tok.type == TOKEN_INT_LITERAL) 
            printf(" val=%llu", (unsigned long long)tok.val.int_value);
        if (tok.type == TOKEN_FLOAT_LITERAL)
            printf(" val=%f", tok.val.float_value);
        if (tok.type == TOKEN_CHAR_LITERAL)
            printf(" val=%c(%02x)", (unsigned char)tok.val.int_value, 
                   (unsigned)tok.val.int_value);
        printf("\n");
        i++;
        if (tok.type == TOKEN_EOF || tok.type == TOKEN_ERROR) break;
        if (i > 100) { printf("... (limit)\n"); break; }
    }
    
    printf("\nTotal: %d tokens\n", i);
    tokenizer_destroy(t);
    return 0;
}