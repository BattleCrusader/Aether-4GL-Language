#include "aether/llvm.h"
#include "aether/str.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────
 * Statement codegen
 * ────────────────────────────────────────────── */

/* Forward declarations */
static void cg_let(LlvmCodegen *lc, AstNode *node);
static void cg_return(LlvmCodegen *lc, AstNode *node);
static void cg_if(LlvmCodegen *lc, AstNode *node);
static void cg_while(LlvmCodegen *lc, AstNode *node);
static void cg_for(LlvmCodegen *lc, AstNode *node);
static void cg_assign(LlvmCodegen *lc, AstNode *node);
static void cg_defer(LlvmCodegen *lc, AstNode *node);
static void cg_expr_stmt(LlvmCodegen *lc, AstNode *node);
static void emit_defers(LlvmCodegen *lc);

/* ──────────────────────────────────────────────
 * Main statement dispatcher
 * ────────────────────────────────────────────── */
void llvm_cg_stmt(LlvmCodegen *lc, AstNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_LET:       cg_let(lc, node); break;
        case NODE_RETURN:    emit_defers(lc); cg_return(lc, node); break;
        case NODE_IF:        cg_if(lc, node); break;
        case NODE_WHILE:     cg_while(lc, node); break;
        case NODE_FOR:       cg_for(lc, node); break;
        case NODE_ASSIGN:    cg_assign(lc, node); break;
        case NODE_DEFER:     cg_defer(lc, node); break;
        case NODE_EXPR_STMT: cg_expr_stmt(lc, node); break;
        case NODE_BLOCK:     llvm_cg_block(lc, node); break;
        default:
            fprintf(stderr, "LLVM: unhandled statement node type %d\n", node->type);
            break;
    }
}

/* ──────────────────────────────────────────────
 * Block — iterate statements
 * ────────────────────────────────────────────── */
void llvm_cg_block(LlvmCodegen *lc, AstNode *node) {
    /* Save defer chain state at block entry */
    LlvmDeferEntry *saved_defers = lc->defer_head;

    for (int i = 0; i < node->data.list.count; i++) {
        llvm_cg_stmt(lc, node->data.list.items[i]);
    }

    /* Emit any defers that were added in this block (not already emitted) */
    while (lc->defer_head != saved_defers) {
        LlvmDeferEntry *entry = lc->defer_head;
        if (entry->body) {
            llvm_cg_stmt(lc, entry->body);
        }
        lc->defer_head = entry->next;
    }
}

/* ──────────────────────────────────────────────
 * Emit all pending defers (LIFO order)
 * ────────────────────────────────────────────── */
static void emit_defers(LlvmCodegen *lc) {
    while (lc->defer_head) {
        LlvmDeferEntry *entry = lc->defer_head;
        if (entry->body) {
            llvm_cg_stmt(lc, entry->body);
        }
        lc->defer_head = entry->next;
    }
}

/* ──────────────────────────────────────────────
 * Let declaration — alloca + store
 * ────────────────────────────────────────────── */
static void cg_let(LlvmCodegen *lc, AstNode *node) {
    StringView name = node->data.let_decl.name->data.ident.name;
    AstNode *type_node = node->data.let_decl.type;
    AstNode *value_node = node->data.let_decl.value;

    LLVMTypeRef llvm_type;
    if (type_node) {
        llvm_type = llvm_type_from_ast(lc, type_node);
    } else if (value_node) {
        /* Infer type from the value expression.
         * For literals, we can determine the type directly. */
        switch (value_node->type) {
            case NODE_LITERAL_INT:
                llvm_type = LLVMInt64TypeInContext(lc->context);
                break;
            case NODE_LITERAL_FLOAT:
                llvm_type = LLVMDoubleTypeInContext(lc->context);
                break;
            case NODE_LITERAL_STRING: {
                /* Aether string is { len: i64, ptr: i8* } */
                LLVMTypeRef elems[2] = {
                    LLVMInt64TypeInContext(lc->context),
                    LLVMPointerType(LLVMInt8TypeInContext(lc->context), 0)
                };
                llvm_type = LLVMStructType(elems, 2, false);
                break;
            }
            case NODE_LITERAL_BOOL:
                llvm_type = LLVMInt1TypeInContext(lc->context);
                break;
            case NODE_LITERAL_CHAR:
                llvm_type = LLVMInt8TypeInContext(lc->context);
                break;
            default:
                llvm_type = LLVMInt64TypeInContext(lc->context);
                break;
        }
    } else {
        llvm_type = LLVMInt64TypeInContext(lc->context);
    }

    LLVMValueRef alloca = LLVMBuildAlloca(lc->builder, llvm_type, "");
    char name_buf[256];
    int nlen = (int)name.len;
    if (nlen > 255) nlen = 255;
    memcpy(name_buf, name.data, nlen);
    name_buf[nlen] = '\0';
    LLVMSetValueName2(alloca, name_buf, nlen);

    if (value_node) {
        LLVMValueRef val = llvm_cg_expr(lc, value_node);
        LLVMBuildStore(lc->builder, val, alloca);
    }

    llvm_declare_local(lc, name, alloca, llvm_type, node->data.let_decl.is_mut);
}

