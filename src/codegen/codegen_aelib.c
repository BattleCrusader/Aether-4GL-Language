#include "codegen_internal.h"

/* ================================================================
 * Metadata extraction for .aelib library format
 * ================================================================ */

/* Helper: get the name of a declaration as a C string.
 * Returns a heap-allocated null-terminated string (caller must free).
 * For identifiers, we know the exact length from StringView. */
char *decl_name(AstNode *node) {
    if (!node) return strdup("");
    switch (node->type) {
        case NODE_FUNC_DECL:
            if (node->data.func.name && node->data.func.name->type == NODE_IDENT) {
                StringView *sv = &node->data.func.name->data.ident.name;
                char *result = (char *)malloc(sv->len + 1);
                if (result) {
                    memcpy(result, sv->data, sv->len);
                    result[sv->len] = '\0';
                }
                return result ? result : strdup("");
            }
            break;
        case NODE_STRUCT_DECL:
            if (node->data.struct_decl.name && node->data.struct_decl.name->type == NODE_IDENT) {
                StringView *sv = &node->data.struct_decl.name->data.ident.name;
                char *result = (char *)malloc(sv->len + 1);
                if (result) {
                    memcpy(result, sv->data, sv->len);
                    result[sv->len] = '\0';
                }
                return result ? result : strdup("");
            }
            break;
        case NODE_CLASS_DECL:
            if (node->data.struct_decl.name && node->data.struct_decl.name->type == NODE_IDENT) {
                StringView *sv = &node->data.struct_decl.name->data.ident.name;
                char *result = (char *)malloc(sv->len + 1);
                if (result) {
                    memcpy(result, sv->data, sv->len);
                    result[sv->len] = '\0';
                }
                return result ? result : strdup("");
            }
            break;
        case NODE_ENUM_DECL:
            if (node->data.enum_decl.name && node->data.enum_decl.name->type == NODE_IDENT) {
                StringView *sv = &node->data.enum_decl.name->data.ident.name;
                char *result = (char *)malloc(sv->len + 1);
                if (result) {
                    memcpy(result, sv->data, sv->len);
                    result[sv->len] = '\0';
                }
                return result ? result : strdup("");
            }
            break;
        case NODE_CONST_DECL:
            if (node->data.let_decl.name && node->data.let_decl.name->type == NODE_IDENT) {
                StringView *sv = &node->data.let_decl.name->data.ident.name;
                char *result = (char *)malloc(sv->len + 1);
                if (result) {
                    memcpy(result, sv->data, sv->len);
                    result[sv->len] = '\0';
                }
                return result ? result : strdup("");
            }
            break;
        case NODE_TYPE_ALIAS:
            /* Type alias: name is first item in list */
            if (node->data.list.count > 0 && node->data.list.items[0]->type == NODE_IDENT) {
                StringView *sv = &node->data.list.items[0]->data.ident.name;
                char *result = (char *)malloc(sv->len + 1);
                if (result) {
                    memcpy(result, sv->data, sv->len);
                    result[sv->len] = '\0';
                }
                return result ? result : strdup("");
            }
            break;
        default:
            break;
    }
    return strdup("");
}

/* Helper: check if a declaration is public — everything is public by default */
bool decl_is_pub(AstNode *node) {
    (void)node;
    return true;
}

/* Helper: get primitive type name for metadata */
const char *prim_type_name(PrimType pt) {
    switch (pt) {
        case PRIM_VOID:   return "void";
        case PRIM_BOOL:   return "bool";
        case PRIM_BYTE:   return "byte";
        case PRIM_U8:     return "u8";
        case PRIM_U16:    return "u16";
        case PRIM_U32:    return "u32";
        case PRIM_U64:    return "u64";
        case PRIM_I8:     return "i8";
        case PRIM_I16:    return "i16";
        case PRIM_I32:    return "i32";
        case PRIM_I64:    return "i64";
        case PRIM_F32:    return "f32";
        case PRIM_F64:    return "f64";
        case PRIM_STRING: return "string";
        default:          return "unknown";
    }
}

/* Helper: get type name string from a type node.
 * Returns a static const string (no allocation needed for primitives)
 * or a heap-allocated null-terminated string for named types.
 * For named types, the caller must free the result. */
