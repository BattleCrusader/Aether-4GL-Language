#include "aether/c_transpiler.h"
#include "aether/str.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────
 * Statement codegen — emit C statements
 * ────────────────────────────────────────────── */

static void c_emit_let(CCodegen *cg, AstNode *node) {
    StringView vname = node->data.let_decl.name->data.ident.name;
    AstNode *type_node = node->data.let_decl.type;
    AstNode *value_node = node->data.let_decl.value;

    c_indent(cg);
    if (type_node) {
        c_emit_type(cg, type_node);
    } else if (value_node) {
        /* Infer type from value expression */
        int is_str = 0;
        if (value_node->type == NODE_LITERAL_STRING) is_str = 1;
        if (value_node->type == NODE_BINARY_OP && value_node->data.binary.op == BIN_CONCAT) is_str = 1;
        if (is_str) {
            fputs("string", cg->out);
        } else if (value_node->type == NODE_LITERAL_FLOAT) {
            fputs("double", cg->out);
        } else if (value_node->type == NODE_LITERAL_BOOL) {
            fputs("uint8_t", cg->out);
        } else if (value_node->type == NODE_LITERAL_CHAR) {
            fputs("uint8_t", cg->out);
        } else {
            fputs("uint64_t", cg->out);
        }
    } else {
        fputs("uint64_t", cg->out);
    }
    fputc(' ', cg->out);
    fprintf(cg->out, "%.*s", (int)vname.len, vname.data);

    if (value_node) {
        fputs(" = ", cg->out);
        c_emit_expr(cg, value_node);
    }
    fputs(";\n", cg->out);
}

static void c_emit_if(CCodegen *cg, AstNode *node) {
    c_indent(cg);
    fputs("if (", cg->out);
    c_emit_expr(cg, node->data.if_node.condition);
    fputs(") {\n", cg->out);
    cg->indent++;
    c_emit_block(cg, node->data.if_node.then_block);
    cg->indent--;
    c_indent(cg);
    fputs("}", cg->out);

    /* Elif chain */
    AstNode *elif = node->data.if_node.elif_chain;
    while (elif) {
        fputs(" else if (", cg->out);
        c_emit_expr(cg, elif->data.if_node.condition);
        fputs(") {\n", cg->out);
        cg->indent++;
        c_emit_block(cg, elif->data.if_node.then_block);
        cg->indent--;
        c_indent(cg);
        fputs("}", cg->out);
        elif = elif->data.if_node.elif_chain;
    }

    /* Else block */
    if (node->data.if_node.else_block) {
        fputs(" else {\n", cg->out);
        cg->indent++;
        c_emit_block(cg, node->data.if_node.else_block);
        cg->indent--;
        c_indent(cg);
        fputs("}", cg->out);
    }
    fputs("\n", cg->out);
}

static void c_emit_while(CCodegen *cg, AstNode *node) {
    c_indent(cg);
    fputs("while (", cg->out);
    c_emit_expr(cg, node->data.while_node.condition);
    fputs(") {\n", cg->out);
    cg->indent++;
    c_emit_block(cg, node->data.while_node.body);
    cg->indent--;
    c_indent(cg);
    fputs("}\n", cg->out);
}

static void c_emit_for(CCodegen *cg, AstNode *node) {
    /* For loop: for var in range/array */
    c_indent(cg);
    fputs("// for loop\n", cg->out);
    c_emit_block(cg, node->data.for_node.body);
}

static void c_emit_return(CCodegen *cg, AstNode *node) {
    c_indent(cg);
    fputs("return", cg->out);
    if (node->data.return_node.value) {
        fputc(' ', cg->out);
        c_emit_expr(cg, node->data.return_node.value);
    }
    fputs(";\n", cg->out);
}

static void c_emit_break(CCodegen *cg, AstNode *node) {
    (void)node;
    c_indent(cg);
    fputs("break;\n", cg->out);
}

static void c_emit_continue(CCodegen *cg, AstNode *node) {
    (void)node;
    c_indent(cg);
    fputs("continue;\n", cg->out);
}

static void c_emit_defer(CCodegen *cg, AstNode *node) {
    c_indent(cg);
    fputs("// defer\n", cg->out);
    c_emit_block(cg, node->data.defer_node.body);
}

static void c_emit_expr_stmt(CCodegen *cg, AstNode *node) {
    c_indent(cg);
    c_emit_expr(cg, node->data.call.callee);
    fputs(";\n", cg->out);
}

