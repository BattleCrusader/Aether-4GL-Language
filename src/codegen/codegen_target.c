#include "codegen_internal.h"

/* ================================================================
 * Target detection
 * ================================================================ */

Target codegen_detect_host(void) {
#if defined(__APPLE__) && defined(__MACH__)
    return TARGET_MACHO64;
#elif defined(__linux__)
    return TARGET_ELF64_HOST;
#else
    return TARGET_FREESTANDING;
#endif
}

const char *target_name(Target t) {
    switch (t) {
        case TARGET_FREESTANDING: return "freestanding ELF64";
        case TARGET_MACHO64:      return "Mach-O 64 (macOS)";
        case TARGET_HOST:         return "host (auto-detect)";
        case TARGET_ELF64_HOST:   return "native ELF64 (Linux)";
        case TARGET_KERNEL:       return "kernel ELF64 (0x1000000)";
        case TARGET_MODULE:       return "Aether OS module (.ko)";
        case TARGET_BINARY:       return "Aether OS binary ELF64 (0x2000000)";
        case TARGET_BOOT:         return "flat binary boot sector";
        case TARGET_ASM_X86_64:  return "x86_64 NASM assembly listing";
        case TARGET_ASM_ARM64:   return "ARM64 assembly listing";
        case TARGET_ASM_RISCV64: return "RISC-V assembly listing";
        case TARGET_UNIVERSAL:   return "universal binary (x86_64 + ARM64)";
        case TARGET_UNIVERSAL_ALL: return "universal binary (all architectures)";
        case TARGET_LIB:          return ".aelib library";
    }
    return "unknown";
}

/* Build struct layout from a STRUCT_DECL node */
StructLayout *build_struct_layout(Arena *a, AstNode *node) {
    if (node->type != NODE_STRUCT_DECL && node->type != NODE_CLASS_DECL) return NULL;
    
    StructLayout *sl = (StructLayout *)arena_alloc(a, sizeof(StructLayout));
    sl->name = arena_strndup(a,
        node->data.struct_decl.name->data.ident.name.data,
        node->data.struct_decl.name->data.ident.name.len);
    
    int offset = 0;
    FieldLayout *prev = NULL;
    
    for (int i = 0; i < node->data.struct_decl.fields.count; i++) {
        AstNode *field = node->data.struct_decl.fields.items[i];
        FieldLayout *fl = (FieldLayout *)arena_alloc(a, sizeof(FieldLayout));
        fl->name = arena_strndup(a,
            field->data.param.name->data.ident.name.data,
            field->data.param.name->data.ident.name.len);
        fl->size = type_size(field->data.param.type);
        fl->offset = offset;
        offset += fl->size;
        fl->next = NULL;
        
        if (prev) prev->next = fl;
        else sl->fields = fl;
        prev = fl;
    }
    
    sl->total_size = offset;
    sl->next = struct_layouts;
    sl->is_class = (node->type == NODE_CLASS_DECL);
    struct_layouts = sl;
    return sl;
}

/* Look up a struct layout by name */
StructLayout *find_struct_layout(const char *name) {
    for (StructLayout *sl = struct_layouts; sl; sl = sl->next) {
        if (strcmp(sl->name, name) == 0) return sl;
    }
    return NULL;
}

/* Look up a field offset within a struct */
int find_field_offset(StructLayout *sl, const char *field_name) {
    for (FieldLayout *fl = sl->fields; fl; fl = fl->next) {
        if (strcmp(fl->name, field_name) == 0) return fl->offset;
    }
    return 0;
}