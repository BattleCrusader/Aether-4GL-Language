#include "aether/tokenizer.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

/* ================================================================
 * Keyword table
 * ================================================================ */

const KeywordEntry KEYWORDS[] = {
    {"func",     TOKEN_KW_FUNC},
    {"let",      TOKEN_KW_LET},
    {"mut",      TOKEN_KW_MUT},
    {"if",       TOKEN_KW_IF},
    {"elif",     TOKEN_KW_ELIF},
    {"else",     TOKEN_KW_ELSE},
    {"while",    TOKEN_KW_WHILE},
    {"for",      TOKEN_KW_FOR},
    {"in",       TOKEN_KW_IN},
    {"return",   TOKEN_KW_RETURN},
    {"true",     TOKEN_KW_TRUE},
    {"false",    TOKEN_KW_FALSE},
    {"none",     TOKEN_KW_NONE},
    {"asm",      TOKEN_KW_ASM},
    {"break",    TOKEN_KW_BREAK},
    {"continue", TOKEN_KW_CONTINUE},
    {"struct",   TOKEN_KW_STRUCT},
    {"enum",     TOKEN_KW_ENUM},
    {"class",    TOKEN_KW_CLASS},
    {"match",    TOKEN_KW_MATCH},
    {"case",     TOKEN_KW_CASE},
    {"try",      TOKEN_KW_TRY},
    {"throw",    TOKEN_KW_THROW},
    {"catch",    TOKEN_KW_CATCH},
    {"and",      TOKEN_KW_AND},
    {"or",       TOKEN_KW_OR},
    {"not",      TOKEN_KW_NOT},
    {"import",   TOKEN_KW_IMPORT},
    {"const",    TOKEN_KW_CONST},
    {"ref",      TOKEN_KW_REF},
    {"owned",    TOKEN_KW_OWNED},
    {"rc",       TOKEN_KW_RC},
    {"heap",     TOKEN_KW_HEAP},
    {"region",   TOKEN_KW_REGION},
    {"private",  TOKEN_KW_PRIVATE},
    {"pub",      TOKEN_KW_PUB},
    {"internal", TOKEN_KW_INTERNAL},
    {"static",   TOKEN_KW_STATIC},
    {"defer",    TOKEN_KW_DEFER},
    {"unsafe",   TOKEN_KW_UNSAFE},
    {"module",   TOKEN_KW_MODULE},
    {"sys",      TOKEN_KW_SYS},
    {"pre",      TOKEN_KW_PRE},
    {"post",     TOKEN_KW_POST},
    {"drop",     TOKEN_KW_DROP},
    {"init",     TOKEN_KW_INIT},
    {"self",     TOKEN_KW_SELF},
    {"type",     TOKEN_KW_TYPE},
    {"trait",    TOKEN_KW_TRAIT},
    {"impl",     TOKEN_KW_IMPL},
    {"pool",     TOKEN_KW_POOL},
    {"protocol", TOKEN_KW_PROTOCOL},
    {"virtual",  TOKEN_KW_VIRTUAL},
    {"dyn",      TOKEN_KW_DYN},
    {"throws",   TOKEN_KW_THROWS},
    {"export",   TOKEN_KW_EXPORT},
    {"entry",    TOKEN_KW_ENTRY},
    {"layout",   TOKEN_KW_LAYOUT},
    {"test",     TOKEN_KW_TEST},
    {"run",      TOKEN_KW_RUN},
    {"iflet",    TOKEN_KW_IF_LET},
};

const int KEYWORD_COUNT = sizeof(KEYWORDS) / sizeof(KEYWORDS[0]);

