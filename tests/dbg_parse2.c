#include "aether/parser.h"
#include "aether/ast.h"
#include <stdio.h>
#include <string.h>

static int count_decls(AstNode *prog) {
    if (!prog || prog->type != NODE_PROGRAM) return -1;
    return prog->data.list.count;
}

int main() {
    const char *tests[] = {
        "func main() { return 42 }",
        "func add(a: int, b: int): int { return a + b }",
        "func main() {\n    let x = 42\n}",
        "enum Color { Red; Green }",
        "struct Point { x: int; y: int }",
    };
    const char *names[] = {
        "func_no_params",
        "func_with_params",
        "let_decl",
        "enum",
        "struct",
    };
    
    for (int i = 0; i < 5; i++) {
        printf("\n=== %s ===\n", names[i]);
        printf("Source: %s\n", tests[i]);
        
        Parser *p = parser_create(tests[i], strlen(tests[i]), names[i]);
        if (!p) { printf("  parser_create failed\n"); continue; }
        
        AstNode *prog = parser_parse(p);
        
        printf("  errors: %d\n", p->error_count);
        if (prog) {
            printf("  decls: %d\n", count_decls(prog));
            ast_dump(prog, 1);
        } else {
            printf("  program is NULL\n");
        }
        
        parser_destroy(p);
    }
    return 0;
}