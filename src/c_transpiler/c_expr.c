#include "aether/c_transpiler.h"
#include "aether/str.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* ──────────────────────────────────────────────
 * Expression codegen — emit C expressions
 * ────────────────────────────────────────────── */

/* ──────────────────────────────────────────────
 * Operator overloading dispatch helpers
 * ────────────────────────────────────────────── */

/* Map a binary operator to its symbol string, or NULL if no overload exists */
static const char *binop_to_op_symbol(BinOp op) {
    switch (op) {
        case BIN_ADD: return "+";
        case BIN_SUB: return "-";
        case BIN_MUL: return "*";
        case BIN_DIV: return "/";
        case BIN_MOD: return "%";
        case BIN_EQ:  return "==";
        case BIN_NEQ: return "!=";
        case BIN_LT:  return "<";
        case BIN_GT:  return ">";
        case BIN_LE:  return "<=";
        case BIN_GE:  return ">=";
        case BIN_BIT_AND: return "&";
        case BIN_BIT_OR:  return "|";
        case BIN_BIT_XOR: return "^";
        case BIN_SHL: return "<<";
        case BIN_SHR: return ">>";
        default: return NULL;
    }
}

/* Map a unary operator to its symbol string, or NULL if no overload exists */
static const char *unaryop_to_op_symbol(UnaryOp op) {
    switch (op) {
        case UNARY_NEG: return "-";
        case UNARY_NOT: return "!";
        case UNARY_BIT_NOT: return "~";
        default: return NULL;
    }
}

/* Build the full op_<symbol> name for a binary operator, e.g. "op_+" */
static const char *binop_to_op_name(BinOp op) {
    const char *sym = binop_to_op_symbol(op);
    if (!sym) return NULL;
    static char buf[32];
    snprintf(buf, sizeof(buf), "op_%s", sym);
    return buf;
}

/* Build the full op_<symbol> name for a unary operator, e.g. "op_-" */
static const char *unaryop_to_op_name(UnaryOp op) {
    const char *sym = unaryop_to_op_symbol(op);
    if (!sym) return NULL;
    static char buf[32];
    snprintf(buf, sizeof(buf), "op_%s", sym);
    return buf;
}

/* Compute the same signature hash the parser computed for an operator function.
 * Hash = djb2 over op_<symbol> + param types + return type. */
static uint32_t compute_op_sig_hash(const char *op_name, AstNode *left_expr, AstNode *right_expr) {
    uint32_t hash = 5381;
    size_t nlen = strlen(op_name);
    for (size_t i = 0; i < nlen; i++) {
        hash = (uint32_t)(((hash << 5) + hash) + (unsigned char)op_name[i]);
    }
    /* Hash left operand type */
    if (left_expr && left_expr->type == NODE_IDENT && left_expr->data.ident.resolved) {
        AstNode *decl = left_expr->data.ident.resolved;
        AstNode *type_node = NULL;
        if (decl->type == NODE_LET) type_node = decl->data.let_decl.type;
        else if (decl->type == NODE_PARAM) type_node = decl->data.param.type;
        if (type_node && type_node->type == NODE_TYPE_PRIMITIVE) {
            hash = (uint32_t)(((hash << 5) + hash) + (unsigned char)type_node->data.type_node.prim);
        } else if (type_node && type_node->type == NODE_TYPE_NAMED) {
            for (size_t i = 0; i < type_node->data.type_node.name.len; i++) {
                hash = (uint32_t)(((hash << 5) + hash) + (unsigned char)type_node->data.type_node.name.data[i]);
            }
        } else {
            hash = (uint32_t)(((hash << 5) + hash) + 0xFF);
        }
    } else if (left_expr && left_expr->type == NODE_LITERAL_INT) {
        hash = (uint32_t)(((hash << 5) + hash) + (unsigned char)PRIM_I64);
    } else if (left_expr && left_expr->type == NODE_LITERAL_FLOAT) {
        hash = (uint32_t)(((hash << 5) + hash) + (unsigned char)PRIM_F64);
    } else {
        hash = (uint32_t)(((hash << 5) + hash) + 0xFF);
    }
    /* Hash right operand type */
    if (right_expr && right_expr->type == NODE_IDENT && right_expr->data.ident.resolved) {
        AstNode *decl = right_expr->data.ident.resolved;
        AstNode *type_node = NULL;
        if (decl->type == NODE_LET) type_node = decl->data.let_decl.type;
        else if (decl->type == NODE_PARAM) type_node = decl->data.param.type;
        if (type_node && type_node->type == NODE_TYPE_PRIMITIVE) {
            hash = (uint32_t)(((hash << 5) + hash) + (unsigned char)type_node->data.type_node.prim);
        } else if (type_node && type_node->type == NODE_TYPE_NAMED) {
            for (size_t i = 0; i < type_node->data.type_node.name.len; i++) {
                hash = (uint32_t)(((hash << 5) + hash) + (unsigned char)type_node->data.type_node.name.data[i]);
            }
        } else {
            hash = (uint32_t)(((hash << 5) + hash) + 0xFF);
        }
    } else if (right_expr && right_expr->type == NODE_LITERAL_INT) {
        hash = (uint32_t)(((hash << 5) + hash) + (unsigned char)PRIM_I64);
    } else if (right_expr && right_expr->type == NODE_LITERAL_FLOAT) {
        hash = (uint32_t)(((hash << 5) + hash) + (unsigned char)PRIM_F64);
    } else {
        hash = (uint32_t)(((hash << 5) + hash) + 0xFF);
    }
    return hash;
}

/* Search the program for an operator overload function matching the given name and signature hash.
 * Returns the function node if found, NULL otherwise. */
static AstNode *find_op_func_by_sig(AstNode *program, const char *op_name, uint32_t sig_hash) {
    if (!program || program->type != NODE_PROGRAM) return NULL;
    size_t nlen = strlen(op_name);
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *decl = program->data.list.items[i];
        if (decl->type == NODE_FUNC_DECL && decl->data.func.is_operator &&
            decl->data.func.name && decl->data.func.name->type == NODE_IDENT) {
            StringView dn = decl->data.func.name->data.ident.name;
            if (dn.len == nlen && memcmp(dn.data, op_name, nlen) == 0 &&
                decl->data.func.sig_hash == sig_hash) {
                return decl;
            }
        }
    }
    return NULL;
}

/* Mangle an operator symbol + signature hash into a C-safe function name.
 * e.g. op_+(int,int) → op_plus_1a2b3c4d
 * Returns a pointer to a static buffer. */
