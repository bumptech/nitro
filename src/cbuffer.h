#ifndef CBUFFER_H
#define CBUFFER_H

#include "common.h"

#include "util.h"

typedef struct nitro_counted_buffer_t {
    void *ptr;
    _Atomic (int) count;
    nitro_free_function ff;
    void *baton;
} nitro_counted_buffer_t;

nitro_counted_buffer_t *nitro_counted_buffer_new(void *backing, nitro_free_function ff, void *baton);

#define nitro_counted_buffer_decref(buf) {\
    if (!atomic_fetch_add(&(buf)->count, -1)) {\
        if ((buf)->ff) {\
            (buf)->ff((buf)->ptr, (buf)->baton);\
        }\
        free((buf));\
    }\
}

#define nitro_counted_buffer_incref(buf) {\
    atomic_fetch_add(&(buf)->count, 1);\
}

//inline void nitro_counted_buffer_incref(nitro_counted_buffer_t *buf);
//inline void nitro_counted_buffer_decref(nitro_counted_buffer_t *buf);

#endif /* CBUFFER_H */
