#include "aether/c_transpiler.h"
#include "aether/str.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────
 * Expression codegen — emit C expressions
 * ────────────────────────────────────────────── */

static void c_emit_literal_int(CCodegen *cg, AstNode *node) {
    uint64_t val = node->data.literal.int_val;
    if (node->data.literal.is_negative) {
        val = ~val + 1;
    }
    fprintf(cg->out, "%llu", (unsigned long long)val);
}

static void c_emit_literal_float(CCodegen *cg, AstNode *node) {
    fprintf(cg->out, "%f", node->data.literal.float_val);
}

static void c_emit_literal_string(CCodegen *cg, AstNode *node) {
    StringView sv = node->data.literal.string_val;
    int len = (int)sv.len;
    /* Strip surrounding quotes if present */
    const char *data = sv.data;
    int content_len = len;
    if (content_len >= 2 && data[0] == '"' && data[content_len-1] == '"') {
        data++;
        content_len -= 2;
    }
    /* Process escape sequences and compute actual content */
    char processed[4096];
    int plen = 0;
    for (int i = 0; i < content_len && plen < 4095; i++) {
        if (data[i] == '\\' && i + 1 < content_len) {
            i++;
            switch (data[i]) {
                case 'n': processed[plen++] = '\n'; break;
                case 't': processed[plen++] = '\t'; break;
                case 'r': processed[plen++] = '\r'; break;
                case '0': processed[plen++] = '\0'; break;
                case '\\': processed[plen++] = '\\'; break;
                case '"': processed[plen++] = '"'; break;
                case '\'': processed[plen++] = '\''; break;
                default: processed[plen++] = data[i]; break;
            }
        } else {
            processed[plen++] = data[i];
        }
    }
    /* Emit as string struct initializer: (string){ len, "content" } */
    fprintf(cg->out, "(string){ %d, \"", plen);
    for (int i = 0; i < plen; i++) {
        char c = processed[i];
        if (c == '"') fputs("\\\"", cg->out);
        else if (c == '\\') fputs("\\\\", cg->out);
        else if (c == '\n') fputs("\\n", cg->out);
        else if (c == '\t') fputs("\\t", cg->out);
        else if (c >= 32 && c < 127) fputc(c, cg->out);
        else fprintf(cg->out, "\\x%02x", (unsigned char)c);
    }
    fputs("\" }", cg->out);
}

static void c_emit_literal_bool(CCodegen *cg, AstNode *node) {
    fputs(node->data.literal.bool_val ? "1" : "0", cg->out);
}

static void c_emit_literal_char(CCodegen *cg, AstNode *node) {
    fprintf(cg->out, "%d", node->data.literal.char_val);
}

static void c_emit_ident(CCodegen *cg, AstNode *node) {
    StringView name = node->data.ident.name;
    fprintf(cg->out, "%.*s", (int)name.len, name.data);
}

/* ──────────────────────────────────────────────
 * Check if an expression produces a string value
 * ────────────────────────────────────────────── */
static int is_string_expr(AstNode *node) {
    if (!node) return 0;
    if (node->type == NODE_LITERAL_STRING) return 1;
    if (node->type == NODE_BINARY_OP && node->data.binary.op == BIN_CONCAT) return 1;
    if (node->type == NODE_IDENT) {
        AstNode *decl = node->data.ident.resolved;
        if (!decl) return 0;
        AstNode *type_node = NULL;
        if (decl->type == NODE_LET) type_node = decl->data.let_decl.type;
        else if (decl->type == NODE_PARAM) type_node = decl->data.param.type;
        if (type_node) {
            if (type_node->type == NODE_TYPE_PRIMITIVE) {
                return type_node->data.type_node.prim == PRIM_STRING;
            }
            if (type_node->type == NODE_TYPE_OPTIONAL) {
                AstNode *elem = type_node->data.type_node.elem_type;
                if (elem && elem->type == NODE_TYPE_PRIMITIVE) {
                    return elem->data.type_node.prim == PRIM_STRING;
                }
            }
        }
    }
    return 0;
}

