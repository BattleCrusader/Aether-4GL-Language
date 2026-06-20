/* ================================================================
 * asm_parser.c — NASM Syntax Parser
 * ================================================================ */

#include "aether/asm_parser.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

static AsmToken asm_lex_next(AsmParser *parser);
static void asm_parser_error(AsmParser *parser, const char *fmt, ...);

/* Helper: copy token text into a buffer with null termination */
static void tok_copy(char *buf, size_t bufsz, const AsmToken *tok) {
    size_t n = tok->len < bufsz - 1 ? tok->len : bufsz - 1;
    memcpy(buf, tok->start, n);
    buf[n] = '\0';
}

/* Helper: get register from token (handles non-null-terminated) */
static AsmRegister reg_from_tok(const AsmToken *tok) {
    char buf[16];
    size_t n = tok->len < 15 ? tok->len : 15;
    memcpy(buf, tok->start, n);
    buf[n] = '\0';
    return asm_reg_from_name(buf);
}

void asm_parser_init(AsmParser *parser, const char *source, size_t source_len, const char *filename) {
    parser->source = source;
    parser->source_len = source_len;
    parser->pos = 0;
    parser->line = 1;
    parser->col = 1;
    parser->filename = filename;
    parser->error_msg[0] = '\0';
    parser->has_error = 0;
}

static char asm_peek(AsmParser *parser) {
    if (parser->pos >= parser->source_len) return '\0';
    return parser->source[parser->pos];
}

static char asm_advance(AsmParser *parser) {
    if (parser->pos >= parser->source_len) return '\0';
    char c = parser->source[parser->pos++];
    if (c == '\n') { parser->line++; parser->col = 1; }
    else { parser->col++; }
    return c;
}

static void asm_skip_space(AsmParser *parser) {
    while (asm_peek(parser) == ' ' || asm_peek(parser) == '\t') asm_advance(parser);
}

static void asm_skip_to_eol(AsmParser *parser) {
    while (asm_peek(parser) != '\0' && asm_peek(parser) != '\n') asm_advance(parser);
}

static int asm_is_register(const char *s, size_t len) {
    if (len < 2 || len > 4) return 0;
    char buf[16];
    if (len >= 16) return 0;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return (asm_reg_from_name(buf) != ASM_REG_COUNT);
}

static int asm_is_size_spec(const char *s, size_t len) {
    if (len == 4 && memcmp(s, "byte", 4) == 0) return 1;
    if (len == 4 && memcmp(s, "word", 4) == 0) return 1;
    if (len == 5 && memcmp(s, "dword", 5) == 0) return 1;
    if (len == 5 && memcmp(s, "qword", 5) == 0) return 1;
    if (len == 5 && memcmp(s, "oword", 5) == 0) return 1;
    if (len == 4 && memcmp(s, "tword", 4) == 0) return 1;
    return 0;
}

static int asm_is_directive(const char *s, size_t len) {
    const char *dirs[] = {
        "section", "global", "extern", "align", "org", "bits",
        "times", "db", "dw", "dd", "dq", "resb", "resw", "resd", "resq",
        "incbin", "equ", "struc", "endstruc", "istruc", "at", "iend",
        "macro", "endmacro", "imacro", "endimacro", NULL
    };
    for (int i = 0; dirs[i]; i++) {
        size_t dlen = strlen(dirs[i]);
        if (len == dlen && memcmp(s, dirs[i], dlen) == 0) return 1;
    }
    return 0;
}

static int asm_is_prefix(const char *s, size_t len) {
    if (len == 4 && memcmp(s, "lock", 4) == 0) return 1;
    if (len == 3 && memcmp(s, "rep", 3) == 0) return 1;
    if (len == 4 && memcmp(s, "repe", 4) == 0) return 1;
    if (len == 5 && memcmp(s, "repne", 5) == 0) return 1;
    return 0;
}

