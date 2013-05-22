#ifndef QUEUE_H
#define QUEUE_H

#ifdef __linux__
#include <sys/uio.h>
#ifndef IOV_MAX
#define IOV_MAX UIO_MAXIOV
#endif
#else
#include <limits.h>
#endif

#include "common.h"
#include "frame.h"

#define INITIAL_QUEUE_SZ 1024

typedef enum {
    NITRO_QUEUE_STATE_EMPTY,
    NITRO_QUEUE_STATE_CONTENTS,
    NITRO_QUEUE_STATE_FULL
} NITRO_QUEUE_STATE;

typedef void (*nitro_queue_state_changed)(NITRO_QUEUE_STATE st, NITRO_QUEUE_STATE last, void *baton);

/* Queue of Frames */
typedef struct nitro_queue_t {
    nitro_frame_t **q;
    nitro_frame_t **head;
    nitro_frame_t **tail;
    nitro_frame_t **end;
    int size;
    int count;
    int capacity;
    pthread_mutex_t lock;
    pthread_cond_t trigger;

    nitro_queue_state_changed state_callback;
    void *baton;

} nitro_queue_t;

nitro_queue_t *nitro_queue_new(int capacity,
    nitro_queue_state_changed queue_cb, void *baton);

nitro_frame_t *nitro_queue_pull(nitro_queue_t *q, int wait);
int nitro_queue_push(nitro_queue_t *q, nitro_frame_t *f,
    int wait);
int nitro_queue_fd_write(nitro_queue_t *q, int fd, 
    nitro_frame_t *partial,
    nitro_frame_t **remain);
typedef nitro_frame_t * (*nitro_queue_encrypt_frame_cb)(nitro_frame_t *, void *); 
int nitro_queue_fd_write_encrypted(nitro_queue_t *q, int fd, 
    nitro_frame_t *partial,
    nitro_frame_t **remain, 
    nitro_queue_encrypt_frame_cb encrypt, void *enc_baton);
void nitro_queue_destroy(nitro_queue_t *q);
inline int nitro_queue_count(nitro_queue_t *q);
typedef nitro_frame_t *(*nitro_queue_frame_generator)(void *baton);
void nitro_queue_move(nitro_queue_t *src, nitro_queue_t *dst);

void nitro_queue_consume(nitro_queue_t *q, 
    nitro_queue_frame_generator gen,
    void *baton);
#endif /* QUEUE_H */
