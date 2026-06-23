/*
 * aelib.c — .aelib library format writer.
 *
 * Produces a binary archive containing compiled code (.o) and metadata
 * (symbol table, type signatures, class layouts) for closed-source
 * library distribution.
 */

#include "aether/aelib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Internal structures
 * ================================================================ */

typedef struct {
    char *name;
    uint8_t kind;
    uint8_t flags;
    char *ns;           /* namespace/class name, NULL = top-level */
    uint8_t *type_data;
    uint32_t type_data_size;
} SymEntry;

struct AelibWriter {
    uint8_t *code_data;
    size_t code_size;
    SymEntry *symbols;
    int sym_count;
    int sym_cap;
    char *string_table;
    size_t string_table_size;
    size_t string_table_cap;
    uint8_t *type_data_blob;
    size_t type_data_blob_size;
    size_t type_data_blob_cap;
};

/* ================================================================
 * String table helpers
 * ================================================================ */

/* Add a string to the string table. Returns its offset. */
static uint32_t string_table_add(AelibWriter *w, const char *s) {
    if (!s || s[0] == '\0') return 0; /* empty string = offset 0 */

    size_t len = strlen(s) + 1; /* include null terminator */
    size_t needed = w->string_table_size + len;
    if (needed > w->string_table_cap) {
        size_t new_cap = w->string_table_cap ? w->string_table_cap * 2 : 256;
        while (new_cap < needed) new_cap *= 2;
        char *new_tab = (char *)realloc(w->string_table, new_cap);
        if (!new_tab) return 0;
        w->string_table = new_tab;
        w->string_table_cap = new_cap;
    }

    uint32_t offset = (uint32_t)w->string_table_size;
    memcpy(w->string_table + offset, s, len);
    w->string_table_size = needed;
    return offset;
}

/* ================================================================
 * Type data blob helpers
 * ================================================================ */

/* NOTE: type data is stored per-symbol (in SymEntry.type_data) and
 * assembled into the blob at write time. The helper below is kept
 * for reference but not used in the current implementation. */

/* ================================================================
 * Public API
 * ================================================================ */

AelibWriter *aelib_create(void) {
    AelibWriter *w = (AelibWriter *)calloc(1, sizeof(AelibWriter));
    if (!w) return NULL;
    w->string_table_cap = 256;
    w->string_table = (char *)malloc(w->string_table_cap);
    if (!w->string_table) { free(w); return NULL; }
    w->string_table[0] = '\0';
    w->string_table_size = 1; /* offset 0 = empty string */

    w->type_data_blob_cap = 1024;
    w->type_data_blob = (uint8_t *)malloc(w->type_data_blob_cap);
    if (!w->type_data_blob) { free(w->string_table); free(w); return NULL; }
    w->type_data_blob_size = 0;

    w->sym_cap = 16;
    w->symbols = (SymEntry *)calloc(w->sym_cap, sizeof(SymEntry));
    if (!w->symbols) { free(w->type_data_blob); free(w->string_table); free(w); return NULL; }

    return w;
}

void aelib_set_code(AelibWriter *w, uint8_t *data, size_t size) {
    if (w->code_data) free(w->code_data);
    w->code_data = data;
    w->code_size = size;
}

int aelib_add_symbol(AelibWriter *w, const char *name, uint8_t kind,
                     bool is_pub, const char *ns,
                     const uint8_t *type_data, uint32_t type_data_size)
{
    if (w->sym_count >= w->sym_cap) {
        int new_cap = w->sym_cap * 2;
        SymEntry *new_syms = (SymEntry *)realloc(w->symbols, new_cap * sizeof(SymEntry));
        if (!new_syms) return -1;
        w->symbols = new_syms;
        w->sym_cap = new_cap;
    }

    SymEntry *e = &w->symbols[w->sym_count];
    /* Use strndup since the name string may not be null-terminated in the source buffer */
    /* We know names are short identifiers (< 64 chars), so pass a safe max */
    e->name = strndup(name ? name : "", name ? strlen(name) : 0);
    e->kind = kind;
    e->flags = is_pub ? AELIB_FLAG_PUBLIC : 0;
    e->ns = ns ? strndup(ns, 64) : NULL;
    e->type_data = NULL;
    e->type_data_size = 0;

    /* Add name and namespace to string table so we have offsets */
    string_table_add(w, e->name);
    if (e->ns) string_table_add(w, e->ns);

    if (type_data && type_data_size > 0) {
        e->type_data = (uint8_t *)malloc(type_data_size);
        if (e->type_data) {
            memcpy(e->type_data, type_data, type_data_size);
            e->type_data_size = type_data_size;
        }
    }

    return w->sym_count++;
}

