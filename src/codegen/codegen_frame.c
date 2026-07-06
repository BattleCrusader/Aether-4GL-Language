#include "codegen_internal.h"

/* ================================================================
 * Stack frame computation
 * ================================================================ */

/* Walk a function's body and collect all variable declarations,
   computing their stack offsets. Returns the total frame size. */
VarSlot *compute_frame(Arena *a, AstNode *func, int *out_frame_size);

static void frame_collect(Arena *a, AstNode *node, VarSlot **list, int *offset) {
    if (!node) return;

    switch (node->type) {
        case NODE_BLOCK:
            for (int i = 0; i < node->data.list.count; i++)
                frame_collect(a, node->data.list.items[i], list, offset);
            break;

        case NODE_LET: {
            int raw_size = 8;
            PrimType ptype = PRIM_VOID;
            if (node->data.let_decl.type) {
                raw_size = type_size(node->data.let_decl.type);
                ptype = prim_from_type(node->data.let_decl.type);
            }
            int vsize = (raw_size + 7) & ~7; /* align to 8 */

            *offset += vsize;

            VarSlot *slot = (VarSlot *)arena_alloc(a, sizeof(VarSlot));
            slot->node = node;
            slot->name = arena_strndup(a,
                node->data.let_decl.name->data.ident.name.data,
                node->data.let_decl.name->data.ident.name.len);
            slot->stack_offset = -(*offset);
            slot->size = vsize;
            slot->actual_size = raw_size;
            slot->prim = ptype;
            slot->next = *list;
            *list = slot;

            /* Visit initializer for nested allocations */
            if (node->data.let_decl.value)
                frame_collect(a, node->data.let_decl.value, list, offset);
            break;
        }

        case NODE_IF:
            frame_collect(a, node->data.if_node.condition, list, offset);
            frame_collect(a, node->data.if_node.then_block, list, offset);
            if (node->data.if_node.elif_chain)
                frame_collect(a, node->data.if_node.elif_chain, list, offset);
            if (node->data.if_node.else_block)
                frame_collect(a, node->data.if_node.else_block, list, offset);
            break;

        case NODE_WHILE:
            frame_collect(a, node->data.while_node.condition, list, offset);
            frame_collect(a, node->data.while_node.body, list, offset);
            break;

        case NODE_FOR:
            /* Create a stack slot for the loop variable (it's an ident, not a let) */
            if (node->data.for_node.var && node->data.for_node.var->type == NODE_IDENT) {
                *offset += 8;
                VarSlot *slot = (VarSlot *)arena_alloc(a, sizeof(VarSlot));
                slot->node = node->data.for_node.var;
                slot->name = arena_strndup(a,
                    node->data.for_node.var->data.ident.name.data,
                    node->data.for_node.var->data.ident.name.len);
                slot->stack_offset = -(*offset);
                slot->size = 8;
                slot->actual_size = 8;
                slot->prim = PRIM_U64;
                slot->next = *list;
                *list = slot;
            }
            /* Also create a slot for the index variable if present */
            if (node->data.for_node.index_var) {
                AstNode *index_var = node->data.for_node.index_var;
                if (index_var && index_var->type == NODE_IDENT) {
                    *offset += 8;
                    VarSlot *slot = (VarSlot *)arena_alloc(a, sizeof(VarSlot));
                    slot->node = index_var;
                    slot->name = arena_strndup(a,
                        index_var->data.ident.name.data,
                        index_var->data.ident.name.len);
                    slot->stack_offset = -(*offset);
                    slot->size = 8;
                    slot->actual_size = 8;
                    slot->prim = PRIM_U64;
                    slot->next = *list;
                    *list = slot;
                }
            }
            frame_collect(a, node->data.for_node.iterable, list, offset);
            frame_collect(a, node->data.for_node.body, list, offset);
            break;

        case NODE_TRY:
            frame_collect(a, node->data.try_node.body, list, offset);
            for (int i = 0; i < node->data.try_node.catch_arms.count; i++) {
                AstNode *arm = node->data.try_node.catch_arms.items[i];
                if (arm->data.catch_arm.var)
                    frame_collect(a, arm->data.catch_arm.var, list, offset);
                if (arm->data.catch_arm.body)
                    frame_collect(a, arm->data.catch_arm.body, list, offset);
            }
            break;

        default:
            break;
    }
}

VarSlot *compute_frame(Arena *a, AstNode *func, int *out_frame_size) {
    VarSlot *slots = NULL;
    int offset = 0; /* grows negative (stack goes down) */

    /* Allocate slots for parameters */
    for (int i = 0; i < func->data.func.params.count; i++) {
        AstNode *param = func->data.func.params.items[i];
        int psize = 8;
        if (param->data.param.type)
            psize = type_size(param->data.param.type);
        psize = (psize + 7) & ~7;
        offset += psize;

        VarSlot *slot = (VarSlot *)arena_alloc(a, sizeof(VarSlot));
        slot->node = param;
        slot->name = arena_strndup(a,
            param->data.param.name->data.ident.name.data,
            param->data.param.name->data.ident.name.len);
        slot->stack_offset = -offset;
        slot->size = psize;
        slot->actual_size = psize;
        slot->prim = prim_from_type(param->data.param.type);
        slot->next = slots;
        slots = slot;
    }

    /* Walk body for let declarations */
    if (func->data.func.body)
        frame_collect(a, func->data.func.body, &slots, &offset);

    /* Align frame to 16 bytes (SysV ABI) */
    offset = (offset + 15) & ~15;

    *out_frame_size = offset;
    return slots;
}

/* Look up a variable's stack offset by its AST node */
int find_var_offset(VarSlot *slots, AstNode *node) {
    while (slots) {
        if (slots->node == node) return slots->stack_offset;
        slots = slots->next;
    }
    return -8; /* fallback */
}

int find_var_offset_by_name(VarSlot *slots, const char *name) {
    while (slots) {
        if (strcmp(slots->name, name) == 0) return slots->stack_offset;
        slots = slots->next;
    }
    return -8;
}

int find_var_size(VarSlot *slots, AstNode *node) {
    while (slots) {
        if (slots->node == node) return slots->actual_size;
        slots = slots->next;
    }
    return 8;
}

int find_var_size_by_name(VarSlot *slots, const char *name) {
    while (slots) {
        if (strcmp(slots->name, name) == 0) return slots->actual_size;
        slots = slots->next;
    }
    return 8;
}

/* Find a VarSlot by AST node pointer */
VarSlot *find_var_slot(VarSlot *slots, AstNode *node) {
    while (slots) {
        if (slots->node == node) return slots;
        slots = slots->next;
    }
    return NULL;
}

/* Emit cleanup for a range of scope depths (from saved_depth to current_depth).
 * This calls destructors and defers for objects created in the try body. */
void cg_emit_cleanup_range(Codegen *cg, VarSlot *slots, int saved_depth, int current_depth) {
    (void)cg;
    (void)slots;
    (void)saved_depth;
    (void)current_depth;
    /* For now, this is a placeholder. The full implementation will walk
     * the cleanup table and emit destructor calls + defer blocks.
     * Phase A of the proper try/catch implementation. */
    cg_comment(cg, "cleanup range (placeholder)");
}