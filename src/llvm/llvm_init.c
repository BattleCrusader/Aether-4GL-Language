#include "aether/llvm.h"
#include <string.h>
#include <stdio.h>

/* ──────────────────────────────────────────────
 * Target triple mapping
 * ────────────────────────────────────────────── */
const char *llvm_target_triple(Target target) {
    switch (target) {
        case TARGET_HOST:
        case TARGET_MACHO64:
        case TARGET_ELF64_HOST:
            return "x86_64-apple-macosx";
        case TARGET_KERNEL:
        case TARGET_BOOT:
        case TARGET_FREESTANDING:
        case TARGET_MODULE:
        case TARGET_BINARY:
            return "x86_64-unknown-none-elf";
        default:
            return "x86_64-apple-macosx";
    }
}

/* ──────────────────────────────────────────────
 * Data layout mapping
 * ────────────────────────────────────────────── */
const char *llvm_target_data_layout(Target target) {
    (void)target;
    /* x86_64 ELF/Mach-O data layout — valid for all our targets */
    return "e-m:e-p:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128";
}

/* ──────────────────────────────────────────────
 * Create LLVM codegen
 * ────────────────────────────────────────────── */
LlvmCodegen *llvm_create(Arena *arena, Target target, int opt_level) {
    LlvmCodegen *lc = (LlvmCodegen *)arena_alloc(arena, sizeof(LlvmCodegen));
    memset(lc, 0, sizeof(*lc));
    lc->arena = arena;
    lc->target = target;
    lc->opt_level = opt_level;

    /* Initialize LLVM targets */
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    /* Create context, module, builder */
    lc->context = LLVMContextCreate();
    lc->module = LLVMModuleCreateWithNameInContext("aether", lc->context);
    lc->builder = LLVMCreateBuilderInContext(lc->context);

    /* Set target triple and data layout */
    const char *triple = llvm_target_triple(target);
    LLVMSetTarget(lc->module, triple);

    const char *layout = llvm_target_data_layout(target);
    LLVMSetDataLayout(lc->module, layout);

    /* Pre-create the Aether string type: { len: i64, ptr: i8* }
     * Must be done AFTER setting data layout on the module. */
    {
        lc->string_type = LLVMStructCreateNamed(lc->context, "aether.string");
        LLVMTypeRef elems[2];
        elems[0] = LLVMInt64TypeInContext(lc->context);
        elems[1] = LLVMPointerType(LLVMInt8TypeInContext(lc->context), 0);
        LLVMStructSetBody(lc->string_type, elems, 2, false);
    }

    return lc;
}

/* ──────────────────────────────────────────────
 * Destroy LLVM codegen
 * ────────────────────────────────────────────── */
void llvm_destroy(LlvmCodegen *lc) {
    if (!lc) return;
    if (lc->builder) LLVMDisposeBuilder(lc->builder);
    if (lc->module) LLVMDisposeModule(lc->module);
    if (lc->context) LLVMContextDispose(lc->context);
    /* Note: arena is owned by the caller, not freed here */
}
