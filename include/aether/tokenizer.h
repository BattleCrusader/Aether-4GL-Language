#ifndef AETHER_TOKENIZER_H
#define AETHER_TOKENIZER_H

#include "defs.h"
#include "str.h"
#include "arena.h"
#include "vector.h"

/*
 * Token types for the Aether language.
 */

typedef enum {
    /* Special */
    TOKEN_EOF,
    TOKEN_ERROR,
    TOKEN_INVALID,

    /* Newlines (statement separators) */
    TOKEN_NEWLINE,

    /* Literals */
    TOKEN_IDENT,
    TOKEN_INT_LITERAL,
    TOKEN_FLOAT_LITERAL,
    TOKEN_STRING_LITERAL,
    TOKEN_CHAR_LITERAL,

    /* Keywords */
    TOKEN_KW_FUNC,      /* func */
    TOKEN_KW_LET,       /* let */
    TOKEN_KW_MUT,       /* mut */
    TOKEN_KW_IF,        /* if */
    TOKEN_KW_ELIF,      /* elif */
    TOKEN_KW_ELSE,      /* else */
    TOKEN_KW_WHILE,     /* while */
    TOKEN_KW_FOR,       /* for */
    TOKEN_KW_IN,        /* in */
    TOKEN_KW_RETURN,    /* return */
    TOKEN_KW_TRUE,      /* true */
    TOKEN_KW_FALSE,     /* false */
    TOKEN_KW_NONE,      /* none */
    TOKEN_KW_ASM,       /* asm */
    TOKEN_KW_BREAK,     /* break */
    TOKEN_KW_CONTINUE,  /* continue */
    TOKEN_KW_STRUCT,    /* struct */
    TOKEN_KW_ENUM,      /* enum */
    TOKEN_KW_CLASS,     /* class */
    TOKEN_KW_MATCH,     /* match */
    TOKEN_KW_CASE,      /* case */
    TOKEN_KW_TRY,       /* try */
    TOKEN_KW_THROW,     /* throw */
    TOKEN_KW_CATCH,     /* catch */
    TOKEN_KW_AND,       /* and */
    TOKEN_KW_OR,        /* or */
    TOKEN_KW_NOT,       /* not */
    TOKEN_KW_IMPORT,    /* import */
    TOKEN_KW_CONST,     /* const */
    TOKEN_KW_REF,       /* ref */
    TOKEN_KW_OWNED,     /* owned */
    TOKEN_KW_RC,        /* rc */
    TOKEN_KW_HEAP,      /* heap */
    TOKEN_KW_REGION,    /* region */
    TOKEN_KW_PRIVATE,   /* private */
    TOKEN_KW_PUB,       /* public */
    TOKEN_KW_INTERNAL,  /* internal */
    TOKEN_KW_STATIC,    /* static */
    TOKEN_KW_DEFER,     /* defer */
    TOKEN_KW_UNSAFE,    /* unsafe */
    TOKEN_KW_MODULE,    /* module */
    TOKEN_KW_SYS,       /* sys */
    TOKEN_KW_CONTRACT,  /* contract */
    TOKEN_KW_IF_LET,    /* if let (contextual) */
    TOKEN_KW_PRE,       /* pre */
    TOKEN_KW_POST,      /* post */
    TOKEN_KW_DROP,      /* drop */
    TOKEN_KW_INIT,      /* init */
    TOKEN_KW_SELF,      /* self */
    TOKEN_KW_TYPE,      /* type */
    TOKEN_KW_TRAIT,     /* trait */
    TOKEN_KW_IMPL,      /* impl */
    TOKEN_KW_POOL,      /* pool */
    TOKEN_KW_PROTOCOL,  /* protocol */
    TOKEN_KW_VIRTUAL,   /* virtual */
    TOKEN_KW_DYN,       /* dyn */
    TOKEN_KW_THROWS,    /* throws */
    TOKEN_KW_EXPORT,    /* export */
    TOKEN_KW_ENTRY,     /* entry */
    TOKEN_KW_LAYOUT,    /* layout */
    TOKEN_KW_RUN,       /* run (#run) */
    TOKEN_KW_PROP,      /* prop */
    TOKEN_KW_INLINE,    /* inline */
    TOKEN_KW_AT,        /* at */
    TOKEN_KW_FINALLY,   /* finally */
    TOKEN_KW_AS,        /* as */
    TOKEN_KW_SPAWN,     /* spawn */
    TOKEN_KW_YIELD,     /* yield */

    /* Operators */
    TOKEN_PLUS,         /* + */
    TOKEN_MINUS,        /* - */
    TOKEN_STAR,         /* * */
    TOKEN_SLASH,        /* / */
    TOKEN_PERCENT,      /* % */
    TOKEN_EQ,           /* = */
    TOKEN_EQ_EQ,        /* == */
    TOKEN_BANG,         /* ! */
    TOKEN_BANG_EQ,      /* != */
    TOKEN_LT,           /* < */
    TOKEN_GT,           /* > */
    TOKEN_LT_EQ,        /* <= */
    TOKEN_GT_EQ,        /* >= */
    TOKEN_AMPERSAND,    /* & */
    TOKEN_AND_AND,      /* && */
    TOKEN_PIPE,         /* | */
    TOKEN_PIPE_PIPE,    /* || */
    TOKEN_CARET,        /* ^ */
    TOKEN_TILDE,        /* ~ */
    TOKEN_LT_LT,        /* << */
    TOKEN_GT_GT,        /* >> */
    TOKEN_ARROW,        /* -> */
    TOKEN_DOT_DOT,      /* .. */
    TOKEN_DOT_DOT_EQ,   /* ..= */
    TOKEN_COLON,        /* : */
    TOKEN_COMMA,        /* , */
    TOKEN_DOT,          /* . */
    TOKEN_AT,           /* @ */
    TOKEN_HASH,         /* # (for #run only) */

    /* Brackets */
    TOKEN_LPAREN,       /* ( */
    TOKEN_RPAREN,       /* ) */
    TOKEN_LBRACKET,     /* [ */
    TOKEN_RBRACKET,     /* ] */
    TOKEN_LBRACE,       /* { */
    TOKEN_RBRACE,       /* } */

    /* Power operator */
    TOKEN_STAR_STAR,    /* ** */

    /* Assignment operators */
    TOKEN_PLUS_EQ,      /* += */
    TOKEN_PLUS_PLUS,    /* ++ */
    TOKEN_MINUS_EQ,     /* -= */
    TOKEN_MINUS_MINUS,  /* -- */
    TOKEN_STAR_EQ,      /* *= */
    TOKEN_SLASH_EQ,     /* /= */

    /* Lambda pipe (used inside |params|) */
    TOKEN_PIPE_LAMBDA,  /* | (as lambda delimiter) */

    /* Question mark */
    TOKEN_QUESTION,     /* ? */

    /* Backslash line continuation */
    TOKEN_BACKSLASH,    /* \ */
    
    /* Semicolon */
    TOKEN_SEMICOLON,    /* ; */

    /* Scope resolution */
    TOKEN_COLON_COLON,  /* :: */

    /* Unicode/custom operator (e.g. ⌛) */
    TOKEN_UNICODE_OP,
} TokenType;

