#include "aether/c_transpiler.h"
#include "aether/str.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Forward declarations for functions defined in other C modules */
void c_emit_try(CCodegen *cg, AstNode *node);
void c_emit_throw(CCodegen *cg, AstNode *node);

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
        if (value_node->type == NODE_BINARY_OP) {
            if (value_node->data.binary.op == BIN_CONCAT) is_str = 1;
            if (value_node->data.binary.op == BIN_ADD) {
                /* Check if either operand is a string */
                AstNode *l = value_node->data.binary.left;
                AstNode *r = value_node->data.binary.right;
                if ((l && l->type == NODE_LITERAL_STRING) ||
                    (r && r->type == NODE_LITERAL_STRING)) is_str = 1;
            }
        }
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
    /* if let pattern = expr { body } — check optional tag, bind value */
    if (node->data.if_node.is_if_let) {
        c_indent(cg);
        fputs("{\n", cg->out);
        cg->indent++;
        c_indent(cg);
        fputs("uint64_t __iflet_val = ", cg->out);
        c_emit_expr(cg, node->data.if_node.condition);
        fputs(";\n", cg->out);
        c_indent(cg);
        fputs("if ((__iflet_val & 0xFF) != 0) {\n", cg->out);
        cg->indent++;
        /* Bind the pattern variable to the extracted value */
        if (node->data.if_node.if_let_pattern) {
            StringView pname = node->data.if_node.if_let_pattern->data.ident.name;
            c_indent(cg);
            fprintf(cg->out, "uint64_t %.*s = __iflet_val >> 8;\n", (int)pname.len, pname.data);
        }
        c_emit_block(cg, node->data.if_node.then_block);
        cg->indent--;
        c_indent(cg);
        fputs("}\n", cg->out);
        cg->indent--;
        c_indent(cg);
        fputs("}\n", cg->out);
        return;
    }

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
    AstNode *var = node->data.for_node.var;
    AstNode *iterable = node->data.for_node.iterable;
    AstNode *index_var = node->data.for_node.index_var;

    if (!iterable) {
        c_indent(cg);
        fputs("// for loop (no iterable)\n", cg->out);
        c_emit_block(cg, node->data.for_node.body);
        return;
    }

    /* Check if iterable is a range: start..end */
    if (iterable->type == NODE_BINARY_OP &&
        (iterable->data.binary.op == BIN_RANGE ||
         iterable->data.binary.op == BIN_RANGE_INCLUSIVE)) {
        int lbl_start = cg->label_counter++;
        int lbl_end = cg->label_counter++;
        c_indent(cg);
        fputs("{\n", cg->out);
        cg->indent++;
        c_indent(cg);
        fputs("uint64_t __range_start = ", cg->out);
        c_emit_expr(cg, iterable->data.binary.left);
        fputs(";\n", cg->out);
        c_indent(cg);
        fputs("uint64_t __range_end = ", cg->out);
        c_emit_expr(cg, iterable->data.binary.right);
        fputs(";\n", cg->out);
        c_indent(cg);
        fprintf(cg->out, "uint64_t __i_%d = __range_start;\n", lbl_start);
        c_indent(cg);
        fprintf(cg->out, "while (__i_%d < __range_end) {\n", lbl_start);
        cg->indent++;
        if (var) {
            StringView vname = var->data.ident.name;
            c_indent(cg);
            fprintf(cg->out, "uint64_t %.*s = __i_%d;\n", (int)vname.len, vname.data, lbl_start);
        }
        if (index_var) {
            StringView iname = index_var->data.ident.name;
            c_indent(cg);
            fprintf(cg->out, "uint64_t %.*s = __i_%d;\n", (int)iname.len, iname.data, lbl_start);
        }
        c_emit_block(cg, node->data.for_node.body);
        cg->indent--;
        c_indent(cg);
        fprintf(cg->out, "__i_%d++;\n", lbl_start);
        c_indent(cg);
        fputs("}\n", cg->out);
        cg->indent--;
        c_indent(cg);
        fputs("}\n", cg->out);
        return;
    }

    /* Array/slice iteration */
    c_indent(cg);
    fputs("{\n", cg->out);
    cg->indent++;
    c_indent(cg);
    fputs("slice __arr = ", cg->out);
    c_emit_expr(cg, iterable);
    fputs(";\n", cg->out);
    c_indent(cg);
    fputs("for (uint64_t __i = 0; __i < __arr.len; __i++) {\n", cg->out);
    cg->indent++;
    if (var) {
        StringView vname = var->data.ident.name;
        c_indent(cg);
        fprintf(cg->out, "uint64_t %.*s = ((uint64_t*)__arr.data)[__i];\n", (int)vname.len, vname.data);
    }
    if (index_var) {
        StringView iname = index_var->data.ident.name;
        c_indent(cg);
        fprintf(cg->out, "uint64_t %.*s = __i;\n", (int)iname.len, iname.data);
    }
    c_emit_block(cg, node->data.for_node.body);
    cg->indent--;
    c_indent(cg);
    fputs("}\n", cg->out);
    cg->indent--;
    c_indent(cg);
    fputs("}\n", cg->out);
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
    /* NODE_EXPR_STMT stores the expression in data.call.callee (via node_expr_stmt) */
    if (node->data.call.callee) {
        c_emit_expr(cg, node->data.call.callee);
    }
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
        case NODE_ASSIGN: {
            /* Direct assignment: target = value */
            c_indent(cg);
            c_emit_expr(cg, node->data.binary.left);
            fputs(" = ", cg->out);
            c_emit_expr(cg, node->data.binary.right);
            fputs(";\n", cg->out);
            break;
        }
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
        case NODE_ASM_BLOCK:
            /* Skip asm blocks in C transpiler — they're NASM-specific.
               The stdlib files with asm blocks are compiled via NASM path. */
            break;
        case NODE_TRY:
            c_emit_try(cg, node);
            break;
        case NODE_THROW:
            c_emit_throw(cg, node);
            break;
        case NODE_CATCH_ARM:
            /* Catch arms are handled inside c_emit_try — skip here */
            break;
        case NODE_MODULE_DECL: {
            /* Emit module body as a block comment, then emit the body declarations */
            StringView mname = node->data.module_decl.name->data.ident.name;
            c_indent(cg);
            fprintf(cg->out, "// module %.*s {\n", (int)mname.len, mname.data);
            cg->indent++;
            for (int i = 0; i < node->data.module_decl.body.count; i++) {
                c_emit_stmt(cg, node->data.module_decl.body.items[i]);
            }
            cg->indent--;
            c_indent(cg);
            fputs("// }\n", cg->out);
            break;
        }
        case NODE_TYPE_ALIAS: {
            /* Type alias: typedef <underlying_type> <name>; */
            /* AST stores name as list.items[0], underlying type as list.items[1] */
            if (node->data.list.count >= 2) {
                c_indent(cg);
                fputs("typedef ", cg->out);
                c_emit_type(cg, node->data.list.items[1]);
                fputc(' ', cg->out);
                /* Emit the name (ident node) */
                StringView aname = node->data.list.items[0]->data.ident.name;
                fprintf(cg->out, "%.*s", (int)aname.len, aname.data);
                fputs(";\n", cg->out);
            }
            break;
        }
        case NODE_TRAIT_DECL: {
            /* Traits are compile-time only — emit as comment */
            StringView tname = node->data.trait_decl.name->data.ident.name;
            c_indent(cg);
            fprintf(cg->out, "// trait %.*s { ... }\n", (int)tname.len, tname.data);
            break;
        }
        case NODE_IMPL_BLOCK: {
            /* Impl blocks are compile-time only — emit as comment */
            StringView tn = node->data.impl_block.trait_name;
            StringView tyn = node->data.impl_block.type_name;
            c_indent(cg);
            fprintf(cg->out, "// impl %.*s for %.*s { ... }\n",
                    (int)tn.len, tn.data,
                    (int)tyn.len, tyn.data);
            break;
        }
        case NODE_POOL_DECL: {
            /* Pool declarations are compile-time only — emit as comment */
            StringView pname = node->data.pool_decl.name->data.ident.name;
            c_indent(cg);
            fprintf(cg->out, "// pool %.*s of size %llu, count %llu\n",
                    (int)pname.len, pname.data,
                    (unsigned long long)node->data.pool_decl.size,
                    (unsigned long long)node->data.pool_decl.count);
            break;
        }
        case NODE_PROTOCOL_DECL: {
            /* Protocol declarations are compile-time only — emit as comment */
            StringView pname = node->data.protocol_decl.name->data.ident.name;
            c_indent(cg);
            fprintf(cg->out, "// protocol %.*s { ... }\n", (int)pname.len, pname.data);
            break;
        }
        case NODE_UNSAFE: {
            /* Unsafe is just a block in C — emit the body */
            /* Body is stored in data.list (first item is the body block) */
            if (node->data.list.count > 0 && node->data.list.items[0]) {
                c_emit_block(cg, node->data.list.items[0]);
            }
            break;
        }
        case NODE_REGION: {
            /* Region is just a block in C — emit the body */
            if (node->data.region_node.body) {
                c_emit_block(cg, node->data.region_node.body);
            }
            break;
        }
        case NODE_RUN_BLOCK:
            /* Compile-time execution — skip entirely */
            break;
        case NODE_ATTR:
            /* Attributes are compile-time metadata — skip entirely */
            break;
        case NODE_PROPERTY:
            /* Properties are compile-time — skip with comment */
            c_indent(cg);
            fputs("// property\n", cg->out);
            break;
        default:
            fprintf(stderr, "C: unhandled statement node type %d\n", node->type);
            break;
    }
}
