#include "aether/llvm.h"
#include <stdlib.h>

/* ──────────────────────────────────────────────
 * Map Aether primitive type to LLVM type
 * ────────────────────────────────────────────── */
LLVMTypeRef llvm_prim_type(LlvmCodegen *lc, PrimType prim) {
    switch (prim) {
        case PRIM_VOID:   return LLVMVoidTypeInContext(lc->context);
        case PRIM_BOOL:   return LLVMInt1TypeInContext(lc->context);
        case PRIM_BYTE:
        case PRIM_U8:     return LLVMInt8TypeInContext(lc->context);
        case PRIM_U16:    return LLVMInt16TypeInContext(lc->context);
        case PRIM_U32:    return LLVMInt32TypeInContext(lc->context);
        case PRIM_U64:    return LLVMInt64TypeInContext(lc->context);
        case PRIM_I8:     return LLVMInt8TypeInContext(lc->context);
        case PRIM_I16:    return LLVMInt16TypeInContext(lc->context);
        case PRIM_I32:    return LLVMInt32TypeInContext(lc->context);
        case PRIM_I64:    return LLVMInt64TypeInContext(lc->context);
        case PRIM_F32:    return LLVMFloatTypeInContext(lc->context);
        case PRIM_F64:    return LLVMDoubleTypeInContext(lc->context);
        case PRIM_STRING: {
            /* Aether string is { len: i64, ptr: i8* } */
            LLVMTypeRef elems[2] = {
                LLVMInt64TypeInContext(lc->context),
                LLVMPointerType(LLVMInt8TypeInContext(lc->context), 0)
            };
            return LLVMStructType(elems, 2, false);
        }
    }
    return LLVMInt64TypeInContext(lc->context); /* fallback */
}

/* ──────────────────────────────────────────────
 * Map Aether type AST node to LLVM type
 * Handles compound types: arrays, slices, pointers,
 * references, optionals, tuples, function types.
 * ────────────────────────────────────────────── */
LLVMTypeRef llvm_type_from_ast(LlvmCodegen *lc, AstNode *type_node) {
    if (!type_node) return LLVMVoidTypeInContext(lc->context);

    switch (type_node->type) {
        case NODE_TYPE_PRIMITIVE:
            return llvm_prim_type(lc, type_node->data.type_node.prim);

        case NODE_TYPE_PTR:
            return LLVMPointerType(
                llvm_type_from_ast(lc, type_node->data.type_node.elem_type), 0);

        case NODE_TYPE_REF:
            /* ref T is a pointer to T */
            return LLVMPointerType(
                llvm_type_from_ast(lc, type_node->data.type_node.elem_type), 0);

        case NODE_TYPE_ARRAY: {
            /* [T; N] — fixed-size array */
            LLVMTypeRef elem = llvm_type_from_ast(lc, type_node->data.type_node.elem_type);
            int count = type_node->data.type_node.array_size;
            if (count <= 0) count = 1;
            return LLVMArrayType(elem, count);
        }

        case NODE_TYPE_SLICE: {
            /* [T] — dynamic slice: { ptr, len } */
            LLVMTypeRef elem = llvm_type_from_ast(lc, type_node->data.type_node.elem_type);
            LLVMTypeRef elems[2] = {
                LLVMPointerType(elem, 0),  /* ptr */
                LLVMInt64TypeInContext(lc->context)  /* len */
            };
            return LLVMStructType(elems, 2, false);
        }

        case NODE_TYPE_OPTIONAL: {
            /* T? — optional: { has_value: bool, value: T } */
            LLVMTypeRef elem = llvm_type_from_ast(lc, type_node->data.type_node.elem_type);
            LLVMTypeRef elems[2] = {
                LLVMInt8TypeInContext(lc->context),  /* has_value tag */
                elem                                  /* value */
            };
            return LLVMStructType(elems, 2, false);
        }

        case NODE_TYPE_TUPLE: {
            /* (T1, T2, ...) — anonymous struct */
            int count = type_node->data.type_node.tuple_types.count;
            LLVMTypeRef *elems = (LLVMTypeRef *)malloc(count * sizeof(LLVMTypeRef));
            for (int i = 0; i < count; i++) {
                elems[i] = llvm_type_from_ast(lc,
                    type_node->data.type_node.tuple_types.items[i]);
            }
            LLVMTypeRef result = LLVMStructType(elems, count, false);
            free(elems);
            return result;
        }

        case NODE_TYPE_FN: {
            /* func(ParamTypes): ReturnType */
            int param_count = type_node->data.type_node.param_types.count;
            LLVMTypeRef ret = type_node->data.type_node.return_type
                ? llvm_type_from_ast(lc, type_node->data.type_node.return_type)
                : LLVMVoidTypeInContext(lc->context);
            LLVMTypeRef *params = (LLVMTypeRef *)malloc(param_count * sizeof(LLVMTypeRef));
            for (int i = 0; i < param_count; i++) {
                params[i] = llvm_type_from_ast(lc,
                    type_node->data.type_node.param_types.items[i]);
            }
            LLVMTypeRef func_type = LLVMPointerType(
                LLVMFunctionType(ret, params, param_count, false), 0);
            free(params);
            return func_type;
        }

        case NODE_TYPE_NAMED: {
            /* Named type — resolved during semantic analysis.
             * For now, return i64 as placeholder. */
            (void)type_node;
            return LLVMInt64TypeInContext(lc->context);
        }

        default:
            return LLVMInt64TypeInContext(lc->context);
    }
}

/* ──────────────────────────────────────────────
 * Return type for throws functions
 * Returns a struct type: { value_type, i8 }
 * where i8 is the error tag (0 = success, 1 = error).
 * ────────────────────────────────────────────── */
LLVMTypeRef llvm_throws_return_type(LlvmCodegen *lc, AstNode *return_type) {
    LLVMTypeRef ret = return_type
        ? llvm_type_from_ast(lc, return_type)
        : LLVMVoidTypeInContext(lc->context);

    LLVMTypeRef elems[2] = {
        ret,                                    /* return value */
        LLVMInt8TypeInContext(lc->context)       /* error tag */
    };
    return LLVMStructType(elems, 2, false);
}