static const char *mangle_op_sig(const char *op_name, uint32_t sig_hash) {
    static char buf[64];
    int pos = 0;
    /* Mangle the symbol part */
    pos += snprintf(buf + pos, sizeof(buf) - pos, "op_");
    for (const char *p = op_name + 3; *p && pos < (int)sizeof(buf) - 12; p++) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c) || c == '_') {
            buf[pos++] = (char)c;
        } else if (c >= 128) {
            uint32_t cp = 0;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; if (*(p+1)) { p++; cp = (cp << 6) | ((unsigned char)*p & 0x3F); } }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; if (*(p+1)) { p++; cp = (cp << 6) | ((unsigned char)*p & 0x3F); } if (*(p+1)) { p++; cp = (cp << 6) | ((unsigned char)*p & 0x3F); } }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; if (*(p+1)) { p++; cp = (cp << 6) | ((unsigned char)*p & 0x3F); } if (*(p+1)) { p++; cp = (cp << 6) | ((unsigned char)*p & 0x3F); } if (*(p+1)) { p++; cp = (cp << 6) | ((unsigned char)*p & 0x3F); } }
            pos += snprintf(buf + pos, sizeof(buf) - pos, "u%04X", (unsigned)cp);
        } else if (c == '+') { memcpy(buf + pos, "plus", 4); pos += 4; }
        else if (c == '-') { memcpy(buf + pos, "minus", 5); pos += 5; }
        else if (c == '*') { memcpy(buf + pos, "star", 4); pos += 4; }
        else if (c == '/') { memcpy(buf + pos, "slash", 5); pos += 5; }
        else if (c == '%') { memcpy(buf + pos, "percent", 7); pos += 7; }
        else if (c == '=') { memcpy(buf + pos, "eq", 2); pos += 2; }
        else if (c == '!') { memcpy(buf + pos, "bang", 4); pos += 4; }
        else if (c == '<') { memcpy(buf + pos, "lt", 2); pos += 2; }
        else if (c == '>') { memcpy(buf + pos, "gt", 2); pos += 2; }
        else if (c == '&') { memcpy(buf + pos, "amp", 3); pos += 3; }
        else if (c == '|') { memcpy(buf + pos, "pipe", 4); pos += 4; }
        else if (c == '^') { memcpy(buf + pos, "caret", 5); pos += 5; }
        else if (c == '~') { memcpy(buf + pos, "tilde", 5); pos += 5; }
        else { buf[pos++] = '_'; }
    }
    /* Append hash suffix */
    pos += snprintf(buf + pos, sizeof(buf) - pos, "_%08x", (unsigned)sig_hash);
    buf[pos] = '\0';
    return buf;
}

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
    /* BIN_ADD with string operand is also string concat */
    if (node->type == NODE_BINARY_OP && node->data.binary.op == BIN_ADD) {
        return is_string_expr(node->data.binary.left) ||
               is_string_expr(node->data.binary.right);
    }
    if (node->type == NODE_CALL) {
        /* A call to a function that returns string is a string expression */
        if (node->data.call.callee && node->data.call.callee->type == NODE_IDENT) {
            AstNode *func_decl = node->data.call.callee->data.ident.resolved;
            if (func_decl && func_decl->type == NODE_FUNC_DECL) {
                AstNode *ret_type = func_decl->data.func.return_type;
                if (ret_type && ret_type->type == NODE_TYPE_PRIMITIVE &&
                    ret_type->data.type_node.prim == PRIM_STRING) {
                    return 1;
                }
            }
        }
    }
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
        /* If no type annotation, check the value expression */
        if (!type_node && decl->type == NODE_LET && decl->data.let_decl.value) {
            return is_string_expr(decl->data.let_decl.value);
        }
    }
    return 0;
}

/* Check if an expression produces a slice value */
static int is_slice_expr(AstNode *node) {
    if (!node) return 0;
    if (node->type == NODE_ARRAY_LIT) return 1;
    if (node->type == NODE_LITERAL_NONE) return 1;
    if (node->type == NODE_IDENT) {
        AstNode *decl = node->data.ident.resolved;
        if (!decl) return 0;
        AstNode *type_node = NULL;
        if (decl->type == NODE_LET) type_node = decl->data.let_decl.type;
        else if (decl->type == NODE_PARAM) type_node = decl->data.param.type;
        if (type_node) {
            if (type_node->type == NODE_TYPE_ARRAY || type_node->type == NODE_TYPE_SLICE) return 1;
        }
    }
    if (node->type == NODE_BINARY_OP && node->data.binary.op == BIN_ADD) {
        return is_slice_expr(node->data.binary.left) || is_slice_expr(node->data.binary.right);
    }
    return 0;
}

/* Check if an expression produces a byte/char value */
static int is_byte_expr(AstNode *node) {
    if (!node) return 0;
    if (node->type == NODE_LITERAL_CHAR) return 1;
    if (node->type == NODE_IDENT) {
        AstNode *decl = node->data.ident.resolved;
        if (!decl) return 0;
        AstNode *type_node = NULL;
        if (decl->type == NODE_LET) type_node = decl->data.let_decl.type;
        else if (decl->type == NODE_PARAM) type_node = decl->data.param.type;
        if (type_node && type_node->type == NODE_TYPE_PRIMITIVE) {
            int p = type_node->data.type_node.prim;
            return p == PRIM_BYTE || p == PRIM_U8 || p == PRIM_I8;
        }
    }
    /* Index into a string produces a byte — only when target is a string, not a slice of strings */
    if (node->type == NODE_INDEX) {
        AstNode *target = node->data.index.target;
        if (target && target->type == NODE_IDENT) {
            AstNode *decl = target->data.ident.resolved;
            if (decl) {
                AstNode *type_node = NULL;
                if (decl->type == NODE_LET) type_node = decl->data.let_decl.type;
                else if (decl->type == NODE_PARAM) type_node = decl->data.param.type;
                if (type_node && type_node->type == NODE_TYPE_PRIMITIVE &&
                    type_node->data.type_node.prim == PRIM_STRING) {
                    return 1;
                }
            }
        }
        /* Also handle chained indexing: strings[i][j] — the outer index returns a string */
        if (target && target->type == NODE_INDEX) {
            /* Indexing into a slice of strings produces a string, then [j] produces a byte */
            return 1;
        }
    }
    return 0;
}

/* Look up a variable's type from the program AST (fallback when resolved is NULL) */
static int is_var_string_type(CCodegen *cg, StringView vname) {
    if (!cg->program) return 0;
    for (int di = 0; di < cg->program->data.list.count; di++) {
        AstNode *d = cg->program->data.list.items[di];
        if (d->type == NODE_LET && d->data.let_decl.name &&
            d->data.let_decl.name->type == NODE_IDENT) {
            StringView dn = d->data.let_decl.name->data.ident.name;
            if (dn.len == vname.len && memcmp(dn.data, vname.data, dn.len) == 0) {
                AstNode *val = d->data.let_decl.value;
                if (val && val->type == NODE_BINARY_OP &&
                    (val->data.binary.op == BIN_CONCAT ||
                     (val->data.binary.op == BIN_ADD && is_string_expr(val)))) {
                    return 1;
                }
                break;
            }
        }
    }
    return 0;
}

/* Forward declaration for property setter dispatch */
static bool c_emit_prop_setter_expr(CCodegen *cg, AstNode *node);

