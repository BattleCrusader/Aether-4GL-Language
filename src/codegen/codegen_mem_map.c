#include "codegen_internal.h"
/* ================================================================
 * Aether OS memory map — used for @kernel_layout verification
 * ================================================================ */
typedef struct {
    const char *name;
    uint64_t start;
    uint64_t end;   /* exclusive */
} MemoryRegion;

static const MemoryRegion KNOWN_REGIONS[] = {
    {"Stage1 MBR",        0x7C00,     0x7E00},
    {"Stage2 loader",     0x7E00,     0x8000},
    {"Page tables / GDT", 0x1000,     0x4000},
    {"Module registry",   0x4000,     0x5000},
    {"Syscall page",      0x5000,     0x6000},
    {"Kernel base",       0x1000000,  0x11E6000},
    {"Binary exec space", 0x2000000,  0x2100000},
    {"Module slots",      0x2100000,  0x2180000},
    {"Available RAM",     0x11E6000,  0x10000000},
    {NULL, 0, 0}
};

/* Check if a given address overlaps any known memory region */
static const MemoryRegion *find_overlapping_region(uint64_t addr) {
    for (int i = 0; KNOWN_REGIONS[i].name; i++) {
        if (addr >= KNOWN_REGIONS[i].start && addr < KNOWN_REGIONS[i].end) {
            return &KNOWN_REGIONS[i];
        }
    }
    return NULL;
}

/* Verify a kernel-layout-annotated function for memory map conflicts */
void cg_verify_kernel_layout(Codegen *cg, AstNode *func) {
    cg_write_fmt(cg, "; @kernel_layout verification for %.*s\n",
        (int)func->data.func.name->data.ident.name.len,
        func->data.func.name->data.ident.name.data);
    cg_write(cg, "; Aether OS memory map:\n");
    for (int i = 0; KNOWN_REGIONS[i].name; i++) {
        cg_write_fmt(cg, ";   %-20s 0x%07lx - 0x%07lx\n",
            KNOWN_REGIONS[i].name,
            (unsigned long)KNOWN_REGIONS[i].start,
            (unsigned long)KNOWN_REGIONS[i].end);
    }
    cg_write(cg, "; end @kernel_layout\n\n");
}

/* Helper: collect extern declarations from asm blocks and emit at top level */
void collect_externs_from_block(Codegen *cg, AstNode *block) {
    if (!block) return;
    if (block->type == NODE_ASM_BLOCK && block->data.asm_block.text) {
        StringView asm_text = block->data.asm_block.text->data.literal.string_val;
        const char *p = asm_text.data;
        const char *end = p + asm_text.len;
        while (p < end) {
            const char *line_start = p;
            while (p < end && *p != '\n') p++;
            if (p > line_start) {
                /* Check if line starts with "extern" */
                const char *s = line_start;
                while (s < p && (*s == ' ' || *s == '\t')) s++;
                if ((p - s) >= 6 && strncmp(s, "extern", 6) == 0) {
                    cg_write_fmt(cg, "%.*s\n", (int)(p - line_start), line_start);
                }
            }
            if (p < end) p++;
        }
    } else if (block->type == NODE_BLOCK) {
        for (int i = 0; i < block->data.list.count; i++) {
            collect_externs_from_block(cg, block->data.list.items[i]);
        }
    }
}