const char *type_node_name(AstNode *type) {
    if (!type) return "void";
    switch (type->type) {
        case NODE_TYPE_PRIMITIVE:
            return prim_type_name(type->data.type_node.prim);
        case NODE_TYPE_NAMED: {
            /* name is a StringView into arena — use strndup with known length */
            StringView *sv = &type->data.type_node.name;
            char *result = (char *)malloc(sv->len + 1);
            if (result) {
                memcpy(result, sv->data, sv->len);
                result[sv->len] = '\0';
            }
            return result ? result : "unknown";
        }
        case NODE_TYPE_PTR:
            return "ptr";
        case NODE_TYPE_REF:
            return "ref";
        case NODE_TYPE_ARRAY:
            return "array";
        case NODE_TYPE_SLICE:
            return "slice";
        case NODE_TYPE_OPTIONAL:
            return "optional";
        default:
            return "unknown";
    }
}

/* Helper: free memory allocated by type_node_name */
void type_node_name_free(const char *name) {
    if (!name) return;
    /* Only free if it's not pointing to a static primitive name.
     * We detect this by checking if name starts with known static strings. */
    static const char *statics[] = {
        "void", "ptr", "ref", "array", "slice", "optional", "unknown",
        "bool", "byte", "u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64",
        "f32", "f64", "string"
    };
    for (size_t i = 0; i < sizeof(statics)/sizeof(statics[0]); i++) {
        if (name == statics[i]) return;
    }
    free((char *)name);
}

/* Build a function signature type data blob.
 * Format: return_type_name (null-terminated), param_count (u8),
 * for each param: name (null-term), type_name (null-term), is_mut (u8) */
uint8_t *build_func_type_data(AstNode *func, size_t *out_size) {
    if (!func || func->type != NODE_FUNC_DECL) {
        *out_size = 0;
        return NULL;
    }

    /* Compute total size. We use strndup for identifiers (known length from StringView)
     * and strdup for type names (always safe). */
    size_t total = 0;

    /* Return type name */
    const char *ret_type_raw = type_node_name(func->data.func.return_type);
    size_t ret_type_len = func->data.func.return_type &&
                         func->data.func.return_type->type == NODE_TYPE_NAMED ?
                         func->data.func.return_type->data.type_node.name.len : strlen(ret_type_raw);
    char *ret_type = strndup(ret_type_raw, ret_type_len);
    total += strlen(ret_type) + 1;

    /* Param count and params */
    int param_count = 0;
    for (int i = 0; i < func->data.func.params.count; i++) {
        AstNode *param = func->data.func.params.items[i];
        if (param->type != NODE_PARAM) continue;
        param_count++;
    }
    total += 1; /* param count */

    /* Collect strings for sizing */
    char **pname_allocs = NULL;
    char **ptype_allocs = NULL;
    if (param_count > 0) {
        pname_allocs = (char **)calloc(param_count, sizeof(char *));
        ptype_allocs = (char **)calloc(param_count, sizeof(char *));
    }
    int pi = 0;
    for (int i = 0; i < func->data.func.params.count; i++) {
        AstNode *param = func->data.func.params.items[i];
        if (param->type != NODE_PARAM) continue;

        /* param name from StringView */
        const char *pname_raw = param->data.param.name ?
            param->data.param.name->data.ident.name.data : "";
        size_t pname_len = param->data.param.name ?
            param->data.param.name->data.ident.name.len : 0;
        pname_allocs[pi] = strndup(pname_raw, pname_len);

        /* param type */
        const char *ptype_raw = type_node_name(param->data.param.type);
        ptype_allocs[pi] = strdup(ptype_raw);

        total += strlen(pname_allocs[pi]) + 1;
        total += strlen(ptype_allocs[pi]) + 1;
        total += 1; /* is_mut */
        pi++;
    }

    uint8_t *data = (uint8_t *)malloc(total);
    if (!data) {
        free(ret_type);
        for (int i = 0; i < param_count; i++) { free(pname_allocs[i]); free(ptype_allocs[i]); }
        free(pname_allocs); free(ptype_allocs);
        *out_size = 0;
        return NULL;
    }

    size_t pos = 0;
    /* Return type */
    size_t rt_len = strlen(ret_type) + 1;
    memcpy(data + pos, ret_type, rt_len);
    pos += rt_len;
    free(ret_type);

    /* Param count */
    uint8_t pcount = (uint8_t)(param_count > 255 ? 255 : param_count);
    data[pos++] = pcount;

    /* Params */
    for (int i = 0; i < param_count; i++) {
        size_t pn_len = strlen(pname_allocs[i]) + 1;
        size_t pt_len = strlen(ptype_allocs[i]) + 1;
        memcpy(data + pos, pname_allocs[i], pn_len);
        pos += pn_len;
        memcpy(data + pos, ptype_allocs[i], pt_len);
        pos += pt_len;
        AstNode *param = func->data.func.params.items[i];
        data[pos++] = param->data.param.is_mut ? 1 : 0;
    }

    for (int i = 0; i < param_count; i++) { free(pname_allocs[i]); free(ptype_allocs[i]); }
    free(pname_allocs); free(ptype_allocs);

    *out_size = pos;
    return data;
}

