#include "cbuffer.h"

nitro_counted_buffer_t *nitro_counted_buffer_new(void *backing, nitro_free_function ff, void *baton) {
    nitro_counted_buffer_t *buf;
    buf = malloc(sizeof(nitro_counted_buffer_t));
    buf->ptr = backing;
    atomic_init(&buf->count, 1);
    buf->ff = ff;
    buf->baton = baton;
    return buf;
}