int aelib_write(AelibWriter *w, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Error: cannot write '%s'\n", path);
        return 1;
    }

    /*
     * Metadata section layout:
     *   AelibMetaHeader (14 bytes)
     *   SymEntry array (18 bytes each × sym_count)
     *   Type data blob
     *   String table
     */

    size_t meta_header_size = sizeof(AelibMetaHeader);
    size_t sym_entries_size = w->sym_count * sizeof(AelibSymEntry);

    /* Compute total type data size from per-symbol data */
    size_t type_data_size = 0;
    for (int i = 0; i < w->sym_count; i++) {
        type_data_size += w->symbols[i].type_data_size;
    }

    size_t string_table_size = w->string_table_size;

    size_t meta_total = meta_header_size + sym_entries_size + type_data_size + string_table_size;

    /* Build the metadata buffer */
    uint8_t *meta_buf = (uint8_t *)malloc(meta_total);
    if (!meta_buf) { fclose(f); return 1; }

    /* Write meta header */
    AelibMetaHeader *mh = (AelibMetaHeader *)meta_buf;
    memcpy(mh->magic, AEMETA_MAGIC, AEMETA_MAGIC_LEN);
    mh->version = AEMETA_VERSION;
    mh->sym_count = (uint32_t)w->sym_count;

    /* Compute offsets within the metadata section */
    uint32_t type_data_offset = (uint32_t)(meta_header_size + sym_entries_size);
    uint32_t string_table_offset = type_data_offset + (uint32_t)type_data_size;

    /* Write symbol entries */
    AelibSymEntry *sym_entries = (AelibSymEntry *)(meta_buf + meta_header_size);
    uint32_t cumulative_td_off = type_data_offset;

    for (int i = 0; i < w->sym_count; i++) {
        SymEntry *src = &w->symbols[i];
        AelibSymEntry *dst = &sym_entries[i];

        /* Find name offset in string table by scanning */
        uint32_t name_off = string_table_offset;
        const char *p = w->string_table;
        uint32_t off = 0;
        while (off < w->string_table_size) {
            if (strcmp(p, src->name) == 0) {
                name_off = string_table_offset + off;
                break;
            }
            size_t slen = strlen(p) + 1;
            off += (uint32_t)slen;
            p += slen;
        }

        /* Find namespace offset */
        uint32_t ns_off = 0;
        if (src->ns) {
            off = 0;
            p = w->string_table;
            while (off < w->string_table_size) {
                if (strcmp(p, src->ns) == 0) {
                    ns_off = string_table_offset + off;
                    break;
                }
                size_t slen = strlen(p) + 1;
                off += (uint32_t)slen;
                p += slen;
            }
        }

        /* Copy type data into the metadata buffer at the cumulative offset */
        if (src->type_data && src->type_data_size > 0) {
            memcpy(meta_buf + cumulative_td_off, src->type_data, src->type_data_size);
        }

        dst->name_offset = name_off;
        dst->kind = src->kind;
        dst->flags = src->flags;
        dst->namespace_offset = ns_off;
        dst->type_data_offset = cumulative_td_off;
        dst->type_data_size = src->type_data_size;

        cumulative_td_off += src->type_data_size;
    }

    /* Copy string table */
    if (string_table_size > 0) {
        memcpy(meta_buf + string_table_offset, w->string_table, string_table_size);
    }

    /* Now write the file header */
    AelibHeader header;
    memcpy(header.magic, AELIB_MAGIC, AELIB_MAGIC_LEN);
    header.version = AELIB_VERSION;
    header.flags = 0;
    header.abi_version = AELIB_ABI_VERSION;

    /* Layout: header (46 bytes) | code section | metadata section */
    uint64_t code_offset = sizeof(AelibHeader);
    uint64_t meta_offset = code_offset + w->code_size;

    header.code_offset = code_offset;
    header.code_size = (uint64_t)w->code_size;
    header.meta_offset = meta_offset;
    header.meta_size = (uint64_t)meta_total;

    /* Write header */
    fwrite(&header, sizeof(header), 1, f);

    /* Write code section */
    if (w->code_data && w->code_size > 0) {
        fwrite(w->code_data, 1, w->code_size, f);
    }

    /* Write metadata section */
    fwrite(meta_buf, 1, meta_total, f);

    fclose(f);
    free(meta_buf);

    return 0;
}