/* Keyword lookup entry */
typedef struct {
    const char *word;
    TokenType type;
} KeywordEntry;

extern const KeywordEntry KEYWORDS[];
extern const int KEYWORD_COUNT;

/* Convert token type to human-readable string */
const char *token_type_name(TokenType type);

/*
 * Token structure
 */
typedef struct {
    TokenType type;
    Location loc;
    StringView text;       /* source text of the token */
    union {
        uint64_t int_value;
        double float_value;
    } val;
} Token;

/*
 * Tokenizer state
 */
typedef struct Tokenizer {
    const char *start;      /* beginning of current token */
    const char *pos;        /* current scan position */
    const char *end;        /* end of input */
    const char *filename;   /* source filename for error reporting */
    
    int line;               /* current line number (1-based) */
    int col;                /* current column (1-based) */
    
    /* Error tracking */
    bool had_error;
    char error_msg[256];
    
    Arena *arena;           /* arena for allocated strings */
} Tokenizer;

/* Create a tokenizer for the given source text */
Tokenizer *tokenizer_create(const char *source, size_t length, const char *filename);

/* Free tokenizer resources */
void tokenizer_destroy(Tokenizer *t);

/* Get the next token */
Token tokenizer_next(Tokenizer *t);

/* Peek at the next token without consuming it */
Token tokenizer_peek(Tokenizer *t);

/* Get all tokens from the tokenizer (returns Vector of Token) */
Vector *tokenizer_tokenize_all(Tokenizer *t);

/* Debug: print a token to stdout */
void token_print(const Token *t);

#endif /* AETHER_TOKENIZER_H */