#include "aether/str.h"
#include "aether/arena.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

StringView sv_from_cstr(const char *cstr) {
    return (StringView){ cstr, strlen(cstr) };
}

StringView sv_from_parts(const char *data, size_t len) {
    return (StringView){ data, len };
}

bool sv_eq(StringView a, StringView b) {
    if (a.len != b.len) return false;
    return memcmp(a.data, b.data, a.len) == 0;
}

bool sv_eq_cstr(StringView sv, const char *cstr) {
    size_t clen = strlen(cstr);
    if (sv.len != clen) return false;
    return memcmp(sv.data, cstr, sv.len) == 0;
}

uint64_t sv_hash(StringView sv) {
    uint64_t hash = 5381;
    for (size_t i = 0; i < sv.len; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)sv.data[i];
    }
    return hash;
}

const char *sv_to_cstr(Arena *a, StringView sv) {
    return arena_strndup(a, sv.data, sv.len);
}

const char *strfmt(Arena *a, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (len < 0) return NULL;

    char *buf = (char *)arena_alloc(a, (size_t)len + 1);
    if (!buf) return NULL;

    va_start(args, fmt);
    vsnprintf(buf, (size_t)len + 1, fmt, args);
    va_end(args);

    return buf;
}