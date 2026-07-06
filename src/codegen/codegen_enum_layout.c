#include "codegen_internal.h"

/* ================================================================
 * Enum layout tracker — tagged union = 8-byte discriminant + max payload
 * Variant names mapped to discriminant values for codegen
 * ================================================================ */

StructLayout *build_enum_layout(Arena *a, AstNode *node) {
    if (node->type != NODE_ENUM_DECL) return NULL;
    
    StructLayout *sl = (StructLayout *)arena_alloc(a, sizeof(StructLayout));
    sl->name = arena_strndup(a,
        node->data.enum_decl.name->data.ident.name.data,
        node->data.enum_decl.name->data.ident.name.len);
    
    /* Enum layout: [discriminant: u64] [payload: max_variant_size]
       Discriminant is always 8 bytes, payload is the max of all variant sizes */
    int max_payload = 0;
    for (int i = 0; i < node->data.enum_decl.variants.count; i++) {
        AstNode *var = node->data.enum_decl.variants.items[i];
        int var_size = 0;
        for (int j = 0; j < var->data.enum_variant.payload_types.count; j++) {
            var_size += type_size(var->data.enum_variant.payload_types.items[j]);
        }
        if (var_size > max_payload) max_payload = var_size;
    }
    
    /* Build field-like entries for the compiler */
    FieldLayout *disc = (FieldLayout *)arena_alloc(a, sizeof(FieldLayout));
    disc->name = "__disc";
    disc->offset = 0;
    disc->size = 8;
    disc->next = NULL;
    sl->fields = disc;
    
    sl->total_size = 8 + max_payload;
    sl->next = enum_layouts;
    enum_layouts = sl;
    
    /* Register variant names for codegen lookup */
    for (int i = 0; i < node->data.enum_decl.variants.count; i++) {
        AstNode *var = node->data.enum_decl.variants.items[i];
        VariantEntry *ve = (VariantEntry *)arena_alloc(a, sizeof(VariantEntry));
        ve->name = arena_strndup(a,
            var->data.enum_variant.name->data.ident.name.data,
            var->data.enum_variant.name->data.ident.name.len);
        ve->discriminant = i;
        ve->next = variant_entries;
        variant_entries = ve;
    }
    return sl;
}