static AsmToken asm_lex_next(AsmParser *parser) {
    AsmToken tok;
    memset(&tok, 0, sizeof(tok));
    tok.line = parser->line;
    tok.col = parser->col;

    asm_skip_space(parser);
    char c = asm_peek(parser);
    tok.start = parser->source + parser->pos;

    if (c == '\0') { tok.type = ASM_TOK_EOF; return tok; }
    if (c == '\n') { tok.type = ASM_TOK_NEWLINE; tok.len = 1; asm_advance(parser); return tok; }
    if (c == ';') { asm_skip_to_eol(parser); return asm_lex_next(parser); }

    if (c == '"' || c == '\'') {
        char quote = c;
        asm_advance(parser);
        tok.type = ASM_TOK_STRING;
        tok.start = parser->source + parser->pos;
        tok.len = 0;
        while (asm_peek(parser) != '\0' && asm_peek(parser) != quote) {
            if (asm_peek(parser) == '\\') { asm_advance(parser); if (asm_peek(parser) != '\0') asm_advance(parser); }
            else { asm_advance(parser); }
            tok.len++;
        }
        if (asm_peek(parser) == quote) asm_advance(parser);
        return tok;
    }

    if (isdigit(c) || (c == '0' && parser->pos + 1 < parser->source_len &&
        (parser->source[parser->pos + 1] == 'x' || parser->source[parser->pos + 1] == 'X'))) {
        tok.type = ASM_TOK_NUMBER;
        tok.int_value = 0;
        tok.start = parser->source + parser->pos;
        if (c == '0') {
            asm_advance(parser);
            c = asm_peek(parser);
            if (c == 'x' || c == 'X') {
                asm_advance(parser);
                tok.start = parser->source + parser->pos;
                while (isxdigit(asm_peek(parser))) {
                    c = asm_advance(parser);
                    if (c >= '0' && c <= '9') tok.int_value = tok.int_value * 16 + (c - '0');
                    else if (c >= 'a' && c <= 'f') tok.int_value = tok.int_value * 16 + (c - 'a' + 10);
                    else tok.int_value = tok.int_value * 16 + (c - 'A' + 10);
                    tok.len++;
                }
                return tok;
            }
            if (c == 'b' || c == 'B') {
                asm_advance(parser);
                tok.start = parser->source + parser->pos;
                while (asm_peek(parser) == '0' || asm_peek(parser) == '1') {
                    c = asm_advance(parser);
                    tok.int_value = tok.int_value * 2 + (c - '0');
                    tok.len++;
                }
                return tok;
            }
            tok.len = 1;
            return tok;
        }
        while (isdigit(asm_peek(parser))) {
            c = asm_advance(parser);
            tok.int_value = tok.int_value * 10 + (c - '0');
            tok.len++;
        }
        return tok;
    }

    if (c == ',') { tok.type = ASM_TOK_COMMA; tok.len = 1; asm_advance(parser); return tok; }
    if (c == ':') { tok.type = ASM_TOK_COLON; tok.len = 1; asm_advance(parser); return tok; }
    if (c == '[') { tok.type = ASM_TOK_LBRACKET; tok.len = 1; asm_advance(parser); return tok; }
    if (c == ']') { tok.type = ASM_TOK_RBRACKET; tok.len = 1; asm_advance(parser); return tok; }
    if (c == '+') { tok.type = ASM_TOK_PLUS; tok.len = 1; asm_advance(parser); return tok; }
    if (c == '-') { tok.type = ASM_TOK_MINUS; tok.len = 1; asm_advance(parser); return tok; }
    if (c == '*') { tok.type = ASM_TOK_STAR; tok.len = 1; asm_advance(parser); return tok; }
    if (c == '$') { tok.type = ASM_TOK_DOLLAR; tok.len = 1; asm_advance(parser); return tok; }
    if (c == '%') { tok.type = ASM_TOK_PERCENT; tok.len = 1; asm_advance(parser); return tok; }

    if (isalpha(c) || c == '_' || c == '.') {
        tok.type = ASM_TOK_IDENT;
        tok.len = 0;
        while (isalnum(asm_peek(parser)) || asm_peek(parser) == '_' || asm_peek(parser) == '.') {
            asm_advance(parser);
            tok.len++;
        }
        const char *word = tok.start;
        size_t wlen = tok.len;
        if (asm_is_register(word, wlen)) tok.type = ASM_TOK_REGISTER;
        else if (asm_is_size_spec(word, wlen)) tok.type = ASM_TOK_SIZE_SPEC;
        else if (asm_is_directive(word, wlen)) tok.type = ASM_TOK_DIRECTIVE;
        else if (asm_is_prefix(word, wlen)) tok.type = ASM_TOK_PREFIX;
        return tok;
    }

    tok.type = ASM_TOK_ERROR;
    tok.len = 1;
    asm_advance(parser);
    return tok;
}

