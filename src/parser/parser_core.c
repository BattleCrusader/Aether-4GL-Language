#include "aether/parser.h"
#include "aether/str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Parser creation/destruction
 * ================================================================ */

Parser *parser_create(const char *source, size_t length, const char *filename) {
    Parser *p = (Parser *)calloc(1, sizeof(Parser));
    if (!p) return NULL;

    p->lexer = lexer_create(source, length, filename);
    if (!p->lexer) { free(p); return NULL; }

    p->arena = arena_create();
    p->has_current = false;
    p->error_count = 0;
    p->panic_mode = false;
    p->current_scope = NULL;

    return p;
}

Parser *parser_create_with_arena(const char *source, size_t length, const char *filename, Arena *arena) {
    Parser *p = (Parser *)calloc(1, sizeof(Parser));
    if (!p) return NULL;

    p->lexer = lexer_create(source, length, filename);
    if (!p->lexer) { free(p); return NULL; }

    p->arena = arena;  /* Use external arena — caller must NOT call arena_destroy on cleanup */
    p->has_current = false;
    p->error_count = 0;
    p->panic_mode = false;
    p->current_scope = NULL;

    return p;
}

void parser_destroy(Parser *p) {
    if (p) {
        lexer_destroy(p->lexer);
        arena_destroy(p->arena);
        free(p);
    }
}

/* ================================================================
 * Token management
 * ================================================================ */

void parser_advance(Parser *p) {
    p->previous = p->current;
    lexer_advance(p->lexer);
    p->current = p->lexer->current;
    p->has_current = true;
}

bool parser_check(Parser *p, TokenType type) {
    if (!p->has_current) parser_advance(p);
    return p->current.type == type;
}

bool parser_check_any(Parser *p, const TokenType *types, int count) {
    if (!p->has_current) parser_advance(p);
    for (int i = 0; i < count; i++) {
        if (p->current.type == types[i]) return true;
    }
    return false;
}

bool parser_match(Parser *p, TokenType type) {
    if (parser_check(p, type)) {
        parser_advance(p);
        return true;
    }
    return false;
}

void parser_expect(Parser *p, TokenType type, const char *context) {
    if (parser_check(p, type)) {
        parser_advance(p);
    } else {
        parser_error(p, p->current, "expected token");
    }
}

void parser_error(Parser *p, Token token, const char *message) {
    fprintf(stderr, "Error at %s:%d:%d: %s (got '%s')\n",
            token.loc.file ? token.loc.file : "?",
            token.loc.line, token.loc.col,
            message, token_type_name(token.type));
    p->error_count++;
    p->panic_mode = true;
}

/* ================================================================
 * Synchronization
 * ================================================================ */

/* Tokens that start a statement */
static const TokenType STMT_START[] = {
    TOKEN_KW_LET, TOKEN_KW_IF, TOKEN_KW_WHILE, TOKEN_KW_FOR,
    TOKEN_KW_RETURN, TOKEN_KW_BREAK, TOKEN_KW_CONTINUE,
    TOKEN_KW_DEFER, TOKEN_KW_MATCH, TOKEN_KW_ASM,
    TOKEN_KW_FUNC, TOKEN_KW_STRUCT, TOKEN_KW_ENUM,
    TOKEN_KW_CONST, TOKEN_KW_IMPORT, TOKEN_KW_MODULE,
    TOKEN_KW_PUB, TOKEN_KW_STATIC,
    TOKEN_KW_UNSAFE, TOKEN_KW_TRY, TOKEN_KW_THROW,
    TOKEN_KW_TYPE,
    TOKEN_KW_SPAWN, TOKEN_KW_YIELD,
    TOKEN_AT,
    TOKEN_RBRACE, /* closing brace can follow statements */
};

bool parser_at_stmt_start(Parser *p) {
    if (!p->has_current) parser_advance(p);
    return parser_check_any(p, STMT_START, sizeof(STMT_START) / sizeof(STMT_START[0]));
}

void parser_sync(Parser *p) {
    p->panic_mode = false;
    while (!parser_check(p, TOKEN_EOF)) {
        if (parser_at_stmt_start(p)) return;
        parser_advance(p);
    }
}
