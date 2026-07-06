#ifndef AETHER_AELIB_H
#define AETHER_AELIB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * .aelib Library Format — Writer API
 *
 * The .aelib format is a binary archive containing compiled code (.o files)
 * and metadata (symbol table, type signatures, class layouts) for
 * closed-source library distribution.
 *
 * File layout:
 *   [Header] 32 bytes
 *   [Code section] — raw .o file content
 *   [Metadata section] — symbol table + type data + string table
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * Constants
 * ================================================================ */

/* Magic bytes */
#define AELIB_MAGIC       "AELIB\0\0"
#define AELIB_MAGIC_LEN   8
#define AELIB_VERSION     0x0001
#define AELIB_ABI_VERSION 0x0001

#define AEMETA_MAGIC     "AEMETA\0\0"
#define AEMETA_MAGIC_LEN  8
#define AEMETA_VERSION    0x0001

/* Symbol kinds */
#define AELIB_SYM_FUNC    0
#define AELIB_SYM_STRUCT  1
#define AELIB_SYM_CLASS   2
#define AELIB_SYM_GLOBAL  3
#define AELIB_SYM_CONST   4
#define AELIB_SYM_ENUM    5

/* Symbol flags */
#define AELIB_FLAG_PUBLIC  0x01
#define AELIB_FLAG_SYS     0x02

/* ================================================================
 * On-disk structures (packed, little-endian)
 * ================================================================ */

#pragma pack(push, 1)

typedef struct {
    uint8_t  magic[8];       /* "AELIB\0" */
    uint16_t version;        /* 0x0001 */
    uint16_t flags;          /* reserved */
    uint16_t abi_version;    /* 0x0001 */
    uint64_t code_offset;    /* offset to code section */
    uint64_t code_size;      /* size of code section */
    uint64_t meta_offset;    /* offset to metadata section */
    uint64_t meta_size;      /* size of metadata section */
} AelibHeader;

typedef struct {
    uint8_t  magic[8];       /* "AEMETA\0\0" */
    uint16_t version;        /* 0x0001 */
    uint32_t sym_count;      /* number of symbol entries */
    /* Followed by sym_count AelibSymEntry structs (18 bytes each):
     *   name_offset(4) + kind(1) + flags(1) + reserved(2) + ns_offset(4) + td_offset(4) + td_size(4)
     * Followed by type data blob (sym_count entries, varaying size)
     * Followed by string table (null-terminated strings concatenated) */
} AelibMetaHeader;

typedef struct {
    uint32_t name_offset;        /* offset into string table */
    uint8_t  kind;               /* AELIB_SYM_* */
    uint8_t  flags;              /* AELIB_FLAG_* */
    uint32_t namespace_offset;   /* class/namespace name, 0 = top-level */
    uint32_t type_data_offset;   /* offset into type data blob */
    uint32_t type_data_size;     /* size of type data for this symbol */
} AelibSymEntry;

#pragma pack(pop)

/* ================================================================
 * Writer API
 * ================================================================ */

/* Opaque writer handle */
typedef struct AelibWriter AelibWriter;

/* Opaque reader handle */
typedef struct AelibReader AelibReader;

/* Create a new .aelib writer */
AelibWriter *aelib_create(void);

/* Set the code section content (the .o file data).
 * The writer takes ownership of the data (will free it on destroy). */
void aelib_set_code(AelibWriter *w, uint8_t *data, size_t size);

/* Add a symbol to the metadata section.
 * name: the symbol name (will be copied into string table)
 * kind: AELIB_SYM_* constant
 * flags: AELIB_FLAG_* bitmask (AELIB_FLAG_PUBLIC, AELIB_FLAG_SYS, etc.)
 * namespace: namespace/class name, NULL for top-level
 * type_data: raw type data blob (will be copied)
 * type_data_size: size of type data
 * Returns the index of the added symbol. */
int aelib_add_symbol(AelibWriter *w, const char *name, uint8_t kind,
                     uint8_t flags, const char *ns,
                     const uint8_t *type_data, uint32_t type_data_size);

/* Write the .aelib file to disk.
 * Returns 0 on success, nonzero on error. */
int aelib_write(AelibWriter *w, const char *path);

/* Destroy the writer and free all resources */
void aelib_destroy(AelibWriter *w);

/* ================================================================
 * Reader API
 * ================================================================ */

/* Open an .aelib file for reading.
 * Returns a reader handle, or NULL on error.
 * The file is memory-mapped. */
AelibReader *aelib_open(const char *path);

/* Close and free an .aelib reader */
void aelib_close(AelibReader *r);

/* Validate an .aelib file (doesn't open it).
 * Returns 0 if valid, -1 if invalid or can't be opened. */
int aelib_validate(const char *path);

/* Get the number of exported symbols */
int aelib_get_symbol_count(const AelibReader *r);

/* Iterate symbols. kind is AELIB_SYM_*.
 * out_name receives the symbol name (caller must free with aelib_free_str).
 * Returns true if found. */
int aelib_find_symbol(const AelibReader *r, const char *name, int kind,
                      char **out_name, int *out_kind, int *out_flags,
                      uint32_t *out_type_data_offset, uint32_t *out_type_data_size);

/* Get a string from the string table by offset.
 * Returns NULL if offset is out of range. */
const char *aelib_get_string(const AelibReader *r, uint32_t offset);

/* Get the raw code section (.o bytes).
 * Returns pointer to code data and sets *out_size. */
const uint8_t *aelib_get_code_section(const AelibReader *r, size_t *out_size);

/* Extract the code section to a .o file on disk.
 * Returns 0 on success, -1 on error. */
int aelib_extract_object(const char *aelib_path, const char *output_o_path);

#ifdef __cplusplus
}
#endif

#endif /* AETHER_AELIB_H */
