#include "codegen_internal.h"

/* Global struct/enum layout lists — shared across all codegen modules */
StructLayout *struct_layouts = NULL;
StructLayout *enum_layouts = NULL;
VariantEntry *variant_entries = NULL;

/* ================================================================ */

Codegen *codegen_create(Arena *a) {
    Codegen *cg = (Codegen *)arena_alloc(a, sizeof(Codegen));
    if (!cg) return NULL;
    cg->arena = a;
    cg->output = (char *)arena_alloc(a, INITIAL_CAP);
    cg->output_len = 0;
    cg->output_cap = INITIAL_CAP;
    cg->label_counter = 0;
    cg->indent_level = 0;
    cg->current_func = NULL;
    cg->target = TARGET_FREESTANDING;
    cg->defer_stack = NULL;
    cg->defer_count = 0;
    cg->auto_drops = NULL;
    cg->entry_addr = 0;
    cg->entry_func = NULL;
    cg->has_layout = false;
    cg->layout_start = 0;
    cg->layout_max = 0;
    cg->layout_bits = 0;
    cg->layout_signature = 0;
    cg->layout_file = NULL;
    cg->linker_script = NULL;
    cg->cleanup_depth = 0;
    return cg;
}

void codegen_set_target(Codegen *cg, Target target) {
    if (target == TARGET_HOST) {
        cg->target = codegen_detect_host();
    } else {
        cg->target = target;
    }
}

void codegen_destroy(Codegen *cg) {
    (void)cg;
}