/* ──────────────────────────────────────────────
 * Return statement
 * ────────────────────────────────────────────── */
static void cg_return(LlvmCodegen *lc, AstNode *node) {
    if (node->data.return_node.value) {
        LLVMValueRef val = llvm_cg_expr(lc, node->data.return_node.value);
        LLVMBuildRet(lc->builder, val);
    } else {
        LLVMBuildRetVoid(lc->builder);
    }
}

/* ──────────────────────────────────────────────
 * If/elif/else
 * ────────────────────────────────────────────── */
static void cg_if(LlvmCodegen *lc, AstNode *node) {
    LLVMBuilderRef B = lc->builder;
    LLVMValueRef func = lc->current_func;

    /* Create blocks for then, else (or elif chain), and merge */
    LLVMBasicBlockRef then_block = LLVMAppendBasicBlock(func, "if.then");
    LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(func, "if.else");
    LLVMBasicBlockRef merge_block = LLVMAppendBasicBlock(func, "if.merge");

    /* Evaluate condition */
    LLVMValueRef cond = llvm_cg_expr(lc, node->data.if_node.condition);
    /* Convert to i1 if needed (comparisons return i1, other values need != 0) */
    LLVMTypeRef cond_type = LLVMTypeOf(cond);
    if (cond_type != LLVMInt1TypeInContext(lc->context)) {
        LLVMValueRef zero = LLVMConstInt(cond_type, 0, false);
        cond = LLVMBuildICmp(B, LLVMIntNE, cond, zero, "ifcond");
    }

    LLVMBuildCondBr(B, cond, then_block, else_block);

    /* Then block */
    LLVMPositionBuilderAtEnd(B, then_block);
    if (node->data.if_node.then_block) {
        llvm_cg_block(lc, node->data.if_node.then_block);
    }
    /* Branch to merge if current block doesn't have a terminator */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(B)) == NULL) {
        LLVMBuildBr(B, merge_block);
    }

    /* Else/elif block */
    LLVMPositionBuilderAtEnd(B, else_block);
    if (node->data.if_node.elif_chain) {
        /* Elif chain — the elif_chain is a linked list of IfNode structs */
        AstNode *elif = node->data.if_node.elif_chain;
        while (elif) {
            if (elif->type == NODE_IF) {
                /* This is an elif — create its own then/else blocks */
                LLVMBasicBlockRef elif_then = LLVMAppendBasicBlock(func, "elif.then");
                LLVMBasicBlockRef elif_else = LLVMAppendBasicBlock(func, "elif.else");

                LLVMValueRef elif_cond = llvm_cg_expr(lc, elif->data.if_node.condition);
                LLVMTypeRef etype = LLVMTypeOf(elif_cond);
                if (etype != LLVMInt1TypeInContext(lc->context)) {
                    LLVMValueRef zero = LLVMConstInt(etype, 0, false);
                    elif_cond = LLVMBuildICmp(B, LLVMIntNE, elif_cond, zero, "elifcond");
                }
                LLVMBuildCondBr(B, elif_cond, elif_then, elif_else);

                LLVMPositionBuilderAtEnd(B, elif_then);
                if (elif->data.if_node.then_block) {
                    llvm_cg_block(lc, elif->data.if_node.then_block);
                }
                if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(B)) == NULL) {
                    LLVMBuildBr(B, merge_block);
                }

                LLVMPositionBuilderAtEnd(B, elif_else);
                elif = elif->data.if_node.elif_chain;
            } else {
                /* This is the else block (not an elif) */
                if (elif->type == NODE_BLOCK) {
                    llvm_cg_block(lc, elif);
                }
                break;
            }
        }
    } else if (node->data.if_node.else_block) {
        llvm_cg_block(lc, node->data.if_node.else_block);
    }

    /* Branch to merge if current block doesn't have a terminator */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(B)) == NULL) {
        LLVMBuildBr(B, merge_block);
    }

    /* Continue at merge block */
    LLVMPositionBuilderAtEnd(B, merge_block);
}

