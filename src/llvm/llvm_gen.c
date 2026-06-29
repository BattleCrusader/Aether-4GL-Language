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
 *   Pass 1: Declare all functions with correct types (no body).
 *            This ensures forward references in calls find the right type.
 *   Pass 2: Generate function bodies.
 * ────────────────────────────────────────────── */
bool llvm_generate(LlvmCodegen *lc, AstNode *program) {
    if (!program || program->type != NODE_PROGRAM) {
        fprintf(stderr, "LLVM: expected NODE_PROGRAM\n");
        return false;
    }

    /* Declare runtime functions */
    llvm_declare_runtime(lc);

    /* Pass 1: Declare all functions with correct types (no body) */
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
        }
    }

    /* Pass 2: Generate function bodies */
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *decl = program->data.list.items[i];
        if (decl->type == NODE_FUNC_DECL) {
            llvm_cg_func_decl(lc, decl);
        }
    }

    return true;
}
