#include "aether/llvm.h"
#include "aether/str.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────
 * Forward declarations for expression codegen
 * Each expression type has its own function.
 * ────────────────────────────────────────────── */
static LLVMValueRef cg_literal_int(LlvmCodegen *lc, AstNode *node);
static LLVMValueRef cg_literal_float(LlvmCodegen *lc, AstNode *node);
static LLVMValueRef cg_literal_string(LlvmCodegen *lc, AstNode *node);
static LLVMValueRef cg_literal_bool(LlvmCodegen *lc, AstNode *node);
static LLVMValueRef cg_ident(LlvmCodegen *lc, AstNode *node);
static LLVMValueRef cg_binary_op(LlvmCodegen *lc, AstNode *node);
static LLVMValueRef cg_unary_op(LlvmCodegen *lc, AstNode *node);
static LLVMValueRef cg_call(LlvmCodegen *lc, AstNode *node);
static LLVMValueRef cg_index(LlvmCodegen *lc, AstNode *node);
static LLVMValueRef cg_field_access(LlvmCodegen *lc, AstNode *node);

/* ──────────────────────────────────────────────
 * Main expression dispatcher
 * ────────────────────────────────────────────── */
LLVMValueRef llvm_cg_expr(LlvmCodegen *lc, AstNode *node) {
    if (!node) return LLVMConstInt(LLVMInt64TypeInContext(lc->context), 0, false);

    switch (node->type) {
        case NODE_LITERAL_INT:    return cg_literal_int(lc, node);
        case NODE_LITERAL_FLOAT:  return cg_literal_float(lc, node);
        case NODE_LITERAL_STRING: return cg_literal_string(lc, node);
        case NODE_LITERAL_BOOL:   return cg_literal_bool(lc, node);
        case NODE_LITERAL_CHAR:   return LLVMConstInt(LLVMInt8TypeInContext(lc->context),
                                        node->data.literal.char_val, false);
        case NODE_LITERAL_NONE:   return LLVMConstNull(LLVMPointerType(
                                        LLVMInt8TypeInContext(lc->context), 0));
        case NODE_IDENT:          return cg_ident(lc, node);
        case NODE_BINARY_OP:      return cg_binary_op(lc, node);
        case NODE_UNARY_OP:       return cg_unary_op(lc, node);
        case NODE_CALL:           return cg_call(lc, node);
        case NODE_INDEX:          return cg_index(lc, node);
        case NODE_FIELD_ACCESS:   return cg_field_access(lc, node);
        default:
            fprintf(stderr, "LLVM: unhandled expression node type %d\n", node->type);
            return LLVMConstInt(LLVMInt64TypeInContext(lc->context), 0, false);
    }
}

/* ──────────────────────────────────────────────
 * Integer literal
 * ────────────────────────────────────────────── */
static LLVMValueRef cg_literal_int(LlvmCodegen *lc, AstNode *node) {
    uint64_t val = node->data.literal.int_val;
    if (node->data.literal.is_negative) {
        val = ~val + 1;
    }
    return LLVMConstInt(LLVMInt64TypeInContext(lc->context), val, false);
}

/* ──────────────────────────────────────────────
 * Float literal
 * ────────────────────────────────────────────── */
static LLVMValueRef cg_literal_float(LlvmCodegen *lc, AstNode *node) {
    return LLVMConstReal(LLVMDoubleTypeInContext(lc->context),
        node->data.literal.float_val);
}

/* ──────────────────────────────────────────────
 * String literal — create a global string constant
 * and return it as an Aether string struct { len, ptr }.
 * ────────────────────────────────────────────── */