/* ================================================================
 * Token type name table
 * ================================================================ */

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        case TOKEN_INVALID: return "INVALID";
        case TOKEN_INDENT: return "INDENT";
        case TOKEN_DEDENT: return "DEDENT";
        case TOKEN_NEWLINE: return "NEWLINE";
        case TOKEN_IDENT: return "IDENT";
        case TOKEN_INT_LITERAL: return "INT";
        case TOKEN_FLOAT_LITERAL: return "FLOAT";
        case TOKEN_STRING_LITERAL: return "STRING";
        case TOKEN_CHAR_LITERAL: return "CHAR";
        case TOKEN_KW_FUNC: return "func";
        case TOKEN_KW_LET: return "let";
        case TOKEN_KW_MUT: return "mut";
        case TOKEN_KW_IF: return "if";
        case TOKEN_KW_ELIF: return "elif";
        case TOKEN_KW_ELSE: return "else";
        case TOKEN_KW_WHILE: return "while";
        case TOKEN_KW_FOR: return "for";
        case TOKEN_KW_IN: return "in";
        case TOKEN_KW_RETURN: return "return";
        case TOKEN_KW_TRUE: return "true";
        case TOKEN_KW_FALSE: return "false";
        case TOKEN_KW_NONE: return "none";
        case TOKEN_KW_ASM: return "asm";
        case TOKEN_KW_BREAK: return "break";
        case TOKEN_KW_CONTINUE: return "continue";
        case TOKEN_KW_STRUCT: return "struct";
        case TOKEN_KW_ENUM: return "enum";
        case TOKEN_KW_CLASS: return "class";
        case TOKEN_KW_MATCH: return "match";
        case TOKEN_KW_CASE: return "case";
        case TOKEN_KW_TRY: return "try";
        case TOKEN_KW_THROW: return "throw";
        case TOKEN_KW_CATCH: return "catch";
        case TOKEN_KW_AND: return "and";
        case TOKEN_KW_OR: return "or";
        case TOKEN_KW_NOT: return "not";
        case TOKEN_KW_IMPORT: return "import";
        case TOKEN_KW_CONST: return "const";
        case TOKEN_KW_REF: return "ref";
        case TOKEN_KW_OWNED: return "owned";
        case TOKEN_KW_RC: return "rc";
        case TOKEN_KW_HEAP: return "heap";
        case TOKEN_KW_REGION: return "region";
        case TOKEN_KW_PRIVATE: return "private";
        case TOKEN_KW_PUB: return "pub";
        case TOKEN_KW_INTERNAL: return "internal";
        case TOKEN_KW_STATIC: return "static";
        case TOKEN_KW_DEFER: return "defer";
        case TOKEN_KW_UNSAFE: return "unsafe";
        case TOKEN_KW_MODULE: return "module";
        case TOKEN_KW_SYS: return "sys";
        case TOKEN_KW_PRE: return "pre";
        case TOKEN_KW_POST: return "post";
        case TOKEN_KW_DROP: return "drop";
        case TOKEN_KW_INIT: return "init";
        case TOKEN_KW_SELF: return "self";
        case TOKEN_KW_TYPE: return "type";
        case TOKEN_KW_TRAIT: return "trait";
        case TOKEN_KW_IMPL: return "impl";
        case TOKEN_KW_POOL: return "pool";
        case TOKEN_KW_PROTOCOL: return "protocol";
        case TOKEN_KW_VIRTUAL: return "virtual";
        case TOKEN_KW_DYN: return "dyn";
        case TOKEN_KW_THROWS: return "throws";
        case TOKEN_KW_EXPORT: return "export";
        case TOKEN_KW_ENTRY: return "entry";
        case TOKEN_KW_LAYOUT: return "layout";
        case TOKEN_KW_TEST: return "test";
        case TOKEN_KW_RUN: return "run";
        case TOKEN_KW_IF_LET: return "iflet";
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_EQ: return "=";
        case TOKEN_EQ_EQ: return "==";
        case TOKEN_BANG: return "!";
        case TOKEN_BANG_EQ: return "!=";
        case TOKEN_LT: return "<";
        case TOKEN_GT: return ">";
        case TOKEN_LT_EQ: return "<=";
        case TOKEN_GT_EQ: return ">=";
        case TOKEN_AMPERSAND: return "&";
        case TOKEN_AND_AND: return "&&";
        case TOKEN_PIPE: return "|";
        case TOKEN_PIPE_PIPE: return "||";
        case TOKEN_CARET: return "^";
        case TOKEN_TILDE: return "~";
        case TOKEN_LT_LT: return "<<";
        case TOKEN_GT_GT: return ">>";
        case TOKEN_ARROW: return "->";
        case TOKEN_DOT_DOT: return "..";
        case TOKEN_DOT_DOT_EQ: return "..=";
        case TOKEN_COLON: return ":";
        case TOKEN_COMMA: return ",";
        case TOKEN_DOT: return ".";
        case TOKEN_AT: return "@";
        case TOKEN_HASH: return "#";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_LBRACKET: return "[";
        case TOKEN_RBRACKET: return "]";
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_PLUS_EQ: return "+=";
        case TOKEN_MINUS_EQ: return "-=";
        case TOKEN_STAR_EQ: return "*=";
        case TOKEN_SLASH_EQ: return "/=";
        case TOKEN_PIPE_LAMBDA: return "|";
        case TOKEN_QUESTION: return "?";
        case TOKEN_BACKSLASH: return "\\";
        case TOKEN_SEMICOLON: return ";";
        case TOKEN_COLON_COLON: return "::";
    }
    return "UNKNOWN";
}