/* ──────────────────────────────────────────────
 * While loop
 * ────────────────────────────────────────────── */
static void cg_while(LlvmCodegen *lc, AstNode *node) {
    LLVMBuilderRef B = lc->builder;
    LLVMValueRef func = lc->current_func;

    LLVMBasicBlockRef cond_block = LLVMAppendBasicBlock(func, "while.cond");
    LLVMBasicBlockRef body_block = LLVMAppendBasicBlock(func, "while.body");
    LLVMBasicBlockRef after_block = LLVMAppendBasicBlock(func, "while.after");

    /* Push loop context for break/continue */
    LlvmLoopFrame frame = { .break_block = after_block, .continue_block = cond_block };
    int saved_depth = lc->loop_depth;
    if (lc->loop_stack) {
        /* Extend stack */
    }
    lc->loop_stack = &frame;
    lc->loop_depth++;

    /* Branch to condition */
    LLVMBuildBr(B, cond_block);

    /* Condition block */
    LLVMPositionBuilderAtEnd(B, cond_block);
    LLVMValueRef cond = llvm_cg_expr(lc, node->data.while_node.condition);
    LLVMTypeRef ctype = LLVMTypeOf(cond);
    if (ctype != LLVMInt1TypeInContext(lc->context)) {
        LLVMValueRef zero = LLVMConstInt(ctype, 0, false);
        cond = LLVMBuildICmp(B, LLVMIntNE, cond, zero, "whilecond");
    }
    LLVMBuildCondBr(B, cond, body_block, after_block);

    /* Body block */
    LLVMPositionBuilderAtEnd(B, body_block);
    if (node->data.while_node.body) {
        llvm_cg_block(lc, node->data.while_node.body);
    }
    /* Branch back to condition */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(B)) == NULL) {
        LLVMBuildBr(B, cond_block);
    }

    /* After block */
    LLVMPositionBuilderAtEnd(B, after_block);

    lc->loop_depth = saved_depth;
}

/* ──────────────────────────────────────────────
 * For loop (range and array iteration)
 * ────────────────────────────────────────────── */
