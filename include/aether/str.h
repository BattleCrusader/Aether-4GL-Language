#ifndef AETHER_STR_H
#define AETHER_STR_H

#include "defs.h"

/*
 * String view (non-owning slice) and string buffer (owning).
 * All strings are UTF-8. Length is in bytes, not codepoints.
 */

/* String view — does NOT own the data */
typedef struct {
    const char *data;
    size_t len;
} StringView;

#define SV(s) ((StringView){ (s), strlen(s) })
#define SV_NULL ((StringView){ NULL, 0 })

/* Create from C string */
StringView sv_from_cstr(const char *cstr);

/* Create from pointer + length */
StringView sv_from_parts(const char *data, size_t len);

/* Compare two string views */
bool sv_eq(StringView a, StringView b);

/* Compare with C string */
bool sv_eq_cstr(StringView sv, const char *cstr);

/* Hash a string view (djb2) */
uint64_t sv_hash(StringView sv);

/* Convert to C string (allocates from arena) */
const char *sv_to_cstr(Arena *a, StringView sv);

/* Format string (printf-style, arena allocated) */
const char *strfmt(Arena *a, const char *fmt, ...);

#endif /* AETHER_STR_H */