#ifndef AETHER_ASM_PARSER_H
#define AETHER_ASM_PARSER_H

#include "aether/asm_ir.h"

/* ================================================================
 * NASM Syntax Parser
 *
 * Parses raw NASM assembly text into the AsmIR intermediate
 * representation. Handles instructions, directives, labels,
 * comments, and all x86_64 addressing modes.
 * ================================================================ */

/* --- Parser state --- */
typedef struct {
    const char *source;
    size_t source_len;
    size_t pos;
    int line;
    int col;
    const char *filename;
    char error_msg[256];
    int has_error;
} AsmParser;

/* --- Token types for NASM parsing --- */
typedef enum {
    ASM_TOK_EOF,
    ASM_TOK_IDENT,        /* identifier or mnemonic */
    ASM_TOK_REGISTER,     /* rax, rbx, xmm0, etc. */
    ASM_TOK_NUMBER,      /* 42, 0xFF, 0b1010 */
    ASM_TOK_STRING,      /* "hello" or 'hello' */
    ASM_TOK_COMMA,       /* , */
    ASM_TOK_COLON,       /* : */
    ASM_TOK_LBRACKET,    /* [ */
    ASM_TOK_RBRACKET,    /* ] */
    ASM_TOK_LPAREN,      /* ( */
    ASM_TOK_RPAREN,      /* ) */
    ASM_TOK_PLUS,        /* + */
    ASM_TOK_MINUS,       /* - */
    ASM_TOK_STAR,        /* * */
    ASM_TOK_DOLLAR,      /* $ */
    ASM_TOK_PERCENT,     /* % */
    ASM_TOK_NEWLINE,     /* end of line */
    ASM_TOK_SIZE_SPEC,   /* byte, word, dword, qword, oword, tword */
    ASM_TOK_DIRECTIVE,   /* section, global, extern, align, etc. */
    ASM_TOK_PREFIX,      /* lock, rep, repe, repne */
    ASM_TOK_ERROR,
} AsmTokenType;

typedef struct {
    AsmTokenType type;
    const char *start;
    size_t len;
    int64_t int_value;
    int line;
    int col;
} AsmToken;

/* --- Parser API --- */
void asm_parser_init(AsmParser *parser, const char *source, size_t source_len, const char *filename);
AsmToken asm_parser_next(AsmParser *parser);
AsmToken asm_parser_peek(AsmParser *parser);
int asm_parser_match(AsmParser *parser, AsmTokenType type);
int asm_parser_expect(AsmParser *parser, AsmTokenType type, const char *context);

/* --- High-level parse --- */
int asm_parse_block(AsmParser *parser, AsmBlock *block);

/* --- Element parsers --- */
int asm_parse_instruction(AsmParser *parser, AsmInstruction *instr);
int asm_parse_operand(AsmParser *parser, AsmOperand *op);
int asm_parse_mem(AsmParser *parser, AsmMem *mem);
int asm_parse_directive(AsmParser *parser, AsmDirective *dir);
int asm_parse_label(AsmParser *parser, AsmLabel *label);
int asm_parse_data_values(AsmParser *parser, AsmOperand *values, int *count, int max);

/* --- Convenience: parse a string directly into a block --- */
int asm_parse_string(const char *source, AsmBlock *block, const char *filename);

#endif /* AETHER_ASM_PARSER_H */
