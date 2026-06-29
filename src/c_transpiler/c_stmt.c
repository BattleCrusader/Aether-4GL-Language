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
    /* Placeholder: emit as while loop */
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
    /* Defer: push body to be emitted at scope exit */
    c_indent(cg);
    fputs("// defer\n", cg->out);
    c_emit_block(cg, node->data.defer_node.body);
}

static void c_emit_expr_stmt(CCodegen *cg, AstNode *node) {
    c_indent(cg);
    c_emit_expr(cg, node->data.call.callee);
    fputs(";\n", cg->out);
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
        default:
            fprintf(stderr, "C: unhandled statement node type %d\n", node->type);
            break;
    }
}
