#include "aether/arena.h"
#include <stdlib.h>
#include <string.h>

#define DEFAULT_BLOCK_SIZE (64 * 1024)  /* 64KB blocks */
#define ALIGN_UP(p, a) (((uintptr_t)(p) + ((a)-1)) & ~((uintptr_t)(a)-1))

static Arena *arena_new_block(Arena *a, size_t min_size) {
    size_t block_size = DEFAULT_BLOCK_SIZE;
    if (min_size > block_size) {
        block_size = min_size + DEFAULT_BLOCK_SIZE;
    }

    char *mem = (char *)malloc(sizeof(Arena) + block_size);
    if (!mem) return NULL;

    Arena *block = (Arena *)mem;
    block->base = mem + sizeof(Arena);
    block->ptr = block->base;
    block->end = block->base + block_size;
    block->next = NULL;

    if (a) {
        /* Find the last block and link */
        while (a->next) a = a->next;
        a->next = block;
    }

    return block;
}

Arena *arena_create(void) {
    return arena_create_with_size(DEFAULT_BLOCK_SIZE);
}

Arena *arena_create_with_size(size_t block_size) {
    char *mem = (char *)malloc(sizeof(Arena) + block_size);
    if (!mem) return NULL;

    Arena *a = (Arena *)mem;
    a->base = mem + sizeof(Arena);
    a->ptr = a->base;
    a->end = a->base + block_size;
    a->next = NULL;
    return a;
}

void *arena_alloc(Arena *a, size_t size) {
    void *ptr = arena_alloc_unzeroed(a, size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

void *arena_alloc_unzeroed(Arena *a, size_t size) {
    if (size == 0) return NULL;
    size = ALIGN_UP(size, 16);

    /* If we can't fit, allocate a new block */
    if (a->ptr + size > a->end) {
        Arena *block = arena_new_block(a, size);
        if (!block) return NULL;
        a->ptr = block->ptr;
        a->end = block->end;
    }

    void *ptr = a->ptr;
    a->ptr += size;
    return ptr;
}

void *arena_alloc_aligned(Arena *a, size_t size, size_t align) {
    if (align < 16) align = 16;
    char *aligned = (char *)ALIGN_UP(a->ptr, align);
    size_t padded = size + (aligned - a->ptr);

    if (a->ptr + padded > a->end) {
        Arena *block = arena_new_block(a, padded + align);
        if (!block) return NULL;
        a->ptr = block->ptr;
        a->end = block->end;
        aligned = (char *)ALIGN_UP(a->ptr, align);
        padded = size;
    }

    a->ptr = aligned + size;
    return memset(aligned, 0, size);
}

char *arena_strdup(Arena *a, const char *s) {
    size_t len = strlen(s);
    return arena_strndup(a, s, len);
}

char *arena_strndup(Arena *a, const char *s, size_t len) {
    char *cpy = (char *)arena_alloc(a, len + 1);
    if (cpy) {
        memcpy(cpy, s, len);
        cpy[len] = '\0';
    }
    return cpy;
}

void arena_reset(Arena *a) {
    /* Free all blocks except the first */
    Arena *block = a->next;
    while (block) {
        Arena *next = block->next;
        free(block);
        block = next;
    }
    a->next = NULL;
    a->ptr = a->base;
}

void arena_destroy(Arena *a) {
    while (a) {
        Arena *next = a->next;
        free(a);
        a = next;
    }
}

size_t arena_used(Arena *a) {
    size_t total = (size_t)(a->ptr - a->base);
    while (a->next) {
        a = a->next;
        total += (size_t)(a->ptr - a->base);
    }
    return total;
}