static LLVMValueRef cg_literal_string(LlvmCodegen *lc, AstNode *node) {
    StringView sv = node->data.literal.string_val;
    int len = (int)sv.len;

    static int str_counter = 0;
    char name[64];
    snprintf(name, sizeof(name), ".str.%d", str_counter++);

    /* Create global string constant */
    LLVMTypeRef arr_type = LLVMArrayType(LLVMInt8TypeInContext(lc->context), len + 1);
    LLVMValueRef global = LLVMAddGlobal(lc->module, arr_type, name);
    LLVMSetInitializer(global, LLVMConstString(sv.data, len, true));
    LLVMSetGlobalConstant(global, true);
    LLVMSetLinkage(global, LLVMPrivateLinkage);
    LLVMSetUnnamedAddr(global, true);

    /* Get pointer to first element */
    LLVMValueRef indices[2] = {
        LLVMConstInt(LLVMInt64TypeInContext(lc->context), 0, false),
        LLVMConstInt(LLVMInt64TypeInContext(lc->context), 0, false)
    };
    LLVMValueRef ptr = LLVMConstGEP2(arr_type, global, indices, 2);

    /* Create Aether string struct: { len: i64, ptr: i8* } */
    LLVMValueRef struct_vals[2] = {
        LLVMConstInt(LLVMInt64TypeInContext(lc->context), len, false),
        ptr
    };
    return LLVMConstStruct(struct_vals, 2, false);
}

/* ──────────────────────────────────────────────
 * Boolean literal
 * ────────────────────────────────────────────── */
static LLVMValueRef cg_literal_bool(LlvmCodegen *lc, AstNode *node) {
    return LLVMConstInt(LLVMInt1TypeInContext(lc->context),
        node->data.literal.bool_val ? 1 : 0, false);
}

/* ──────────────────────────────────────────────
 * Identifier — look up in locals, then globals
 * ────────────────────────────────────────────── */
static LLVMValueRef cg_ident(LlvmCodegen *lc, AstNode *node) {
    StringView name = node->data.ident.name;

    LLVMValueRef local = llvm_lookup_local(lc, name);
    if (local) {
        LLVMTypeRef ptr_type = LLVMTypeOf(local);
        LLVMTypeRef elem_type = LLVMGetElementType(ptr_type);
        return LLVMBuildLoad2(lc->builder, elem_type, local, "");
    }

    LLVMValueRef global = llvm_lookup_global(lc, name);
    if (global) {
        LLVMTypeRef ptr_type = LLVMTypeOf(global);
        LLVMTypeRef elem_type = LLVMGetElementType(ptr_type);
        return LLVMBuildLoad2(lc->builder, elem_type, global, "");
    }

    fprintf(stderr, "LLVM: undefined identifier '%.*s'\n", (int)name.len, name.data);
    return LLVMConstInt(LLVMInt64TypeInContext(lc->context), 0, false);
}

/* ──────────────────────────────────────────────
 * Binary operation
 * ────────────────────────────────────────────── */
static LLVMValueRef cg_binary_op(LlvmCodegen *lc, AstNode *node) {
    LLVMValueRef L = llvm_cg_expr(lc, node->data.binary.left);
    LLVMValueRef R = llvm_cg_expr(lc, node->data.binary.right);
    LLVMBuilderRef B = lc->builder;

    switch (node->data.binary.op) {
        case BIN_ADD: return LLVMBuildAdd(B, L, R, "addtmp");
        case BIN_SUB: return LLVMBuildSub(B, L, R, "subtmp");
        case BIN_MUL: return LLVMBuildMul(B, L, R, "multmp");
        case BIN_DIV: return LLVMBuildSDiv(B, L, R, "divtmp");
        case BIN_MOD: return LLVMBuildSRem(B, L, R, "modtmp");
        case BIN_EQ:  return LLVMBuildICmp(B, LLVMIntEQ, L, R, "eqtmp");
        case BIN_NEQ: return LLVMBuildICmp(B, LLVMIntNE, L, R, "neqtmp");
        case BIN_LT:  return LLVMBuildICmp(B, LLVMIntSLT, L, R, "lttmp");
        case BIN_GT:  return LLVMBuildICmp(B, LLVMIntSGT, L, R, "gttmp");
        case BIN_LE:  return LLVMBuildICmp(B, LLVMIntSLE, L, R, "letmp");
        case BIN_GE:  return LLVMBuildICmp(B, LLVMIntSGE, L, R, "getmp");
        case BIN_BIT_AND: return LLVMBuildAnd(B, L, R, "andtmp");
        case BIN_BIT_OR:  return LLVMBuildOr(B, L, R, "ortmp");
        case BIN_BIT_XOR: return LLVMBuildXor(B, L, R, "xortmp");
        case BIN_SHL: return LLVMBuildShl(B, L, R, "shltmp");
        case BIN_SHR: return LLVMBuildAShr(B, L, R, "shrtmp");
        case BIN_AND: return LLVMBuildAnd(B, L, R, "andtmp");
        case BIN_OR:  return LLVMBuildOr(B, L, R, "ortmp");
        case BIN_CONCAT: {
            /* String concatenation — call __aether_concat */
            return llvm_call_concat(lc, L, R);
        }
        case BIN_OR_ELSE: {
            /* Optional unwrap: x or default — if x is none, use default */
            /* For now, just return the right side */
            return R;
        }
        default:
            fprintf(stderr, "LLVM: unhandled binary op %d\n", node->data.binary.op);
            return LLVMConstInt(LLVMInt64TypeInContext(lc->context), 0, false);
    }
}

