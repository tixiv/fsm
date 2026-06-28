
#pragma once

#include <stddef.h>

typedef struct {
    void *data;
    size_t count;        // element count
    size_t capacity;     // in number of elements
    size_t element_size; // elemnt size in bytes
} Dyn_array;

void dyn_array_init(Dyn_array *a, size_t element_size, size_t initial_capacity);

void *dyn_array_push(Dyn_array *a);
