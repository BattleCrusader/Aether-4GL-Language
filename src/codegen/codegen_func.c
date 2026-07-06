#include "codegen_internal.h"

/* ================================================================
 * Function-level code generation (2-pass: frame then body)
 * ================================================================ */

const char *entry_point_name(Target t) {
    switch (t) {
        case TARGET_MACHO64:    return "_main";
        case TARGET_ELF64_HOST: return "_start";
        default:                return "_start";
    }
}

static const char *global_prefix(Target t) {
    (void)t;
    /* NASM handles underscore prefix automatically for Mach-O.
     * For ELF, no prefix is needed either. */
    return "";
}

void cg_func(Codegen *cg, AstNode *func) {
    int frame_size = 0;
    VarSlot *slots = compute_frame(cg->arena, func, &frame_size);

    char fn_label[256];
    const char *gpref = global_prefix(cg->target);
    snprintf(fn_label, sizeof(fn_label), "%s%.*s",
        gpref,
        (int)func->data.func.name->data.ident.name.len,
        func->data.func.name->data.ident.name.data);

    cg_write_fmt(cg, "; function %s (frame=%d)\n", fn_label, frame_size);
    /* For @layout functions, skip label and prologue — the asm block is the entire output */
    if (!func->data.func.has_layout) {
        cg_write_fmt(cg, "global %s\n", fn_label);
        cg_write_fmt(cg, "%s:\n", fn_label);
    }

    /* Prologue — skip for @layout functions (flat binary boot sectors, etc.) */
    if (!func->data.func.has_layout) {
        cg_inst1(cg, "push", "rbp");
        cg_inst1(cg, "mov", "rbp, rsp");

        /* Allocate stack frame */
        if (frame_size > 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "sub rsp, %d", frame_size);
            cg_inst(cg, buf);
        }

        /* Save incoming args from registers to their stack slots */
        for (int i = 0; i < func->data.func.params.count && i < 6; i++) {
            const char *regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
            AstNode *param = func->data.func.params.items[i];
            int offset = find_var_offset(slots, param);
            char buf[64];
            snprintf(buf, sizeof(buf), "mov [rbp%+d], %s", offset, regs[i]);
            cg_inst(cg, buf);
        }
        /* Args > 6 are on the stack already; they're at rbp+16, rbp+24, etc.
           For now we only handle up to 6 args. */
    }

    /* Pre-conditions: check before body */
    if (func->data.func.pre_conditions.count > 0) {
        cg_comment(cg, "pre-conditions");
        for (int i = 0; i < func->data.func.pre_conditions.count; i++) {
            int fail_label = cg_new_label(cg);
            int end_label = cg_new_label(cg);
            cg_expr(cg, func->data.func.pre_conditions.items[i], slots);
            cg_inst(cg, "test rax, rax");
            cg_write_fmt(cg, "    jnz L_%x\n", end_label);
            cg_write_fmt(cg, "L_%x:\n", fail_label);
            cg_comment(cg, "pre-condition failed — panic");
            cg_inst(cg, "mov rdi, 1");  /* exit code 1 */
            cg_inst(cg, "mov rax, 0x2000001");  /* macOS exit */
            cg_inst(cg, "syscall");
            cg_write_fmt(cg, "L_%x:\n", end_label);
        }
    }

    /* Body */
    if (func->data.func.body) {
        cg_stmt(cg, func->data.func.body, slots);
    }

    /* Default return — only if the body didn't already return */
    /* Check if the last statement is a return */
    int body_has_return = 0;
    if (func->data.func.body && func->data.func.body->type == NODE_RETURN) {
        body_has_return = 1;
    }
    /* Also check if body is a block whose last statement is a return or asm block */
    if (func->data.func.body && func->data.func.body->type == NODE_BLOCK) {
        AstNodeList *stmts = &func->data.func.body->data.list;
        if (stmts->count > 0) {
            AstNode *last = stmts->items[stmts->count - 1];
            if (last->type == NODE_RETURN) {
                body_has_return = 1;
            }
            /* If the last statement is an asm block, check if it contains ret */
            if (last->type == NODE_ASM_BLOCK && last->data.asm_block.text) {
                StringView asm_text = last->data.asm_block.text->data.literal.string_val;
                const char *p = asm_text.data;
                const char *end = p + asm_text.len;
                while (p < end) {
                    if (p[0] == 'r' && p[1] == 'e' && p[2] == 't' &&
                        (end - p == 3 || p[3] == '\n' || p[3] == ' ' || p[3] == '\t' || p[3] == '\r')) {
                        body_has_return = 1;
                        break;
                    }
                    p++;
                }
            }
        }
    }
    /* Also check if body itself is an asm block with ret */
    if (func->data.func.body && func->data.func.body->type == NODE_ASM_BLOCK) {
        AstNode *last = func->data.func.body;
        if (last->data.asm_block.text) {
            StringView asm_text = last->data.asm_block.text->data.literal.string_val;
            const char *p = asm_text.data;
            const char *end = p + asm_text.len;
            while (p < end) {
                if (p[0] == 'r' && p[1] == 'e' && p[2] == 't' &&
                    (end - p == 3 || p[3] == '\n' || p[3] == ' ' || p[3] == '\t' || p[3] == '\r')) {
                    body_has_return = 1;
                    break;
                }
                p++;
            }
        }
    }
    if (!body_has_return) {
        cg_comment(cg, "default return");
        /* Check if body is/was an asm block — if so, asm already set rax */
        int body_is_asm = 0;
        if (func->data.func.body && func->data.func.body->type == NODE_ASM_BLOCK) {
            body_is_asm = 1;
        } else if (func->data.func.body && func->data.func.body->type == NODE_BLOCK) {
            AstNodeList *stmts = &func->data.func.body->data.list;
            if (stmts->count > 0 && stmts->items[stmts->count - 1]->type == NODE_ASM_BLOCK) {
                body_is_asm = 1;
            }
        }
        if (!body_is_asm && func->data.func.return_type) {
            cg_inst(cg, "xor rax, rax");  /* default return value = 0 */
        }
        if (func->data.func.is_throws) {
            cg_inst(cg, "xor rdx, rdx");  /* clear error tag = success */
        }
    }

    /* Post-conditions: check before defers (save return value first) */
    if (func->data.func.post_conditions.count > 0) {
        cg_comment(cg, "post-conditions");
        cg_inst(cg, "push rax");  /* save return value */
        for (int i = 0; i < func->data.func.post_conditions.count; i++) {
            int fail_label = cg_new_label(cg);
            int end_label = cg_new_label(cg);
            cg_expr(cg, func->data.func.post_conditions.items[i], slots);
            cg_inst(cg, "test rax, rax");
            cg_write_fmt(cg, "    jnz L_%x\n", end_label);
            cg_write_fmt(cg, "L_%x:\n", fail_label);
            cg_comment(cg, "post-condition failed — panic");
            cg_inst(cg, "mov rdi, 1");
            cg_inst(cg, "mov rax, 0x2000001");
            cg_inst(cg, "syscall");
            cg_write_fmt(cg, "L_%x:\n", end_label);
        }
        cg_inst(cg, "pop rax");  /* restore return value */
    }

    cg_emit_defers(cg, slots);
    if (!func->data.func.has_layout) {
        cg_inst1(cg, "mov", "rsp, rbp");
        cg_inst1(cg, "pop", "rbp");
        cg_inst(cg, "ret");
    }
    cg_write(cg, "\n");

    cg_defer_clear(cg);
}