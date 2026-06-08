#ifndef AETHER_VECTOR_H
#define AETHER_VECTOR_H

#include "defs.h"

/*
 * Dynamic array (vector) — arena-allocated, grows by doubling.
 * Stores items of a fixed element size.
 */

typedef struct {
    void *data;
    size_t len;
    size_t cap;
    size_t elem_size;
    Arena *arena;  /* NULL = use malloc */
} Vector;

/* Create a vector using arena allocation */
Vector *vector_create(Arena *a, size_t elem_size);

/* Create a vector using malloc (for temporary use) */
Vector *vector_create_temp(size_t elem_size);

/* Push an item by pointer (copies elem_size bytes) */
void *vector_push(Vector *v, const void *item);

/* Push an item by value for pointer-sized types */
void *vector_push_unsafe(Vector *v);

/* Get pointer to element at index */
void *vector_get(Vector *v, size_t index);

/* Set element at index (copies elem_size bytes) */
void vector_set(Vector *v, size_t index, const void *item);

/* Pop last element */
void vector_pop(Vector *v);

/* Clear all elements (doesn't free memory) */
void vector_clear(Vector *v);

/* Destroy vector */
void vector_destroy(Vector *v);

/* Macros for type-safe usage */
#define VECTOR_OF(T) \
    struct { T *data; size_t len; size_t cap; size_t elem_size; Arena *arena; }

#define vec_push(v, item) do { \
    typeof(item) _tmp = (item); \
    (v) = vector_push(v, &_tmp); \
} while(0)

#define vec_get(v, i, T) (*(T*)vector_get(v, i))
#define vec_last(v, T) (*(T*)vector_get(v, (v)->len - 1))

#endif /* AETHER_VECTOR_H */