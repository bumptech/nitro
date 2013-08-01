/*
 * Nitro
 *
 * queue.h - Queues hold messages on their way in or out of
 *           nitro sockets.
 *
 *  -- LICENSE --
 *
 * Copyright 2013 Bump Technologies, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY BUMP TECHNOLOGIES, INC. ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BUMP TECHNOLOGIES, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Bump Technologies, Inc.
 *
 */
#ifndef NITRO_QUEUE_H
#define NITRO_QUEUE_H

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

#define QUEUE_GROWTH_FACTOR 3
#define INITIAL_QUEUE_SZ 1024
#define SHRINK_QUEUE_SZ (INITIAL_QUEUE_SZ << (QUEUE_GROWTH_FACTOR * 3))

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
    int send_target;
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
                         nitro_frame_t **remain,
                         int *frames_written);
typedef nitro_frame_t *(*nitro_queue_encrypt_frame_cb)(nitro_frame_t *, void *);
int nitro_queue_fd_write_encrypted(nitro_queue_t *q, int fd,
                                   nitro_frame_t *partial,
                                   nitro_frame_t **remain,
                                   int *frames_written,
                                   nitro_queue_encrypt_frame_cb encrypt, void *enc_baton);
void nitro_queue_destroy(nitro_queue_t *q);

inline int nitro_queue_count(
    nitro_queue_t *q) {
    return q->count;
}

typedef nitro_frame_t *(*nitro_queue_frame_generator)(void *baton);
void nitro_queue_move(nitro_queue_t *src, nitro_queue_t *dst);

void nitro_queue_consume(nitro_queue_t *q,
                         nitro_queue_frame_generator gen,
                         void *baton);
#endif /* QUEUE_H */
