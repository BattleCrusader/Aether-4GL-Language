#include "aether/lexer.h"
#include <stdio.h>
#include <stdlib.h>

Lexer *lexer_create(const char *source, size_t length, const char *filename) {
    Lexer *l = (Lexer *)calloc(1, sizeof(Lexer));
    if (!l) return NULL;
    
    l->tok = tokenizer_create(source, length, filename);
    if (!l->tok) { free(l); return NULL; }
    
    l->arena = arena_create();
    l->has_current = false;
    l->has_next = false;
    l->error_count = 0;
    
    return l;
}

void lexer_destroy(Lexer *l) {
    if (l) {
        tokenizer_destroy(l->tok);
        arena_destroy(l->arena);
        free(l);
    }
}

Token lexer_advance(Lexer *l) {
    Token prev = l->current;
    
    if (l->has_next) {
        l->current = l->next;
        l->has_next = false;
    } else {
        l->current = tokenizer_next(l->tok);
    }
    
    l->has_current = true;
    return prev;
}

Token lexer_peek(Lexer *l) {
    if (!l->has_current) {
        l->current = tokenizer_next(l->tok);
        l->has_current = true;
    }
    return l->current;
}

Token lexer_peek_next(Lexer *l) {
    /* Ensure current is populated */
    lexer_peek(l);
    
    if (!l->has_next) {
        l->next = tokenizer_next(l->tok);
        l->has_next = true;
    }
    return l->next;
}

bool lexer_expect(Lexer *l, TokenType type) {
    Token tok = lexer_peek(l);
    if (tok.type == type) {
        lexer_advance(l);
        return true;
    }
    return false;
}

Token lexer_expect_or_error(Lexer *l, TokenType type, const char *context) {
    Token tok = lexer_peek(l);
    if (tok.type == type) {
        lexer_advance(l);
        return tok;
    }
    
    fprintf(stderr, "Error: expected %s but got %s ('%.*s') at %s:%d:%d [%s]\n",
            token_type_name(type),
            token_type_name(tok.type),
            (int)tok.text.len, tok.text.data,
            tok.loc.file, tok.loc.line, tok.loc.col,
            context ? context : "");
    
    l->error_count++;
    return tok;
}

void lexer_sync_to(Lexer *l, TokenType *targets, int target_count) {
    while (true) {
        Token tok = lexer_peek(l);
        if (tok.type == TOKEN_EOF) return;
        
        for (int i = 0; i < target_count; i++) {
            if (tok.type == targets[i]) return;
        }
        
        lexer_advance(l);
    }
}

bool lexer_check(Lexer *l, TokenType type) {
    return lexer_peek(l).type == type;
}

bool lexer_check_any(Lexer *l, const TokenType *types, int count) {
    TokenType cur = lexer_peek(l).type;
    for (int i = 0; i < count; i++) {
        if (cur == types[i]) return true;
    }
    return false;
}

int lexer_line(Lexer *l) { return l->current.loc.line; }
int lexer_col(Lexer *l) { return l->current.loc.col; }

void lexer_dump(Lexer *l) {
    Token tok = lexer_peek(l);
    printf("Lexer state: current token = %s ('%.*s') at %s:%d:%d, errors=%d\n",
           token_type_name(tok.type), (int)tok.text.len, tok.text.data,
           tok.loc.file, tok.loc.line, tok.loc.col,
           l->error_count);
}