AsmToken asm_parser_peek(AsmParser *parser) {
    size_t saved_pos = parser->pos;
    int saved_line = parser->line;
    int saved_col = parser->col;
    AsmToken tok = asm_lex_next(parser);
    parser->pos = saved_pos;
    parser->line = saved_line;
    parser->col = saved_col;
    return tok;
}

AsmToken asm_parser_next(AsmParser *parser) {
    return asm_lex_next(parser);
}

int asm_parser_match(AsmParser *parser, AsmTokenType type) {
    AsmToken tok = asm_parser_peek(parser);
    if (tok.type == type) { asm_parser_next(parser); return 1; }
    return 0;
}

int asm_parser_expect(AsmParser *parser, AsmTokenType type, const char *context) {
    AsmToken tok = asm_parser_next(parser);
    if (tok.type != type) {
        asm_parser_error(parser, "expected token in %s", context);
        return 0;
    }
    return 1;
}

static void asm_parser_error(AsmParser *parser, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(parser->error_msg, sizeof(parser->error_msg), fmt, args);
    va_end(args);
    parser->has_error = 1;
}

int asm_parse_mem(AsmParser *parser, AsmMem *mem) {
    asm_mem_init(mem);
    if (!asm_parser_match(parser, ASM_TOK_LBRACKET)) {
        asm_parser_error(parser, "expected '[' for memory operand");
        return 0;
    }
    AsmToken tok = asm_parser_peek(parser);

    if (tok.type == ASM_TOK_IDENT && tok.len == 3 && memcmp(tok.start, "rel", 3) == 0) {
        asm_parser_next(parser);
        tok = asm_parser_next(parser);
        if (tok.type == ASM_TOK_IDENT || tok.type == ASM_TOK_REGISTER) {
            mem->mode = ASM_ADDR_RIP_REL;
            tok_copy(mem->label, sizeof(mem->label), &tok);
        }
        asm_parser_expect(parser, ASM_TOK_RBRACKET, "memory operand");
        return 1;
    }

    if (tok.type == ASM_TOK_REGISTER) {
        asm_parser_next(parser);
        mem->base = reg_from_tok(&tok);
        mem->mode = ASM_ADDR_BASE;
        tok = asm_parser_peek(parser);
    } else if (tok.type == ASM_TOK_IDENT) {
        asm_parser_next(parser);
        mem->mode = ASM_ADDR_DIRECT;
        tok_copy(mem->label, sizeof(mem->label), &tok);
        asm_parser_expect(parser, ASM_TOK_RBRACKET, "memory operand");
        return 1;
    } else if (tok.type == ASM_TOK_NUMBER) {
        asm_parser_next(parser);
        mem->mode = ASM_ADDR_DIRECT;
        mem->displacement = tok.int_value;
        asm_parser_expect(parser, ASM_TOK_RBRACKET, "memory operand");
        return 1;
    } else {
        asm_parser_error(parser, "expected register or label in memory operand");
        return 0;
    }

    int sign = 1;
    tok = asm_parser_peek(parser);
    if (tok.type == ASM_TOK_PLUS || tok.type == ASM_TOK_MINUS) {
        if (tok.type == ASM_TOK_MINUS) sign = -1;
        asm_parser_next(parser);
        tok = asm_parser_peek(parser);
        if (tok.type == ASM_TOK_REGISTER) {
            asm_parser_next(parser);
            mem->index = reg_from_tok(&tok);
            mem->mode = ASM_ADDR_BASE_INDEX;
            if (asm_parser_peek(parser).type == ASM_TOK_STAR) {
                asm_parser_next(parser);
                tok = asm_parser_next(parser);
                if (tok.type == ASM_TOK_NUMBER) mem->scale = (int)tok.int_value;
            }
            tok = asm_parser_peek(parser);
            if (tok.type == ASM_TOK_PLUS || tok.type == ASM_TOK_MINUS) {
                int s2 = (tok.type == ASM_TOK_MINUS) ? -1 : 1;
                asm_parser_next(parser);
                tok = asm_parser_next(parser);
                if (tok.type == ASM_TOK_NUMBER) {
                    mem->displacement = s2 * tok.int_value;
                    mem->mode = ASM_ADDR_BASE_INDEX_DISP;
                }
            }
        } else if (tok.type == ASM_TOK_NUMBER) {
            asm_parser_next(parser);
            mem->displacement = sign * tok.int_value;
            mem->mode = ASM_ADDR_BASE_DISP;
        }
    }

    asm_parser_expect(parser, ASM_TOK_RBRACKET, "memory operand");
    return 1;
}

