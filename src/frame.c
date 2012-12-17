#include "nitro.h"
#include "nitro-private.h"

nitro_frame_t *nitro_frame_copy(nitro_frame_t *f) {
    nitro_frame_t *result = malloc(sizeof(nitro_frame_t));
    memcpy(result, f, sizeof(nitro_frame_t));
    buffer_incref(f->buffer);
    return result;
    
}
nitro_frame_t *nitro_frame_new(void *data, uint32_t size, nitro_free_function ff, void *baton) {
    nitro_frame_t *f = malloc(sizeof(nitro_frame_t));

    nitro_counted_buffer * buffer = nitro_counted_buffer_new(data, ff,baton);

    f->buffer = buffer;
    f->size = size;
    f->prev = f->next = NULL;
    pthread_mutex_init(&f->lock, NULL);
    return f;
}

void nitro_frame_destroy(nitro_frame_t *f) {
    buffer_decref(NULL, f->buffer);
    free(f);
}

nitro_frame_t * nitro_frame_new_copy(void *data, uint32_t size) {
    char *n = malloc(size);
    memmove(n, data, size);
    return nitro_frame_new(n, size, just_free, NULL);
}

inline void * nitro_frame_data(nitro_frame_t *fr) {
    return ((nitro_counted_buffer *)fr->buffer)->ptr;
}

inline uint32_t nitro_frame_size(nitro_frame_t *fr) {
    return fr->size;
}