/* ──────────────────────────────────────────────
 * Unary operation
 * ────────────────────────────────────────────── */
static LLVMValueRef cg_unary_op(LlvmCodegen *lc, AstNode *node) {
    LLVMValueRef operand = llvm_cg_expr(lc, node->data.unary.operand);
    LLVMBuilderRef B = lc->builder;

    switch (node->data.unary.op) {
        case UNARY_NEG:
            return LLVMBuildNeg(B, operand, "negtmp");
        case UNARY_NOT: {
            /* Logical not: icmp eq %operand, 0 */
            LLVMValueRef zero = LLVMConstInt(LLVMInt64TypeInContext(lc->context), 0, false);
            return LLVMBuildICmp(B, LLVMIntEQ, operand, zero, "nottmp");
        }
        case UNARY_BIT_NOT:
            return LLVMBuildNot(B, operand, "bntmp");
        case UNARY_INC:
        case UNARY_DEC: {
            /* Prefix/postfix inc/dec: load, add/sub 1, store, return */
            /* The operand should be an alloca pointer from an ident lookup.
             * We need to find the alloca for the variable. */
            if (node->data.unary.operand && node->data.unary.operand->type == NODE_IDENT) {
                StringView name = node->data.unary.operand->data.ident.name;
                LLVMValueRef alloca = llvm_lookup_local(lc, name);
                if (!alloca) alloca = llvm_lookup_global(lc, name);
                if (alloca) {
                    LLVMValueRef val = LLVMBuildLoad2(B, LLVMTypeOf(alloca), alloca, "");
                    LLVMValueRef one = LLVMConstInt(LLVMInt64TypeInContext(lc->context), 1, false);
                    LLVMValueRef result = (node->data.unary.op == UNARY_INC)
                        ? LLVMBuildAdd(B, val, one, "inctmp")
                        : LLVMBuildSub(B, val, one, "dectmp");
                    LLVMBuildStore(B, result, alloca);
                    return result;
                }
            }
            return LLVMConstInt(LLVMInt64TypeInContext(lc->context), 0, false);
        }
        case UNARY_ARRAY_LEN: {
            /* Array length: load first 8 bytes of array header */
            LLVMTypeRef ptr_type = LLVMPointerType(LLVMInt64TypeInContext(lc->context), 0);
            LLVMValueRef ptr = LLVMBuildBitCast(B, operand, ptr_type, "");
            return LLVMBuildLoad2(B, LLVMInt64TypeInContext(lc->context), ptr, "len");
        }
        case UNARY_REF:
            /* Return the alloca pointer (operand is already the ident's alloca) */
            return operand;
        case UNARY_DEREF:
            /* Dereference: load from pointer */
            return LLVMBuildLoad2(B, LLVMPointerType(LLVMInt64TypeInContext(lc->context), 0),
                operand, "deref");
        default:
            fprintf(stderr, "LLVM: unhandled unary op %d\n", node->data.unary.op);
            return LLVMConstInt(LLVMInt64TypeInContext(lc->context), 0, false);
    }
}

/* ──────────────────────────────────────────────
 * Function call — handles print() built-in and
 * regular function calls.
 * ────────────────────────────────────────────── */
