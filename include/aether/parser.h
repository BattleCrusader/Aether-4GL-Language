#ifndef AETHER_PARSER_H
#define AETHER_PARSER_H

#include "defs.h"
#include "lexer.h"
#include "ast.h"
#include "arena.h"

/* Precedence levels (lower = tighter) */
typedef enum {
    PREC_MIN = 0,
    PREC_ASSIGNMENT,    /* = += -= *= /= */
    PREC_TERNARY,       /* ? : */
    PREC_LOGICAL_OR,    /* || */
    PREC_LOGICAL_AND,   /* && */
    PREC_COMPARISON,    /* == != < > <= >= */
    PREC_CAST,          /* as */
    PREC_BIT_OR,        /* | */
    PREC_BIT_XOR,       /* ^ */
    PREC_BIT_AND,       /* & */
    PREC_SHIFT,         /* << >> */
    PREC_RANGE,         /* .. ..= */
    PREC_TERM,          /* + - */
    PREC_FACTOR,        /* * / % */
    PREC_UNARY,         /* ! - ~ ref * & owned mut */
    PREC_CALL,          /* () [] . -> */
    PREC_PRIMARY,       /* literals, identifiers, parenthesized */
} Precedence;

/* Parser state */
typedef struct Parser {
    Lexer *lexer;
    Arena *arena;
    
    /* Current and look-ahead tokens */
    Token current;          /* current token */
    Token previous;         /* previously consumed token */
    bool has_current;       /* whether current is populated */
    
    /* Error tracking */
    int error_count;
    bool panic_mode;        /* true when recovering from error */
    char error_msg[256];
    
    /* Scope tracking */
    AstNodeList *current_scope; /* for collecting declarations */
} Parser;

/* Create a parser */
Parser *parser_create(const char *source, size_t length, const char *filename);
Parser *parser_create_with_arena(const char *source, size_t length, const char *filename, Arena *arena);

/* Destroy a parser */
void parser_destroy(Parser *p);

/* Parse the entire source file into a program node */
AstNode *parser_parse(Parser *p);

/* ================================================================
 * Internal parsing functions (exposed for testability)
 * ================================================================ */

/* Parse top-level declarations */
void parse_declaration(Parser *p, AstNodeList *decls);

/* Parse a function declaration: func name(params) rettype? block */
AstNode *parse_func_decl(Parser *p);

/* Parse a struct declaration */
AstNode *parse_struct_decl(Parser *p);

/* Parse an enum declaration */
AstNode *parse_enum_decl(Parser *p);

/* Parse a statement */
AstNode *parse_statement(Parser *p);

/* Parse an expression (with precedence climbing) */
AstNode *parse_expr(Parser *p);

/* Parse expression with minimum precedence */
AstNode *parse_expr_prec(Parser *p, Precedence min_prec);

/* Parse a block (indentation-sensitive) */
AstNode *parse_block(Parser *p);

/* Parse block with explicit braces */
AstNode *parse_block_braced(Parser *p);

/* Parse parameter list */
AstNodeList parse_params(Parser *p);

/* Parse optional type annotation: `: type` */
AstNode *parse_type_annotation(Parser *p);

/* Parse a type expression */
AstNode *parse_type(Parser *p);

/* Parse a pattern for match arms */
AstNode *parse_pattern(Parser *p);

/* Parse attribute/decorator like @export, @entry */
AstNode *parse_attribute(Parser *p);

/* Utility: consume current token, assert type */
void parser_advance(Parser *p);

/* Check current token type */
bool parser_check(Parser *p, TokenType type);

/* Check if current token is any of the given types */
bool parser_check_any(Parser *p, const TokenType *types, int count);

/* Match and consume if current token matches */
bool parser_match(Parser *p, TokenType type);

/* Expect a specific token type or report error */
void parser_expect(Parser *p, TokenType type, const char *context);

/* Report an error */
void parser_error(Parser *p, Token token, const char *message);

/* Check if at a statement-start token */
bool parser_at_stmt_start(Parser *p);

/* Synchronize to next statement boundary */
void parser_sync(Parser *p);

#endif /* AETHER_PARSER_H */