#ifndef AETHER_ARENA_H
#define AETHER_ARENA_H

#include "defs.h"
#include <stdalign.h>

/*
 * Arena allocator — bump pointer, batch-free.
 * All AST nodes, token arrays, etc. are allocated from arenas.
 * No individual frees. Entire arena is freed at once.
 */

typedef struct Arena {
    char *base;
    char *ptr;
    char *end;
    struct Arena *next;  /* linked list of arena blocks */
} Arena;

/* Create a new arena with default block size (64KB) */
Arena *arena_create(void);

/* Create with custom block size */
Arena *arena_create_with_size(size_t block_size);

/* Allocate zeroed memory from arena. Returns 16-byte aligned pointer. */
void *arena_alloc(Arena *a, size_t size);

/* Allocate without zeroing */
void *arena_alloc_unzeroed(Arena *a, size_t size);

/* Allocate with specific alignment */
void *arena_alloc_aligned(Arena *a, size_t size, size_t align);

/* Duplicate a string into arena memory */
char *arena_strdup(Arena *a, const char *s);

/* Duplicate a string with length limit */
char *arena_strndup(Arena *a, const char *s, size_t len);

/* Reset arena to beginning (frees all blocks except the first) */
void arena_reset(Arena *a);

/* Destroy arena and all its blocks */
void arena_destroy(Arena *a);

/* Total bytes used so far */
size_t arena_used(Arena *a);

#endif /* AETHER_ARENA_H */