static void cg_for(LlvmCodegen *lc, AstNode *node) {
    LLVMBuilderRef B = lc->builder;
    LLVMValueRef func = lc->current_func;

    /* Determine if this is a range loop or array iteration */
    AstNode *iterable = node->data.for_node.iterable;
    bool is_range = (iterable && iterable->type == NODE_BINARY_OP &&
        (iterable->data.binary.op == BIN_RANGE ||
         iterable->data.binary.op == BIN_RANGE_INCLUSIVE));

    if (is_range) {
        /* Range loop: for i in start..end */
        LLVMValueRef start = llvm_cg_expr(lc, iterable->data.binary.left);
        LLVMValueRef end = llvm_cg_expr(lc, iterable->data.binary.right);
        bool inclusive = (iterable->data.binary.op == BIN_RANGE_INCLUSIVE);

        /* Allocate loop variable */
        LLVMValueRef i_alloca = LLVMBuildAlloca(B, LLVMInt64TypeInContext(lc->context), "");
        LLVMBuildStore(B, start, i_alloca);

        /* Register loop variable */
        StringView var_name = node->data.for_node.var->data.ident.name;
        llvm_declare_local(lc, var_name, i_alloca, LLVMInt64TypeInContext(lc->context), false);

        LLVMBasicBlockRef cond_block = LLVMAppendBasicBlock(func, "for.cond");
        LLVMBasicBlockRef body_block = LLVMAppendBasicBlock(func, "for.body");
        LLVMBasicBlockRef inc_block = LLVMAppendBasicBlock(func, "for.inc");
        LLVMBasicBlockRef after_block = LLVMAppendBasicBlock(func, "for.after");

        LlvmLoopFrame frame = { .break_block = after_block, .continue_block = inc_block };
        lc->loop_stack = &frame;
        lc->loop_depth++;

        LLVMBuildBr(B, cond_block);

        /* Condition */
        LLVMPositionBuilderAtEnd(B, cond_block);
        LLVMValueRef i_val = LLVMBuildLoad2(B, LLVMInt64TypeInContext(lc->context), i_alloca, "");
        LLVMValueRef cond = inclusive
            ? LLVMBuildICmp(B, LLVMIntSLE, i_val, end, "forcond")
            : LLVMBuildICmp(B, LLVMIntSLT, i_val, end, "forcond");
        LLVMBuildCondBr(B, cond, body_block, after_block);

        /* Body */
        LLVMPositionBuilderAtEnd(B, body_block);
        if (node->data.for_node.body) {
            llvm_cg_block(lc, node->data.for_node.body);
        }
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(B)) == NULL) {
            LLVMBuildBr(B, inc_block);
        }

        /* Increment */
        LLVMPositionBuilderAtEnd(B, inc_block);
        LLVMValueRef i_cur = LLVMBuildLoad2(B, LLVMInt64TypeInContext(lc->context), i_alloca, "");
        LLVMValueRef i_next = LLVMBuildAdd(B, i_cur,
            LLVMConstInt(LLVMInt64TypeInContext(lc->context), 1, false), "forinc");
        LLVMBuildStore(B, i_next, i_alloca);
        LLVMBuildBr(B, cond_block);

        /* After */
        LLVMPositionBuilderAtEnd(B, after_block);
        lc->loop_depth--;
    } else {
        /* Array iteration: for val in arr or for i, val in arr */
        LLVMValueRef arr = llvm_cg_expr(lc, iterable);

        /* Allocate index variable */
        LLVMValueRef idx_alloca = LLVMBuildAlloca(B, LLVMInt64TypeInContext(lc->context), "");
        LLVMBuildStore(B, LLVMConstInt(LLVMInt64TypeInContext(lc->context), 0, false), idx_alloca);

        /* Get array length (first 8 bytes) */
        LLVMTypeRef i64_ptr = LLVMPointerType(LLVMInt64TypeInContext(lc->context), 0);
        LLVMValueRef len_ptr = LLVMBuildBitCast(B, arr, i64_ptr, "");
        LLVMValueRef len = LLVMBuildLoad2(B, LLVMInt64TypeInContext(lc->context), len_ptr, "arrlen");

        LLVMBasicBlockRef cond_block = LLVMAppendBasicBlock(func, "for.cond");
        LLVMBasicBlockRef body_block = LLVMAppendBasicBlock(func, "for.body");
        LLVMBasicBlockRef inc_block = LLVMAppendBasicBlock(func, "for.inc");
        LLVMBasicBlockRef after_block = LLVMAppendBasicBlock(func, "for.after");

        LlvmLoopFrame frame = { .break_block = after_block, .continue_block = inc_block };
        lc->loop_stack = &frame;
        lc->loop_depth++;

        LLVMBuildBr(B, cond_block);

        /* Condition */
        LLVMPositionBuilderAtEnd(B, cond_block);
        LLVMValueRef idx = LLVMBuildLoad2(B, LLVMInt64TypeInContext(lc->context), idx_alloca, "");
        LLVMValueRef cond = LLVMBuildICmp(B, LLVMIntSLT, idx, len, "forcond");
        LLVMBuildCondBr(B, cond, body_block, after_block);

        /* Body */
        LLVMPositionBuilderAtEnd(B, body_block);
        /* Load element: arr[idx] — skip the 8-byte length header */
        LLVMValueRef data_ptr = LLVMBuildGEP2(B,
            LLVMArrayType(LLVMInt8TypeInContext(lc->context), 0),
            arr,
            (LLVMValueRef[]){
                LLVMConstInt(LLVMInt64TypeInContext(lc->context), 0, false),
                idx
            }, 2, "elemptr");
        LLVMValueRef elem = LLVMBuildLoad2(B, LLVMInt64TypeInContext(lc->context), data_ptr, "");

        /* Register loop variable */
        StringView var_name = node->data.for_node.var->data.ident.name;
        LLVMValueRef var_alloca = LLVMBuildAlloca(B, LLVMInt64TypeInContext(lc->context), "");
        LLVMBuildStore(B, elem, var_alloca);
        llvm_declare_local(lc, var_name, var_alloca, LLVMInt64TypeInContext(lc->context), false);

        /* If there's an index variable, register it too */
        if (node->data.for_node.index_var) {
            StringView idx_name = node->data.for_node.index_var->data.ident.name;
            LLVMValueRef idx_val = LLVMBuildLoad2(B, LLVMInt64TypeInContext(lc->context), idx_alloca, "");
            LLVMValueRef idx_var_alloca = LLVMBuildAlloca(B, LLVMInt64TypeInContext(lc->context), "");
            LLVMBuildStore(B, idx_val, idx_var_alloca);
            llvm_declare_local(lc, idx_name, idx_var_alloca, LLVMInt64TypeInContext(lc->context), false);
        }

        if (node->data.for_node.body) {
            llvm_cg_block(lc, node->data.for_node.body);
        }
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(B)) == NULL) {
            LLVMBuildBr(B, inc_block);
        }

        /* Increment */
        LLVMPositionBuilderAtEnd(B, inc_block);
        LLVMValueRef idx_cur = LLVMBuildLoad2(B, LLVMInt64TypeInContext(lc->context), idx_alloca, "");
        LLVMValueRef idx_next = LLVMBuildAdd(B, idx_cur,
            LLVMConstInt(LLVMInt64TypeInContext(lc->context), 1, false), "forinc");
        LLVMBuildStore(B, idx_next, idx_alloca);
        LLVMBuildBr(B, cond_block);

        /* After */
        LLVMPositionBuilderAtEnd(B, after_block);
        lc->loop_depth--;
    }
}

