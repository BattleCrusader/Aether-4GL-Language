#include "codegen_internal.h"
/* ================================================================
 * Defer helpers
 * ================================================================ */
void cg_defer_push(Codegen *cg, AstNode *body) {
    if (cg->current_func && cg->current_func->type == NODE_FUNC_DECL) {
        node_list_append(&cg->current_func->data.func.defer_list, body);
    }
}

void cg_emit_defers(Codegen *cg, VarSlot *slots) {
    if (!cg->current_func || cg->current_func->type != NODE_FUNC_DECL) return;
    AstNodeList *defers = &cg->current_func->data.func.defer_list;
    if (defers->count == 0 && cg->auto_drops == NULL) return;
    cg_comment(cg, "defers");
    for (int i = defers->count - 1; i >= 0; i--) {
        cg_stmt(cg, defers->items[i], slots);
    }
    /* Emit auto-drop calls for class-typed variables (LIFO — push order is natural) */
    for (AutoDrop *ad = cg->auto_drops; ad; ad = ad->next) {
        cg_write_fmt(cg, "    lea rdi, [rbp%+d]\n", ad->stack_offset);
        cg_write_fmt(cg, "    call %s_drop\n", ad->class_name);
    }
}

void cg_defer_clear(Codegen *cg) {
    if (cg->current_func && cg->current_func->type == NODE_FUNC_DECL) {
        cg->current_func->data.func.defer_list.count = 0;
    }
}