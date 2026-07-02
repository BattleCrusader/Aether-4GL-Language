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
        /* Wrap value in (void*) cast for owned/rc types */
        if (type_node && type_node->type == NODE_TYPE_REF &&
            (type_node->data.type_node.is_owned || type_node->data.type_node.is_rc)) {
            fputs("(void*)(", cg->out);
            c_emit_expr(cg, value_node);
            fputs(")", cg->out);
        } else {
            c_emit_expr(cg, value_node);
        }
    }
    fputs(";\n", cg->out);

    /* Auto-drop tracking for class-typed variables */
    if (type_node && type_node->type == NODE_TYPE_NAMED) {
        StringView tn = type_node->data.type_node.name;
        /* Check if this named type is a class by scanning program declarations */
        for (int i = 0; i < cg->program->data.list.count; i++) {
            AstNode *decl = cg->program->data.list.items[i];
            if (decl->type == NODE_CLASS_DECL && decl->data.struct_decl.name &&
                decl->data.struct_decl.name->type == NODE_IDENT) {
                StringView dn = decl->data.struct_decl.name->data.ident.name;
                if (tn.len == dn.len && memcmp(tn.data, dn.data, tn.len) == 0) {
                    /* Track this variable for auto-drop */
                    struct AutoDropEntry *entry = (struct AutoDropEntry *)arena_alloc(cg->arena, sizeof(struct AutoDropEntry));
                    int vn_len = (int)vname.len;
                    if (vn_len > 63) vn_len = 63;
                    memcpy(entry->var_name, vname.data, vn_len);
                    entry->var_name[vn_len] = '\0';
                    int cn_len = (int)tn.len;
                    if (cn_len > 63) cn_len = 63;
                    memcpy(entry->class_name, tn.data, cn_len);
                    entry->class_name[cn_len] = '\0';
                    entry->kind = 0;  /* class _drop */
                    entry->next = cg->auto_drop_list;
                    cg->auto_drop_list = entry;
                    break;
                }
            }
        }
    }
    /* Auto-cleanup for owned T: free on scope exit */
    if (type_node && type_node->type == NODE_TYPE_REF && type_node->data.type_node.is_owned) {
        struct AutoDropEntry *entry = (struct AutoDropEntry *)arena_alloc(cg->arena, sizeof(struct AutoDropEntry));
        int vn_len = (int)vname.len;
        if (vn_len > 63) vn_len = 63;
        memcpy(entry->var_name, vname.data, vn_len);
        entry->var_name[vn_len] = '\0';
        entry->class_name[0] = '\0';
        entry->kind = 1;  /* owned free */
        entry->next = cg->auto_drop_list;
        cg->auto_drop_list = entry;
    }
    /* Auto-cleanup for rc T: release on scope exit */
    if (type_node && type_node->type == NODE_TYPE_REF && type_node->data.type_node.is_rc) {
        struct AutoDropEntry *entry = (struct AutoDropEntry *)arena_alloc(cg->arena, sizeof(struct AutoDropEntry));
        int vn_len = (int)vname.len;
        if (vn_len > 63) vn_len = 63;
        memcpy(entry->var_name, vname.data, vn_len);
        entry->var_name[vn_len] = '\0';
        entry->class_name[0] = '\0';
        entry->kind = 2;  /* rc release */
        entry->next = cg->auto_drop_list;
        cg->auto_drop_list = entry;
    }
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
    if (cg->current_throws_func) {
        /* In a throws function, return the value wrapped in the error struct */
        StringView fn = cg->current_throws_func->data.func.name->data.ident.name;
        AstNode *ret_type = cg->current_throws_func->data.func.return_type;
        fprintf(cg->out, "ThrowResult_%.*s __ret = {0};\n", (int)fn.len, fn.data);
        c_indent(cg);
        if (node->data.return_node.value) {
            fputs("__ret.val = (", cg->out);
            c_emit_type(cg, ret_type);
            fputs(")(", cg->out);
            c_emit_expr(cg, node->data.return_node.value);
            fputs(");\n", cg->out);
        }
        c_indent(cg);
        fputs("return __ret;\n", cg->out);
    } else {
        fputs("return", cg->out);
        if (node->data.return_node.value) {
            fputc(' ', cg->out);
            c_emit_expr(cg, node->data.return_node.value);
        }
        fputs(";\n", cg->out);
    }
}

