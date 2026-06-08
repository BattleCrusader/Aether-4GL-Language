#ifndef AETHER_DEFS_H
#define AETHER_DEFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Source location — every token and AST node carries one */
typedef struct {
    const char *file;
    int line;
    int col;
    int len;
} Location;

#define LOCATION(F, L, C, N) ((Location){ (F), (L), (C), (N) })
#define NO_LOCATION ((Location){ NULL, 0, 0, 0 })

/* Opaque pointer for arena allocation */
typedef struct Arena Arena;

#endif /* AETHER_DEFS_H */