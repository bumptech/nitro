#include "cbuffer.h"

nitro_counted_buffer_t *nitro_counted_buffer_new(void *backing, nitro_free_function ff, void *baton) {
    nitro_counted_buffer_t *buf;
    buf = malloc(sizeof(nitro_counted_buffer_t));
    buf->ptr = backing;
    buf->count = 1;
    pthread_mutex_init(&buf->lock, NULL);
    buf->ff = ff;
    buf->baton = baton;
    return buf;
}

void nitro_counted_buffer_decref(nitro_counted_buffer_t *buf) {
    pthread_mutex_lock(&buf->lock);
    buf->count--;

    if (!buf->count) {
        if (buf->ff) {
            buf->ff(buf->ptr, buf->baton);
        }

        free(buf);
    } else {
        pthread_mutex_unlock(&buf->lock);
    }
}

void nitro_counted_buffer_incref(nitro_counted_buffer_t *buf) {
    pthread_mutex_lock(&buf->lock);
    buf->count++;
    pthread_mutex_unlock(&buf->lock);
}