int asm_parse_operand(AsmParser *parser, AsmOperand *op) {
    AsmToken tok = asm_parser_peek(parser);
    if (tok.type == ASM_TOK_SIZE_SPEC) {
        asm_parser_next(parser);
        return asm_parse_mem(parser, &op->mem);
    }
    if (tok.type == ASM_TOK_REGISTER) {
        asm_parser_next(parser);
        asm_op_reg(op, reg_from_tok(&tok));
        return 1;
    }
    if (tok.type == ASM_TOK_LBRACKET) {
        AsmMem mem;
        if (!asm_parse_mem(parser, &mem)) return 0;
        asm_op_mem(op, &mem);
        return 1;
    }
    if (tok.type == ASM_TOK_NUMBER) {
        asm_parser_next(parser);
        asm_op_imm(op, tok.int_value);
        return 1;
    }
    if (tok.type == ASM_TOK_STRING) {
        asm_parser_next(parser);
        asm_op_expr(op, tok.start);
        return 1;
    }
    if (tok.type == ASM_TOK_IDENT) {
        asm_parser_next(parser);
        char buf[ASM_MAX_LABEL_LEN];
        tok_copy(buf, sizeof(buf), &tok);
        asm_op_label(op, buf);
        return 1;
    }
    if (tok.type == ASM_TOK_DOLLAR) {
        asm_parser_next(parser);
        asm_op_imm(op, 0);
        return 1;
    }
    asm_parser_error(parser, "expected operand");
    return 0;
}

int asm_parse_data_values(AsmParser *parser, AsmOperand *values, int *count, int max) {
    *count = 0;
    while (*count < max) {
        AsmOperand op;
        if (!asm_parse_operand(parser, &op)) return 0;
        values[(*count)++] = op;
        if (!asm_parser_match(parser, ASM_TOK_COMMA)) break;
    }
    return 1;
}

int asm_parse_instruction(AsmParser *parser, AsmInstruction *instr) {
    AsmToken tok = asm_parser_peek(parser);
    int has_lock = 0, has_rep = 0;
    while (tok.type == ASM_TOK_PREFIX) {
        if (tok.len == 4 && memcmp(tok.start, "lock", 4) == 0) has_lock = 1;
        else has_rep = 1;
        asm_parser_next(parser);
        tok = asm_parser_peek(parser);
    }
    if (tok.type != ASM_TOK_IDENT) {
        asm_parser_error(parser, "expected instruction mnemonic");
        return 0;
    }
    asm_parser_next(parser);
    char mnemonic[ASM_MAX_MNEMONIC_LEN];
    tok_copy(mnemonic, sizeof(mnemonic), &tok);
    asm_instr_init(instr, mnemonic);
    instr->is_lock_prefix = has_lock;
    instr->is_rep_prefix = has_rep;

    tok = asm_parser_peek(parser);
    if (tok.type == ASM_TOK_SIZE_SPEC) {
        asm_parser_next(parser);
        instr->has_size_spec = 1;
        if (tok.len == 4 && memcmp(tok.start, "byte", 4) == 0) instr->size_spec = ASM_SIZE_BYTE;
        else if (tok.len == 4 && memcmp(tok.start, "word", 4) == 0) instr->size_spec = ASM_SIZE_WORD;
        else if (tok.len == 5 && memcmp(tok.start, "dword", 5) == 0) instr->size_spec = ASM_SIZE_DWORD;
        else if (tok.len == 5 && memcmp(tok.start, "qword", 5) == 0) instr->size_spec = ASM_SIZE_QWORD;
        else if (tok.len == 5 && memcmp(tok.start, "oword", 5) == 0) instr->size_spec = ASM_SIZE_OWORD;
        else if (tok.len == 4 && memcmp(tok.start, "tword", 4) == 0) instr->size_spec = ASM_SIZE_TWORD;
    }

    tok = asm_parser_peek(parser);
    if (tok.type != ASM_TOK_NEWLINE && tok.type != ASM_TOK_EOF && tok.type != ASM_TOK_COLON) {
        AsmOperand op;
        if (!asm_parse_operand(parser, &op)) return 0;
        asm_instr_add_operand(instr, &op);
        while (asm_parser_match(parser, ASM_TOK_COMMA)) {
            if (!asm_parse_operand(parser, &op)) return 0;
            asm_instr_add_operand(instr, &op);
        }
    }
    return 1;
}

