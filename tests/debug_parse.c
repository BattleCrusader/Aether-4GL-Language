#include "aether/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dump_tokens(const char *source) {
    Tokenizer *t = tokenizer_create(source, strlen(source), "dbg");
    int i = 0;
    while (1) {
        Token tok = tokenizer_next(t);
        printf("  %3d: [%-12s] '%.*s'\n", i, token_type_name(tok.type),
               (int)tok.text.len, tok.text.data);
        i++; if (tok.type == TOKEN_EOF || tok.type == TOKEN_ERROR) break;
        if (i > 40) break;
    }
    tokenizer_destroy(t);
}

int main() {
    printf("=== func with params ===\n");
    dump_tokens("func add(a int, b int) int { return a + b }");
    printf("\n=== enum ===\n");
    dump_tokens("enum Color { Red Green Blue Rgb(u8, u8, u8) }");
    printf("\n=== complex ===\n");
    dump_tokens("struct Point { x int; y int }\n"
        "enum Shape { Circle(Point, int); Rect(Point, Point) }\n"
        "func area(s Shape) int { match s { case Circle(_, r) -> 3 * r * r case _ -> 0 } }\n"
    );
    return 0;
}