static void c_emit_binary_op(CCodegen *cg, AstNode *node) {
    /* BIN_ADD is overloaded: numeric add vs string concat. */
    if (node->data.binary.op == BIN_ADD) {
        AstNode *left = node->data.binary.left;
        AstNode *right = node->data.binary.right;
        int left_str = is_string_expr(left);
        int right_str = is_string_expr(right);
        if (left_str || right_str) {
            fputs("__aether_concat(", cg->out);
            c_emit_expr(cg, left);
            fputs(", ", cg->out);
            c_emit_expr(cg, right);
            fputc(')', cg->out);
            return;
        }
    }

    /* BIN_CONCAT is always string concat — auto-convert numerics with itoa */
    if (node->data.binary.op == BIN_CONCAT) {
        fputs("__aether_concat(", cg->out);
        AstNode *left = node->data.binary.left;
        AstNode *right = node->data.binary.right;
        /* Wrap non-string operands with __aether_itoa */
        int left_is_str = is_string_expr(left);
        int right_is_str = is_string_expr(right);
        if (!left_is_str) fputs("__aether_itoa(", cg->out);
        c_emit_expr(cg, left);
        if (!left_is_str) fputc(')', cg->out);
        fputs(", ", cg->out);
        if (!right_is_str) fputs("__aether_itoa(", cg->out);
        c_emit_expr(cg, right);
        if (!right_is_str) fputc(')', cg->out);
        fputc(')', cg->out);
        return;
    }

    /* BIN_OR_ELSE: short-circuit logical OR (a or b → a || b) */
    if (node->data.binary.op == BIN_OR_ELSE) {
        fputc('(', cg->out);
        c_emit_expr(cg, node->data.binary.left);
        fputs(" || ", cg->out);
        c_emit_expr(cg, node->data.binary.right);
        fputc(')', cg->out);
        return;
    }

    fputc('(', cg->out);
    c_emit_expr(cg, node->data.binary.left);
    switch (node->data.binary.op) {
        case BIN_ADD: fputs(" + ", cg->out); break;
        case BIN_SUB: fputs(" - ", cg->out); break;
        case BIN_MUL: fputs(" * ", cg->out); break;
        case BIN_DIV: fputs(" / ", cg->out); break;
        case BIN_MOD: fputs(" % ", cg->out); break;
        case BIN_EQ:  fputs(" == ", cg->out); break;
        case BIN_NEQ: fputs(" != ", cg->out); break;
        case BIN_LT:  fputs(" < ", cg->out); break;
        case BIN_GT:  fputs(" > ", cg->out); break;
        case BIN_LE:  fputs(" <= ", cg->out); break;
        case BIN_GE:  fputs(" >= ", cg->out); break;
        case BIN_BIT_AND: fputs(" & ", cg->out); break;
        case BIN_BIT_OR:  fputs(" | ", cg->out); break;
        case BIN_BIT_XOR: fputs(" ^ ", cg->out); break;
        case BIN_SHL: fputs(" << ", cg->out); break;
        case BIN_SHR: fputs(" >> ", cg->out); break;
        case BIN_AND: fputs(" && ", cg->out); break;
        case BIN_OR:  fputs(" || ", cg->out); break;
        case BIN_ASSIGN: fputs(" = ", cg->out); break;
        case BIN_ADD_ASSIGN: fputs(" += ", cg->out); break;
        case BIN_SUB_ASSIGN: fputs(" -= ", cg->out); break;
        case BIN_MUL_ASSIGN: fputs(" *= ", cg->out); break;
        case BIN_DIV_ASSIGN: fputs(" /= ", cg->out); break;
        default:
            fputs(" + ", cg->out);
            break;
    }
    c_emit_expr(cg, node->data.binary.right);
    fputc(')', cg->out);
}