static void c_emit_break(CCodegen *cg, AstNode *node) {
    c_indent(cg);
    StringView label = node->data.ident.name;
    if (label.len > 0) {
        /* Labeled break: goto <label>_end */
        fprintf(cg->out, "goto %.*s_end;\n", (int)label.len, label.data);
    } else {
        fputs("break;\n", cg->out);
    }
}

static void c_emit_continue(CCodegen *cg, AstNode *node) {
    c_indent(cg);
    StringView label = node->data.ident.name;
    if (label.len > 0) {
        /* Labeled continue: goto <label>_continue */
        fprintf(cg->out, "goto %.*s_continue;\n", (int)label.len, label.data);
    } else {
        fputs("continue;\n", cg->out);
    }
}

/* ──────────────────────────────────────────────
 * Spawn and yield
 * ────────────────────────────────────────────── */

static void c_emit_spawn(CCodegen *cg, AstNode *node) {
    /* spawn func(args...) → On host: just call func(args) synchronously */
    AstNode *call = node->data.spawn_node.call;
    if (!call || !call->data.call.callee) {
        c_indent(cg);
        fputs("// spawn (invalid)\n", cg->out);
        return;
    }
    c_indent(cg);
    c_emit_expr(cg, call->data.call.callee);
    fputs("(", cg->out);
    for (int i = 0; i < call->data.call.args.count; i++) {
        if (i > 0) fputs(", ", cg->out);
        c_emit_expr(cg, call->data.call.args.items[i]);
    }
    fputs(");\n", cg->out);
}

static void c_emit_yield(CCodegen *cg, AstNode *node) {
    (void)node;
    c_indent(cg);
    fputs("__aether_yield();\n", cg->out);
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

    /* Emit contract check function for debug builds (skipped in release, opt_level >= 2) */
    if (node->data.struct_decl.contracts.count > 0 && cg->opt_level < 2) {
        c_indent(cg);
        fprintf(cg->out, "static void %.*s_check_contracts(%.*s self) {\n",
                (int)sname.len, sname.data, (int)sname.len, sname.data);
        cg->indent++;
        for (int i = 0; i < node->data.struct_decl.contracts.count; i++) {
            c_indent(cg);
            fputs("assert((", cg->out);
            c_emit_expr(cg, node->data.struct_decl.contracts.items[i]);
            fputs(") && \"contract failed\");\n", cg->out);
        }
        cg->indent--;
        c_indent(cg);
        fputs("}\n\n", cg->out);
    }
}

/* ──────────────────────────────────────────────
 * Block — emit a list of statements
 * ────────────────────────────────────────────── */
