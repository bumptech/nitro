#include "nitro.h"
#include "nitro-private.h"

nitro_frame_t *nitro_frame_new(void *data, uint32_t size, nitro_free_function ff, void *baton) {
    nitro_frame_t *f = malloc(sizeof(nitro_frame_t));

    f->data = data;
    f->size = size;
    f->ff = ff;
    f->baton = baton;
    f->prev = f->next = NULL;
    pthread_mutex_init(&f->lock, NULL);
    f->ref_count = 1;
    return f;
}

void nitro_frame_retain(nitro_frame_t *f) {
    pthread_mutex_lock(&f->lock);
    f->ref_count++;
    pthread_mutex_unlock(&f->lock);
}

void nitro_frame_destroy(nitro_frame_t *f) {
    // This decrements the ref_count and frees if necessary
    pthread_mutex_lock(&f->lock);
    f->ref_count--;
    pthread_mutex_unlock(&f->lock);

    assert(f->ref_count >= 0);
    if (f->ref_count == 0)
    {
        f->ff(f->data, f->baton);
        free(f);
    }
}

nitro_frame_t * nitro_frame_new_copy(void *data, uint32_t size) {
    char *n = malloc(size);
    memmove(n, data, size);
    return nitro_frame_new(n, size, just_free, NULL);
}

inline void * nitro_frame_data(nitro_frame_t *fr) {
    return fr->data;
}

inline uint32_t nitro_frame_size(nitro_frame_t *fr) {
    return fr->size;
}


