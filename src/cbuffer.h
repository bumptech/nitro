#ifndef CBUFFER_H
#define CBUFFER_H

#include "common.h"

#include "util.h"

typedef struct nitro_counted_buffer_t {
    void *ptr;
    int count;
    nitro_free_function ff;
    void *baton;
} nitro_counted_buffer_t;

nitro_counted_buffer_t *nitro_counted_buffer_new(void *backing, nitro_free_function ff, void *baton);

#define nitro_counted_buffer_decref(buf) {\
    nitro_counted_buffer_t *__tmp_buf = (buf);\
    if (__sync_fetch_and_sub(&__tmp_buf->count, 1) == 1) {\
        if ((__tmp_buf)->ff) {\
            (__tmp_buf)->ff((__tmp_buf)->ptr, (__tmp_buf)->baton);\
        }\
        free((__tmp_buf));\
    }\
}

#define nitro_counted_buffer_incref(buf) {\
    nitro_counted_buffer_t *__tmp_buf = (buf);\
    __sync_fetch_and_add(&(__tmp_buf)->count, 1);\
}

//inline void nitro_counted_buffer_incref(nitro_counted_buffer_t *buf);
//inline void nitro_counted_buffer_decref(nitro_counted_buffer_t *buf);

#endif /* CBUFFER_H */