static void c_emit_enum_decl(CCodegen *cg, AstNode *node) {
    StringView ename = node->data.enum_decl.name->data.ident.name;
    c_indent(cg);
    fprintf(cg->out, "typedef enum {\n", (int)ename.len, ename.data);
    cg->indent++;
    for (int i = 0; i < node->data.enum_decl.variants.count; i++) {
        AstNode *variant = node->data.enum_decl.variants.items[i];
        StringView vname = variant->data.enum_variant.name->data.ident.name;
        c_indent(cg);
        fprintf(cg->out, "%.*s", (int)vname.len, vname.data);
        if (i < node->data.enum_decl.variants.count - 1) fputs(",", cg->out);
        fputs("\n", cg->out);
    }
    cg->indent--;
    c_indent(cg);
    fprintf(cg->out, "} %.*s;\n\n", (int)ename.len, ename.data);
}

static void c_emit_struct_decl(CCodegen *cg, AstNode *node) {
    StringView sname = node->data.struct_decl.name->data.ident.name;
    c_indent(cg);
    fprintf(cg->out, "typedef struct {\n");
    cg->indent++;
    for (int i = 0; i < node->data.struct_decl.fields.count; i++) {
        AstNode *field = node->data.struct_decl.fields.items[i];
        AstNode *ftype = field->data.param.type;
        StringView fname = field->data.param.name->data.ident.name;
        c_indent(cg);
        if (ftype) c_emit_type(cg, ftype);
        else fputs("uint64_t", cg->out);
        fprintf(cg->out, " %.*s;\n", (int)fname.len, fname.data);
    }
    cg->indent--;
    c_indent(cg);
    fprintf(cg->out, "} %.*s;\n\n", (int)sname.len, sname.data);
}

/* ──────────────────────────────────────────────
 * Block — emit a list of statements
 * ────────────────────────────────────────────── */
void c_emit_block(CCodegen *cg, AstNode *node) {
    if (!node || node->type != NODE_BLOCK) return;
    for (int i = 0; i < node->data.list.count; i++) {
        c_emit_stmt(cg, node->data.list.items[i]);
    }
}

/* ──────────────────────────────────────────────
 * Main statement dispatcher
 * ────────────────────────────────────────────── */
void c_emit_stmt(CCodegen *cg, AstNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_LET:
            c_emit_let(cg, node);
            break;
        case NODE_CONST_DECL:
            c_emit_let(cg, node);
            break;
        case NODE_IF:
            c_emit_if(cg, node);
            break;
        case NODE_WHILE:
            c_emit_while(cg, node);
            break;
        case NODE_FOR:
            c_emit_for(cg, node);
            break;
        case NODE_RETURN:
            c_emit_return(cg, node);
            break;
        case NODE_BREAK:
            c_emit_break(cg, node);
            break;
        case NODE_CONTINUE:
            c_emit_continue(cg, node);
            break;
        case NODE_DEFER:
            c_emit_defer(cg, node);
            break;
        case NODE_EXPR_STMT:
            c_emit_expr_stmt(cg, node);
            break;
        case NODE_BLOCK:
            c_emit_block(cg, node);
            break;
        case NODE_ENUM_DECL:
            c_emit_enum_decl(cg, node);
            break;
        case NODE_STRUCT_DECL:
        case NODE_CLASS_DECL:
            c_emit_struct_decl(cg, node);
            break;
        case NODE_MATCH: {
            /* Match statement: emit as if/else chain */
            c_indent(cg);
            fputs("{\n", cg->out);
            cg->indent++;
            c_indent(cg);
            fputs("uint64_t __match_val = ", cg->out);
            c_emit_expr(cg, node->data.match_node.value);
            fputs(";\n", cg->out);
            for (int i = 0; i < node->data.match_node.arms.count; i++) {
                AstNode *arm = node->data.match_node.arms.items[i];
                c_indent(cg);
                if (i == 0) {
                    fputs("if (__match_val == ", cg->out);
                } else {
                    fputs("else if (__match_val == ", cg->out);
                }
                if (arm->data.match_arm.pattern) {
                    c_emit_expr(cg, arm->data.match_arm.pattern);
                }
                fputs(") {\n", cg->out);
                cg->indent++;
                if (arm->data.match_arm.body) {
                    c_emit_block(cg, arm->data.match_arm.body);
                }
                cg->indent--;
                c_indent(cg);
                fputs("}\n", cg->out);
            }
            cg->indent--;
            c_indent(cg);
            fputs("}\n", cg->out);
            break;
        }
        default:
            fprintf(stderr, "C: unhandled statement node type %d\n", node->type);
            break;
    }
}
