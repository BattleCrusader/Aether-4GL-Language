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
    /* BIN_ADD with string operand is also string concat */
    if (node->type == NODE_BINARY_OP && node->data.binary.op == BIN_ADD) {
        return is_string_expr(node->data.binary.left) ||
               is_string_expr(node->data.binary.right);
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

static void c_emit_binary_op(CCodegen *cg, AstNode *node) {
    /* BIN_ADD is overloaded: numeric add vs string concat. */
    if (node->data.binary.op == BIN_ADD) {
        AstNode *left = node->data.binary.left;
        AstNode *right = node->data.binary.right;
        int left_str = is_string_expr(left);
        int right_str = is_string_expr(right);
        if (left_str || right_str) {
            fputs("__aether_concat(", cg->out);
            if (!left_str) fputs("__aether_itoa(", cg->out);
            c_emit_expr(cg, left);
            if (!left_str) fputc(')', cg->out);
            fputs(", ", cg->out);
            if (!right_str) fputs("__aether_itoa(", cg->out);
            c_emit_expr(cg, right);
            if (!right_str) fputc(')', cg->out);
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
        case UNARY_ADDR:
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
    } else if (node->data.call.callee->type == NODE_FIELD_ACCESS) {
        /* Handle scoped calls like Option::Some(42) — just emit the value */
        c_emit_expr(cg, node->data.call.args.items[0]);
        return;
    } else {
        fprintf(stderr, "C: call with non-ident callee not yet supported\n");
        return;
    }

    /* Prefix sys function names to avoid C library conflicts */
    if (func_decl && func_decl->data.func.is_sys) {
        fputs("__aether_sys_", cg->out);
    }
    fprintf(cg->out, "%.*s(", (int)fname.len, fname.data);

    /* Emit arguments, filling in defaults for omitted optional params */

    int total_params = 0;
    int varargs_param_idx = -1;
    if (func_decl) {
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
        fputs("(slice){ (uint64_t[]){", cg->out);
        for (int i = 0; i < vararg_count; i++) {
            if (i > 0) fputs(", ", cg->out);
            c_emit_expr(cg, node->data.call.args.items[regular_count + i]);
        }
        fprintf(cg->out, "}, %d }", vararg_count);
        fputs(")", cg->out);
        return;
    }

    for (int i = 0; i < total_params; i++) {
        if (i > 0) fputs(", ", cg->out);
        if (func_decl) (void)func_decl->data.func.params.items[i];
        if (i < (int)node->data.call.args.count) {
            AstNode *arg = node->data.call.args.items[i];
            /* Auto-add & for struct args passed to void* params */
            int needs_ref = 0;
            if (func_decl && i < func_decl->data.func.params.count) {
                AstNode *ptype = func_decl->data.func.params.items[i]->data.param.type;
                if (ptype && (ptype->type == NODE_TYPE_REF || ptype->type == NODE_TYPE_PTR) &&
                    arg->type == NODE_IDENT) {
                    needs_ref = 1;
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
            /* Omitted optional param — fill with NULL */
            fputs("(string){ 0, NULL }", cg->out);
        }
    }
    fputc(')', cg->out);
}

static void c_emit_index(CCodegen *cg, AstNode *node) {
    /* Check if target is a slice type — use ptr[index] access */
    int is_slice = 0;
    if (node->data.index.target && node->data.index.target->type == NODE_IDENT) {
        AstNode *decl = node->data.index.target->data.ident.resolved;
        if (decl) {
            AstNode *type_node = NULL;
            if (decl->type == NODE_LET) type_node = decl->data.let_decl.type;
            else if (decl->type == NODE_PARAM) type_node = decl->data.param.type;
            if (type_node && (type_node->type == NODE_TYPE_ARRAY || type_node->type == NODE_TYPE_SLICE)) {
                is_slice = 1;
            }
        }
    }
    if (is_slice) {
        fputs("((uint64_t*)(", cg->out);
        c_emit_expr(cg, node->data.index.target);
        fputs(".ptr))[", cg->out);
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
    if (node->data.field.target && node->data.field.target->type == NODE_IDENT) {
        AstNode *decl = node->data.field.target->data.ident.resolved;
        if (decl) {
            AstNode *type_node = NULL;
            if (decl->type == NODE_LET) type_node = decl->data.let_decl.type;
            else if (decl->type == NODE_PARAM) type_node = decl->data.param.type;
            if (type_node && (type_node->type == NODE_TYPE_REF || type_node->type == NODE_TYPE_PTR)) {
                is_ptr = 1;
            }
        }
    }
    c_emit_expr(cg, node->data.field.target);
    fputs(is_ptr ? "->" : ".", cg->out);
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
        case NODE_LITERAL_NONE:   fputs("(slice){ NULL, 0 }", cg->out); break;
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
        case NODE_MATCH: {
            /* Match expression: emit as ternary chain.
             * match val { case p1 -> e1; case p2 -> e2; case _ -> e3 }
             * → (val) == (p1) ? (e1) : (val) == (p2) ? (e2) : (e3) */
            AstNode *val = node->data.match_node.value;
            int arms = node->data.match_node.arms.count;
            for (int i = 0; i < arms; i++) {
                AstNode *arm = node->data.match_node.arms.items[i];
                int is_default = (arm->data.match_arm.pattern &&
                    arm->data.match_arm.pattern->type == NODE_IDENT &&
                    arm->data.match_arm.pattern->data.ident.name.len == 1 &&
                    arm->data.match_arm.pattern->data.ident.name.data[0] == '_');
                if (!is_default) {
                    fputs("(", cg->out);
                    c_emit_expr(cg, val);
                    fputs(") == (", cg->out);
                    c_emit_expr(cg, arm->data.match_arm.pattern);
                    fputs(") ? (", cg->out);
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
            /* Array literal: emit as slice compound literal */
            int count = node->data.array_lit.elements.count;
            if (count == 0) {
                fputs("(slice){ NULL, 0 }", cg->out);
            } else {
                fputs("(slice){ (uint64_t[]){", cg->out);
                for (int i = 0; i < count; i++) {
                    if (i > 0) fputs(", ", cg->out);
                    c_emit_expr(cg, node->data.array_lit.elements.items[i]);
                }
                fprintf(cg->out, "}, %d }", count);
            }
            break;
        }
        default:
            fprintf(stderr, "C: unhandled expression node type %d\n", node->type);
            break;
    }
}
