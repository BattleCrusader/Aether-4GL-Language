#include "aether/vector.h"
#include "aether/arena.h"
#include <stdlib.h>
#include <string.h>

#define VEC_INIT_CAP 16

Vector *vector_create(Arena *a, size_t elem_size) {
    Vector *v = (Vector *)arena_alloc(a, sizeof(Vector));
    if (!v) return NULL;
    v->data = arena_alloc(a, VEC_INIT_CAP * elem_size);
    if (!v->data) return NULL;
    v->len = 0;
    v->cap = VEC_INIT_CAP;
    v->elem_size = elem_size;
    v->arena = a;
    return v;
}

Vector *vector_create_temp(size_t elem_size) {
    Vector *v = (Vector *)malloc(sizeof(Vector));
    if (!v) return NULL;
    v->data = malloc(VEC_INIT_CAP * elem_size);
    if (!v->data) { free(v); return NULL; }
    v->len = 0;
    v->cap = VEC_INIT_CAP;
    v->elem_size = elem_size;
    v->arena = NULL;
    return v;
}

static int vector_grow(Vector *v) {
    size_t new_cap = v->cap * 2;
    size_t new_size = new_cap * v->elem_size;

    if (v->arena) {
        /* Can't realloc in arena; allocate new and copy */
        void *new_data = arena_alloc(v->arena, new_size);
        if (!new_data) return -1;
        memcpy(new_data, v->data, v->len * v->elem_size);
        v->data = new_data;
    } else {
        void *new_data = realloc(v->data, new_size);
        if (!new_data) return -1;
        v->data = new_data;
    }
    v->cap = new_cap;
    return 0;
}

void *vector_push(Vector *v, const void *item) {
    if (v->len >= v->cap) {
        if (vector_grow(v) != 0) return NULL;
    }
    void *slot = (char *)v->data + (v->len * v->elem_size);
    memcpy(slot, item, v->elem_size);
    v->len++;
    return slot;
}

void *vector_push_unsafe(Vector *v) {
    if (v->len >= v->cap) {
        if (vector_grow(v) != 0) return NULL;
    }
    void *slot = (char *)v->data + (v->len * v->elem_size);
    v->len++;
    return slot;
}

void *vector_get(Vector *v, size_t index) {
    if (index >= v->len) return NULL;
    return (char *)v->data + (index * v->elem_size);
}

void vector_set(Vector *v, size_t index, const void *item) {
    if (index >= v->len) return;
    memcpy((char *)v->data + (index * v->elem_size), item, v->elem_size);
}

void vector_pop(Vector *v) {
    if (v->len > 0) v->len--;
}

void vector_clear(Vector *v) {
    v->len = 0;
}

void vector_destroy(Vector *v) {
    if (v && !v->arena) {
        free(v->data);
        free(v);
    }
}