static void c_emit_binary_op(CCodegen *cg, AstNode *node) {
    /* String compound assignment: text += expr → text = __aether_concat(text, ...) */
    if (node->data.binary.op == BIN_ADD_ASSIGN) {
        AstNode *left = node->data.binary.left;
        AstNode *right = node->data.binary.right;
        if (is_string_expr(left)) {
            c_emit_expr(cg, left);
            fputs(" = __aether_concat(", cg->out);
            c_emit_expr(cg, left);
            fputs(", ", cg->out);
            if (right && right->type == NODE_IDENT) {
                AstNode *decl = right->data.ident.resolved;
                if (decl) {
                    AstNode *arg_type = NULL;
                    if (decl->type == NODE_PARAM) arg_type = decl->data.param.type;
                    if (decl->type == NODE_LET) arg_type = decl->data.let_decl.type;
                    if (arg_type && arg_type->type == NODE_TYPE_PRIMITIVE &&
                        arg_type->data.type_node.prim == PRIM_BYTE) {
                        /* byte → wrap as (string){1, (char[]){byte, '\0'}} */
                        fputs("(string){ 1, (char[]){(", cg->out);
                        c_emit_expr(cg, right);
                        fputs("), '\\0'} }", cg->out);
                        fputs(")", cg->out);
                        fputs(";\n", cg->out);
                        return;
                    }
                }
            }
            c_emit_expr(cg, right);
            fputs(")", cg->out);
            fputs(";\n", cg->out);
            return;
        }
    }

    /* Property setter dispatch: t.prop = value → propName_setter(&t, value) */
    if (node->data.binary.op == BIN_ASSIGN) {
        if (c_emit_prop_setter_expr(cg, node)) {
            return;
        }
    }

    /* Operator overloading dispatch: look up op_<symbol> by signature hash.
       Works for all types (primitives, structs, etc.) with proper overloading. */
    {
        const char *op_name = binop_to_op_name(node->data.binary.op);
        if (op_name) {
            AstNode *left = node->data.binary.left;
            AstNode *right = node->data.binary.right;
            uint32_t sig_hash = compute_op_sig_hash(op_name, left, right);
            AstNode *op_func = find_op_func_by_sig(cg->program, op_name, sig_hash);
            if (op_func) {
                const char *mangled = mangle_op_sig(op_name, sig_hash);
                fputs(mangled, cg->out);
                fputc('(', cg->out);
                c_emit_expr(cg, left);
                fputs(", ", cg->out);
                c_emit_expr(cg, right);
                fputc(')', cg->out);
                return;
            }
        }
    }

    /* BIN_CUSTOM: custom operator (unicode symbol like ⌛) — emit as op_<symbol>(left, right) */
    if (node->data.binary.op == BIN_CUSTOM) {
        char sym_buf[64];
        size_t sym_len = node->data.binary.custom_op.len;
        if (sym_len > 63) sym_len = 63;
        memcpy(sym_buf, node->data.binary.custom_op.data, sym_len);
        sym_buf[sym_len] = '\0';
        /* Build op_<symbol> name and compute hash */
        char op_name[64];
        snprintf(op_name, sizeof(op_name), "op_%s", sym_buf);
        uint32_t sig_hash = compute_op_sig_hash(op_name, node->data.binary.left, node->data.binary.right);
        const char *mangled = mangle_op_sig(op_name, sig_hash);
        fputs(mangled, cg->out);
        fputc('(', cg->out);
        c_emit_expr(cg, node->data.binary.left);
        fputs(", ", cg->out);
        c_emit_expr(cg, node->data.binary.right);
        fputc(')', cg->out);
        return;
    }

    /* BIN_ADD is overloaded: numeric add vs string concat vs slice concat. */
    if (node->data.binary.op == BIN_ADD) {
        AstNode *left = node->data.binary.left;
        AstNode *right = node->data.binary.right;
        int left_str = is_string_expr(left);
        int right_str = is_string_expr(right);
        if (left_str || right_str) {
            fputs("__aether_concat(", cg->out);
            if (!left_str) {
                if (is_byte_expr(left)) {
                    fputs("(string){ 1, (char[]){(", cg->out);
                    c_emit_expr(cg, left);
                    fputs("), '\\0'} }", cg->out);
                } else {
                    fputs("__aether_itoa(", cg->out);
                    c_emit_expr(cg, left);
                    fputc(')', cg->out);
                }
            } else {
                c_emit_expr(cg, left);
            }
            fputs(", ", cg->out);
            if (!right_str) {
                if (is_byte_expr(right)) {
                    fputs("(string){ 1, (char[]){(", cg->out);
                    c_emit_expr(cg, right);
                    fputs("), '\\0'} }", cg->out);
                } else {
                    fputs("__aether_itoa(", cg->out);
                    c_emit_expr(cg, right);
                    fputc(')', cg->out);
                }
            } else {
                c_emit_expr(cg, right);
            }
            fputc(')', cg->out);
            return;
        }
        /* Check for slice+slice concatenation */
        int left_slice = is_slice_expr(left);
        int right_slice = is_slice_expr(right);
        if (left_slice || right_slice) {
            fputs("__aether_slice_concat(", cg->out);
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
        /* Wrap non-string operands with __aether_itoa or 1-char string for bytes */
        int left_is_str = is_string_expr(left);
        int right_is_str = is_string_expr(right);
        if (!left_is_str) {
            if (is_byte_expr(left)) {
                fputs("(string){ 1, (char[]){(", cg->out);
                c_emit_expr(cg, left);
                fputs("), '\\0'} }", cg->out);
            } else {
                fputs("__aether_itoa(", cg->out);
                c_emit_expr(cg, left);
                fputc(')', cg->out);
            }
        } else {
            c_emit_expr(cg, left);
        }
        fputs(", ", cg->out);
        if (!right_is_str) {
            if (is_byte_expr(right)) {
                fputs("(string){ 1, (char[]){(", cg->out);
                c_emit_expr(cg, right);
                fputs("), '\\0'} }", cg->out);
            } else {
                fputs("__aether_itoa(", cg->out);
                c_emit_expr(cg, right);
                fputc(')', cg->out);
            }
        } else {
            c_emit_expr(cg, right);
        }
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

    /* BIN_ERROR_CONTEXT: throws_call ? "context message"
       Wraps the call in a compound expression that stores the context
       message before propagating the error. */
    if (node->data.binary.op == BIN_ERROR_CONTEXT) {
        /* Emit: ({ __aether_error_msg = context_string; left; }) */
        fputs("({ __aether_error_msg = ", cg->out);
        c_emit_expr(cg, node->data.binary.right);
        fputs("; ", cg->out);
        c_emit_expr(cg, node->data.binary.left);
        fputs("; })", cg->out);
        return;
    }

    /* String comparison needs __aether_string_eq */
    if (node->data.binary.op == BIN_EQ &&
        (is_string_expr(node->data.binary.left) || is_string_expr(node->data.binary.right))) {
        fputs("__aether_string_eq(", cg->out);
        c_emit_expr(cg, node->data.binary.left);
        fputs(", ", cg->out);
        c_emit_expr(cg, node->data.binary.right);
        fputc(')', cg->out);
        return;
    }
    if (node->data.binary.op == BIN_NEQ &&
        (is_string_expr(node->data.binary.left) || is_string_expr(node->data.binary.right))) {
        fputs("!__aether_string_eq(", cg->out);
        c_emit_expr(cg, node->data.binary.left);
        fputs(", ", cg->out);
        c_emit_expr(cg, node->data.binary.right);
        fputs(")", cg->out);
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

/* Property setter dispatch for BIN_ASSIGN expressions (t.prop = value).
 * Returns true if a setter was emitted, false to fall through to normal assignment. */
static bool c_emit_prop_setter_expr(CCodegen *cg, AstNode *node) {
    return false;
}

static void c_emit_unary_op(CCodegen *cg, AstNode *node) {
    /* Operator overloading dispatch: look up op_<symbol> by signature hash. */
    {
        const char *op_name = unaryop_to_op_name(node->data.unary.op);
        if (op_name) {
            AstNode *operand = node->data.unary.operand;
            uint32_t sig_hash = compute_op_sig_hash(op_name, operand, NULL);
            AstNode *op_func = find_op_func_by_sig(cg->program, op_name, sig_hash);
            if (op_func) {
                const char *mangled = mangle_op_sig(op_name, sig_hash);
                fputs(mangled, cg->out);
                fputc('(', cg->out);
                c_emit_expr(cg, operand);
                fputc(')', cg->out);
                return;
            }
        }
    }

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
            /* Array length: stored in first 8 bytes of header.
               For slice types, use .len field instead. */
            {
                /* Check if operand is a slice type or string */
                AstNode *operand = node->data.unary.operand;
                int has_len_field = 0;
                if (operand && operand->type == NODE_IDENT) {
                    AstNode *decl = operand->data.ident.resolved;
                    if (decl) {
                        AstNode *type_node = NULL;
                        if (decl->type == NODE_LET) type_node = decl->data.let_decl.type;
                        else if (decl->type == NODE_PARAM) type_node = decl->data.param.type;
                        if (type_node && (type_node->type == NODE_TYPE_SLICE ||
                            type_node->type == NODE_TYPE_ARRAY ||
                            (type_node->type == NODE_TYPE_PRIMITIVE &&
                             type_node->data.type_node.prim == PRIM_STRING))) {
                            has_len_field = 1;
                        }
                    }
                }
                if (operand && operand->type == NODE_FIELD_ACCESS) {
                    /* Field access: both string and slice have .len */
                    has_len_field = 1;
                }
                if (has_len_field) {
                    c_emit_expr(cg, operand);
                    fputs(".len", cg->out);
                } else {
                    fputs("(*((uint64_t*)(", cg->out);
                    c_emit_expr(cg, operand);
                    fputs(")))", cg->out);
                }
            }
            break;
        case UNARY_REF:
        case UNARY_ADDR:
            fputs("(&(", cg->out);
            c_emit_expr(cg, node->data.unary.operand);
            fputs("))", cg->out);
            break;
        case UNARY_DEREF:
            fputs("(*((uint64_t*)(", cg->out);
            c_emit_expr(cg, node->data.unary.operand);
            fputs(")))", cg->out);
            break;
        case UNARY_HEAP:
        case UNARY_OWNED: {
            /* heap expr / owned expr — allocate on heap, store value, return pointer */
            /* Emit as: ({ uint64_t *__p = __aether_alloc(8); *__p = (expr); (uint64_t)__p; }) */
            int tmp = cg->label_counter++;
            fputs("({ uint64_t *__hp_", cg->out);
            fprintf(cg->out, "%d", tmp);
            fputs(" = (uint64_t*)__aether_alloc(8); *__hp_", cg->out);
            fprintf(cg->out, "%d", tmp);
            fputs(" = (", cg->out);
            c_emit_expr(cg, node->data.unary.operand);
            fputs("); (uint64_t)__hp_", cg->out);
            fprintf(cg->out, "%d", tmp);
            fputs("; })", cg->out);
            break;
        }
        default:
            c_emit_expr(cg, node->data.unary.operand);
            break;
    }
}

static void c_emit_call(CCodegen *cg, AstNode *node) {
    if (!node->data.call.callee) {
        fprintf(stderr, "C: call with NULL callee\n");
        return;
    }

    StringView fname;
    AstNode *func_decl = NULL;
    if (node->data.call.callee->type == NODE_IDENT) {
        fname = node->data.call.callee->data.ident.name;
        if (node->data.call.callee->data.ident.resolved) {
            func_decl = node->data.call.callee->data.ident.resolved;
        }

        /* Built-in functions: sizeof, alignof, offsetof, typeName, panic */
        if (fname.len == 6 && memcmp(fname.data, "sizeof", 6) == 0 && node->data.call.args.count >= 1) {
            fputs("sizeof(", cg->out);
            /* If the argument is an identifier, check if it's a type name */
            AstNode *arg = node->data.call.args.items[0];
            if (arg->type == NODE_IDENT) {
                StringView an = arg->data.ident.name;
                if (sv_eq_cstr(an, "u8") || sv_eq_cstr(an, "i8") || sv_eq_cstr(an, "byte") || sv_eq_cstr(an, "bool")) {
                    fputs("uint8_t", cg->out);
                } else if (sv_eq_cstr(an, "u16") || sv_eq_cstr(an, "i16")) {
                    fputs("uint16_t", cg->out);
                } else if (sv_eq_cstr(an, "u32") || sv_eq_cstr(an, "i32") || sv_eq_cstr(an, "f32") || sv_eq_cstr(an, "float")) {
                    fputs("uint32_t", cg->out);
                } else if (sv_eq_cstr(an, "u64") || sv_eq_cstr(an, "i64") || sv_eq_cstr(an, "f64") || sv_eq_cstr(an, "int") || sv_eq_cstr(an, "double")) {
                    fputs("uint64_t", cg->out);
                } else if (sv_eq_cstr(an, "string")) {
                    fputs("string", cg->out);
                } else if (sv_eq_cstr(an, "void")) {
                    fputs("void", cg->out);
                } else {
                    c_emit_type(cg, arg);
                }
            } else {
                c_emit_type(cg, arg);
            }
            fputc(')', cg->out);
            return;
        }
        if (fname.len == 7 && memcmp(fname.data, "alignof", 7) == 0 && node->data.call.args.count >= 1) {
            fputs("_Alignof(", cg->out);
            AstNode *arg = node->data.call.args.items[0];
            if (arg->type == NODE_IDENT) {
                StringView an = arg->data.ident.name;
                if (sv_eq_cstr(an, "u8") || sv_eq_cstr(an, "i8") || sv_eq_cstr(an, "byte") || sv_eq_cstr(an, "bool")) {
                    fputs("uint8_t", cg->out);
                } else if (sv_eq_cstr(an, "u16") || sv_eq_cstr(an, "i16")) {
                    fputs("uint16_t", cg->out);
                } else if (sv_eq_cstr(an, "u32") || sv_eq_cstr(an, "i32") || sv_eq_cstr(an, "f32") || sv_eq_cstr(an, "float")) {
                    fputs("uint32_t", cg->out);
                } else if (sv_eq_cstr(an, "u64") || sv_eq_cstr(an, "i64") || sv_eq_cstr(an, "f64") || sv_eq_cstr(an, "int") || sv_eq_cstr(an, "double")) {
                    fputs("uint64_t", cg->out);
                } else if (sv_eq_cstr(an, "string")) {
                    fputs("string", cg->out);
                } else {
                    c_emit_type(cg, arg);
                }
            } else {
                c_emit_type(cg, arg);
            }
            fputc(')', cg->out);
            return;
        }
        if (fname.len == 8 && memcmp(fname.data, "offsetof", 8) == 0 && node->data.call.args.count >= 2) {
            fputs("offsetof(", cg->out);
            AstNode *type_arg = node->data.call.args.items[0];
            if (type_arg->type == NODE_IDENT) {
                StringView an = type_arg->data.ident.name;
                fprintf(cg->out, "%.*s", (int)an.len, an.data);
            } else {
                c_emit_type(cg, type_arg);
            }
            fputs(", ", cg->out);
            if (node->data.call.args.items[1]->type == NODE_IDENT) {
                StringView fn = node->data.call.args.items[1]->data.ident.name;
                fprintf(cg->out, "%.*s", (int)fn.len, fn.data);
            }
            fputc(')', cg->out);
            return;
        }
        if (fname.len == 8 && memcmp(fname.data, "typeName", 8) == 0 && node->data.call.args.count >= 1) {
            /* typeName(T) — emit a compile-time string literal with the type name */
            AstNode *arg = node->data.call.args.items[0];
            const char *tn = NULL;
            if (arg->type == NODE_IDENT) {
                StringView an = arg->data.ident.name;
                if (sv_eq_cstr(an, "u8") || sv_eq_cstr(an, "byte")) tn = "u8";
                else if (sv_eq_cstr(an, "i8")) tn = "i8";
                else if (sv_eq_cstr(an, "u16")) tn = "u16";
                else if (sv_eq_cstr(an, "i16")) tn = "i16";
                else if (sv_eq_cstr(an, "u32")) tn = "u32";
                else if (sv_eq_cstr(an, "i32")) tn = "i32";
                else if (sv_eq_cstr(an, "f32")) tn = "f32";
                else if (sv_eq_cstr(an, "u64")) tn = "u64";
                else if (sv_eq_cstr(an, "i64")) tn = "i64";
                else if (sv_eq_cstr(an, "f64")) tn = "f64";
                else if (sv_eq_cstr(an, "bool")) tn = "bool";
                else if (sv_eq_cstr(an, "string")) tn = "string";
                else if (sv_eq_cstr(an, "void")) tn = "void";
                else if (sv_eq_cstr(an, "int")) tn = "int";
                else if (sv_eq_cstr(an, "float")) tn = "float";
                else if (sv_eq_cstr(an, "double")) tn = "double";
                else tn = c_type_name(arg);
            } else {
                tn = c_type_name(arg);
            }
            if (!tn) tn = "unknown";
            fprintf(cg->out, "(string){ %zu, \"%s\" }", strlen(tn), tn);
            return;
        }
        if (fname.len == 5 && memcmp(fname.data, "panic", 5) == 0) {
            /* panic(msg) — emit fprintf(stderr, msg); exit(1); */
            fputs("({ fprintf(stderr, \"%s\\n\", ", cg->out);
            if (node->data.call.args.count >= 1) {
                c_emit_expr(cg, node->data.call.args.items[0]);
            } else {
                fputs("\"panic\"", cg->out);
            }
            fputs(".data); exit(1); 0; })", cg->out);
            return;
        }

        /* Constructor pattern: TypeName(expr) — e.g. string(ch), u8(expr) */
        if (node->data.call.args.count == 1) {
            int is_constructor = 0;
            const char *ctor_type = NULL;
            if (fname.len == 6 && memcmp(fname.data, "string", 6) == 0) {
                is_constructor = 1;
                ctor_type = "string";
            } else if (fname.len == 2 && memcmp(fname.data, "u8", 2) == 0) {
                is_constructor = 1;
                ctor_type = "uint8_t";
            } else if (fname.len == 3 && memcmp(fname.data, "u16", 3) == 0) {
                is_constructor = 1;
                ctor_type = "uint16_t";
            } else if (fname.len == 3 && memcmp(fname.data, "u32", 3) == 0) {
                is_constructor = 1;
                ctor_type = "uint32_t";
            } else if (fname.len == 3 && memcmp(fname.data, "u64", 3) == 0) {
                is_constructor = 1;
                ctor_type = "uint64_t";
            } else if (fname.len == 2 && memcmp(fname.data, "i8", 2) == 0) {
                is_constructor = 1;
                ctor_type = "int8_t";
            } else if (fname.len == 3 && memcmp(fname.data, "i16", 3) == 0) {
                is_constructor = 1;
                ctor_type = "int16_t";
            } else if (fname.len == 3 && memcmp(fname.data, "i32", 3) == 0) {
                is_constructor = 1;
                ctor_type = "int32_t";
            } else if (fname.len == 3 && memcmp(fname.data, "i64", 3) == 0) {
                is_constructor = 1;
                ctor_type = "int64_t";
            } else if (fname.len == 3 && memcmp(fname.data, "f32", 3) == 0) {
                is_constructor = 1;
                ctor_type = "float";
            } else if (fname.len == 3 && memcmp(fname.data, "f64", 3) == 0) {
                is_constructor = 1;
                ctor_type = "double";
            } else if (fname.len == 4 && memcmp(fname.data, "bool", 4) == 0) {
                is_constructor = 1;
                ctor_type = "uint8_t";
            } else if (fname.len == 4 && memcmp(fname.data, "byte", 4) == 0) {
                is_constructor = 1;
                ctor_type = "uint8_t";
            }
            if (is_constructor) {
                if (strcmp(ctor_type, "string") == 0) {
                    /* string(expr) — create a 1-char string from a byte */
                    fputs("(string){ 1, (char[]){(", cg->out);
                    c_emit_expr(cg, node->data.call.args.items[0]);
                    fputs("), '\\0'} }", cg->out);
                } else {
                    /* Numeric constructor: just cast */
                    fputs("((", cg->out);
                    fputs(ctor_type, cg->out);
                    fputs(")(", cg->out);
                    c_emit_expr(cg, node->data.call.args.items[0]);
                    fputs("))", cg->out);
                }
                return;
            }
        }
    } else if (node->data.call.callee->type == NODE_FIELD_ACCESS) {
        /* Handle method calls: target.method(args) → method(&target, args) */
        AstNode *target = node->data.call.callee->data.field.target;
        AstNode *field = node->data.call.callee->data.field.field;
        if (field && field->type == NODE_IDENT) {
            StringView mn = field->data.ident.name;
            /* Disambiguate getter vs setter: if args are present, it's a setter */
            int is_setter = (node->data.call.args.count > 0);
            fprintf(cg->out, "%.*s%s(&", (int)mn.len, mn.data, is_setter ? "_setter" : "_getter");
            c_emit_expr(cg, target);
            for (int ai = 0; ai < node->data.call.args.count; ai++) {
                fputs(", ", cg->out);
                c_emit_expr(cg, node->data.call.args.items[ai]);
            }
            fputc(')', cg->out);
        } else {
            /* Fallback: just emit the first arg (for scoped calls like Option::Some(42)) */
            if (node->data.call.args.count > 0)
                c_emit_expr(cg, node->data.call.args.items[0]);
        }
        return;
    } else {
        fprintf(stderr, "C: call with non-ident callee not yet supported\n");
        return;
    }

    /* For throws function calls, wrap in compound expression for error propagation */
    int is_throws_call = (func_decl && func_decl->type == NODE_FUNC_DECL && func_decl->data.func.is_throws);
    if (is_throws_call) {
        StringView fn = func_decl->data.func.name->data.ident.name;
        fprintf(cg->out, "({ ThrowResult_%.*s __r = ", (int)fn.len, fn.data);
    }

    /* Prefix sys function names to avoid C library conflicts */
    if (func_decl && func_decl->data.func.is_sys) {
        fputs("__aether_sys_", cg->out);
    }
    /* Mangle operator overload function names (op_+ → op_plus_<hash>, etc.) */
    if (fname.len >= 3 && memcmp(fname.data, "op_", 3) == 0) {
        char op_name[64];
        size_t nlen = fname.len;
        if (nlen > 63) nlen = 63;
        memcpy(op_name, fname.data, nlen);
        op_name[nlen] = '\0';
        /* Compute hash from the function's own sig_hash (stored in func_decl) */
        uint32_t sig_hash = 0;
        if (func_decl && func_decl->data.func.is_operator) {
            sig_hash = func_decl->data.func.sig_hash;
        } else {
            sig_hash = compute_op_sig_hash(op_name, NULL, NULL);
        }
        const char *mangled = mangle_op_sig(op_name, sig_hash);
        fputs(mangled, cg->out);
        fputc('(', cg->out);
    } else {
        fprintf(cg->out, "%.*s(", (int)fname.len, fname.data);
    }

    /* Emit arguments, filling in defaults for omitted optional params */

    int total_params = 0;
    int varargs_param_idx = -1;
    if (func_decl && func_decl->type == NODE_FUNC_DECL) {
        total_params = func_decl->data.func.params.count;
        for (int i = 0; i < total_params; i++) {
            if (func_decl->data.func.params.items[i]->data.param.is_varargs) {
                varargs_param_idx = i;
                break;
            }
        }
    }
    if (total_params == 0) {
        total_params = (int)node->data.call.args.count;
    }

    int regular_count = (varargs_param_idx >= 0) ? varargs_param_idx : total_params;
    int vararg_count = (varargs_param_idx >= 0) ? (int)node->data.call.args.count - regular_count : 0;

    /* If variadic, pack extra args into a slice compound literal */
    if (varargs_param_idx >= 0) {
        /* Emit regular args first (function name already emitted above) */
        for (int i = 0; i < regular_count; i++) {
            if (i > 0) fputs(", ", cg->out);
            AstNode *arg = node->data.call.args.items[i];
            if (fname.len == 5 && memcmp(fname.data, "print", 5) == 0 &&
                !is_string_expr(arg)) {
                fputs("__aether_itoa(", cg->out);
                c_emit_expr(cg, arg);
                fputc(')', cg->out);
            } else {
                c_emit_expr(cg, arg);
            }
        }
        if (regular_count > 0) fputs(", ", cg->out);
        /* Pack variadic args into a slice using compound literal */
        fputs("(slice){ (void*)(uint64_t[]){", cg->out);
        for (int i = 0; i < vararg_count; i++) {
            if (i > 0) fputs(", ", cg->out);
            c_emit_expr(cg, node->data.call.args.items[regular_count + i]);
        }
        fprintf(cg->out, "}, %d, 8 }", vararg_count);
        fputs(")", cg->out);
        return;
    }

    for (int i = 0; i < total_params; i++) {
        if (i > 0) fputs(", ", cg->out);
        if (func_decl) (void)func_decl->data.func.params.items[i];
        if (i < (int)node->data.call.args.count) {
            AstNode *arg = node->data.call.args.items[i];
            /* Auto-add & for struct args passed to ref/ptr params */
            int needs_ref = 0;
            if (func_decl && i < func_decl->data.func.params.count) {
                AstNode *ptype = func_decl->data.func.params.items[i]->data.param.type;
                if (ptype && (ptype->type == NODE_TYPE_REF || ptype->type == NODE_TYPE_PTR) &&
                    arg->type == NODE_IDENT) {
                    /* Only add & if the argument is not already a pointer/ref type */
                    int already_ptr = 0;
                    AstNode *arg_decl = arg->data.ident.resolved;
                    if (arg_decl) {
                        AstNode *arg_type = NULL;
                        if (arg_decl->type == NODE_LET) arg_type = arg_decl->data.let_decl.type;
                        else if (arg_decl->type == NODE_PARAM) arg_type = arg_decl->data.param.type;
                        if (arg_type && (arg_type->type == NODE_TYPE_REF || arg_type->type == NODE_TYPE_PTR)) {
                            already_ptr = 1;
                        }
                    }
                    if (!already_ptr) needs_ref = 1;
                }
            }
            if (needs_ref) fputs("&(", cg->out);
            /* Auto-wrap non-string args to print() with __aether_itoa */
            if (fname.len == 5 && memcmp(fname.data, "print", 5) == 0 &&
                !is_string_expr(arg)) {
                fputs("__aether_itoa(", cg->out);
                c_emit_expr(cg, arg);
                fputc(')', cg->out);
            } else {
                c_emit_expr(cg, arg);
            }
            if (needs_ref) fputs(")", cg->out);
        } else {
            /* Omitted optional param — fill with default value if available */
            if (func_decl && i < func_decl->data.func.params.count) {
                AstNode *param = func_decl->data.func.params.items[i];
                if (param->data.param.default_value) {
                    c_emit_expr(cg, param->data.param.default_value);
                } else {
                    fputs("(string){ 0, NULL }", cg->out);
                }
            } else {
                fputs("(string){ 0, NULL }", cg->out);
            }
        }
    }
    fputc(')', cg->out);

    /* Error propagation for throws function calls: close compound expression with error check */
    if (is_throws_call) {
        int has_return = (func_decl->data.func.return_type != NULL);
        if (has_return) {
            fputs("; if (__r.err) { longjmp(__aether_jmp_buf, 1); } __r.val; })", cg->out);
        } else {
            fputs("; if (__r.err) { longjmp(__aether_jmp_buf, 1); } })", cg->out);
        }
    }
}

static void c_emit_index(CCodegen *cg, AstNode *node) {
    /* Check if target is a slice or string type — use .data[index] access */
    int is_slice_or_string = 0;
    AstNode *target = node->data.index.target;
    if (target) {
        AstNode *type_node = NULL;
        if (target->type == NODE_IDENT) {
            AstNode *decl = target->data.ident.resolved;
            if (decl) {
                if (decl->type == NODE_LET) type_node = decl->data.let_decl.type;
                else if (decl->type == NODE_PARAM) type_node = decl->data.param.type;
            }
        } else if (target->type == NODE_FIELD_ACCESS) {
            /* Field access on a struct — the field is a slice or string type.
               Both have .data, so use .data[index] access. */
            is_slice_or_string = 2;
        } else if (target->type == NODE_INDEX) {
            /* Chained indexing: strings[i][j] — the inner index returns a string.
               Use .data[j] access for the outer index. */
            is_slice_or_string = 2;
        }
        if (type_node) {
            if (type_node->type == NODE_TYPE_ARRAY || type_node->type == NODE_TYPE_SLICE) {
                is_slice_or_string = 1;
            }
            if (type_node->type == NODE_TYPE_PRIMITIVE &&
                type_node->data.type_node.prim == PRIM_STRING) {
                is_slice_or_string = 2;
            }
        }
    }
    if (is_slice_or_string == 1) {
        /* Check if target is a plain string — it has no elem_size field */
        int is_plain_string = 0;
        if (target && target->type == NODE_IDENT) {
            AstNode *decl = target->data.ident.resolved;
            if (decl) {
                AstNode *ttype = NULL;
                if (decl->type == NODE_LET) ttype = decl->data.let_decl.type;
                else if (decl->type == NODE_PARAM) ttype = decl->data.param.type;
                if (ttype && ttype->type == NODE_TYPE_PRIMITIVE &&
                    ttype->data.type_node.prim == PRIM_STRING) {
                    is_plain_string = 1;
                }
            }
        }
        if (is_plain_string) {
            c_emit_expr(cg, node->data.index.target);
            fputs(".data[", cg->out);
            c_emit_expr(cg, node->data.index.index);
            fputc(']', cg->out);
            return;
        }
        /* Use elem_size from the slice struct for proper byte-offset indexing.
           Cast to char*, offset by index * elem_size, then cast to uint64_t* for the value.
           For string slices, cast to string* to read the full 16-byte struct. */
        /* Check if this is a [string] slice */
        int is_string_slice = 0;
        if (target && target->type == NODE_IDENT) {
            AstNode *decl = target->data.ident.resolved;
            if (decl) {
                AstNode *type_node = NULL;
                if (decl->type == NODE_LET) type_node = decl->data.let_decl.type;
                else if (decl->type == NODE_PARAM) type_node = decl->data.param.type;
                if (type_node && (type_node->type == NODE_TYPE_ARRAY || type_node->type == NODE_TYPE_SLICE)) {
                    AstNode *elem_type = type_node->data.type_node.elem_type;
                    if (elem_type && elem_type->type == NODE_TYPE_PRIMITIVE &&
                        elem_type->data.type_node.prim == PRIM_STRING) {
                        is_string_slice = 1;
                    }
                }
            }
        }
        if (is_string_slice) {
            fputs("(*((string*)((char*)(", cg->out);
            c_emit_expr(cg, node->data.index.target);
            fputs(".data) + (", cg->out);
            c_emit_expr(cg, node->data.index.index);
            fputs(") * (", cg->out);
            c_emit_expr(cg, node->data.index.target);
            fputs(".elem_size))))", cg->out);
        } else if (target && target->type == NODE_FIELD_ACCESS) {
            /* Field access on a struct — check if the field is a string slice */
            StringView fname = target->data.field.field->data.ident.name;
            if (fname.len == 4 && memcmp(fname.data, "keys", 4) == 0) {
                fputs("(*((string*)((char*)(", cg->out);
                c_emit_expr(cg, node->data.index.target);
                fputs(".data) + (", cg->out);
                c_emit_expr(cg, node->data.index.index);
                fputs(") * (", cg->out);
                c_emit_expr(cg, node->data.index.target);
                fputs(".elem_size))))", cg->out);
            } else if (fname.len == 8 && memcmp(fname.data, "occupied", 8) == 0) {
                fputs("(*((uint8_t*)((char*)(", cg->out);
                c_emit_expr(cg, node->data.index.target);
                fputs(".data) + (", cg->out);
                c_emit_expr(cg, node->data.index.index);
                fputs(") * (", cg->out);
                c_emit_expr(cg, node->data.index.target);
                fputs(".elem_size))))", cg->out);
            } else {
                fputs("(*((uint64_t*)((char*)(", cg->out);
                c_emit_expr(cg, node->data.index.target);
                fputs(".data) + (", cg->out);
                c_emit_expr(cg, node->data.index.index);
                fputs(") * (", cg->out);
                c_emit_expr(cg, node->data.index.target);
                fputs(".elem_size))))", cg->out);
            }
        } else {
            fputs("(*((uint64_t*)((char*)(", cg->out);
            c_emit_expr(cg, node->data.index.target);
            fputs(".data) + (", cg->out);
            c_emit_expr(cg, node->data.index.index);
            fputs(") * (", cg->out);
            c_emit_expr(cg, node->data.index.target);
            fputs(".elem_size))))", cg->out);
        }
    } else if (is_slice_or_string == 2) {
        c_emit_expr(cg, node->data.index.target);
        fputs(".data[", cg->out);
        c_emit_expr(cg, node->data.index.index);
        fputc(']', cg->out);
    } else {
        c_emit_expr(cg, node->data.index.target);
        fputc('[', cg->out);
        c_emit_expr(cg, node->data.index.index);
        fputc(']', cg->out);
    }
}

static void c_emit_field_access(CCodegen *cg, AstNode *node) {
    /* Check if target is a pointer type — use -> instead of . */
    int is_ptr = 0;
    int is_enum_variant = 0;
    if (node->data.field.target && node->data.field.target->type == NODE_IDENT) {
        AstNode *decl = node->data.field.target->data.ident.resolved;
        if (decl) {
            AstNode *type_node = NULL;
            if (decl->type == NODE_LET) type_node = decl->data.let_decl.type;
            else if (decl->type == NODE_PARAM) type_node = decl->data.param.type;
            if (type_node && (type_node->type == NODE_TYPE_REF || type_node->type == NODE_TYPE_PTR)) {
                is_ptr = 1;
            }
            /* Enum variant access: MyError::NotFound → just emit NotFound */
            if (decl->type == NODE_ENUM_DECL) {
                is_enum_variant = 1;
            }
        }
    }
    if (is_enum_variant) {
        /* Just emit the variant name directly — C enums don't use type prefix */
        StringView field_name = node->data.field.field->data.ident.name;
        fprintf(cg->out, "%.*s", (int)field_name.len, field_name.data);
    } else {
        c_emit_expr(cg, node->data.field.target);
        fputs(is_ptr ? "->" : ".", cg->out);
        StringView field_name = node->data.field.field->data.ident.name;
        fprintf(cg->out, "%.*s", (int)field_name.len, field_name.data);
    }
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
        case NODE_LITERAL_NONE:   fputs("0", cg->out); break;
        case NODE_IDENT: {
            /* Check if this is a const declaration — evaluate the const value */
            AstNode *decl = node->data.ident.resolved;
            if (decl && decl->type == NODE_CONST_DECL && decl->data.let_decl.value) {
                c_emit_expr(cg, decl->data.let_decl.value);
            } else {
                c_emit_ident(cg, node);
            }
            break;
        }
        case NODE_BINARY_OP:      c_emit_binary_op(cg, node); break;
        case NODE_UNARY_OP:       c_emit_unary_op(cg, node); break;
        case NODE_CALL:           c_emit_call(cg, node); break;
        case NODE_INDEX:          c_emit_index(cg, node); break;
        case NODE_FIELD_ACCESS:   c_emit_field_access(cg, node); break;
        case NODE_CAST: {
            /* Cast: (target_type)expr */
            fputs("((", cg->out);
            c_emit_type(cg, node->data.binary.right);
            fputs(")(", cg->out);
            c_emit_expr(cg, node->data.binary.left);
            fputs("))", cg->out);
            break;
        }
        case NODE_TERNARY: {
            /* Ternary: cond ? then_val : else_val
             * data.list: items[0]=cond, items[1]=then_val, items[2]=else_val */
            fputc('(', cg->out);
            if (node->data.list.count >= 1) c_emit_expr(cg, node->data.list.items[0]);
            fputs(" ? ", cg->out);
            if (node->data.list.count >= 2) c_emit_expr(cg, node->data.list.items[1]);
            fputs(" : ", cg->out);
            if (node->data.list.count >= 3) c_emit_expr(cg, node->data.list.items[2]);
            fputc(')', cg->out);
            break;
        }
        case NODE_MATCH: {
            /* Match expression: emit as ternary chain.
             * match val { case p1 -> e1; case p2 -> e2; case _ -> e3 }
             * → (val) == (p1) ? (e1) : (val) == (p2) ? (e2) : (e3) */
            AstNode *val = node->data.match_node.value;
            int arms = node->data.match_node.arms.count;
            for (int i = 0; i < arms; i++) {
                AstNode *arm = node->data.match_node.arms.items[i];
                AstNode *pat = arm->data.match_arm.pattern;
                int is_default = (pat && pat->type == NODE_IDENT &&
                    pat->data.ident.name.len == 1 &&
                    pat->data.ident.name.data[0] == '_');
                if (!is_default) {
                    fputs("(", cg->out);
                    /* Range pattern: emit as >= && <= */
                    if (pat && pat->type == NODE_BINARY_OP &&
                        (pat->data.binary.op == BIN_RANGE ||
                         pat->data.binary.op == BIN_RANGE_INCLUSIVE)) {
                        c_emit_expr(cg, val);
                        fputs(") >= (", cg->out);
                        c_emit_expr(cg, pat->data.binary.left);
                        fputs(") && (", cg->out);
                        c_emit_expr(cg, val);
                        fputs(") <= (", cg->out);
                        c_emit_expr(cg, pat->data.binary.right);
                        fputs(")", cg->out);
                    } else {
                        c_emit_expr(cg, val);
                        fputs(") == (", cg->out);
                        c_emit_expr(cg, pat);
                        fputs(")", cg->out);
                    }
                    /* Emit additional comma-separated patterns as || */
                    for (int j = 0; j < arm->data.match_arm.patterns.count; j++) {
                        AstNode *extra = arm->data.match_arm.patterns.items[j];
                        if (!extra) continue;
                        fputs(" || (", cg->out);
                        c_emit_expr(cg, val);
                        fputs(") == (", cg->out);
                        c_emit_expr(cg, extra);
                        fputs(")", cg->out);
                    }
                    fputs(" ? (", cg->out);
                    if (arm->data.match_arm.body) c_emit_expr(cg, arm->data.match_arm.body);
                    fputs(")", cg->out);
                    if (i < arms - 1) fputs(" : ", cg->out);
                } else {
                    if (arm->data.match_arm.body) c_emit_expr(cg, arm->data.match_arm.body);
                }
            }
            break;
        }
        case NODE_ARRAY_LIT: {
            /* Array literal: emit as slice compound literal with elem_size */
            int count = node->data.array_lit.elements.count;
            if (count == 0) {                fputs("(slice){ NULL, 0, 8 }", cg->out);            } else {                int es = 8;                int is_str = 0;                const char *elem_type = "uint64_t";                if (count > 0) {                    AstNode *first = node->data.array_lit.elements.items[0];                    if (first && first->type == NODE_LITERAL_STRING) {                        is_str = 1;                    } else if (first && is_string_expr(first)) {                        is_str = 1;                    }                    /* Determine element type for non-string arrays */                    if (!is_str) {                        if (first->type == NODE_LITERAL_INT || first->type == NODE_LITERAL_FLOAT ||                            first->type == NODE_LITERAL_BOOL || first->type == NODE_LITERAL_CHAR) {                            elem_type = "uint64_t";                        } else {                            /* Struct or other complex type - use the type name of the element */                            AstNode *decl = NULL;                            if (first->type == NODE_IDENT) decl = first->data.ident.resolved;                            if (decl && decl->type == NODE_LET) {                                char buf[256];                                const char *t = c_type_name(decl->data.let_decl.type);                                snprintf(buf, sizeof(buf), "%s", t);                                elem_type = strdup(buf);                            } else if (decl && decl->type == NODE_PARAM) {                                char buf[256];                                const char *t = c_type_name(decl->data.param.type);                                snprintf(buf, sizeof(buf), "%s", t);                                elem_type = strdup(buf);                            }                        }                    }                }                if (is_str) es = 16;                fputs("(slice){ (void*)(", cg->out);                fputs(is_str ? "string" : elem_type, cg->out);                fprintf(cg->out, "[]){", count);                for (int i = 0; i < count; i++) {                    if (i > 0) fputs(", ", cg->out);                    c_emit_expr(cg, node->data.array_lit.elements.items[i]);                }                fprintf(cg->out, "}, %d, %d }", count, es);            }            break;        }
        case NODE_LAMBDA: {
            /* Lambda: emit the function name (function pointer) for the
             * pre-collected lambda function. The function itself was already
             * registered in cg->lambda_decls during the pre-pass.
             * Cast to generic function pointer to avoid type mismatch. */
            int lambda_id = (int)(intptr_t)node->data.lambda.captures;
            fprintf(cg->out, "(int64_t(*)())__lambda_%d", lambda_id);
            break;
        }
        case NODE_SLICE: {
            /* Slice expression: a[i..j] — emit the slice struct manually */
            /* data.slice has target, start, end */
            fputs("(slice){ .data = ((char*)(", cg->out);
            c_emit_expr(cg, node->data.slice.target);
            fputs(".data) + (", cg->out);
            if (node->data.slice.start) {
                c_emit_expr(cg, node->data.slice.start);
            } else {
                fputs("0", cg->out);
            }
            fputs(") * (", cg->out);
            c_emit_expr(cg, node->data.slice.target);
            fputs(".elem_size)), .len = (", cg->out);
            if (node->data.slice.end && node->data.slice.start) {
                /* len = end - start */
                fputs("(", cg->out);
                c_emit_expr(cg, node->data.slice.end);
                fputs(") - (", cg->out);
                c_emit_expr(cg, node->data.slice.start);
                fputs(")", cg->out);
            } else if (node->data.slice.end) {
                c_emit_expr(cg, node->data.slice.end);
            } else if (node->data.slice.start) {
                fputs("(", cg->out);
                c_emit_expr(cg, node->data.slice.target);
                fputs(".len) - (", cg->out);
                c_emit_expr(cg, node->data.slice.start);
                fputs(")", cg->out);
            } else {
                c_emit_expr(cg, node->data.slice.target);
                fputs(".len", cg->out);
            }
            fputs("), .elem_size = (", cg->out);
            c_emit_expr(cg, node->data.slice.target);
            fputs(".elem_size) }", cg->out);
            break;
        }
        default:
            fprintf(stderr, "C: unhandled expression node type %d\n", node->type);
            break;
    }
}