int asm_parse_directive(AsmParser *parser, AsmDirective *dir) {
    memset(dir, 0, sizeof(AsmDirective));
    AsmToken tok = asm_parser_next(parser);
    if (tok.type != ASM_TOK_DIRECTIVE) { asm_parser_error(parser, "expected directive"); return 0; }
    tok_copy(dir->name, sizeof(dir->name), &tok);

    if (tok.len == 7 && memcmp(tok.start, "section", 7) == 0) {
        dir->type = ASM_DIR_SECTION;
        tok = asm_parser_next(parser);
        if (tok.type == ASM_TOK_IDENT || tok.type == ASM_TOK_STRING)
            tok_copy(dir->name, sizeof(dir->name), &tok);
    } else if (tok.len == 6 && memcmp(tok.start, "global", 6) == 0) {
        dir->type = ASM_DIR_GLOBAL;
        tok = asm_parser_next(parser);
        if (tok.type == ASM_TOK_IDENT) tok_copy(dir->name, sizeof(dir->name), &tok);
    } else if (tok.len == 6 && memcmp(tok.start, "extern", 6) == 0) {
        dir->type = ASM_DIR_EXTERN;
        tok = asm_parser_next(parser);
        if (tok.type == ASM_TOK_IDENT) tok_copy(dir->name, sizeof(dir->name), &tok);
    } else if (tok.len == 5 && memcmp(tok.start, "align", 5) == 0) {
        dir->type = ASM_DIR_ALIGN;
        tok = asm_parser_next(parser);
        if (tok.type == ASM_TOK_NUMBER) dir->value = tok.int_value;
    } else if (tok.len == 3 && memcmp(tok.start, "org", 3) == 0) {
        dir->type = ASM_DIR_ORG;
        tok = asm_parser_next(parser);
        if (tok.type == ASM_TOK_NUMBER) dir->value = tok.int_value;
    } else if (tok.len == 4 && memcmp(tok.start, "bits", 4) == 0) {
        dir->type = ASM_DIR_BITS;
        tok = asm_parser_next(parser);
        if (tok.type == ASM_TOK_NUMBER) dir->value = tok.int_value;
    } else if (tok.len == 5 && memcmp(tok.start, "times", 5) == 0) {
        dir->type = ASM_DIR_TIMES;
        tok = asm_parser_next(parser);
        if (tok.type == ASM_TOK_NUMBER) dir->value = tok.int_value;
    } else if (tok.len == 2 && memcmp(tok.start, "db", 2) == 0) {
        dir->type = ASM_DIR_DB;
        asm_parse_data_values(parser, dir->data_operands, &dir->data_count, ASM_MAX_OPERANDS);
    } else if (tok.len == 2 && memcmp(tok.start, "dw", 2) == 0) {
        dir->type = ASM_DIR_DW;
        asm_parse_data_values(parser, dir->data_operands, &dir->data_count, ASM_MAX_OPERANDS);
    } else if (tok.len == 2 && memcmp(tok.start, "dd", 2) == 0) {
        dir->type = ASM_DIR_DD;
        asm_parse_data_values(parser, dir->data_operands, &dir->data_count, ASM_MAX_OPERANDS);
    } else if (tok.len == 2 && memcmp(tok.start, "dq", 2) == 0) {
        dir->type = ASM_DIR_DQ;
        asm_parse_data_values(parser, dir->data_operands, &dir->data_count, ASM_MAX_OPERANDS);
    } else if (tok.len == 4 && memcmp(tok.start, "resb", 4) == 0) {
        dir->type = ASM_DIR_RESB;
        tok = asm_parser_next(parser);
        if (tok.type == ASM_TOK_NUMBER) dir->value = tok.int_value;
    } else if (tok.len == 4 && memcmp(tok.start, "resw", 4) == 0) {
        dir->type = ASM_DIR_RESW;
        tok = asm_parser_next(parser);
        if (tok.type == ASM_TOK_NUMBER) dir->value = tok.int_value;
    } else if (tok.len == 4 && memcmp(tok.start, "resd", 4) == 0) {
        dir->type = ASM_DIR_RESD;
        tok = asm_parser_next(parser);
        if (tok.type == ASM_TOK_NUMBER) dir->value = tok.int_value;
    } else if (tok.len == 4 && memcmp(tok.start, "resq", 4) == 0) {
        dir->type = ASM_DIR_RESQ;
        tok = asm_parser_next(parser);
        if (tok.type == ASM_TOK_NUMBER) dir->value = tok.int_value;
    } else if (tok.len == 6 && memcmp(tok.start, "incbin", 6) == 0) {
        dir->type = ASM_DIR_INCBIN;
        tok = asm_parser_next(parser);
        if (tok.type == ASM_TOK_STRING) tok_copy(dir->name, sizeof(dir->name), &tok);
    } else if (tok.len == 3 && memcmp(tok.start, "equ", 3) == 0) {
        dir->type = ASM_DIR_EQU;
        tok = asm_parser_next(parser);
        if (tok.type == ASM_TOK_NUMBER) dir->value = tok.int_value;
    }
    return 1;
}

