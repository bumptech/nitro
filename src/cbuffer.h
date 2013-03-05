#ifndef CBUFFER_H
#define CBUFFER_H

#include "common.h"

#include "util.h"

typedef struct nitro_counted_buffer_t {
    void *ptr;
    int count;
    pthread_mutex_t lock;
    nitro_free_function ff;
    void *baton;
} nitro_counted_buffer_t;

nitro_counted_buffer_t *nitro_counted_buffer_new(void *backing, nitro_free_function ff, void *baton);
void nitro_counted_buffer_incref(nitro_counted_buffer_t *buf);
void nitro_counted_buffer_decref(nitro_counted_buffer_t *buf);

#endif /* CBUFFER_H */
