#ifndef AETHER_LEXER_H
#define AETHER_LEXER_H

#include "defs.h"
#include "tokenizer.h"
#include "arena.h"

/*
 * Lexer — a higher-level interface over the tokenizer.
 * Provides look-ahead (peek), synchronization, and error recovery.
 */

typedef struct {
    Tokenizer *tok;         /* underlying tokenizer */
    Token current;          /* current token (what peek returns) */
    Token next;             /* next token */
    bool has_current;       /* whether current is populated */
    bool has_next;          /* whether next is populated */
    Arena *arena;           /* arena for lexer state */
    int error_count;        /* count of errors encountered */
} Lexer;

/* Create a lexer from source text */
Lexer *lexer_create(const char *source, size_t length, const char *filename);

/* Destroy a lexer */
void lexer_destroy(Lexer *l);

/* Advance to next token and return the previous one */
Token lexer_advance(Lexer *l);

/* Peek at current token (does not advance) */
Token lexer_peek(Lexer *l);

/* Peek one past current (one-token look-ahead) */
Token lexer_peek_next(Lexer *l);

/* Consume current token if it matches expected type.
   Returns true if matched, false otherwise. */
bool lexer_expect(Lexer *l, TokenType type);

/* Consume current token, asserting it matches expected type.
   Reports error and advances if not matched. */
Token lexer_expect_or_error(Lexer *l, TokenType type, const char *context);

/* Skip tokens until we find one of the given set (synchronization) */
void lexer_sync_to(Lexer *l, TokenType *targets, int target_count);

/* Check if current token is of given type */
bool lexer_check(Lexer *l, TokenType type);

/* Check if current token is any of the given types */
bool lexer_check_any(Lexer *l, const TokenType *types, int count);

/* Get current line number */
int lexer_line(Lexer *l);

/* Get current column */
int lexer_col(Lexer *l);

/* Debug: dump lexer state */
void lexer_dump(Lexer *l);

#endif /* AETHER_LEXER_H */