void c_emit_block(CCodegen *cg, AstNode *node) {
    if (!node || node->type != NODE_BLOCK) return;
    /* Save current auto_drop_list to restore at block exit */
    struct AutoDropEntry *saved_drops = cg->auto_drop_list;
    cg->auto_drop_list = NULL;

    for (int i = 0; i < node->data.list.count; i++) {
        c_emit_stmt(cg, node->data.list.items[i]);
    }

    /* Emit auto-drop calls for class-typed variables declared in this block (LIFO) */
    if (cg->auto_drop_list) {
        /* Reverse the list to get LIFO order (most recently declared first) */
        struct AutoDropEntry *prev = NULL;
        struct AutoDropEntry *curr = cg->auto_drop_list;
        struct AutoDropEntry *next = NULL;
        while (curr) {
            next = curr->next;
            curr->next = prev;
            prev = curr;
            curr = next;
        }
        cg->auto_drop_list = prev;
        /* Emit drop calls */
        for (struct AutoDropEntry *e = cg->auto_drop_list; e; e = e->next) {
            c_indent(cg);
            if (e->kind == 0) {
                /* class _drop */
                fprintf(cg->out, "%s_drop(&%s);\n", e->class_name, e->var_name);
            } else if (e->kind == 1) {
                /* owned T: free the pointer */
                fprintf(cg->out, "__aether_free(%s);\n", e->var_name);
            } else if (e->kind == 2) {
                /* rc T: release */
                fprintf(cg->out, "__aether_rc_release(%s);\n", e->var_name);
            }
        }
    }

    /* Restore parent block's auto_drop_list */
    cg->auto_drop_list = saved_drops;
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
                AstNode *pat = arm->data.match_arm.pattern;
                c_indent(cg);
                if (i == 0) {
                    fputs("if (", cg->out);
                } else {
                    fputs("else if (", cg->out);
                }
                /* Wildcard _ pattern: always true */
                if (pat && pat->type == NODE_IDENT &&
                    sv_eq_cstr(pat->data.ident.name, "_")) {
                    fputs("1", cg->out);
                } else {
                    /* Emit the first pattern */
                    int emitted = 0;
                    if (pat) {
                        /* Range pattern: BIN_RANGE or BIN_RANGE_INCLUSIVE */
                        if (pat->type == NODE_BINARY_OP &&
                            (pat->data.binary.op == BIN_RANGE ||
                             pat->data.binary.op == BIN_RANGE_INCLUSIVE)) {
                            fputs("__match_val >= ", cg->out);
                            c_emit_expr(cg, pat->data.binary.left);
                            fputs(" && __match_val <= ", cg->out);
                            c_emit_expr(cg, pat->data.binary.right);
                            emitted = 1;
                        } else {
                            fputs("__match_val == ", cg->out);
                            c_emit_expr(cg, pat);
                            emitted = 1;
                        }
                    }
                    /* Emit additional comma-separated patterns as || */
                    for (int j = 0; j < arm->data.match_arm.patterns.count; j++) {
                        AstNode *extra = arm->data.match_arm.patterns.items[j];
                        fputs(" || __match_val == ", cg->out);
                        c_emit_expr(cg, extra);
                    }
                    if (!emitted) fputs("1", cg->out);
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
            /* Traits: emit vtable struct type with function pointers for each method.
             * Also emit a dyn_Trait struct: { void* data; VTable_Trait* vtable; } */
            StringView tname = node->data.trait_decl.name->data.ident.name;
            c_indent(cg);
            fprintf(cg->out, "typedef struct {\n");
            cg->indent++;
            for (int mi = 0; mi < node->data.trait_decl.methods.count; mi++) {
                AstNode *method = node->data.trait_decl.methods.items[mi];
                if (method->type != NODE_FUNC_DECL) continue;
                StringView mn = method->data.func.name->data.ident.name;
                c_indent(cg);
                /* Emit function pointer: ret_type (*name)(void* self, ...) */
                if (method->data.func.return_type) {
                    c_emit_type(cg, method->data.func.return_type);
                } else {
                    fputs("void", cg->out);
                }
                fprintf(cg->out, " (*%.*s)(void* self", (int)mn.len, mn.data);
                for (int pi = 0; pi < method->data.func.params.count; pi++) {
                    AstNode *param = method->data.func.params.items[pi];
                    if (param->data.param.name && param->data.param.name->type == NODE_IDENT) {
                        StringView pn = param->data.param.name->data.ident.name;
                        if (pn.len == 4 && memcmp(pn.data, "self", 4) == 0) continue;
                    }
                    fputs(", ", cg->out);
                    if (param->data.param.type) {
                        c_emit_type(cg, param->data.param.type);
                    } else {
                        fputs("uint64_t", cg->out);
                    }
                }
                fputs(");\n", cg->out);
            }
            cg->indent--;
            c_indent(cg);
            fprintf(cg->out, "} VTable_%.*s;\n", (int)tname.len, tname.data);
            /* Emit dyn_Trait struct */
            c_indent(cg);
            fprintf(cg->out, "typedef struct { void* data; VTable_%.*s* vtable; } Dyn_%.*s;\n",
                    (int)tname.len, tname.data, (int)tname.len, tname.data);
            break;
        }
        case NODE_IMPL_BLOCK: {
            /* Impl blocks: emit a static vtable instance with concrete function pointers.
             * Mark each method as an impl method so name mangling prefixes with type name.
             * Also emit method bodies immediately after the vtable. */
            StringView tn = node->data.impl_block.trait_name;
            StringView tyn = node->data.impl_block.type_name;
            /* Mark methods as impl methods for name mangling */
            for (int mi = 0; mi < node->data.impl_block.methods.count; mi++) {
                AstNode *method = node->data.impl_block.methods.items[mi];
                if (method->type == NODE_FUNC_DECL) {
                    method->data.func.is_impl_method = true;
                    method->data.func.impl_type_name = tyn;
                }
            }
            /* Emit prototypes for impl methods before the vtable */
            for (int mi = 0; mi < node->data.impl_block.methods.count; mi++) {
                AstNode *method = node->data.impl_block.methods.items[mi];
                if (method->type == NODE_FUNC_DECL) {
                    StringView mn = method->data.func.name->data.ident.name;
                    if (method->data.func.return_type) {
                        c_emit_type(cg, method->data.func.return_type);
                    } else {
                        fputs("void", cg->out);
                    }
                    fprintf(cg->out, " %.*s_%.*s%s(",
                            (int)tyn.len, tyn.data,
                            (int)mn.len, mn.data,
                            method->data.func.return_type ? "_getter" : "_setter");
                    for (int pi = 0; pi < method->data.func.params.count; pi++) {
                        if (pi > 0) fputs(", ", cg->out);
                        AstNode *param = method->data.func.params.items[pi];
                        if (param->data.param.type) {
                            c_emit_type(cg, param->data.param.type);
                        } else {
                            fputs("uint64_t", cg->out);
                        }
                    }
                    fputs(");\n", cg->out);
                }
            }
            c_indent(cg);
            fprintf(cg->out, "static VTable_%.*s vtable_%.*s_for_%.*s = {\n",
                    (int)tn.len, tn.data, (int)tn.len, tn.data, (int)tyn.len, tyn.data);
            cg->indent++;
            for (int mi = 0; mi < node->data.impl_block.methods.count; mi++) {
                AstNode *method = node->data.impl_block.methods.items[mi];
                if (method->type != NODE_FUNC_DECL) continue;
                StringView mn = method->data.func.name->data.ident.name;
                c_indent(cg);
                fprintf(cg->out, ".%.*s = (void(*)(void*))%.*s_%.*s%s,\n",
                        (int)mn.len, mn.data,
                        (int)tyn.len, tyn.data, (int)mn.len, mn.data,
                        method->data.func.return_type ? "_getter" : "_setter");
            }
            cg->indent--;
            c_indent(cg);
            fputs("};\n", cg->out);
            /* Emit method bodies immediately after the vtable */
            for (int mi = 0; mi < node->data.impl_block.methods.count; mi++) {
                AstNode *method = node->data.impl_block.methods.items[mi];
                if (method->type == NODE_FUNC_DECL) {
                    /* Emit the function body with the mangled name directly */
                    StringView mn = method->data.func.name->data.ident.name;
                    /* Return type */
                    if (method->data.func.return_type) {
                        c_emit_type(cg, method->data.func.return_type);
                    } else {
                        fputs("void", cg->out);
                    }
                    fprintf(cg->out, " %.*s_%.*s%s(",
                            (int)tyn.len, tyn.data,
                            (int)mn.len, mn.data,
                            method->data.func.return_type ? "_getter" : "_setter");
                    /* Parameters */
                    for (int pi = 0; pi < method->data.func.params.count; pi++) {
                        if (pi > 0) fputs(", ", cg->out);
                        AstNode *param = method->data.func.params.items[pi];
                        if (param->data.param.type) {
                            c_emit_type(cg, param->data.param.type);
                        } else {
                            fputs("uint64_t", cg->out);
                        }
                        fputc(' ', cg->out);
                        if (param->data.param.name && param->data.param.name->type == NODE_IDENT) {
                            StringView pn = param->data.param.name->data.ident.name;
                            fprintf(cg->out, "%.*s", (int)pn.len, pn.data);
                        }
                    }
                    fputs(") {\n", cg->out);
                    cg->indent++;
                    if (method->data.func.body) {
                        if (method->data.func.body->type == NODE_BLOCK) {
                            c_emit_block(cg, method->data.func.body);
                        } else {
                            c_indent(cg);
                            fputs("return (", cg->out);
                            c_emit_expr(cg, method->data.func.body);
                            fputs(");\n", cg->out);
                        }
                    }
                    cg->indent--;
                    c_indent(cg);
                    fputs("}\n\n", cg->out);
                }
            }
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
            /* Properties are methods that look like fields.
               Emit as a function declaration (same as NODE_FUNC_DECL).
               The property's getter/setter is stored as a func_decl in data.func. */
            c_emit_func_decl(cg, node);
            break;
        case NODE_SPAWN:
            c_emit_spawn(cg, node);
            break;
        case NODE_YIELD:
            c_emit_yield(cg, node);
            break;
        default:
            fprintf(stderr, "C: unhandled statement node type %d\n", node->type);
            break;
    }
}