static void c_emit_unary_op(CCodegen *cg, AstNode *node) {
    switch (node->data.unary.op) {
        case UNARY_NEG:
            fputs("(-(", cg->out);
            c_emit_expr(cg, node->data.unary.operand);
            fputs("))", cg->out);
            break;
        case UNARY_NOT:
            fputs("(!(", cg->out);
            c_emit_expr(cg, node->data.unary.operand);
            fputs("))", cg->out);
            break;
        case UNARY_BIT_NOT:
            fputs("(~(", cg->out);
            c_emit_expr(cg, node->data.unary.operand);
            fputs("))", cg->out);
            break;
        case UNARY_INC:
            fputs("((", cg->out);
            c_emit_expr(cg, node->data.unary.operand);
            fputs(")++)", cg->out);
            break;
        case UNARY_DEC:
            fputs("((", cg->out);
            c_emit_expr(cg, node->data.unary.operand);
            fputs(")--", cg->out);
            break;
        case UNARY_ARRAY_LEN:
            /* Array length: stored in first 8 bytes of header */
            fputs("(*((uint64_t*)(", cg->out);
            c_emit_expr(cg, node->data.unary.operand);
            fputs(")))", cg->out);
            break;
        case UNARY_REF:
            fputs("(&(", cg->out);
            c_emit_expr(cg, node->data.unary.operand);
            fputs("))", cg->out);
            break;
        case UNARY_DEREF:
            fputs("(*(", cg->out);
            c_emit_expr(cg, node->data.unary.operand);
            fputs("))", cg->out);
            break;
        default:
            c_emit_expr(cg, node->data.unary.operand);
            break;
    }
}

static void c_emit_call(CCodegen *cg, AstNode *node) {
    if (!node->data.call.callee || node->data.call.callee->type != NODE_IDENT) {
        fprintf(stderr, "C: call with non-ident callee not yet supported\n");
        return;
    }

    StringView fname = node->data.call.callee->data.ident.name;
    fprintf(cg->out, "%.*s(", (int)fname.len, fname.data);

    /* Look up the function declaration to check param types */
    AstNode *func_decl = NULL;
    if (node->data.call.callee->data.ident.resolved) {
        func_decl = node->data.call.callee->data.ident.resolved;
    }

    for (int i = 0; i < node->data.call.args.count; i++) {
        if (i > 0) fputs(", ", cg->out);
        c_emit_expr(cg, node->data.call.args.items[i]);
    }
    fputc(')', cg->out);
}

static void c_emit_index(CCodegen *cg, AstNode *node) {
    c_emit_expr(cg, node->data.index.target);
    fputc('[', cg->out);
    c_emit_expr(cg, node->data.index.index);
    fputc(']', cg->out);
}

static void c_emit_field_access(CCodegen *cg, AstNode *node) {
    c_emit_expr(cg, node->data.field.target);
    fputc('.', cg->out);
    StringView field_name = node->data.field.field->data.ident.name;
    fprintf(cg->out, "%.*s", (int)field_name.len, field_name.data);
}

/* ──────────────────────────────────────────────
 * Main expression dispatcher
 * ────────────────────────────────────────────── */
void c_emit_expr(CCodegen *cg, AstNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_LITERAL_INT:    c_emit_literal_int(cg, node); break;
        case NODE_LITERAL_FLOAT:  c_emit_literal_float(cg, node); break;
        case NODE_LITERAL_STRING: c_emit_literal_string(cg, node); break;
        case NODE_LITERAL_BOOL:   c_emit_literal_bool(cg, node); break;
        case NODE_LITERAL_CHAR:   c_emit_literal_char(cg, node); break;
        case NODE_LITERAL_NONE:   fputs("{ 0, { 0 } }", cg->out); break;
        case NODE_IDENT:          c_emit_ident(cg, node); break;
        case NODE_BINARY_OP:      c_emit_binary_op(cg, node); break;
        case NODE_UNARY_OP:       c_emit_unary_op(cg, node); break;
        case NODE_CALL:           c_emit_call(cg, node); break;
        case NODE_INDEX:          c_emit_index(cg, node); break;
        case NODE_FIELD_ACCESS:   c_emit_field_access(cg, node); break;
        default:
            fprintf(stderr, "C: unhandled expression node type %d\n", node->type);
            break;
    }
}
