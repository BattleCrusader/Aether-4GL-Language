#include "aether/llvm.h"
#include "aether/str.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/* ──────────────────────────────────────────────
 * Main codegen entry point
 *
 * Walks the AST program and generates LLVM IR
 * for all top-level declarations.
 *
 * Two-pass approach:
 *   Pass 1: Declare all functions and globals with correct types (no body).
 *            This ensures forward references in calls find the right type.
 *   Pass 2: Generate function bodies and initialize globals.
 * ────────────────────────────────────────────── */
bool llvm_generate(LlvmCodegen *lc, AstNode *program) {
    if (!program || program->type != NODE_PROGRAM) {
        fprintf(stderr, "LLVM: expected NODE_PROGRAM\n");
        return false;
    }

    /* Declare runtime functions */
    llvm_declare_runtime(lc);

    /* Pass 1: Declare all functions and globals with correct types (no body) */
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *decl = program->data.list.items[i];
        if (decl->type == NODE_FUNC_DECL) {
            StringView fname = decl->data.func.name->data.ident.name;
            int param_count = decl->data.func.params.count;

            /* Build parameter types */
            LLVMTypeRef *param_types = NULL;
            if (param_count > 0) {
                param_types = (LLVMTypeRef *)malloc(param_count * sizeof(LLVMTypeRef));
                for (int j = 0; j < param_count; j++) {
                    AstNode *param_type_node = decl->data.func.params.items[j]->data.param.type;
                    if (param_type_node) {
                        param_types[j] = llvm_type_from_ast(lc, param_type_node);
                    } else {
                        param_types[j] = LLVMInt64TypeInContext(lc->context);
                    }
                }
            }

            /* Return type */
            LLVMTypeRef ret_type = decl->data.func.return_type
                ? llvm_type_from_ast(lc, decl->data.func.return_type)
                : LLVMVoidTypeInContext(lc->context);

            LLVMTypeRef func_type = LLVMFunctionType(ret_type, param_types, param_count, false);

            char name[256];
            int nlen = (int)fname.len;
            if (nlen > 255) nlen = 255;
            memcpy(name, fname.data, nlen);
            name[nlen] = '\0';

            /* Only add if not already declared (e.g. by runtime) */
            LLVMValueRef func = LLVMGetNamedFunction(lc->module, name);
            if (!func) {
                func = LLVMAddFunction(lc->module, name, func_type);
            }
            llvm_declare_global(lc, fname, func, func_type);

            free(param_types);
        } else if (decl->type == NODE_LET && decl->data.let_decl.is_global) {
            /* Global let declaration — create LLVM global variable */
            StringView vname = decl->data.let_decl.name->data.ident.name;
            AstNode *type_node = decl->data.let_decl.type;

            LLVMTypeRef llvm_type = type_node
                ? llvm_type_from_ast(lc, type_node)
                : LLVMInt64TypeInContext(lc->context);

            char name[256];
            int nlen = (int)vname.len;
            if (nlen > 255) nlen = 255;
            memcpy(name, vname.data, nlen);
            name[nlen] = '\0';

            LLVMValueRef global = LLVMAddGlobal(lc->module, llvm_type, name);
            if (!decl->data.let_decl.is_mut) {
                LLVMSetGlobalConstant(global, true);
            }
            LLVMSetLinkage(global, LLVMInternalLinkage);

            llvm_declare_global(lc, vname, global, llvm_type);
        } else if (decl->type == NODE_CONST_DECL) {
            /* Const declaration — create LLVM global constant */
            StringView vname = decl->data.let_decl.name->data.ident.name;
            AstNode *type_node = decl->data.let_decl.type;

            LLVMTypeRef llvm_type = type_node
                ? llvm_type_from_ast(lc, type_node)
                : LLVMInt64TypeInContext(lc->context);

            char name[256];
            int nlen = (int)vname.len;
            if (nlen > 255) nlen = 255;
            memcpy(name, vname.data, nlen);
            name[nlen] = '\0';

            LLVMValueRef global = LLVMAddGlobal(lc->module, llvm_type, name);
            LLVMSetGlobalConstant(global, true);
            LLVMSetLinkage(global, LLVMInternalLinkage);

            llvm_declare_global(lc, vname, global, llvm_type);
        }
    }

    /* Pass 2: Generate function bodies and initialize globals */
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *decl = program->data.list.items[i];
        if (decl->type == NODE_FUNC_DECL) {
            llvm_cg_func_decl(lc, decl);
        } else if (decl->type == NODE_LET && decl->data.let_decl.is_global) {
            /* Initialize global let with its value */
            if (decl->data.let_decl.value) {
                StringView vname = decl->data.let_decl.name->data.ident.name;
                LLVMValueRef global = llvm_lookup_global(lc, vname);
                if (global) {
                    LLVMValueRef val = llvm_cg_expr(lc, decl->data.let_decl.value);
                    LLVMSetInitializer(global, val);
                }
            }
        } else if (decl->type == NODE_CONST_DECL) {
            /* Initialize const with its value */
            if (decl->data.let_decl.value) {
                StringView vname = decl->data.let_decl.name->data.ident.name;
                LLVMValueRef global = llvm_lookup_global(lc, vname);
                if (global) {
                    LLVMValueRef val = llvm_cg_expr(lc, decl->data.let_decl.value);
                    LLVMSetInitializer(global, val);
                }
            }
        }
    }

    return true;
}
