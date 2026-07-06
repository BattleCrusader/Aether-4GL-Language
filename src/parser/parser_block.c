#include "aether/parser.h"
#include "aether/parser_internal.h"
#include "aether/str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Block parsing (indentation-based)
 * ================================================================ */

AstNode *parse_block(Parser *p) {
    AstNode *block = node_block(p->arena, NO_LOCATION);

    /* Consume statements until dedent or EOF */
    while (!parser_check(p, TOKEN_EOF)) {
        /* Skip newlines */
        while (parser_match(p, TOKEN_NEWLINE));

        /* Dedent or EOF ends the block */
        if (parser_check(p, TOKEN_RBRACE) ||
            parser_check(p, TOKEN_EOF)) {
            break;
        }

        AstNode *stmt = parse_statement(p);
        if (stmt) {
            node_list_append(&block->data.list, stmt);
        }
    }

    return block;
}

AstNode *parse_block_braced(Parser *p) {
    AstNode *block = node_block(p->arena, p->previous.loc);

    while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
        /* Skip newlines, semicolons, and indent/dedent tokens */
        while (parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_SEMICOLON) ||
               parser_match(p, TOKEN_NEWLINE) || parser_match(p, TOKEN_NEWLINE));

        if (parser_check(p, TOKEN_RBRACE)) break;

        AstNode *stmt = parse_statement(p);
        if (stmt) {
            node_list_append(&block->data.list, stmt);
        }
    }

    parser_expect(p, TOKEN_RBRACE, "block");
    return block;
}
