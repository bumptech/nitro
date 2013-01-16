#include "nitro.h"
#include "nitro-private.h"

#include <sys/time.h>

void fatal(char *why) {
    fprintf(stderr, "fatal error: %s\n", why);
}

nitro_counted_buffer *nitro_counted_buffer_new(void *backing, nitro_free_function ff, void *baton) {
    nitro_counted_buffer *buf;
    ZALLOC(buf);
    buf->ptr = backing;
    buf->count = 1;
    pthread_mutex_init(&buf->lock, NULL);
    buf->ff = ff;
    buf->baton = baton;
    return buf;
}

void nitro_counted_buffer_decref(nitro_counted_buffer *buf) {
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

void nitro_counted_buffer_incref(nitro_counted_buffer *buf) {
    pthread_mutex_lock(&buf->lock);
    buf->count++;
    pthread_mutex_unlock(&buf->lock);
}

void buffer_decref(void *data, void *bufptr) {
    nitro_counted_buffer *buf = (nitro_counted_buffer *)bufptr;
    nitro_counted_buffer_decref(buf);
}

void just_free(void *data, void *unused) {
    free(data);
}

double now_double() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((double)tv.tv_sec +
            ((double)tv.tv_usec / 1000000));
}
