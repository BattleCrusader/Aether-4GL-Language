#include "codegen_internal.h"

/* ================================================================
 * Expression codegen
 * ================================================================ */

void cg_expr(Codegen *cg, AstNode *node, VarSlot *slots);

void cg_expr(Codegen *cg, AstNode *node, VarSlot *slots) {
    if (!node) return;

    switch (node->type) {
        case NODE_LITERAL_INT: {
            uint64_t val = node->data.literal.int_val;
            char buf[32];
            snprintf(buf, sizeof(buf), "mov rax, %llu", (unsigned long long)val);
            cg_inst(cg, buf);
            break;
        }

        case NODE_LITERAL_FLOAT: {
            /* Float literal: store the 64-bit pattern in the constant pool and load it.
               For now, just emit the integer pattern of the float bits. */
            uint64_t val;
            memcpy(&val, &node->data.literal.float_val, sizeof(val));
            char buf[64];
            snprintf(buf, sizeof(buf), "mov rax, 0x%016llx", (unsigned long long)val);
            cg_inst(cg, buf);
            break;
        }

        case NODE_LITERAL_BOOL:
            cg_inst1(cg, "mov", node->data.literal.bool_val ? "rax, 1" : "rax, 0");
            break;

        case NODE_LITERAL_NONE:
            cg_inst1(cg, "xor", "rax, rax");
            break;

        case NODE_LITERAL_STRING: {
            /* Emit lea to string in .rodata */
            const char *label = cg_emit_string(cg, node->data.literal.string_val);
            if (!label) { cg_warn(cg, node, "cg_emit_string failed"); break; }
            char buf[128];
            snprintf(buf, sizeof(buf), "lea rax, [rel %s]", label);
            cg_inst(cg, buf);
            break;
        }

        case NODE_LITERAL_CHAR:
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "mov eax, %u", (unsigned)node->data.literal.char_val);
                cg_inst(cg, buf);
            }
            break;

        case NODE_IDENT: {
            /* Check if this ident resolves to a const declaration */
            if (node->data.ident.resolved && node->data.ident.resolved->type == NODE_CONST_DECL) {
                /* Const value was folded to a literal by semantic analysis */
                AstNode *val = node->data.ident.resolved->data.let_decl.value;
                if (val && val->type == NODE_LITERAL_INT) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "mov rax, %llu",
                        (unsigned long long)val->data.literal.int_val);
                    cg_inst(cg, buf);
                    break;
                }
                cg_inst1(cg, "mov", "rax, 0");
                break;
            }
            int offset = -8;
            int actual_size = 8;
            if (node->data.ident.resolved) {
                offset = find_var_offset(slots, node->data.ident.resolved);
                actual_size = find_var_size(slots, node->data.ident.resolved);
            } else {
                const char *vname = arena_strndup(cg->arena,
                    node->data.ident.name.data, node->data.ident.name.len);
                offset = find_var_offset_by_name(slots, vname);
                actual_size = find_var_size_by_name(slots, vname);
            }
            cg_load_var(cg, find_var_slot(slots, node->data.ident.resolved));
            break;
        }

        case NODE_FIELD_ACCESS: {
            /* s.field — load based on struct var's stack offset + field offset */
            cg_comment(cg, "field access");
            if (node->data.field.target && node->data.field.target->type == NODE_IDENT) {
                int var_offset = -8;
                if (node->data.field.target->data.ident.resolved)
                    var_offset = find_var_offset(slots, node->data.field.target->data.ident.resolved);
                
                /* Find the struct type of this variable */
                const char *struct_name = NULL;
                AstNode *decl = node->data.field.target->data.ident.resolved;
                if (decl && (decl->type == NODE_LET || decl->type == NODE_PARAM)) {
                    AstNode *type_node = decl->type == NODE_LET ? decl->data.let_decl.type : decl->data.param.type;
                    if (type_node && type_node->type == NODE_TYPE_NAMED) {
                        struct_name = arena_strndup(cg->arena, type_node->data.type_node.name.data, type_node->data.type_node.name.len);
                    }
                }
                
                if (struct_name) {
                    StructLayout *sl = find_struct_layout(struct_name);
                    if (sl && node->data.field.field && node->data.field.field->type == NODE_IDENT) {
                        int field_off = find_field_offset(sl,
                            arena_strndup(cg->arena,
                                node->data.field.field->data.ident.name.data,
                                node->data.field.field->data.ident.name.len));
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov rax, [rbp%+d]", var_offset + field_off);
                        cg_inst(cg, buf);
                    }
                }
            }
            break;
        }

        case NODE_INDEX: {
            /* array[index] — compute element address */
            cg_comment(cg, "array index");
            /* Push index, then array base address, compute offset = base + index * elem_size */
            cg_expr(cg, node->data.index.index, slots);
            cg_inst1(cg, "push", "rax");
            cg_expr(cg, node->data.index.target, slots);
            cg_inst1(cg, "pop", "rcx");   /* rcx = index */
            /* Compute element offset: rax = base, rcx = index, result = base + index * elem_size */
            /* Determine element size from the target type */
            int elem_size = 8; /* default */
            if (node->data.index.target->type == NODE_TYPE_ARRAY && node->data.index.target->data.type_node.elem_type) {
                elem_size = type_size(node->data.index.target->data.type_node.elem_type);
            } else if (node->data.index.target->type == NODE_TYPE_SLICE && node->data.index.target->data.type_node.elem_type) {
                elem_size = type_size(node->data.index.target->data.type_node.elem_type);
            } else if (node->data.index.target->type == NODE_IDENT) {
                /* Try to infer from the variable's declared type */
                /* For now, check if the variable has a type annotation */
                /* We'll look up the let declaration */
                if (is_string_expr(node->data.index.target)) {
                    elem_size = 1;
                }
            }
            /* Scale index by element size */
            switch (elem_size) {
                case 1: cg_inst(cg, "add rax, rcx"); break;
                case 2: cg_inst(cg, "shl rcx, 1"); cg_inst1(cg, "add", "rax, rcx"); break;
                case 4: cg_inst(cg, "shl rcx, 2"); cg_inst1(cg, "add", "rax, rcx"); break;
                case 8: cg_inst(cg, "shl rcx, 3"); cg_inst1(cg, "add", "rax, rcx"); break;
                default:
                    cg_inst(cg, "shl rcx, 3"); cg_inst1(cg, "add", "rax, rcx"); break;
            }
            /* Read element from computed address */
            switch (elem_size) {
                case 1: cg_inst(cg, "movzx eax, byte [rax]"); break;
                case 2: cg_inst(cg, "movzx eax, word [rax]"); break;
                case 4: cg_inst(cg, "mov eax, [rax]"); break;
                default: cg_inst(cg, "mov rax, [rax]"); break;
            }
            break;
        }

        case NODE_SLICE: {
            cg_warn(cg, node, "slice not yet implemented");
            cg_inst1(cg, "xor", "rax, rax");
            break;
        }

        case NODE_ARRAY_LIT: {
            /* Array literal: [1, 2, 3]
               Layout on stack: [8 bytes: count][elem0][elem1]...[elemN]
               Returns pointer to the array (address of count field) in rax. */
            int count = node->data.array_lit.elements.count;
            if (count == 0) {
                cg_inst1(cg, "xor", "rax, rax");
                break;
            }

            /* Determine element size from first element's type */
            int elem_size = 8; /* default */
            if (count > 0) {
                AstNode *first = node->data.array_lit.elements.items[0];
                /* Try to infer element size from the expression type */
                if (first->type == NODE_LITERAL_INT) elem_size = 8;
                else if (first->type == NODE_LITERAL_BOOL) elem_size = 1;
                else if (first->type == NODE_LITERAL_CHAR) elem_size = 4;
                else if (first->type == NODE_LITERAL_FLOAT) elem_size = 8;
            }

            /* Total size: 8 bytes for count + count * elem_size, rounded up to 16 */
            int total_size = 8 + count * elem_size;
            int aligned_size = ((total_size + 15) / 16) * 16;

            /* Allocate stack space: sub rsp, aligned_size */
            char buf[64];
            snprintf(buf, sizeof(buf), "sub rsp, %d", aligned_size);
            cg_inst(cg, buf);

            /* Store count header at [rsp] */
            snprintf(buf, sizeof(buf), "mov qword [rsp], %d", count);
            cg_inst(cg, buf);

            /* Evaluate each element and store it */
            int offset = 8; /* start after count header */
            for (int i = 0; i < count; i++) {
                cg_expr(cg, node->data.array_lit.elements.items[i], slots);
                switch (elem_size) {
                    case 1:
                        snprintf(buf, sizeof(buf), "mov byte [rsp+%d], al", offset);
                        break;
                    case 2:
                        snprintf(buf, sizeof(buf), "mov word [rsp+%d], ax", offset);
                        break;
                    case 4:
                        snprintf(buf, sizeof(buf), "mov dword [rsp+%d], eax", offset);
                        break;
                    default:
                        snprintf(buf, sizeof(buf), "mov qword [rsp+%d], rax", offset);
                        break;
                }
                cg_inst(cg, buf);
                offset += elem_size;
            }

            /* Return array pointer in rax */
            cg_inst1(cg, "mov", "rax, rsp");
            break;
        }

        case NODE_BINARY_OP: {
            /* Right side first (push), then left (in rax).
               After this block: rax=left, rcx=right */
            cg_expr(cg, node->data.binary.right, slots);
            cg_inst1(cg, "push", "rax");

            /* For plain assignment, skip left evaluation — it would overwrite rax.
               For compound assignments (+= etc.), evaluate left to get current value. */
            if (node->data.binary.op == BIN_ASSIGN) {
                /* Assignment: right side is in rax (pushed above), pop it back */
                cg_inst1(cg, "pop", "rax");
            } else {
                cg_expr(cg, node->data.binary.left, slots);
                cg_inst1(cg, "pop", "rcx");   /* rcx = right, rax = left */
            }

            switch (node->data.binary.op) {
                case BIN_ADD: {
                    /* If either operand is a string, do concat instead of numeric add */
                    bool left_str = is_string_expr(node->data.binary.left);
                    bool right_str = is_string_expr(node->data.binary.right);
                    if (left_str || right_str) {
                        cg_comment(cg, "string concat (+)");
                        /* rax = left, rcx = right. Save rcx before itoa (clobbers rcx). */
                        bool left_num = is_numeric_expr(node->data.binary.left);
                        bool right_num = is_numeric_expr(node->data.binary.right);
                        if (left_num || right_num) {
                            cg_inst1(cg, "push", "rcx");
                        }
                        if (left_num) {
                            cg_inst1(cg, "mov", "rdi, rax");
                            cg_inst(cg, "call __aether_itoa");
                        }
                        if (right_num) {
                            cg_inst1(cg, "push", "rax");
                            cg_inst1(cg, "pop", "rdi");
                            cg_inst1(cg, "pop", "rax");
                            cg_inst1(cg, "push", "rdi");
                            cg_inst1(cg, "mov", "rdi, rax");
                            cg_inst(cg, "call __aether_itoa");
                            cg_inst1(cg, "mov", "rcx, rax");
                            cg_inst1(cg, "pop", "rax");
                        } else if (left_num) {
                            cg_inst1(cg, "pop", "rcx");
                        }
                        cg_inst1(cg, "push", "rax");
                        cg_inst1(cg, "push", "rcx");
                        cg_inst(cg, "call __aether_concat");
                        cg_inst(cg, "add rsp, 16");
                        break;
                    }
                    cg_inst1(cg, "add", "rax, rcx");
                    break;
                }
                case BIN_SUB: cg_inst1(cg, "sub",  "rax, rcx"); break;
                case BIN_MUL: cg_inst1(cg, "mul",  "rcx"); break;
                case BIN_POWER: {
                    /* Power: rax = rax ** rcx (left = base, right = exponent) */
                    int pow_start = cg_new_label(cg);
                    int pow_done = cg_new_label(cg);
                    cg_comment(cg, "power operator **");
                    /* Save base (rax) and exponent (rcx) */
                    cg_inst1(cg, "push", "rcx");   /* save exponent */
                    cg_inst1(cg, "push", "rax");   /* save base */
                    /* Load base into rcx for mul */
                    cg_inst(cg, "mov rcx, [rsp]");
                    /* Initialize result = 1 */
                    cg_inst1(cg, "mov", "rax, 1");
                    /* Load exponent into r8 (mul clobbers rdx) */
                    cg_inst(cg, "mov r8, [rsp+8]");
                    /* If exponent == 0, skip loop */
                    cg_inst(cg, "test r8, r8");
                    cg_write_fmt(cg, "    jz L_%x\n", pow_done);
                    cg_write_fmt(cg, "L_%x:\n", pow_start);
                    /* Multiply result by base (rcx) */
                    cg_inst1(cg, "mul", "rcx");
                    /* Decrement exponent */
                    cg_inst1(cg, "dec", "r8");
                    cg_write_fmt(cg, "    jnz L_%x\n", pow_start);
                    cg_write_fmt(cg, "L_%x:\n", pow_done);
                    cg_inst(cg, "add rsp, 16");  /* pop base and exponent */
                    break;
                }
                case BIN_DIV: cg_inst(cg, "xor rdx, rdx"); cg_inst1(cg, "div", "rcx"); break;
                case BIN_MOD: cg_inst(cg, "xor rdx, rdx"); cg_inst1(cg, "div", "rcx"); cg_inst1(cg, "mov", "rax, rdx"); break;
                case BIN_EQ:  cg_inst1(cg, "cmp",  "rax, rcx"); cg_inst(cg, "sete al");  cg_inst(cg, "movzx rax, al"); break;
                case BIN_NEQ: cg_inst1(cg, "cmp",  "rax, rcx"); cg_inst(cg, "setne al"); cg_inst(cg, "movzx rax, al"); break;
                case BIN_LT:  cg_inst1(cg, "cmp",  "rax, rcx"); cg_inst(cg, "setl al");  cg_inst(cg, "movzx rax, al"); break;
                case BIN_GT:  cg_inst1(cg, "cmp",  "rax, rcx"); cg_inst(cg, "setg al");  cg_inst(cg, "movzx rax, al"); break;
                case BIN_LE:  cg_inst1(cg, "cmp",  "rax, rcx"); cg_inst(cg, "setle al"); cg_inst(cg, "movzx rax, al"); break;
                case BIN_GE:  cg_inst1(cg, "cmp",  "rax, rcx"); cg_inst(cg, "setge al"); cg_inst(cg, "movzx rax, al"); break;
                /* Bitwise */
                case BIN_BIT_AND: cg_inst1(cg, "and", "rax, rcx"); break;
                case BIN_BIT_OR:  cg_inst1(cg, "or",  "rax, rcx"); break;
                case BIN_BIT_XOR: cg_inst1(cg, "xor", "rax, rcx"); break;
                case BIN_SHL:     cg_inst1(cg, "shl", "rax, cl"); break;
                case BIN_SHR:     cg_inst1(cg, "shr", "rax, cl"); break;
                /* Assignment: x = expr — store result to variable's stack slot */
                case BIN_ASSIGN: {
                    cg_comment(cg, "assign");
                    /* Right side already in rax, left side is the target ident */
                    if (node->data.binary.left && node->data.binary.left->type == NODE_IDENT) {
                        int off = find_var_offset_by_name(slots,
                            arena_strndup(cg->arena,
                                node->data.binary.left->data.ident.name.data,
                                node->data.binary.left->data.ident.name.len));
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov qword [rbp%+d], rax", off);
                        cg_inst(cg, buf);
                    }
                    break;
                }
                /* Compound assignment: x += expr etc — left value in rax, right in rcx */
                case BIN_ADD_ASSIGN: {
                    cg_comment(cg, "+=");
                    bool left_is_str = false;
                    if (node->data.binary.left && node->data.binary.left->type == NODE_IDENT &&
                        node->data.binary.left->data.ident.resolved) {
                        AstNode *ld = node->data.binary.left->data.ident.resolved;
                        AstNode *lt = NULL;
                        if (ld->type == NODE_LET) lt = ld->data.let_decl.type;
                        else if (ld->type == NODE_PARAM) lt = ld->data.param.type;
                        if (lt && lt->type == NODE_TYPE_PRIMITIVE && lt->data.type_node.prim == PRIM_STRING)
                            left_is_str = true;
                    }
                    if (left_is_str) {
                        /* Convert right operand to string if numeric */
                        if (is_numeric_expr(node->data.binary.right)) {
                            cg_inst1(cg, "mov", "rdi, rcx");
                            cg_inst(cg, "call __aether_itoa");
                            cg_inst1(cg, "mov", "rcx, rax");
                        }
                        cg_inst1(cg, "push", "rax");
                        cg_inst1(cg, "push", "rcx");
                        cg_inst(cg, "call __aether_concat");
                        cg_inst(cg, "add rsp, 16");
                    } else {
                        cg_inst1(cg, "add", "rax, rcx");
                    }
                    goto store_assign;
                }
                case BIN_SUB_ASSIGN: cg_comment(cg, "-="); cg_inst1(cg, "sub", "rax, rcx"); goto store_assign;
                case BIN_MUL_ASSIGN: cg_comment(cg, "*="); cg_inst1(cg, "mul", "rcx"); goto store_assign;
                case BIN_DIV_ASSIGN: cg_comment(cg, "/="); cg_inst(cg, "xor rdx, rdx"); cg_inst1(cg, "div", "rcx"); goto store_assign;
                store_assign:
                    cg_comment(cg, "store compound assign");
                    if (node->data.binary.left && node->data.binary.left->type == NODE_IDENT) {
                        int off = find_var_offset_by_name(slots,
                            arena_strndup(cg->arena,
                                node->data.binary.left->data.ident.name.data,
                                node->data.binary.left->data.ident.name.len));
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov qword [rbp%+d], rax", off);
                        cg_inst(cg, buf);
                    }
                    break;
                /* Logical (short-circuit with unique labels) */
                case BIN_AND: {
                    int lbl_false = cg_new_label(cg);
                    char lbl[32];
                    cg_inst1(cg, "test", "rax, rax");
                    snprintf(lbl, sizeof(lbl), "jz L_and_false_%d", lbl_false);
                    cg_inst(cg, lbl);
                    cg_inst1(cg, "test", "rcx, rcx");
                    cg_inst(cg, "setnz al");
                    cg_inst(cg, "movzx rax, al");
                    snprintf(lbl, sizeof(lbl), "L_and_false_%d:", lbl_false);
                    cg_write_fmt(cg, "%s\n", lbl);
                    /* If left was false, rax is still 0 (correct — short-circuit) */
                    break;
                }
                case BIN_OR: {
                    int lbl_true = cg_new_label(cg);
                    char lbl[32];
                    cg_inst1(cg, "test", "rax, rax");
                    snprintf(lbl, sizeof(lbl), "jnz L_or_true_%d", lbl_true);
                    cg_inst(cg, lbl);
                    cg_inst1(cg, "test", "rcx, rcx");
                    cg_inst(cg, "setnz al");
                    cg_inst(cg, "movzx rax, al");
                    snprintf(lbl, sizeof(lbl), "L_or_true_%d:", lbl_true);
                    cg_write_fmt(cg, "%s\n", lbl);
                    /* If left was true, rax is still nonzero — normalize to 1 */
                    cg_inst1(cg, "test", "rax, rax");
                    cg_inst(cg, "setnz al");
                    cg_inst(cg, "movzx rax, al");
                    break;
                }
                case BIN_RANGE: cg_comment(cg, "range"); cg_inst1(cg, "mov", "rax, rcx"); break;
                case BIN_OR_ELSE: {
                    /* x or default: if x is none (0), use default */
                    cg_comment(cg, "optional unwrap (or)");
                    int lbl_has_val = cg_new_label(cg);
                    char lbl[32];
                    cg_inst1(cg, "test", "rax, rax");
                    snprintf(lbl, sizeof(lbl), "jnz L_or_else_has_%d", lbl_has_val);
                    cg_inst(cg, lbl);
                    /* rax is 0 (none), use default (rcx) */
                    cg_inst1(cg, "mov", "rax, rcx");
                    snprintf(lbl, sizeof(lbl), "L_or_else_has_%d:", lbl_has_val);
                    cg_write_fmt(cg, "%s\n", lbl);
                    break;
                }
                case BIN_CONCAT: {
                    cg_comment(cg, "string concat");
                    /* rax = left, rcx = right */
                    /* Auto-convert numeric operands to strings.
                       itoa clobbers rcx (div instruction), so save rcx first. */
                    bool left_num = is_numeric_expr(node->data.binary.left);
                    bool right_num = is_numeric_expr(node->data.binary.right);
                    if (left_num || right_num) {
                        cg_inst1(cg, "push", "rcx");     /* save right value */
                    }
                    if (left_num) {
                        cg_inst1(cg, "mov", "rdi, rax");
                        cg_inst(cg, "call __aether_itoa");
                        /* rax now = string pointer */
                    }
                    if (right_num) {
                        cg_inst1(cg, "push", "rax");     /* save left string */
                        cg_inst1(cg, "pop", "rdi");      /* rdi = left string (temp) */
                        cg_inst1(cg, "pop", "rax");      /* rax = right value */
                        cg_inst1(cg, "push", "rdi");     /* save left string on stack */
                        cg_inst1(cg, "mov", "rdi, rax"); /* convert right */
                        cg_inst(cg, "call __aether_itoa");
                        cg_inst1(cg, "mov", "rcx, rax"); /* rcx = right string */
                        cg_inst1(cg, "pop", "rax");      /* rax = left string */
                    } else if (left_num) {
                        cg_inst1(cg, "pop", "rcx");      /* restore rcx = right (already a string) */
                    }
                    /* Now rax = left (string), rcx = right (string) */
                    /* If either is neither string nor numeric (e.g. array pointer), null it */
                    if (!is_string_expr(node->data.binary.left) && !left_num) {
                        cg_inst(cg, "xor rax, rax");
                    }
                    if (!is_string_expr(node->data.binary.right) && !right_num) {
                        cg_inst(cg, "xor rcx, rcx");
                    }
                    cg_inst1(cg, "push", "rax");   /* left arg (rbp+24) */
                    cg_inst1(cg, "push", "rcx");   /* right arg (rbp+16) */
                    cg_inst(cg, "call __aether_concat");
                    cg_inst(cg, "add rsp, 16");    /* pop both args */
                    break;
                }
                default: cg_inst1(cg, "add", "rax, rcx"); break;
            }
            break;
        }

        case NODE_UNARY_OP: {
            cg_expr(cg, node->data.unary.operand, slots);
            switch (node->data.unary.op) {
                case UNARY_NEG: cg_inst1(cg, "neg", "rax"); break;
                case UNARY_NOT: cg_inst(cg, "test rax, rax"); cg_inst(cg, "sete al"); cg_inst(cg, "movzx rax, al"); break;
                case UNARY_BIT_NOT: cg_inst1(cg, "not", "rax"); break;
                case UNARY_DEREF: cg_inst(cg, "mov rax, [rax]"); break;
                case UNARY_INC:
                    cg_comment(cg, "increment");
                    cg_inst(cg, "add rax, 1");
                    /* Store back to variable if operand is an ident */
                    if (node->data.unary.operand && node->data.unary.operand->type == NODE_IDENT) {
                        int off = find_var_offset_by_name(slots,
                            node->data.unary.operand->data.ident.name.data);
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov qword [rbp%+d], rax", off);
                        cg_inst(cg, buf);
                    }
                    break;
                case UNARY_DEC:
                    cg_comment(cg, "decrement");
                    cg_inst(cg, "sub rax, 1");
                    /* Store back to variable if operand is an ident */
                    if (node->data.unary.operand && node->data.unary.operand->type == NODE_IDENT) {
                        int off = find_var_offset_by_name(slots,
                            node->data.unary.operand->data.ident.name.data);
                        char buf[64];
                        snprintf(buf, sizeof(buf), "mov qword [rbp%+d], rax", off);
                        cg_inst(cg, buf);
                    }
                    break;
                case UNARY_HEAP: {
                    /* heap Expr — allocate, store result, return pointer */
                    cg_comment(cg, "heap alloc");
                    /* Save evaluated value to stack */
                    cg_inst1(cg, "push", "rax");
                    /* Allocate 8 bytes (pointer-sized) */
                    cg_inst1(cg, "mov", "rdi, 8");
                    cg_inst(cg, "call __aether_alloc");
                    /* Pop value into rcx, store to [rax] */
                    cg_inst1(cg, "pop", "rcx");
                    cg_inst(cg, "mov [rax], rcx");
                    /* rax now holds the heap pointer */
                    break;
                }
                case UNARY_ARRAY_LEN: {
                    /* #expr — array length: read 8-byte length from array header */
                    cg_comment(cg, "array length");
                    /* expr is already in rax (the array pointer) */
                    /* Array layout: [length: u64][data...] */
                    cg_inst(cg, "mov rax, [rax]");
                    break;
                }
                default: break;
            }
            break;
        }

        case NODE_CALL: {
            /* Check for built-in functions on host targets */
            bool is_host = (cg->target == TARGET_MACHO64 || cg->target == TARGET_ELF64_HOST || cg->target == TARGET_LIB);
            if (is_host && node->data.call.callee->type == NODE_IDENT) {
                char fn_name[256];
                int nlen = (int)node->data.call.callee->data.ident.name.len;
                if (nlen > 255) nlen = 255;
                memcpy(fn_name, node->data.call.callee->data.ident.name.data, nlen);
                fn_name[nlen] = '\0';

                /* Built-in: print(string) — host: write syscall, freestanding: 0x5008 syscall page */
                if (strcmp(fn_name, "print") == 0 && node->data.call.args.count >= 1) {
                    AstNode *arg = node->data.call.args.items[0];
                    cg_comment(cg, "print() built-in");
                    if (arg->type == NODE_LITERAL_STRING) {
                        const char *label = cg_emit_string(cg, arg->data.literal.string_val);
                        if (!label) { cg_warn(cg, arg, "cg_emit_string failed"); break; }
                        char processed[8192];
                        int plen = process_string_literal(arg->data.literal.string_val, processed, sizeof(processed) - 1);
                        if (cg->target == TARGET_MACHO64) {
                            cg_inst1(cg, "mov", "rdi, 1");
                            cg_write_fmt(cg, "    lea rsi, [rel %s]\n", label);
                            cg_write_fmt(cg, "    mov rdx, %d\n", plen);
                            cg_inst1(cg, "mov", "rax, 0x2000004");
                            cg_inst(cg, "syscall");
                        } else if (cg->target == TARGET_ELF64_HOST) {
                            cg_inst1(cg, "mov", "rdi, 1");
                            cg_write_fmt(cg, "    lea rsi, [rel %s]\n", label);
                            cg_write_fmt(cg, "    mov rdx, %d\n", plen);
                            cg_inst1(cg, "mov", "rax, 1");
                            cg_inst(cg, "syscall");
                        } else {
                            /* Freestanding: call through kernel syscall page slot 1 (puts) */
                            cg_inst(cg, "mov rax, [0x5008]");
                            cg_inst(cg, "call rax");
                        }
                        cg_inst1(cg, "xor", "rax, rax");
                    } else {
                        cg_comment(cg, "print() runtime string");
                        cg_expr(cg, arg, slots);
                        if (is_numeric_expr(arg)) {
                            cg_inst1(cg, "mov", "rdi, rax");
                            cg_inst(cg, "call __aether_itoa");
                        }
                        cg_inst1(cg, "push", "rax");
                        int sl_id = cg->label_counter++;
                        cg_inst(cg, "xor rcx, rcx");
                        cg_inst(cg, "test rax, rax");
                        cg_write_fmt(cg, "    jz .strlen_done_%d\n", sl_id);
                        cg_inst(cg, "mov rdi, rax");
                        cg_write_fmt(cg, ".strlen_loop_%d:\n", sl_id);
                        cg_write_fmt(cg, "    cmp byte [rdi + rcx], 0\n");
                        cg_write_fmt(cg, "    je .strlen_done_%d\n", sl_id);
                        cg_write_fmt(cg, "    inc rcx\n");
                        cg_write_fmt(cg, "    jmp .strlen_loop_%d\n", sl_id);
                        cg_write_fmt(cg, ".strlen_done_%d:\n", sl_id);
                        cg_inst1(cg, "pop", "rsi");
                        cg_inst1(cg, "mov", "rdx, rcx");
                        if (cg->target == TARGET_MACHO64) {
                            cg_inst1(cg, "mov", "rdi, 1");
                            cg_inst1(cg, "mov", "rax, 0x2000004");
                            cg_inst(cg, "syscall");
                        } else if (cg->target == TARGET_ELF64_HOST) {
                            cg_inst1(cg, "mov", "rdi, 1");
                            cg_inst1(cg, "mov", "rax, 1");
                            cg_inst(cg, "syscall");
                        } else {
                            cg_inst(cg, "mov rax, [0x5008]");
                            cg_inst(cg, "call rax");
                        }
                        cg_inst1(cg, "xor", "rax, rax");
                    }
                    break;
                }

                /* Built-in: exit(code) — emit exit syscall inline */
                if (strcmp(fn_name, "exit") == 0 && node->data.call.args.count >= 1) {
                    AstNode *arg = node->data.call.args.items[0];
                    cg_comment(cg, "exit() built-in");
                    cg_expr(cg, arg, slots);
                    cg_inst1(cg, "mov", "rdi, rax");
                    if (cg->target == TARGET_MACHO64) {
                        cg_inst1(cg, "mov", "rax, 0x2000001");
                    } else {
                        cg_inst1(cg, "mov", "rax, 60");
                    }
                    cg_inst(cg, "syscall");
                    break;
                }
            }

            /* Check for enum construction: EnumName::Variant(args) */
            if (node->data.call.callee->type == NODE_FIELD_ACCESS) {
                AstNode *target = node->data.call.callee->data.field.target;
                AstNode *field = node->data.call.callee->data.field.field;
                if (target && target->type == NODE_IDENT && field && field->type == NODE_IDENT) {
                    char enum_name[256], variant_name[256];
                    int nlen = (int)target->data.ident.name.len;
                    if (nlen > 255) nlen = 255; memcpy(enum_name, target->data.ident.name.data, nlen); enum_name[nlen] = '\0';
                    int vlen = (int)field->data.ident.name.len;
                    if (vlen > 255) vlen = 255; memcpy(variant_name, field->data.ident.name.data, vlen); variant_name[vlen] = '\0';
                    
                    /* Find the discriminant for this variant */
                    int disc_val = -1;
                    for (VariantEntry *ve = variant_entries; ve; ve = ve->next) {
                        if (strcmp(ve->name, variant_name) == 0) { disc_val = ve->discriminant; break; }
                    }
                    
                    if (disc_val >= 0) {
                        cg_comment(cg, "enum construction");
                        /* Build the tagged union in rax.
                           For now, just return the discriminant as a proxy value.
                           Full payload handling deferred to later phases. */
                        char buf[32];
                        snprintf(buf, sizeof(buf), "mov rax, %d", disc_val);
                        cg_inst(cg, buf);
                        break;
                    }
                }
            }

            cg_comment(cg, "function call");
            int argc = node->data.call.args.count;

            /* Check if callee has a variadic parameter */
            bool callee_has_varargs = false;
            int regular_param_count = argc;
            if (node->data.call.callee->type == NODE_IDENT &&
                node->data.call.callee->data.ident.resolved &&
                node->data.call.callee->data.ident.resolved->type == NODE_FUNC_DECL) {
                AstNode *func_decl = node->data.call.callee->data.ident.resolved;
                for (int i = 0; i < func_decl->data.func.params.count; i++) {
                    if (func_decl->data.func.params.items[i]->data.param.is_varargs) {
                        callee_has_varargs = true;
                        regular_param_count = i;
                        break;
                    }
                }
            }

            if (callee_has_varargs) {
                int vararg_count = argc - regular_param_count;
                cg_inst1(cg, "mov", "rdi, 8");
                if (vararg_count > 0) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "add rdi, %d", vararg_count * 8);
                    cg_inst(cg, buf);
                }
                cg_inst(cg, "call __aether_alloc");
                cg_inst1(cg, "mov", "r15, rax");
                cg_inst1(cg, "mov", "rcx, 0");
                if (vararg_count > 0) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "mov rcx, %d", vararg_count);
                    cg_inst(cg, buf);
                }
                cg_inst(cg, "mov [r15], rcx");
                for (int i = argc - 1; i >= regular_param_count; i--) {
                    cg_expr(cg, node->data.call.args.items[i], slots);
                    cg_inst1(cg, "push", "rax");
                }
                for (int i = regular_param_count; i < argc; i++) {
                    cg_inst1(cg, "pop", "rcx");
                    char buf[32];
                    snprintf(buf, sizeof(buf), "mov [r15+8+%d*8], rcx", i - regular_param_count);
                    cg_inst(cg, buf);
                }
                /* Now push regular args right-to-left */
                for (int i = regular_param_count - 1; i >= 0; i--) {
                    cg_expr(cg, node->data.call.args.items[i], slots);
                    cg_inst1(cg, "push", "rax");
                }
                int reg_count = regular_param_count < 6 ? regular_param_count : 6;
                const char *regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
                for (int i = 0; i < reg_count; i++) {
                    cg_inst1(cg, "pop", regs[i]);
                }
                {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "mov %s, r15", regs[regular_param_count < 6 ? regular_param_count : 5]);
                    cg_inst(cg, buf);
                }
                int stack_cleanup = regular_param_count > 6 ? regular_param_count - 6 : 0;
                if (node->data.call.callee->type == NODE_IDENT) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "%.*s",
                        (int)node->data.call.callee->data.ident.name.len,
                        node->data.call.callee->data.ident.name.data);
                    cg_inst1(cg, "call", buf);
                }
                if (stack_cleanup > 0) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "add rsp, %d", stack_cleanup * 8);
                    cg_inst(cg, buf);
                }
            } else {
                const char *regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
                int reg_count = argc < 6 ? argc : 6;
                for (int i = argc - 1; i >= 0; i--) {
                    cg_expr(cg, node->data.call.args.items[i], slots);
                    cg_inst1(cg, "push", "rax");
                }
                for (int i = 0; i < reg_count; i++) {
                    cg_inst1(cg, "pop", regs[i]);
                }
                int stack_cleanup = argc > 6 ? argc - 6 : 0;
                if (node->data.call.callee->type == NODE_IDENT) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "%.*s",
                        (int)node->data.call.callee->data.ident.name.len,
                        node->data.call.callee->data.ident.name.data);
                    if (node->data.call.callee->data.ident.resolved &&
                        node->data.call.callee->data.ident.resolved->type == NODE_FUNC_DECL &&
                        node->data.call.callee->data.ident.resolved->data.func.is_sys) {
                        int idx = node->data.call.callee->data.ident.resolved->data.func.sys_index;
                        if (idx >= 0) {
                            cg_comment(cg, "syscall via 0x5000 table");
                            char tmp[64];
                            snprintf(tmp, sizeof(tmp), "mov rax, 0x%x", 0x5000 + idx * 8);
                            cg_inst(cg, tmp);
                            cg_inst(cg, "call [rax]");
                        } else {
                            cg_inst1(cg, "call", buf);
                        }
                    } else {
                        cg_inst1(cg, "call", buf);
                    }
                }
                if (stack_cleanup > 0) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "add rsp, %d", stack_cleanup * 8);
                    cg_inst(cg, buf);
                }
            }
            break;
        }

        case NODE_ASSIGN:
            /* Assignment: left = right — handled as BIN_ASSIGN */
            break;

        case NODE_LAMBDA: {
            /* Non-capturing lambda: generate a unique function, return its address.
             * Capturing lambdas (with env) deferred to later phase. */
            cg_comment(cg, "lambda");
            int lambda_id = cg_new_label(cg);
            char fn_name[64];
            snprintf(fn_name, sizeof(fn_name), "L_lambda_%x", lambda_id);

            /* Emit the lambda function body (will be placed in .text) */
            /* For now, just return 0 as placeholder — full lambda codegen deferred */
            cg_inst1(cg, "mov", "rax, 0");
            break;
        }

        case NODE_MATCH: {
            int end_label = cg_new_label(cg);
            cg_comment(cg, "match expression");
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
                } else if (pat->type == NODE_BINARY_OP &&
                           (pat->data.binary.op == BIN_RANGE || pat->data.binary.op == BIN_RANGE_INCLUSIVE)) {
                    /* Range pattern: case 1..9 or case 1..=9 */
                    bool inclusive = (pat->data.binary.op == BIN_RANGE_INCLUSIVE);
                    /* Compare rax with start */
                    cg_expr(cg, pat->data.binary.left, slots);
                    cg_inst1(cg, "push", "rax");  /* save start */
                    cg_inst1(cg, "mov", "rax, [rsp+8]");  /* reload matched value */
                    cg_inst1(cg, "pop", "rcx");   /* rcx = start */
                    cg_inst1(cg, "cmp", "rax, rcx");
                    cg_write_fmt(cg, "    jl L_%x\n", next_label);  /* if rax < start, skip */
                    /* Compare rax with end */
                    cg_expr(cg, pat->data.binary.right, slots);
                    cg_inst1(cg, "mov", "rcx, rax");  /* rcx = end */
                    cg_inst1(cg, "mov", "rax, [rsp]");  /* reload matched value */
                    cg_inst1(cg, "cmp", "rax, rcx");
                    if (inclusive) {
                        cg_write_fmt(cg, "    jg L_%x\n", next_label);  /* if rax > end, skip */
                    } else {
                        cg_write_fmt(cg, "    jge L_%x\n", next_label);  /* if rax >= end, skip */
                    }
                    cg_write_fmt(cg, "    jmp L_%x\n", body_label);
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

        default:
            if (node->type != NODE_LITERAL_FLOAT && node->type != NODE_MATCH_ARM)
                cg_warn(cg, node, "unsupported expression in codegen");
            cg_inst1(cg, "mov", "rax, 0");
            break;
    }
}