/* ──────────────────────────────────────────────
 * Assignment (regular and compound)
 * ────────────────────────────────────────────── */
static void cg_assign(LlvmCodegen *lc, AstNode *node) {
    LLVMBuilderRef B = lc->builder;

    /* The target is the left side of the assignment.
     * It could be an identifier or a field access. */
    AstNode *target = node->data.binary.left;
    AstNode *value_node = node->data.binary.right;
    BinOp op = node->data.binary.op;

    /* Find the alloca for the target */
    LLVMValueRef alloca = NULL;
    if (target->type == NODE_IDENT) {
        StringView name = target->data.ident.name;
        alloca = llvm_lookup_local(lc, name);
        if (!alloca) alloca = llvm_lookup_global(lc, name);
    } else if (target->type == NODE_FIELD_ACCESS) {
        /* Field access target — compute GEP */
        LLVMValueRef obj = llvm_cg_expr(lc, target->data.field.target);
        StringView field_name = target->data.field.field->data.ident.name;

        int field_index = 0;
        if (target->data.field.target->type == NODE_IDENT &&
            target->data.field.target->data.ident.resolved) {
            AstNode *decl = target->data.field.target->data.ident.resolved;
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
        alloca = LLVMBuildGEP2(B,
            LLVMArrayType(LLVMInt8TypeInContext(lc->context), 0),
            obj, indices, 2, "fieldptr");
    }

    if (!alloca) {
        fprintf(stderr, "LLVM: cannot find target for assignment\n");
        return;
    }

    /* Evaluate the value */
    LLVMValueRef val = llvm_cg_expr(lc, value_node);

    /* For compound assignment, load current value, apply op, store back */
    if (op != BIN_ASSIGN) {
        LLVMValueRef current = LLVMBuildLoad2(B, LLVMTypeOf(alloca), alloca, "");
        switch (op) {
            case BIN_ADD_ASSIGN: val = LLVMBuildAdd(B, current, val, "addasgn"); break;
            case BIN_SUB_ASSIGN: val = LLVMBuildSub(B, current, val, "subasgn"); break;
            case BIN_MUL_ASSIGN: val = LLVMBuildMul(B, current, val, "mulasgn"); break;
            case BIN_DIV_ASSIGN: val = LLVMBuildSDiv(B, current, val, "divasgn"); break;
            default: break;
        }
    }

    LLVMBuildStore(B, val, alloca);
}

/* ──────────────────────────────────────────────
 * Defer — push onto LIFO chain
 * ────────────────────────────────────────────── */
static void cg_defer(LlvmCodegen *lc, AstNode *node) {
    /* The deferred body is stored in the node's data.
     * NODE_DEFER has the body in data.defer.body */
    AstNode *body = node->data.defer_node.body;

    LlvmDeferEntry *entry = (LlvmDeferEntry *)arena_alloc(lc->arena, sizeof(LlvmDeferEntry));
    entry->body = body;
    entry->next = lc->defer_head;
    lc->defer_head = entry;
}

/* ──────────────────────────────────────────────
 * Expression statement — evaluate and discard
 * ────────────────────────────────────────────── */
static void cg_expr_stmt(LlvmCodegen *lc, AstNode *node) {
    if (node->data.call.callee) {
        llvm_cg_expr(lc, node->data.call.callee);
    }
}