/* ================================================================
 * Tokenizer internals
 * ================================================================ */

static bool is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static bool is_ident_continue(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static bool is_hex_char(char c) {
    return isdigit((unsigned char)c) ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static Token make_token(Tokenizer *t, TokenType type) {
    Token tok;
    tok.type = type;
    tok.loc.file = t->filename;
    tok.loc.line = t->line;
    tok.loc.col = t->col;
    tok.loc.len = (int)(t->pos - t->start);
    tok.text = sv_from_parts(t->start, (size_t)(t->pos - t->start));
    tok.val.int_value = 0;
    tok.val.float_value = 0.0;
    return tok;
}

static Token error_token(Tokenizer *t, const char *msg) {
    Token tok;
    tok.type = TOKEN_ERROR;
    tok.loc.file = t->filename;
    tok.loc.line = t->line;
    tok.loc.col = t->col;
    tok.loc.len = (int)(t->pos - t->start);
    tok.text = sv_from_cstr(msg);
    tok.val.int_value = 0;
    tok.val.float_value = 0.0;
    t->had_error = true;
    snprintf(t->error_msg, sizeof(t->error_msg), "%s", msg);
    return tok;
}

/* ================================================================
 * Tokenizer creation/destruction
 * ================================================================ */

Tokenizer *tokenizer_create(const char *source, size_t length, const char *filename) {
    Tokenizer *t = (Tokenizer *)calloc(1, sizeof(Tokenizer));
    if (!t) return NULL;

    t->start = source;
    t->pos = source;
    t->end = source + length;
    t->filename = filename ? filename : "<unknown>";
    t->line = 1;
    t->col = 1;

    /* Indent stack starts with level 0 (top-level) */
    t->indent_cap = 64;
    t->indent_stack = (int *)malloc(sizeof(int) * t->indent_cap);
    t->indent_stack[0] = 0;
    t->indent_depth = 0;

    /* Pending token buffer */
    t->pending_cap = 16;
    t->pending = (Token *)malloc(sizeof(Token) * t->pending_cap);
    t->pending_count = 0;

    t->had_error = false;
    t->error_msg[0] = '\0';

    return t;
}

void tokenizer_destroy(Tokenizer *t) {
    if (t) {
        free(t->indent_stack);
        free(t->pending);
        free(t);
    }
}

/* ================================================================
 * Indentation engine
 * ================================================================ */

static int count_indent(const char *line_start, const char *line_end) {
    int spaces = 0;
    const char *p = line_start;
    while (p < line_end && *p == ' ') {
        spaces++;
        p++;
    }
    /* Tabs are not allowed */
    if (p < line_end && *p == '\t') {
        return -1;  /* signal tab error */
    }
    return spaces;
}

/* Queue a token for later emission */
static void queue_token(Tokenizer *t, Token tok) {
    if (t->pending_count >= t->pending_cap) {
        t->pending_cap *= 2;
        t->pending = (Token *)realloc(t->pending, sizeof(Token) * t->pending_cap);
    }
    t->pending[t->pending_count++] = tok;
}

/* Dequeue next pending token. Returns true if one was available. */
static bool dequeue_token(Tokenizer *t, Token *out) {
    if (t->pending_count > 0) {
        *out = t->pending[0];
        /* Shift remaining tokens */
        memmove(t->pending, t->pending + 1, sizeof(Token) * (t->pending_count - 1));
        t->pending_count--;
        return true;
    }
    return false;
}

/* Handle indentation at the start of a new line */
static void handle_indent(Tokenizer *t, const char *line_start, const char *line_content) {
    int spaces = count_indent(line_start, line_content);
    
    if (spaces < 0) {
        /* Tab error */
        queue_token(t, error_token(t, "Tabs are not allowed for indentation. Use spaces."));
        return;
    }

    /* Blank/empty line — no indent change */
    if (line_content >= t->end || *line_content == '\n') {
        return;
    }

    int current_indent = t->indent_stack[t->indent_depth];

    if (spaces > current_indent) {
        /* Push new indent level */
        t->indent_depth++;
        if (t->indent_depth >= t->indent_cap) {
            t->indent_cap *= 2;
            t->indent_stack = (int *)realloc(t->indent_stack, sizeof(int) * t->indent_cap);
        }
        t->indent_stack[t->indent_depth] = spaces;
        queue_token(t, make_token(t, TOKEN_INDENT));
    } else if (spaces < current_indent) {
        /* Pop back to matching indent level */
        while (t->indent_depth > 0 && spaces < t->indent_stack[t->indent_depth]) {
            t->indent_depth--;
            queue_token(t, make_token(t, TOKEN_DEDENT));
        }
        if (spaces != t->indent_stack[t->indent_depth]) {
            queue_token(t, error_token(t, "Indentation doesn't match any outer level"));
        }
    }
}

/* Emit any remaining dedents at EOF */
static void emit_final_dedents(Tokenizer *t) {
    while (t->indent_depth > 0) {
        t->indent_depth--;
        queue_token(t, make_token(t, TOKEN_DEDENT));
    }
}

/* ================================================================
 * Character scanning helpers
 * ================================================================ */

static int next_char(Tokenizer *t) {
    if (t->pos >= t->end) return EOF;
    unsigned char c = (unsigned char)*t->pos;
    t->pos++;
    t->col++;
    if (c == '\n') {
        t->line++;
        t->col = 1;
    }
    return c;
}

static void skip_whitespace_inline(Tokenizer *t) {
    while (t->pos < t->end && (*t->pos == ' ' || *t->pos == '\t')) {
        if (*t->pos == '\t') {
            /* Tabs outside indentation = 4 spaces for column tracking */
            t->col += 4;
        } else {
            t->col++;
        }
        t->pos++;
    }
}

/* Skip a whole line (advance to \n or EOF). Returns true if we hit a newline. */
static bool skip_line(Tokenizer *t) {
    while (t->pos < t->end) {
        if (*t->pos == '\n') {
            t->pos++;
            t->line++;
            t->col = 1;
            return true;
        }
        t->pos++;
        t->col++;
    }
    return false;
}

/* ================================================================
 * Token scanning functions
 * ================================================================ */

static Token scan_comment(Tokenizer *t) {
    /* Already consumed '#', consume rest of line */
    skip_line(t);
    /* Return the next real token (recursive but depth-limited by line count) */
    return tokenizer_next(t);
}

static Token scan_block_comment(Tokenizer *t) {
    /* Already consumed '#{', consume until matching '}#' (nestable) */
    int depth = 1;
    while (t->pos < t->end && depth > 0) {
        if (t->pos + 1 < t->end && t->pos[0] == '}' && t->pos[1] == '#') {
            depth--;
            t->pos += 2;
            t->col += 2;
        } else if (t->pos + 1 < t->end && t->pos[0] == '#' && t->pos[1] == '{') {
            depth++;
            t->pos += 2;
            t->col += 2;
        } else {
            next_char(t);
        }
    }
    if (depth > 0) {
        return error_token(t, "Unclosed block comment '#{ ... }#'");
    }
    /* Consume trailing newline if present, so we don't get an extra NEWLINE token */
    if (t->pos < t->end && *t->pos == '\n') {
        t->pos++;
        t->line++;
        t->col = 1;
    }
    return tokenizer_next(t);
}

static Token scan_ident_or_keyword(Tokenizer *t) {
    t->start = t->pos - 1; /* include first char already consumed */
    while (t->pos < t->end && is_ident_continue(*t->pos)) {
        t->pos++;
    }

    StringView word = sv_from_parts(t->start, (size_t)(t->pos - t->start));

    /* Check keyword table */
    for (int i = 0; i < KEYWORD_COUNT; i++) {
        if (sv_eq_cstr(word, KEYWORDS[i].word)) {
            return make_token(t, KEYWORDS[i].type);
        }
    }

    /* Also check 'if let' as a two-word contextual keyword */
    /* (handled at parser level, not tokenizer) */

    return make_token(t, TOKEN_IDENT);
}

static Token scan_number(Tokenizer *t) {
    t->start = t->pos - 1;
    bool is_float = false;

    /* Check for prefix: 0x, 0b, 0o */
    if (t->start + 1 < t->end && t->start[0] == '0') {
        char next = t->start[1];
        if (next == 'x' || next == 'X') {
            t->pos = t->start + 2;
            while (t->pos < t->end && (*t->pos == '_' || is_hex_char(*t->pos))) {
                if (*t->pos == '_') { t->pos++; continue; }
                t->pos++;
            }
            Token tok = make_token(t, TOKEN_INT_LITERAL);
            /* Strip underscores before strtoull */
            char clean[32]; int ci = 0;
            for (const char *p = t->start + 2; p < t->pos && ci < 30; p++) {
                if (*p != '_') clean[ci++] = *p;
            }
            clean[ci] = '\0';
            tok.val.int_value = strtoull(clean, NULL, 16);
            return tok;
        }
        if (next == 'b' || next == 'B') {
            t->pos = t->start + 2;
            uint64_t val = 0;
            while (t->pos < t->end && (*t->pos == '0' || *t->pos == '1' || *t->pos == '_')) {
                if (*t->pos == '_') { t->pos++; continue; }
                val = (val << 1) | (*t->pos - '0');
                t->pos++;
            }
            Token tok = make_token(t, TOKEN_INT_LITERAL);
            tok.val.int_value = val;
            return tok;
        }
        if (next == 'o' || next == 'O') {
            t->pos = t->start + 2;
            uint64_t val = 0;
            while (t->pos < t->end && ((*t->pos >= '0' && *t->pos <= '7') || *t->pos == '_')) {
                if (*t->pos == '_') { t->pos++; continue; }
                val = (val << 3) | (*t->pos - '0');
                t->pos++;
            }
            Token tok = make_token(t, TOKEN_INT_LITERAL);
            tok.val.int_value = val;
            return tok;
        }
    }

    /* Decimal integer or float */
    while (t->pos < t->end && (isdigit((unsigned char)*t->pos) || *t->pos == '_')) {
        if (*t->pos == '_') { t->pos++; continue; }
        t->pos++;
    }

    /* Check for float (dot after digits) */
    if (t->pos < t->end && *t->pos == '.' && 
        (t->pos + 1 < t->end && isdigit((unsigned char)*(t->pos + 1)))) {
        is_float = true;
        t->pos++; /* consume '.' */
        while (t->pos < t->end && (isdigit((unsigned char)*t->pos) || *t->pos == '_')) {
            if (*t->pos == '_') { t->pos++; continue; }
            t->pos++;
        }
    }

    /* Check for exponent */
    if (t->pos < t->end && (*t->pos == 'e' || *t->pos == 'E')) {
        is_float = true;
        t->pos++;
        if (t->pos < t->end && (*t->pos == '+' || *t->pos == '-')) t->pos++;
        while (t->pos < t->end && (isdigit((unsigned char)*t->pos) || *t->pos == '_')) {
            if (*t->pos == '_') { t->pos++; continue; }
            t->pos++;
        }
    }

    /* Parse value — strip underscores for strtoull */
    char clean[32]; int ci = 0;
    for (const char *p = t->start; p < t->pos && ci < 30; p++) {
        if (*p != '_') clean[ci++] = *p;
    }
    clean[ci] = '\0';

    Token tok = make_token(t, is_float ? TOKEN_FLOAT_LITERAL : TOKEN_INT_LITERAL);
    if (is_float) {
        tok.val.float_value = strtod(clean, NULL);
    } else {
        tok.val.int_value = strtoull(clean, NULL, 10);
    }
    return tok;
}

static Token scan_string(Tokenizer *t) {
    t->start = t->pos - 1; /* include opening quote */
    /* Simple string scanning — just capture raw text between quotes */
    while (t->pos < t->end) {
        char c = *t->pos;
        if (c == '"') {
            t->pos++; t->col++; /* consume closing quote */
            break;
        }
        if (c == '\\' && t->pos + 1 < t->end) {
            t->pos++; t->col++; /* skip escape initiator */
            c = *t->pos;         /* skip escaped char */
        }
        if (c == '\n') {
            t->line++;
            t->col = 1;
        } else {
            t->col++;
        }
        t->pos++;
    }
    
    Token tok = make_token(t, TOKEN_STRING_LITERAL);
    /* Process escape sequences and compute actual value later */
    return tok;
}

static Token scan_char(Tokenizer *t) {
    t->start = t->pos - 1; /* include opening quote */
    uint64_t char_val = 0;
    
    if (t->pos < t->end && *t->pos == '\\') {
        t->pos++; t->col++;
        if (t->pos < t->end) {
            switch (*t->pos) {
                case 'n': char_val = '\n'; break;
                case 'r': char_val = '\r'; break;
                case 't': char_val = '\t'; break;
                case '\\': char_val = '\\'; break;
                case '\'': char_val = '\''; break;
                case 'x': {
                    t->pos++; t->col++;
                    if (t->pos + 1 < t->end && is_hex_char(*t->pos) && is_hex_char(*(t->pos+1))) {
                        char hex[3] = {t->pos[0], t->pos[1], 0};
                        char_val = strtoul(hex, NULL, 16);
                        t->pos++; t->col++;
                    }
                    break;
                }
            }
            t->pos++; t->col++;
        }
    } else if (t->pos < t->end) {
        char_val = (unsigned char)*t->pos;
        t->pos++; t->col++;
    }
    
    /* Consume closing quote */
    if (t->pos < t->end && *t->pos == '\'') {
        t->pos++; t->col++;
    }
    
    Token tok = make_token(t, TOKEN_CHAR_LITERAL);
    tok.val.int_value = char_val;
    return tok;
}

/* ================================================================
 * Main tokenizer_next function
 * ================================================================ */

Token tokenizer_next(Tokenizer *t) {
    /* First, check pending queue */
    Token pending;
    if (dequeue_token(t, &pending)) {
        return pending;
    }

    /* Main scan loop */
    while (t->pos < t->end) {
        t->start = t->pos;
        
        /* Check if we're at the start of a new line (after \n or at start of file) */
        if (t->col == 1) {
            /* Record the beginning of this line for indentation */
            const char *line_start = t->pos;
            
            /* Skip whitespace at line start */
            skip_whitespace_inline(t);
            
            /* If we hit a newline, blank line — skip */
            if (t->pos < t->end && *t->pos == '\n') {
                t->pos++;
                t->line++;
                t->col = 1;
                continue;
            }
            
            /* If EOF after whitespace on last line, done */
            if (t->pos >= t->end) {
                emit_final_dedents(t);
                Token eof_pending;
                if (dequeue_token(t, &eof_pending)) return eof_pending;
                return make_token(t, TOKEN_EOF);
            }
            
            /* Comment at line start? Process indentation then handle comment */
            if (*t->pos == '#') {
                /* Handle indentation just in case */
                if (t->pos + 1 < t->end && t->pos[1] == '{') {
                    /* Block comment — treat as no indent change (like blank line) */
                    t->pos += 2;
                    t->col += 2;
                    Token result = scan_block_comment(t);
                    if (result.type != TOKEN_EOF && dequeue_token(t, &pending)) return pending;
                    return result;
                } else {
                    /* Line comment — skip entire line */
                    skip_line(t);
                    continue;
                }
            }
            
            /* Process indentation */
            handle_indent(t, line_start, t->pos);
            
            /* Check for pending tokens from indent handling */
            if (dequeue_token(t, &pending)) {
                /* We need to return this pending token, but also save our position
                   so the next call resumes at the right spot */
                /* Return the indentation token; next call will process the line's content */
                return pending;
            }
        }

        /* Skip inline whitespace */
        skip_whitespace_inline(t);
        if (t->pos >= t->end) break;
        t->start = t->pos;

        char c = *t->pos;

        /* Newline — emit newline token */
        if (c == '\n') {
            t->pos++;
            t->line++;
            t->col = 1;
            /* Check if we need to go through indentation on next call */
            return make_token(t, TOKEN_NEWLINE);
        }

        /* Line continuation backslash */
        if (c == '\\') {
            t->pos++; t->col++;
            return make_token(t, TOKEN_BACKSLASH);
        }

        /* Comments */
        if (c == '#') {
            if (t->pos + 1 < t->end && t->pos[1] == '{') {
                t->pos += 2;
                t->col += 2;
                return scan_block_comment(t);
            }
            t->pos++; t->col++;
            return scan_comment(t);
        }

        /* Identifiers and keywords */
        if (is_ident_start(c)) {
            t->pos++; t->col++;
            return scan_ident_or_keyword(t);
        }

        /* Numbers */
        if (isdigit((unsigned char)c)) {
            t->pos++; t->col++;
            return scan_number(t);
        }

        /* String literals */
        if (c == '"') {
            t->pos++; t->col++;
            return scan_string(t);
        }

        /* Char literals */
        if (c == '\'') {
            t->pos++; t->col++;
            return scan_char(t);
        }

        /* Multi-character operators */
        t->pos++; t->col++;
        if (t->pos < t->end) {
            char next = *t->pos;

            /* Two-char operators */
            switch (c) {
                case '=': if (next == '=') { t->pos++; t->col++; return make_token(t, TOKEN_EQ_EQ); } return make_token(t, TOKEN_EQ);
                case '!': if (next == '=') { t->pos++; t->col++; return make_token(t, TOKEN_BANG_EQ); } return make_token(t, TOKEN_BANG);
                case '<': 
                    if (next == '=') { t->pos++; t->col++; return make_token(t, TOKEN_LT_EQ); }
                    if (next == '<') { t->pos++; t->col++; return make_token(t, TOKEN_LT_LT); }
                    return make_token(t, TOKEN_LT);
                case '>':
                    if (next == '=') { t->pos++; t->col++; return make_token(t, TOKEN_GT_EQ); }
                    if (next == '>') { t->pos++; t->col++; return make_token(t, TOKEN_GT_GT); }
                    return make_token(t, TOKEN_GT);
                case '&': if (next == '&') { t->pos++; t->col++; return make_token(t, TOKEN_AND_AND); } return make_token(t, TOKEN_AMPERSAND);
                case '|': if (next == '|') { t->pos++; t->col++; return make_token(t, TOKEN_PIPE_PIPE); } return make_token(t, TOKEN_PIPE);
                case '-': if (next == '>') { t->pos++; t->col++; return make_token(t, TOKEN_ARROW); } 
                          if (next == '=') { t->pos++; t->col++; return make_token(t, TOKEN_MINUS_EQ); } 
                          return make_token(t, TOKEN_MINUS);
                case '+': if (next == '=') { t->pos++; t->col++; return make_token(t, TOKEN_PLUS_EQ); } return make_token(t, TOKEN_PLUS);
                case '*': if (next == '=') { t->pos++; t->col++; return make_token(t, TOKEN_STAR_EQ); } return make_token(t, TOKEN_STAR);
                case '/': if (next == '=') { t->pos++; t->col++; return make_token(t, TOKEN_SLASH_EQ); } return make_token(t, TOKEN_SLASH);
                case '.': 
                    if (next == '.') {
                        t->pos++; t->col++;
                        if (t->pos < t->end && *t->pos == '=') { t->pos++; t->col++; return make_token(t, TOKEN_DOT_DOT_EQ); }
                        return make_token(t, TOKEN_DOT_DOT);
                    }
                    return make_token(t, TOKEN_DOT);
                case ':': if (next == ':') { t->pos++; t->col++; return make_token(t, TOKEN_COLON_COLON); }
                          return make_token(t, TOKEN_COLON);
            }
        }

        /* Single character tokens */
        switch (c) {
            case '(': return make_token(t, TOKEN_LPAREN);
            case ')': return make_token(t, TOKEN_RPAREN);
            case '[': return make_token(t, TOKEN_LBRACKET);
            case ']': return make_token(t, TOKEN_RBRACKET);
            case '{': return make_token(t, TOKEN_LBRACE);
            case '}': return make_token(t, TOKEN_RBRACE);
            case ':': return make_token(t, TOKEN_COLON);
            case ',': return make_token(t, TOKEN_COMMA);
            case '@': return make_token(t, TOKEN_AT);
            case '~': return make_token(t, TOKEN_TILDE);
            case '^': return make_token(t, TOKEN_CARET);
            case '%': return make_token(t, TOKEN_PERCENT);
            case ';': return make_token(t, TOKEN_SEMICOLON);
            case '?': return make_token(t, TOKEN_QUESTION);
            default: return error_token(t, "Unexpected character");
        }
    }

    /* EOF — emit any final dedents */
    emit_final_dedents(t);
    if (dequeue_token(t, &pending)) return pending;
    return make_token(t, TOKEN_EOF);
}

Token tokenizer_peek(Tokenizer *t) {
    /* Peek uses a destructive approach: advance, but caller consumes.
       For proper look-ahead the parser should advance and build a 
       proper peek buffer. */
    return tokenizer_next(t);
}

Vector *tokenizer_tokenize_all(Tokenizer *t) {
    Vector *tokens = vector_create_temp(sizeof(Token));
    if (!tokens) return NULL;

    while (true) {
        Token tok = tokenizer_next(t);
        Vector *slot = vector_push_unsafe(tokens);
        if (!slot) { vector_destroy(tokens); return NULL; }
        memcpy(slot, &tok, sizeof(Token));
        if (tok.type == TOKEN_EOF || tok.type == TOKEN_ERROR) break;
    }

    return tokens;
}

void token_print(const Token *t) {
    printf("[%s] ", token_type_name(t->type));
    if (t->type == TOKEN_IDENT || t->type == TOKEN_STRING_LITERAL || 
        t->type == TOKEN_INT_LITERAL || t->type == TOKEN_FLOAT_LITERAL) {
        printf("%.*s", (int)t->text.len, t->text.data);
    }
    if (t->type == TOKEN_INT_LITERAL) {
        printf(" (%llu)", (unsigned long long)t->val.int_value);
    }
    if (t->type == TOKEN_FLOAT_LITERAL) {
        printf(" (%f)", t->val.float_value);
    }
    if (t->type == TOKEN_CHAR_LITERAL) {
        printf(" '%c' (0x%02x)", (unsigned char)t->val.int_value, (unsigned int)t->val.int_value);
    }
    printf(" [%s:%d:%d]\n", t->loc.file, t->loc.line, t->loc.col);
}