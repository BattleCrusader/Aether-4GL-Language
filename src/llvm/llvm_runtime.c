#include "aether/llvm.h"
#include <string.h>
#include <stdio.h>

/* ──────────────────────────────────────────────
 * Runtime helper declarations
 *
 * These functions are linked from the runtime
 * library (libaether_rt.a) or emitted inline.
 * ────────────────────────────────────────────── */

/* ──────────────────────────────────────────────
 * Declare all runtime functions in the module
 * ────────────────────────────────────────────── */
void llvm_declare_runtime(LlvmCodegen *lc) {
    LLVMContextRef ctx = lc->context;
    LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx);
    LLVMTypeRef i8ptr = LLVMPointerType(LLVMInt8TypeInContext(ctx), 0);

    /* __aether_alloc(size: i64) -> ptr */
    {
        LLVMTypeRef param_types[1] = { i64 };
        LLVMTypeRef func_type = LLVMFunctionType(i8ptr, param_types, 1, false);
        LLVMValueRef func = LLVMAddFunction(lc->module, "__aether_alloc", func_type);
        LLVMSetLinkage(func, LLVMExternalLinkage);
    }

    /* __aether_free(ptr) -> void */
    {
        LLVMTypeRef param_types[1] = { i8ptr };
        LLVMTypeRef func_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx), param_types, 1, false);
        LLVMValueRef func = LLVMAddFunction(lc->module, "__aether_free", func_type);
        LLVMSetLinkage(func, LLVMExternalLinkage);
    }

    /* __aether_concat(left: ptr, right: ptr) -> ptr */
    {
        LLVMTypeRef param_types[2] = { i8ptr, i8ptr };
        LLVMTypeRef func_type = LLVMFunctionType(i8ptr, param_types, 2, false);
        LLVMValueRef func = LLVMAddFunction(lc->module, "__aether_concat", func_type);
        LLVMSetLinkage(func, LLVMExternalLinkage);
    }

    /* __aether_itoa(value: i64) -> ptr */
    {
        LLVMTypeRef param_types[1] = { i64 };
        LLVMTypeRef func_type = LLVMFunctionType(i8ptr, param_types, 1, false);
        LLVMValueRef func = LLVMAddFunction(lc->module, "__aether_itoa", func_type);
        LLVMSetLinkage(func, LLVMExternalLinkage);
    }

    /* print() is defined in std/io.ae — the two-pass codegen
     * will declare it from the AST with the correct type. */
}

/* ──────────────────────────────────────────────
 * Call __aether_alloc(size)
 * ────────────────────────────────────────────── */
LLVMValueRef llvm_call_alloc(LlvmCodegen *lc, LLVMValueRef size) {
    LLVMValueRef func = LLVMGetNamedFunction(lc->module, "__aether_alloc");
    if (!func) {
        llvm_declare_runtime(lc);
        func = LLVMGetNamedFunction(lc->module, "__aether_alloc");
    }
    LLVMValueRef args[1] = { size };
    return LLVMBuildCall2(lc->builder, LLVMGetElementType(LLVMTypeOf(func)),
        func, args, 1, "alloc");
}

/* ──────────────────────────────────────────────
 * Call __aether_concat(left, right)
 * ────────────────────────────────────────────── */
LLVMValueRef llvm_call_concat(LlvmCodegen *lc, LLVMValueRef left, LLVMValueRef right) {
    LLVMValueRef func = LLVMGetNamedFunction(lc->module, "__aether_concat");
    if (!func) {
        llvm_declare_runtime(lc);
        func = LLVMGetNamedFunction(lc->module, "__aether_concat");
    }
    LLVMValueRef args[2] = { left, right };
    return LLVMBuildCall2(lc->builder, LLVMGetElementType(LLVMTypeOf(func)),
        func, args, 2, "concat");
}

/* ──────────────────────────────────────────────
 * Call __aether_itoa(value)
 * ────────────────────────────────────────────── */
LLVMValueRef llvm_call_itoa(LlvmCodegen *lc, LLVMValueRef value) {
    LLVMValueRef func = LLVMGetNamedFunction(lc->module, "__aether_itoa");
    if (!func) {
        llvm_declare_runtime(lc);
        func = LLVMGetNamedFunction(lc->module, "__aether_itoa");
    }
    LLVMValueRef args[1] = { value };
    return LLVMBuildCall2(lc->builder, LLVMGetElementType(LLVMTypeOf(func)),
        func, args, 1, "itoa");
}