static LLVMValueRef cg_call(LlvmCodegen *lc, AstNode *node) {
    if (!node->data.call.callee || node->data.call.callee->type != NODE_IDENT) {
        fprintf(stderr, "LLVM: call with non-ident callee not yet supported\n");
        return LLVMConstInt(LLVMInt64TypeInContext(lc->context), 0, false);
    }

    StringView fname = node->data.call.callee->data.ident.name;
    int argc = node->data.call.args.count;

    /* No built-ins — all functions are regular function calls.
     * print() is defined in std/io.ae and linked normally. */

    /* Look up the function in the module */
    char name[256];
    int nlen = (int)fname.len;
    if (nlen > 255) nlen = 255;
    memcpy(name, fname.data, nlen);
    name[nlen] = '\0';

    LLVMValueRef func = LLVMGetNamedFunction(lc->module, name);
    if (!func) {
        /* Forward declaration — create a placeholder function.
         * The actual definition will be added when we reach it. */
        int nparams = argc;
        LLVMTypeRef *param_types = (LLVMTypeRef *)malloc(nparams * sizeof(LLVMTypeRef));
        for (int i = 0; i < nparams; i++) {
            param_types[i] = LLVMInt64TypeInContext(lc->context);
        }
        LLVMTypeRef func_type = LLVMFunctionType(
            LLVMInt64TypeInContext(lc->context), param_types, nparams, false);
        func = LLVMAddFunction(lc->module, name, func_type);
        free(param_types);
    }

    /* Evaluate arguments */
    LLVMValueRef *args = (LLVMValueRef *)malloc(argc * sizeof(LLVMValueRef));
    for (int i = 0; i < argc; i++) {
        args[i] = llvm_cg_expr(lc, node->data.call.args.items[i]);
    }

    LLVMValueRef result = LLVMBuildCall2(lc->builder,
        LLVMGlobalGetValueType(func), func, args, argc, "calltmp");
    free(args);
    return result;
}

/* ──────────────────────────────────────────────
 * Index expression: arr[i] or s[i]
 * ────────────────────────────────────────────── */
static LLVMValueRef cg_index(LlvmCodegen *lc, AstNode *node) {
    LLVMValueRef target = llvm_cg_expr(lc, node->data.index.target);
    LLVMValueRef index = llvm_cg_expr(lc, node->data.index.index);
    LLVMBuilderRef B = lc->builder;

    /* GEP into the array/string pointer */
    LLVMValueRef indices[2] = {
        LLVMConstInt(LLVMInt64TypeInContext(lc->context), 0, false),
        index
    };
    LLVMValueRef ptr = LLVMBuildGEP2(B,
        LLVMArrayType(LLVMInt8TypeInContext(lc->context), 0),
        target, indices, 2, "idxptr");
    return LLVMBuildLoad2(B, LLVMInt8TypeInContext(lc->context), ptr, "idxval");
}

/* ──────────────────────────────────────────────
 * Field access: obj.field
 * ────────────────────────────────────────────── */
static LLVMValueRef cg_field_access(LlvmCodegen *lc, AstNode *node) {
    LLVMValueRef target = llvm_cg_expr(lc, node->data.field.target);
    LLVMBuilderRef B = lc->builder;

    StringView field_name = node->data.field.field->data.ident.name;

    int field_index = 0;
    if (node->data.field.target->type == NODE_IDENT &&
        node->data.field.target->data.ident.resolved) {
        AstNode *decl = node->data.field.target->data.ident.resolved;
        if (decl->type == NODE_STRUCT_DECL || decl->type == NODE_CLASS_DECL) {
            for (int i = 0; i < decl->data.struct_decl.fields.count; i++) {
                AstNode *field = decl->data.struct_decl.fields.items[i];
                if (sv_eq(field->data.param.name->data.ident.name, field_name)) {
                    field_index = i;
                    break;
                }
            }
        }
    }

    LLVMValueRef indices[2] = {
        LLVMConstInt(LLVMInt64TypeInContext(lc->context), 0, false),
        LLVMConstInt(LLVMInt32TypeInContext(lc->context), field_index, false)
    };
    LLVMValueRef ptr = LLVMBuildGEP2(B,
        LLVMArrayType(LLVMInt8TypeInContext(lc->context), 0),
        target, indices, 2, "fieldptr");
    return LLVMBuildLoad2(B, LLVMInt64TypeInContext(lc->context), ptr, "fieldval");
}
