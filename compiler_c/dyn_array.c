#include "dyn_array.h"
#include <malloc.h>
#include <string.h>
#include <assert.h>


void dyn_array_init(Dyn_array *a, size_t element_size, size_t initial_capacity) {
    a->data = malloc(initial_capacity * element_size);
    a->element_size = element_size;
    a->capacity = initial_capacity;
    a->count = 0;
}

void *dyn_array_push(Dyn_array *a) {
    assert(a->data);
    if (a->count >= a->capacity) {
        a->capacity *= 2;
        a->data = realloc(a->data, a->capacity * a->element_size);
    }
    void *e = a->data + a->count * a->element_size;
    memset(e, 0, a->element_size);
    a->count++;
    return e;
}

void dyn_array_push_p(Dyn_array *a, void *element) {
    *((void **)dyn_array_push(a)) = element;
}