/* Build a struct/class layout type data blob.
 * Format: field_count (u16), for each field: name (null-term), type_name (null-term), offset (u64), size (u64) */
uint8_t *build_struct_type_data(AstNode *decl, size_t *out_size) {
    if (!decl) { *out_size = 0; return NULL; }
    bool is_class = (decl->type == NODE_CLASS_DECL);
    if (decl->type != NODE_STRUCT_DECL && !is_class) {
        *out_size = 0;
        return NULL;
    }

    /* First pass: compute total size */
    size_t total = 2; /* field_count (u16) */
    for (int i = 0; i < decl->data.struct_decl.fields.count; i++) {
        AstNode *field = decl->data.struct_decl.fields.items[i];
        if (field->type != NODE_FIELD) continue;
        const char *fname = field->data.param.name ?
            field->data.param.name->data.ident.name.data : "";
        const char *ftype = type_node_name(field->data.param.type);
        /* Use strndup to safely null-terminate field names from arena */
        char *fname_safe = fname[0] ? strndup(fname, field->data.param.name->data.ident.name.len) : (char *)"";
        size_t fn_len = strlen(fname_safe) + 1;
        size_t ft_len = strlen(ftype) + 1;
        total += fn_len;
        total += ft_len;
        total += 8 + 8; /* offset + size */
        if (fname_safe[0]) free(fname_safe);
    }

    uint8_t *data = (uint8_t *)malloc(total);
    if (!data) { *out_size = 0; return NULL; }

    size_t pos = 0;
    uint16_t fcount = (uint16_t)(decl->data.struct_decl.fields.count);
    memcpy(data + pos, &fcount, 2);
    pos += 2;

    uint64_t field_offset = 0;
    for (int i = 0; i < decl->data.struct_decl.fields.count; i++) {
        AstNode *field = decl->data.struct_decl.fields.items[i];
        if (field->type != NODE_FIELD) continue;
        const char *fname = field->data.param.name ?
            field->data.param.name->data.ident.name.data : "";
        const char *ftype = type_node_name(field->data.param.type);
        /* Use strndup to safely null-terminate field names from arena */
        char *fname_safe = fname[0] ? strndup(fname, field->data.param.name->data.ident.name.len) : (char *)"";
        size_t fn_len = strlen(fname_safe) + 1;
        size_t ft_len = strlen(ftype) + 1;
        memcpy(data + pos, fname_safe, fn_len);
        pos += fn_len;
        memcpy(data + pos, ftype, ft_len);
        pos += ft_len;
        if (fname_safe[0]) free(fname_safe);
        /* Use a simple sequential offset model (8-byte aligned) */
        uint64_t size = 8;
        memcpy(data + pos, &field_offset, 8);
        pos += 8;
        memcpy(data + pos, &size, 8);
        pos += 8;
        field_offset += size;
    }

    *out_size = pos;
    return data;
}

/* Build an enum layout type data blob.
 * Format: variant_count (u8), for each variant: name (null-term), discriminant (u64), payload_type (null-term) */
