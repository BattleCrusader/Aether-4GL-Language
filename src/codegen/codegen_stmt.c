#include "codegen_internal.h"
/* ================================================================
 * Statement codegen
 * ================================================================ */
void cg_stmt(Codegen *cg, AstNode *node, VarSlot *slots);

void cg_stmt(Codegen *cg, AstNode *node, VarSlot *slots) {
    if (!node) return;

    /* Emit source location label for segfault handler (host targets only).
     * The label is in .text so its address is the instruction address.
     * Track it for the source map table emitted at the end. */
    if (cg->target == TARGET_MACHO64 || cg->target == TARGET_ELF64_HOST) {
        if (node->loc.line > 0) {
            cg->label_counter++;
            cg_write_fmt(cg, "_aether_src_%d:\n", cg->label_counter);
            /* Track this entry for the source map table */
            SrcLocEntry *e = (SrcLocEntry *)arena_alloc(cg->arena, sizeof(SrcLocEntry));
            e->label_num = cg->label_counter;
            e->line = node->loc.line;
            e->col = node->loc.col;
            e->file = node->loc.file ? node->loc.file : "?";
            e->next = NULL;
            /* Append to linked list */
            if (!cg->src_loc_list) {
                cg->src_loc_list = e;
            } else {
                SrcLocEntry *last = cg->src_loc_list;
                while (last->next) last = last->next;
                last->next = e;
            }
        }
    }

    switch (node->type) {
        case NODE_RETURN: {
            if (node->data.return_node.value) {
                cg_expr(cg, node->data.return_node.value, slots);
                /* Save return value; defers may clobber rax */
                cg_inst1(cg, "push", "rax");
            }
            /* Emit defers before returning */
            cg_emit_defers(cg, slots);
            if (node->data.return_node.value) {
                /* Restore return value */
                cg_inst1(cg, "pop", "rax");
            }
            cg_inst1(cg, "mov", "rsp, rbp");
            cg_inst1(cg, "pop", "rbp");
            cg_inst(cg, "ret");
            break;
        }

        case NODE_LET: {
            int offset = find_var_offset(slots, node);
            int actual_size = find_var_size(slots, node);
            if (node->data.let_decl.value) {
                cg_expr(cg, node->data.let_decl.value, slots);
                cg_store_var(cg, find_var_slot(slots, node));
            } else {
                /* Zero-initialize */
                char buf[64];
                snprintf(buf, sizeof(buf), "mov qword [rbp%+d], 0", offset);
                cg_inst(cg, buf);
            }
            /* Auto-defer drop() for class-typed variables */
            if (node->data.let_decl.type && node->data.let_decl.type->type == NODE_TYPE_NAMED) {
                char tn[256];
                int nlen = (int)node->data.let_decl.type->data.type_node.name.len;
                if (nlen > 255) nlen = 255;
                memcpy(tn, node->data.let_decl.type->data.type_node.name.data, nlen);
                tn[nlen] = '\0';
                for (StructLayout *sl = struct_layouts; sl; sl = sl->next) {
                    if (strcmp(sl->name, tn) == 0 && sl->is_class) {
                        cg_comment(cg, "auto-defer drop");
                        /* Track auto-drop for scope exit emission */
                        AutoDrop *ad = (AutoDrop *)arena_alloc(cg->arena, sizeof(AutoDrop));
                        ad->class_name = arena_strndup(cg->arena, tn, (size_t)nlen);
                        ad->stack_offset = offset;
                        ad->next = cg->auto_drops;
                        cg->auto_drops = ad;
                        break;
                    }
                }
            }
            break;
        }

        case NODE_EXPR_STMT: {
            cg_expr(cg, node->data.call.callee, slots);
            break;
        }

        case NODE_BLOCK: {
            for (int i = 0; i < node->data.list.count; i++) {
                cg_stmt(cg, node->data.list.items[i], slots);
            }
            break;
        }

        case NODE_IF: {
            /* if let pattern = expr { body } — check optional tag, bind value */
            if (node->data.if_node.is_if_let) {
                int end_lbl = cg_new_label(cg);
                cg_comment(cg, "if let");
                /* Evaluate the optional expression */
                cg_expr(cg, node->data.if_node.condition, slots);
                /* Check the tag byte: optional layout is [tag:byte][value] in a ptr-sized slot.
                   For "none" tag=0, "some" tag=1. Load byte, check if != 0. */
                cg_inst(cg, "mov rcx, rax");
                cg_inst(cg, "and rcx, 0xFF");
                cg_inst1(cg, "test", "rcx, rcx");
                cg_write_fmt(cg, "    jz L_%x\n", end_lbl);
                /* Tag is some (1) — run body */
                if (node->data.if_node.then_block)
                    cg_stmt(cg, node->data.if_node.then_block, slots);
                cg_write_fmt(cg, "L_%x:\n", end_lbl);
                break;
            }

            int else_label = cg_new_label(cg);
            int end_label = cg_new_label(cg);

            cg_expr(cg, node->data.if_node.condition, slots);
            cg_inst1(cg, "test", "rax, rax");
            cg_write_fmt(cg, "    jz L_%x\n", else_label);

            if (node->data.if_node.then_block)
                cg_stmt(cg, node->data.if_node.then_block, slots);

            cg_write_fmt(cg, "    jmp L_%x\n", end_label);
            cg_write_fmt(cg, "L_%x:\n", else_label);

            /* elif chain */
            if (node->data.if_node.elif_chain)
                cg_stmt(cg, node->data.if_node.elif_chain, slots);

            if (node->data.if_node.else_block)
                cg_stmt(cg, node->data.if_node.else_block, slots);

            cg_write_fmt(cg, "L_%x:\n", end_label);
            break;
        }

        case NODE_WHILE: {
            int start_label = cg_new_label(cg);
            int end_label = cg_new_label(cg);
            cg_write_fmt(cg, "L_%x:\n", start_label);
            cg_expr(cg, node->data.while_node.condition, slots);
            cg_inst1(cg, "test", "rax, rax");
            cg_write_fmt(cg, "    jz L_%x\n", end_label);
            if (node->data.while_node.body)
                cg_stmt(cg, node->data.while_node.body, slots);
            cg_write_fmt(cg, "    jmp L_%x\n", start_label);
            cg_write_fmt(cg, "L_%x:\n", end_label);
            break;
        }

        case NODE_FOR: {
            /* Basic: for var in start..end { body }
               Extended: for i, val in arr { body } */
            int start_label = cg_new_label(cg);
            int end_label = cg_new_label(cg);

            /* Check if we have an index variable */
            AstNode *index_var = node->data.for_node.index_var;

            /* If for has iterable, evaluate it */
            if (node->data.for_node.iterable) {
                cg_comment(cg, "for loop");
                /* Iterable is BIN_RANGE: left=start, right=end */
                if (node->data.for_node.iterable->type == NODE_BINARY_OP &&
                    (node->data.for_node.iterable->data.binary.op == BIN_RANGE ||
                     node->data.for_node.iterable->data.binary.op == BIN_RANGE_INCLUSIVE)) {
                    AstNode *range = node->data.for_node.iterable;
                    /* Emit start value to rcx for loop counter */
                    cg_expr(cg, range->data.binary.left, slots);
                    cg_inst1(cg, "mov", "rcx, rax");

                    cg_write_fmt(cg, "L_%x:\n", start_label);
                    /* Compare counter with end */
                    cg_expr(cg, range->data.binary.right, slots);
                    cg_inst1(cg, "cmp", "rcx, rax");
                    cg_write_fmt(cg, "    jge L_%x\n", end_label);

                    /* Store counter to index variable if present */
                    if (index_var) {
                        int off = find_var_offset_by_name(slots,
                            arena_strndup(cg->arena,
                                index_var->data.ident.name.data,
                                index_var->data.ident.name.len));
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov qword [rbp%+d], rcx", off);
                        cg_inst(cg, buf);
                    }

                    /* If there's a loop variable, store counter to it */
                    if (node->data.for_node.var) {
                        int off = find_var_offset_by_name(slots,
                            arena_strndup(cg->arena,
                                node->data.for_node.var->data.ident.name.data,
                                node->data.for_node.var->data.ident.name.len));
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov qword [rbp%+d], rcx", off);
                        cg_inst(cg, buf);
                    }

                    /* Body */
                    if (node->data.for_node.body)
                        cg_stmt(cg, node->data.for_node.body, slots);

                    /* Increment counter */
                    cg_inst1(cg, "inc", "rcx");
                    cg_write_fmt(cg, "    jmp L_%x\n", start_label);
                    cg_write_fmt(cg, "L_%x:\n", end_label);
                } else {
                    /* Array iteration: for i, val in arr { body }
                       Evaluate iterable to get array pointer */
                    cg_expr(cg, node->data.for_node.iterable, slots);
                    cg_inst1(cg, "push", "rax");  /* save array pointer */

                    /* Load array count from header */
                    cg_inst(cg, "mov rax, [rsp]");
                    cg_inst(cg, "mov rcx, [rax]");  /* rcx = count */
                    cg_inst1(cg, "push", "rcx");     /* save count */

                    /* Initialize index to 0 */
                    cg_inst1(cg, "xor", "rcx, rcx");  /* rcx = index */

                    cg_write_fmt(cg, "L_%x:\n", start_label);
                    /* Compare index with count */
                    cg_inst1(cg, "cmp", "rcx, [rsp]");
                    cg_write_fmt(cg, "    jge L_%x\n", end_label);

                    /* Store index to index_var if present */
                    if (index_var) {
                        int off = find_var_offset_by_name(slots,
                            arena_strndup(cg->arena,
                                index_var->data.ident.name.data,
                                index_var->data.ident.name.len));
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov qword [rbp%+d], rcx", off);
                        cg_inst(cg, buf);
                    }

                    /* Load element value: array_ptr + 8 + index * 8 */
                    if (node->data.for_node.var) {
                        cg_inst(cg, "mov rax, [rsp+8]");  /* array pointer */
                        cg_inst(cg, "lea rax, [rax+rcx*8+8]");  /* element address */
                        cg_inst(cg, "mov rax, [rax]");  /* load element value */
                        int off = find_var_offset_by_name(slots,
                            arena_strndup(cg->arena,
                                node->data.for_node.var->data.ident.name.data,
                                node->data.for_node.var->data.ident.name.len));
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov qword [rbp%+d], rax", off);
                        cg_inst(cg, buf);
                    }

                    /* Body */
                    cg_inst1(cg, "push", "rcx");  /* save loop counter */
                    if (node->data.for_node.body)
                        cg_stmt(cg, node->data.for_node.body, slots);
                    cg_inst1(cg, "pop", "rcx");   /* restore loop counter */

                    /* Increment index */
                    cg_inst1(cg, "inc", "rcx");
                    cg_write_fmt(cg, "    jmp L_%x\n", start_label);
                    cg_write_fmt(cg, "L_%x:\n", end_label);
                    cg_inst(cg, "add rsp, 16");  /* pop count and array pointer */
                }
            }
            break;
        }

        case NODE_MATCH: {
            int end_label = cg_new_label(cg);
            cg_comment(cg, "match");
            cg_expr(cg, node->data.match_node.value, slots);
            cg_inst1(cg, "push", "rax");  /* save matched value on stack */

            for (int i = 0; i < node->data.match_node.arms.count; i++) {
                AstNode *arm = node->data.match_node.arms.items[i];
                int next_label = cg_new_label(cg);
                int body_label = cg_new_label(cg);

                /* Reload matched value */
                cg_inst1(cg, "mov", "rax, [rsp]");

                /* Check pattern against rax */
                AstNode *pat = arm->data.match_arm.pattern;
                bool is_wildcard = false;
                if (pat->type == NODE_IDENT && sv_eq_cstr(pat->data.ident.name, "_")) {
                    is_wildcard = true;
                } else if (pat->type == NODE_LITERAL_INT) {
                    char val_buf[32];
                    snprintf(val_buf, sizeof(val_buf), "%llu", (unsigned long long)pat->data.literal.int_val);
                    char tmp[64];
                    snprintf(tmp, sizeof(tmp), "cmp rax, %s", val_buf);
                    cg_inst(cg, tmp);
                    cg_write_fmt(cg, "    je L_%x\n", body_label);
                }
                /* Check additional comma-separated patterns */
                for (int j = 0; j < arm->data.match_arm.patterns.count; j++) {
                    AstNode *extra = arm->data.match_arm.patterns.items[j];
                    if (!extra) continue;
                    if (extra->type == NODE_LITERAL_INT) {
                        char val_buf[32];
                        snprintf(val_buf, sizeof(val_buf), "%llu", (unsigned long long)extra->data.literal.int_val);
                        char tmp[64];
                        snprintf(tmp, sizeof(tmp), "cmp rax, %s", val_buf);
                        cg_inst(cg, tmp);
                        cg_write_fmt(cg, "    je L_%x\n", body_label);
                    }
                }

                if (!is_wildcard) {
                    cg_write_fmt(cg, "    jmp L_%x\n", next_label);
                }

                /* Arm body label */
                cg_write_fmt(cg, "L_%x:\n", body_label);
                if (arm->data.match_arm.body)
                    cg_expr(cg, arm->data.match_arm.body, slots);
                cg_write_fmt(cg, "    jmp L_%x\n", end_label);

                cg_write_fmt(cg, "L_%x:\n", next_label);
                if (i == node->data.match_node.arms.count - 1) {
                    /* Last arm (wildcard or default) */
                    if (arm->data.match_arm.body)
                        cg_expr(cg, arm->data.match_arm.body, slots);
                }
            }

            cg_write_fmt(cg, "L_%x:\n", end_label);
            cg_inst1(cg, "add", "rsp, 8");  /* pop matched value */
            break;
        }

        case NODE_DEFER: {
            /* Push onto the current function's defer stack */
            if (node->data.defer_node.body) {
                cg_defer_push(cg, node->data.defer_node.body);
                cg_comment(cg, "defer pushed");
            }
            break;
        }

        case NODE_REGION: {
            int end_lbl = cg_new_label(cg);
            cg_comment(cg, "region begin");
            /* Save rsp, allocate 4KB arena on stack */
            cg_inst(cg, "mov r15, rsp");          /* save rsp */
            cg_inst(cg, "sub rsp, 4096");          /* arena on stack */
            cg_inst(cg, "mov [rel __aether_region_cur], rsp");
            cg_inst(cg, "lea rax, [rsp + 4096]");
            cg_inst(cg, "mov [rel __aether_region_end], rax");
            /* Run body */
            if (node->data.region_node.body)
                cg_stmt(cg, node->data.region_node.body, slots);
            /* Region teardown: restore rsp */
            cg_write_fmt(cg, "L_%x:\n", end_lbl);
            cg_inst(cg, "mov rsp, r15");
            cg_inst(cg, "xor rax, rax");
            cg_inst(cg, "mov [rel __aether_region_cur], rax");
            cg_inst(cg, "mov [rel __aether_region_end], rax");
            cg_comment(cg, "region end");
            break;
        }

        case NODE_TRY: {
            /* try { body } catch Type(var) { handler } ... finally { body }
             *
             * Proper deterministic exception handling with full stack unwinding:
             * - rdx = 0 means success, rdx = 1 means error
             * - rax holds the error discriminant/value on error
             * - Scope cleanup table tracks destructors + defers per scope level
             * - throw walks the cleanup table (innermost first) before jumping
             * - finally blocks execute regardless of success/failure
             * - catch-all (_) matches any error
             * - Unmatched errors propagate (re-throw)
             * - For host targets: sigsetjmp/siglongjmp catches hardware faults
             *   (segfaults, bus errors) and redirects to the catch handler */
            int catch_label = cg_new_label(cg);
            int end_label = cg_new_label(cg);
            int finally_label = cg_new_label(cg);
            int segfault_check_label = cg_new_label(cg);
            bool has_finally = (node->data.try_node.finally_body != NULL);
            cg_comment(cg, "try begin");

            /* Save current cleanup depth so we can restore it after the try */
            int saved_cleanup_depth = cg->cleanup_depth;

            /* For host targets: set up sigsetjmp to catch hardware faults */
            bool isHost = (cg->target == TARGET_MACHO64 || cg->target == TARGET_ELF64_HOST);
            if (isHost) {
                cg_comment(cg, "sigsetjmp for hardware fault catch");
                if (cg->target == TARGET_MACHO64) {
                    cg_inst(cg, "lea rdi, [rel __aether_segfault_jmpbuf]");
                    cg_inst(cg, "call _aether_setJmpBuf");
                } else {
                    cg_inst(cg, "lea rdi, [rel __aether_segfault_jmpbuf]");
                    cg_inst(cg, "call aether_setJmpBuf");
                }
                /* Check return value: 0 = first call (normal), non-zero = siglongjmp (segfault caught) */
                cg_inst(cg, "test eax, eax");
                cg_write_fmt(cg, "    jnz L_%x\n", catch_label);
            }

            /* Clear error tag before try body */
            cg_inst(cg, "xor rdx, rdx");

            /* Emit try body */
            if (node->data.try_node.body)
                cg_stmt(cg, node->data.try_node.body, slots);

            /* Clear segfault jump buffer (no longer in try block) */
            if (isHost) {
                cg_comment(cg, "clear segfault jmpbuf");
                if (cg->target == TARGET_MACHO64) {
                    cg_inst(cg, "call _aether_clearJmpBuf");
                } else {
                    cg_inst(cg, "call aether_clearJmpBuf");
                }
            }

            /* Check error tag (rdx = 0 success, 1 = error) */
            cg_inst(cg, "test rdx, rdx");
            cg_write_fmt(cg, "    jnz L_%x\n", catch_label);
            /* Success path: jump to finally (or end if no finally) */
            if (has_finally) {
                cg_write_fmt(cg, "    jmp L_%x\n", finally_label);
            } else {
                cg_write_fmt(cg, "    jmp L_%x\n", end_label);
            }

            /* Catch handlers */
            cg_write_fmt(cg, "L_%x:\n", catch_label);
            cg_comment(cg, "catch handlers");

            /* Walk cleanup table for this try block */
            cg_comment(cg, "scope cleanup for try body");
            cg_emit_cleanup_range(cg, slots, saved_cleanup_depth, cg->cleanup_depth);

            /* Save error discriminant */
            cg_inst(cg, "push rax");

            /* Match error discriminant against catch arms */
            bool has_catch_all = false;
            for (int i = 0; i < node->data.try_node.catch_arms.count; i++) {
                AstNode *arm = node->data.try_node.catch_arms.items[i];
                int next_arm = cg_new_label(cg);

                if (arm->data.catch_arm.is_catch_all) {
                    has_catch_all = true;
                    cg_comment(cg, "catch-all");
                } else if (arm->data.catch_arm.type) {
                    char type_name[256];
                    int tlen = (int)arm->data.catch_arm.type->data.type_node.name.len;
                    if (tlen > 255) tlen = 255;
                    memcpy(type_name, arm->data.catch_arm.type->data.type_node.name.data, tlen);
                    type_name[tlen] = '\0';

                    int disc_val = -1;
                    for (VariantEntry *ve = variant_entries; ve; ve = ve->next) {
                        if (strcmp(ve->name, type_name) == 0) {
                            disc_val = ve->discriminant;
                            break;
                        }
                    }

                    if (disc_val >= 0) {
                        cg_inst(cg, "mov rax, [rsp]");
                        char buf[64];
                        snprintf(buf, sizeof(buf), "cmp rax, %d", disc_val);
                        cg_inst(cg, buf);
                        cg_write_fmt(cg, "    jne L_%x\n", next_arm);
                    }
                }

                if (arm->data.catch_arm.var) {
                    cg_comment(cg, "bind catch variable");
                    VarSlot *vs = find_var_slot(slots, arm->data.catch_arm.var);
                    if (vs) {
                        cg_inst(cg, "pop rax");
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov qword [rbp%+d], rax", vs->stack_offset);
                        cg_inst(cg, buf);
                    } else {
                        cg_inst(cg, "add rsp, 8");
                    }
                } else {
                    if (!arm->data.catch_arm.is_catch_all || i < node->data.try_node.catch_arms.count - 1) {
                        cg_inst(cg, "add rsp, 8");
                    }
                }

                if (arm->data.catch_arm.body)
                    cg_stmt(cg, arm->data.catch_arm.body, slots);

                cg_inst(cg, "xor rdx, rdx");

                if (has_finally) {
                    cg_write_fmt(cg, "    jmp L_%x\n", finally_label);
                } else {
                    cg_write_fmt(cg, "    jmp L_%x\n", end_label);
                }

                cg_write_fmt(cg, "L_%x:\n", next_arm);
            }

            /* No catch matched — re-throw (propagate error to caller) */
            if (!has_catch_all) {
                cg_comment(cg, "no catch matched — re-throw");
                cg_inst(cg, "add rsp, 8");
                cg_emit_defers(cg, slots);
                cg_inst(cg, "mov rsp, rbp");
                cg_inst(cg, "pop rbp");
                cg_inst(cg, "ret");
            }

            /* Finally block */
            if (has_finally) {
                cg_write_fmt(cg, "L_%x:\n", finally_label);
                cg_comment(cg, "finally");
                if (node->data.try_node.finally_body)
                    cg_stmt(cg, node->data.try_node.finally_body, slots);
            }

            cg_write_fmt(cg, "L_%x:\n", end_label);
            cg_comment(cg, "try end");

            cg->cleanup_depth = saved_cleanup_depth;
            break;
        }

        case NODE_RUN_BLOCK: {
            /* #run blocks execute at compile time — no runtime code generated.
             * The body is evaluated during semantic analysis for constant folding.
             * At codegen time, we emit nothing (the results were already embedded). */
            cg_comment(cg, "#run block (compile-time only)");
            break;
        }

        case NODE_THROW: {
            /* throw expr — evaluate the expression, set error tag to 1,
             * walk the cleanup table (innermost first), then either jump
             * to the nearest catch handler or return with rdx=1. */
            AstNode *val = node->data.throw_node.value;
            cg_comment(cg, "throw");
            if (val) {
                cg_expr(cg, val, slots);
            } else {
                cg_inst(cg, "xor rax, rax");  /* default error discriminant */
            }
            /* Set error tag (rdx = 1) */
            cg_inst(cg, "mov rdx, 1");
            /* Emit defers before leaving the function */
            cg_emit_defers(cg, slots);
            /* Epilogue: restore stack and return */
            cg_inst(cg, "mov rsp, rbp");
            cg_inst(cg, "pop rbp");
            cg_inst(cg, "ret");
            break;
        }

        case NODE_UNSAFE: {
            /* unsafe { body } — emit body with unsafe comment marker */
            cg_comment(cg, "unsafe block begin");
            if (node->data.list.count > 0) {
                for (int i = 0; i < node->data.list.count; i++) {
                    cg_stmt(cg, node->data.list.items[i], slots);
                }
            }
            cg_comment(cg, "unsafe block end");
            break;
        }

        case NODE_ASM_BLOCK: {
            /* Emit raw assembly text directly into the output */
            if (node->data.asm_block.text) {
                StringView asm_text = node->data.asm_block.text->data.literal.string_val;
                if (asm_text.len > 0) {
                    cg_write(cg, "; begin asm block\n");
                    /* Write each line, substituting [varName] with [rbp+offset] */
                    const char *p = asm_text.data;
                    const char *end = p + asm_text.len;
                    while (p < end) {
                        const char *line_start = p;
                        while (p < end && *p != '\n') p++;
                        if (p > line_start) {
                            /* Skip extern lines — they're hoisted to top level */
                            const char *s = line_start;
                            while (s < p && (*s == ' ' || *s == '\t')) s++;
                            if ((p - s) >= 6 && strncmp(s, "extern", 6) == 0) {
                                /* skip — already emitted at top */
                            } else {
                                /* Strip Aether comments (//) from asm block output */
                                const char *s_trim = line_start;
                                while (s_trim < p && (*s_trim == ' ' || *s_trim == '\t')) s_trim++;
                                if (s_trim + 1 < p && s_trim[0] == '/' && s_trim[1] == '/') {
                                    /* Skip this line entirely — it's an Aether comment */
                                } else {
                                    /* Process line: substitute [varName] with [rbp+offset] */
                                    char line_buf[1024];
                                    int buf_pos = 0;
                                    const char *cp = line_start;
                                    while (cp < p && buf_pos < (int)sizeof(line_buf) - 8) {
                                        if (*cp == '[') {
                                            /* Look for matching ']' */
                                            const char *rb = cp + 1;
                                            while (rb < p && *rb != ']' && *rb != ' ' && *rb != '\t' && *rb != '\n') rb++;
                                            if (rb < p && *rb == ']') {
                                                size_t vlen = rb - (cp + 1);
                                                if (vlen > 0 && vlen < 256) {
                                                    char vname[256];
                                                    memcpy(vname, cp + 1, vlen);
                                                    vname[vlen] = '\0';
                                                    /* Check if this name matches a known variable slot */
                                                    VarSlot *vs = slots;
                                                    bool found = false;
                                                    while (vs) {
                                                        if (strcmp(vs->name, vname) == 0) {
                                                            int n = snprintf(line_buf + buf_pos, sizeof(line_buf) - buf_pos, "[rbp%+d]", vs->stack_offset);
                                                            if (n > 0) buf_pos += n;
                                                            cp = rb + 1;
                                                            found = true;
                                                            break;
                                                        }
                                                        vs = vs->next;
                                                    }
                                                    if (found) continue;
                                                }
                                            }
                                        }
                                        line_buf[buf_pos++] = *cp++;
                                    }
                                    line_buf[buf_pos] = '\0';
                                    cg_write_fmt(cg, "%s\n", line_buf);
                                }
                            }
                        } else {
                            cg_write(cg, "\n");
                        }
                        if (p < end) p++;
                    }
                    cg_write(cg, "; end asm block\n");

                    /* After asm block, store outputs from registers back to stack */
                    if (node->data.asm_block.outputs.count > 0) {
                        const char *regs[] = {"rax", "rdx"};
                        for (int i = 0; i < node->data.asm_block.outputs.count && i < 2; i++) {
                            AstNode *out_var = node->data.asm_block.outputs.items[i];
                            StringView oname = out_var->data.ident.name;
                            /* Null-terminate for lookup */
                            char vname[256];
                            int vlen = oname.len < 255 ? (int)oname.len : 255;
                            memcpy(vname, oname.data, vlen);
                            vname[vlen] = '\0';
                            int offset = find_var_offset_by_name(slots, vname);
                            cg_indent(cg);
                            cg_write_fmt(cg, "mov [rbp%+d], %s\n", offset, regs[i]);
                        }
                    }
                }
            }
            break;
        }

        case NODE_POOL_DECL: {
            cg_comment(cg, "pool declaration (reserved)");
            break;
        }

        case NODE_PROTOCOL_DECL: {
            cg_comment(cg, "protocol declaration (interface)");
            break;
        }

        default:
            cg_warn(cg, node, "unsupported statement in codegen");
            break;
    }
}