void aelib_destroy(AelibWriter *w) {
    if (!w) return;
    free(w->code_data);
    for (int i = 0; i < w->sym_count; i++) {
        free(w->symbols[i].name);
        free(w->symbols[i].ns);
        free(w->symbols[i].type_data);
    }
    free(w->symbols);
    free(w->string_table);
    free(w->type_data_blob);
    free(w);
}

/* ================================================================
 * Reader implementation
 * ================================================================ */

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

struct AelibReader {
    uint8_t *data;
    size_t   size;
    AelibHeader hdr;
    AelibMetaHeader meta_hdr;
    int sym_count;
    size_t sym_table_off;   /* offset of symbol table within data */
    size_t string_table_off;
    size_t string_table_size;
    size_t type_data_off;
    size_t type_data_size;
};

/* ─── Helper: read little-endian ─── */

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t read_u64(const uint8_t *p) {
    return (uint64_t)read_u32(p) | ((uint64_t)read_u32(p + 4) << 32);
}

static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/* ─── aelib_validate ─── */

int aelib_validate(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    AelibHeader hdr;
    if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) { fclose(f); return -1; }
    if (memcmp(hdr.magic, AELIB_MAGIC, AELIB_MAGIC_LEN) != 0) { fclose(f); return -1; }
    uint16_t version;
    memcpy(&version, (uint8_t *)&hdr + 8, 2);
    if (version != AELIB_VERSION) { fclose(f); return -1; }
    uint64_t code_off, meta_off;
    memcpy(&code_off, (uint8_t *)&hdr + 14, 8);
    memcpy(&meta_off, (uint8_t *)&hdr + 30, 8);
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (code_off > (uint64_t)fsize || meta_off > (uint64_t)fsize) { fclose(f); return -1; }
    fclose(f);
    return 0;
}

/* ─── aelib_open ─── */

AelibReader *aelib_open(const char *path) {
    if (aelib_validate(path) != 0) return NULL;

    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;
    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return NULL; }
    size_t fsize = (size_t)st.st_size;

    uint8_t *data = (uint8_t *)mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (data == MAP_FAILED) {
        data = (uint8_t *)malloc(fsize);
        if (!data) return NULL;
        FILE *f = fopen(path, "rb");
        if (!f) { free(data); return NULL; }
        if (fread(data, 1, fsize, f) != (size_t)fsize) { free(data); fclose(f); return NULL; }
        fclose(f);
    }

    AelibReader *r = (AelibReader *)calloc(1, sizeof(AelibReader));
    if (!r) {
        munmap(data, fsize);
        return NULL;
    }

    r->data = data;
    r->size = fsize;

    /* Parse AelibHeader (46 bytes) */
    memcpy(r->hdr.magic,       data +  0, 8);
    r->hdr.version     = read_u16(data +  8);
    r->hdr.flags       = read_u16(data + 10);
    r->hdr.abi_version = read_u16(data + 12);
    r->hdr.code_offset = read_u64(data + 14);
    r->hdr.code_size   = read_u64(data + 22);
    r->hdr.meta_offset = read_u64(data + 30);
    r->hdr.meta_size   = read_u64(data + 38);

    /* Parse AemetaHeader */
    size_t mo = (size_t)r->hdr.meta_offset;
    memcpy(r->meta_hdr.magic,       data + mo,      8);
    r->meta_hdr.version      = read_u16(data + mo +  8);
    r->meta_hdr.sym_count    = read_u32(data + mo + 10);

    /* Symbol table: starts at mo + 14 (= sizeof(AemetaHeader) = 8+2+4) */
    r->sym_table_off = mo + 14;
    r->sym_count = (int)r->meta_hdr.sym_count;

    /* Type data blob: immediately after symbol table
     * Each AelibSymEntry is 18 bytes: name_off(4)+kind(1)+flags(1)+ns_off(4)+td_off(4)+td_sz(4) */
    r->type_data_off = r->sym_table_off + (size_t)r->sym_count * 18;

    /* String table: after type data blob.
     * The writer stores name_offset and type_data_offset as ABSOLUTE offsets
     * from the start of the metadata section (i.e., from mo).
     * So to resolve a name: data + mo + name_off
     * To resolve type data: data + mo + td_off
     *
     * We still compute string_table_off for the aelib_get_string() helper
     * which takes offsets relative to the string table start. */
    size_t total_type_data = 0;
    for (int i = 0; i < r->sym_count; i++) {
        size_t off = r->sym_table_off + (size_t)i * 18;
        uint32_t td_sz = read_u32(data + off + 14);
        total_type_data += td_sz;
    }
    r->type_data_size = total_type_data;
    r->string_table_off = r->type_data_off + total_type_data;
    r->string_table_size = (size_t)(r->hdr.meta_offset + r->hdr.meta_size) - r->string_table_off;

    return r;
}