int asm_parse_label(AsmParser *parser, AsmLabel *label) {
    memset(label, 0, sizeof(AsmLabel));
    AsmToken tok = asm_parser_peek(parser);
    if (tok.type != ASM_TOK_IDENT) return 0;
    size_t saved_pos = parser->pos;
    int saved_line = parser->line;
    int saved_col = parser->col;
    asm_parser_next(parser);
    if (asm_parser_peek(parser).type == ASM_TOK_COLON) {
        asm_parser_next(parser);
        tok_copy(label->name, sizeof(label->name), &tok);
        label->is_local = (tok.start[0] == '.');
        return 1;
    }
    parser->pos = saved_pos;
    parser->line = saved_line;
    parser->col = saved_col;
    return 0;
}

int asm_parse_block(AsmParser *parser, AsmBlock *block) {
    asm_block_init(block);
    while (1) {
        AsmToken tok = asm_parser_peek(parser);
        if (tok.type == ASM_TOK_EOF) break;
        if (tok.type == ASM_TOK_NEWLINE) { asm_parser_next(parser); continue; }

        AsmLabel label;
        if (asm_parse_label(parser, &label)) {
            AsmElement elem;
            memset(&elem, 0, sizeof(elem));
            elem.type = ASM_ELEM_LABEL;
            elem.label = label;
            asm_block_add(block, &elem);
            continue;
        }

        if (tok.type == ASM_TOK_DIRECTIVE) {
            AsmDirective dir;
            if (asm_parse_directive(parser, &dir)) {
                AsmElement elem;
                memset(&elem, 0, sizeof(elem));
                elem.type = ASM_ELEM_DIRECTIVE;
                elem.directive = dir;
                asm_block_add(block, &elem);
            }
            while (asm_parser_peek(parser).type != ASM_TOK_NEWLINE &&
                   asm_parser_peek(parser).type != ASM_TOK_EOF) asm_parser_next(parser);
            continue;
        }

        if (tok.type == ASM_TOK_LBRACKET) {
        /* [org 0x7C00] or [bits 16] — bracket-wrapped directives */
        AsmToken next = asm_parser_peek(parser);
        if (next.type == ASM_TOK_DIRECTIVE) {
            asm_parser_next(parser); /* consume [ */
            AsmDirective dir;
            if (asm_parse_directive(parser, &dir)) {
                AsmElement elem;
                memset(&elem, 0, sizeof(elem));
                elem.type = ASM_ELEM_DIRECTIVE;
                elem.directive = dir;
                asm_block_add(block, &elem);
            }
            asm_parser_expect(parser, ASM_TOK_RBRACKET, "bracket directive");
            continue;
        }
        /* Not a bracket directive, fall through to instruction parsing */
    }

    if (tok.type == ASM_TOK_IDENT || tok.type == ASM_TOK_PREFIX || tok.type == ASM_TOK_LBRACKET) {
            AsmInstruction instr;
            if (asm_parse_instruction(parser, &instr)) {
                AsmElement elem;
                memset(&elem, 0, sizeof(elem));
                elem.type = ASM_ELEM_INSTRUCTION;
                elem.instr = instr;
                asm_block_add(block, &elem);
            }
            while (asm_parser_peek(parser).type != ASM_TOK_NEWLINE &&
                   asm_parser_peek(parser).type != ASM_TOK_EOF) asm_parser_next(parser);
            continue;
        }

        asm_parser_next(parser);
    }
    return !parser->has_error;
}

int asm_parse_string(const char *source, AsmBlock *block, const char *filename) {
    AsmParser parser;
    asm_parser_init(&parser, source, strlen(source), filename);
    return asm_parse_block(&parser, block);
}