uint8_t *build_enum_type_data(AstNode *decl, size_t *out_size) {
    if (!decl || decl->type != NODE_ENUM_DECL) {
        *out_size = 0;
        return NULL;
    }

    size_t total = 1; /* variant_count */
    for (int i = 0; i < decl->data.enum_decl.variants.count; i++) {
        AstNode *var = decl->data.enum_decl.variants.items[i];
        if (var->type != NODE_ENUM_VARIANT) continue;
        const char *vname = var->data.enum_variant.name ?
            var->data.enum_variant.name->data.ident.name.data : "";
        total += strlen(vname) + 1;
        total += 8; /* discriminant */
        /* Payload type: if no payload, store empty string */
        const char *ptype = "";
        if (var->data.enum_variant.payload_types.count > 0) {
            AstNode *pt = var->data.enum_variant.payload_types.items[0];
            ptype = type_node_name(pt);
        }
        total += strlen(ptype) + 1;
    }

    uint8_t *data = (uint8_t *)malloc(total);
    if (!data) { *out_size = 0; return NULL; }

    size_t pos = 0;
    uint8_t vcount = (uint8_t)(decl->data.enum_decl.variants.count > 255 ? 255 : decl->data.enum_decl.variants.count);
    data[pos++] = vcount;

    for (int i = 0; i < decl->data.enum_decl.variants.count && i < 255; i++) {
        AstNode *var = decl->data.enum_decl.variants.items[i];
        if (var->type != NODE_ENUM_VARIANT) continue;
        const char *vname = var->data.enum_variant.name ?
            var->data.enum_variant.name->data.ident.name.data : "";
        size_t vn_len = strlen(vname) + 1;
        memcpy(data + pos, vname, vn_len);
        pos += vn_len;
        uint64_t disc = (uint64_t)i;
        memcpy(data + pos, &disc, 8);
        pos += 8;
        const char *ptype = "";
        if (var->data.enum_variant.payload_types.count > 0) {
            AstNode *pt = var->data.enum_variant.payload_types.items[0];
            ptype = type_node_name(pt);
        }
        size_t pt_len = strlen(ptype) + 1;
        memcpy(data + pos, ptype, pt_len);
        pos += pt_len;
    }

    *out_size = pos;
    return data;
}

int codegen_extract_metadata(Codegen *cg, AstNode *program) {
    if (!cg->aelib_writer) {
        cg->aelib_writer = aelib_create();
        if (!cg->aelib_writer) {
            fprintf(stderr, "Error: failed to create aelib writer\n");
            return 1;
        }
    }

    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *node = program->data.list.items[i];
        if (!node) continue;

        const char *name = decl_name(node);
        uint8_t kind;
        uint8_t *type_data = NULL;
        size_t type_data_size = 0;

        switch (node->type) {
            case NODE_FUNC_DECL:
                kind = AELIB_SYM_FUNC;
                type_data = build_func_type_data(node, &type_data_size);
                break;
            case NODE_STRUCT_DECL:
                kind = AELIB_SYM_STRUCT;
                type_data = build_struct_type_data(node, &type_data_size);
                break;
            case NODE_CLASS_DECL:
                kind = AELIB_SYM_CLASS;
                type_data = build_struct_type_data(node, &type_data_size);
                break;
            case NODE_ENUM_DECL:
                kind = AELIB_SYM_ENUM;
                type_data = build_enum_type_data(node, &type_data_size);
                break;
            case NODE_CONST_DECL:
                kind = AELIB_SYM_CONST;
                break;
            default:
                continue; /* skip non-exportable decls */
        }

        if (name && name[0]) {
            uint8_t sym_flags = AELIB_FLAG_PUBLIC;
            if (node->type == NODE_FUNC_DECL && node->data.func.is_sys) {
                sym_flags |= AELIB_FLAG_SYS;
            }
            aelib_add_symbol(cg->aelib_writer, name, kind, sym_flags, NULL,
                             type_data, (uint32_t)type_data_size);
        }

        free(type_data);
    }

    return 0;
}

void codegen_add_aelib_import(Codegen *cg, const char *path) {
    if (cg->aelib_import_count >= cg->aelib_import_cap) {
        int nc = cg->aelib_import_cap ? cg->aelib_import_cap * 2 : 8;
        char **na = (char **)realloc(cg->aelib_imports, nc * sizeof(char *));
        if (!na) return;
        cg->aelib_imports = na;
        cg->aelib_import_cap = nc;
    }
    cg->aelib_imports[cg->aelib_import_count++] = strdup(path);
}