/* ─── aelib_close ─── */

void aelib_close(AelibReader *r) {
    if (!r) return;
    munmap(r->data, r->size);
    free(r);
}

/* ─── aelib_get_symbol_count ─── */

int aelib_get_symbol_count(const AelibReader *r) {
    return r ? r->sym_count : 0;
}

/* ─── aelib_find_symbol ─── */

int aelib_find_symbol(const AelibReader *r, const char *name, int kind,
                      char **out_name, int *out_kind, int *out_flags,
                      uint32_t *out_td_offset, uint32_t *out_td_size) {
    if (!r) return 0;
    size_t mo = (size_t)r->hdr.meta_offset;
    for (int i = 0; i < r->sym_count; i++) {
        size_t off = r->sym_table_off + (size_t)i * 18;
        uint32_t name_off = read_u32(r->data + off);
        /* name_off is absolute from metadata start */
        const char *sym_name = (const char *)r->data + mo + name_off;
        if (strcmp(sym_name, name) != 0) continue;
        uint8_t k = r->data[off + 4];
        if (kind >= 0 && k != (uint8_t)kind) continue;

        if (out_name) {
            *out_name = strdup(sym_name);
        }
        if (out_kind) *out_kind = k;
        if (out_flags) *out_flags = r->data[off + 5];
        if (out_td_offset) *out_td_offset = read_u32(r->data + off + 10);
        if (out_td_size) *out_td_size = read_u32(r->data + off + 14);
        return 1;
    }
    return 0;
}

/* ─── aelib_get_string ─── */

const char *aelib_get_string(const AelibReader *r, uint32_t offset) {
    if (!r) return NULL;
    if (offset >= r->string_table_size) return NULL;
    return (const char *)r->data + r->string_table_off + offset;
}

/* ─── aelib_get_code_section ─── */

const uint8_t *aelib_get_code_section(const AelibReader *r, size_t *out_size) {
    if (!r) { if (out_size) *out_size = 0; return NULL; }
    if (out_size) *out_size = (size_t)r->hdr.code_size;
    return r->data + (size_t)r->hdr.code_offset;
}

/* ─── aelib_extract_object ─── */

int aelib_extract_object(const char *aelib_path, const char *output_o_path) {
    AelibReader *r = aelib_open(aelib_path);
    if (!r) return -1;
    size_t code_size;
    const uint8_t *code = aelib_get_code_section(r, &code_size);
    if (!code || code_size == 0) { aelib_close(r); return -1; }
    FILE *f = fopen(output_o_path, "wb");
    if (!f) { aelib_close(r); return -1; }
    if (fwrite(code, 1, code_size, f) != code_size) {
        fclose(f); aelib_close(r); remove(output_o_path); return -1;
    }
    fclose(f);
    aelib_close(r);
    return 0;
}
