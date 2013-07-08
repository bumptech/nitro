/*
 * Nitro
 *
 * queue.c - Queues hold messages on their way in or out of
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
#include "common.h"

#include "err.h"
#include "frame.h"
#include "queue.h"
#include "buffer.h"

extern inline int nitro_queue_count(nitro_queue_t *q);

#define QUEUE_GROWTH_FACTOR 3

#define NITRO_MAX_IOV IOV_MAX
#define QUEUE_FD_BUFFER_GUESS (32 * 1024)
#define QUEUE_FD_BUFFER_PADDING (2 * 1024)

static void nitro_queue_issue_callbacks(nitro_queue_t *q, int old_count);

static void nitro_queue_grow(nitro_queue_t *q, int suggestion) {
    /* Assumed
      1. lock is held
      2. suggestion <= capacity
    */

    suggestion = (q->capacity && suggestion > q->capacity) ? q->capacity : suggestion;
    int size;

    if (!q->size) {
        size = INITIAL_QUEUE_SZ;
    } else {
        size = (q->capacity && (q->size << QUEUE_GROWTH_FACTOR) > q->capacity) ?
               q->capacity : (q->size << QUEUE_GROWTH_FACTOR);
    }

    while (size < suggestion) {
        size <<= QUEUE_GROWTH_FACTOR;
    }

    nitro_frame_t **old_q = q->q;
    nitro_frame_t **old_head = q->head;
    nitro_frame_t **old_end = q->end;
    q->q = realloc(q->q, size * sizeof(nitro_frame_t *));

    q->head = q->q + (old_head - old_q);
    q->tail = q->head + q->count;
    q->size = size;
    q->end = q->q + q->size;

    /* copy the wrap-around */
    int got = (old_end - old_head);

    /* note, can be negative */
    int extra = q->count - got;

    int i;

    for (i = 0; i < extra; i++) {
        q->head[got + i] = q->q[i];
    }
}

nitro_queue_t *nitro_queue_new(int capacity,
                               nitro_queue_state_changed queue_cb, void *baton) {
    nitro_queue_t *q;
    ZALLOC(q);
    q->capacity = capacity;
    q->state_callback = queue_cb;
    q->baton = baton;
    q->send_target = QUEUE_FD_BUFFER_GUESS;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->trigger, NULL);
    nitro_queue_grow(q, 0);

    return q;
}

nitro_frame_t *nitro_queue_pull(nitro_queue_t *q,
                                int wait) {
    nitro_frame_t *ptr = NULL;
    pthread_mutex_lock(&q->lock);

    while (q->count == 0) {
        if (!wait) {
            pthread_mutex_unlock(&q->lock);
            nitro_set_error(NITRO_ERR_EAGAIN);
            return NULL;
        }

        pthread_cond_wait(&q->trigger, &q->lock);
    }

    ptr = *q->head;
    q->head++;
    q->count--;

    /* Wrap? */
    if (q->head == q->end) {
        q->head = q->q;
    }

    nitro_queue_issue_callbacks(q, q->count + 1);

    if (q->capacity && q->count == (q->capacity - 1)) {
        pthread_cond_broadcast(&q->trigger);
    }

    pthread_mutex_unlock(&q->lock);
    return ptr;
}

int nitro_queue_push(nitro_queue_t *q,
                     nitro_frame_t *f, int wait) {
    pthread_mutex_lock(&q->lock);

    while (q->capacity && q->count == q->capacity) {
        if (!wait) {
            pthread_mutex_unlock(&q->lock);
            return nitro_set_error(NITRO_ERR_EAGAIN);
        }

        pthread_cond_wait(&q->trigger, &q->lock);
    }

    if (q->count == q->size) {
        nitro_queue_grow(q, 0);
    }

    /* tail marks to the next empty spot */
    *q->tail = f;
    q->tail++;
    q->count++;

    if (q->tail == q->end) {
        q->tail = q->q;
    }

    nitro_queue_issue_callbacks(q, q->count - 1);

    if (q->count == 1) {
        pthread_cond_broadcast(&q->trigger);
    }

    pthread_mutex_unlock(&q->lock);

    return 0;
}

#define IOV_TOTAL(i) ((i[0].iov_len) + (i[1].iov_len) + (i[2].iov_len) + (i[3].iov_len))

/* "internal" functions, mass population and eviction */
int nitro_queue_fd_write(nitro_queue_t *q, int fd,
                         nitro_frame_t *partial,
                         nitro_frame_t **remain,
                         int *frames_written
                        ) {
    /* Does gather IO to avoid copying buffers around */
    pthread_mutex_lock(&q->lock);
    int actual_iovs = 0;
    int accum_bytes = 0;
    int ret = 0;
    int fwritten = 0;
    nitro_frame_t **iter = q->head;
    struct iovec vectors[NITRO_MAX_IOV];

    if (partial) {
        int num;
        struct iovec *f_vs = nitro_frame_iovs(partial, &num);
        memcpy(&(vectors[0]), f_vs, num * sizeof(struct iovec));
        accum_bytes += IOV_TOTAL(f_vs);
        actual_iovs += num;
    }

    int old_count = q->count;
    int temp_count = old_count;
    int byte_target = q->send_target + QUEUE_FD_BUFFER_PADDING;;

    while (accum_bytes < byte_target && actual_iovs < (NITRO_MAX_IOV - 5) && temp_count) {
        int num;
        struct iovec *f_vs = nitro_frame_iovs(*iter, &num);
        memcpy(&(vectors[actual_iovs]), f_vs, num * sizeof(struct iovec));
        accum_bytes += IOV_TOTAL(f_vs);
        actual_iovs += num;
        ++iter;
        --temp_count;

        if (iter == q->end) {
            iter = q->q;
        }
    }

    if (!accum_bytes) {
        goto out;
    }

    int actual_bytes = writev(fd, (const struct iovec *)vectors, actual_iovs);

    /* On error, we don't move the queue pointers at all.
       We'll let the caller sort out the errno. */
    if (actual_bytes == -1) {
        nitro_set_error(NITRO_ERR_ERRNO);
        ret = -1;
        goto out;
    }

    ret = actual_bytes;

    /* Sweep up *wholly* sent things by destroying the frames and
       advancing the queue; if a frame is left partially sent
       at the end, update its iovectors to represent the fractional
       state and return it as a "remainder" (but still pop it off
       this queue) */
    int i = 0, r = 0, done = 0;
    *remain = NULL;

    if (partial) {
        i = 0;

        do {
            r = nitro_frame_iovs_advance(partial, partial->iovs, i++, actual_bytes, &done);
            actual_bytes -= r;
        } while (actual_bytes && !done);

        if (done) {
            nitro_frame_destroy(partial);
            ++fwritten;
        } else {
            assert(!actual_bytes);
            *remain = partial;
        }
    }

    while (actual_bytes) {
        nitro_frame_t *fr = *q->head;
        struct iovec scratch[4];
        memcpy(scratch, fr->iovs, sizeof(scratch));
        i = 0;

        do {
            r = nitro_frame_iovs_advance(fr, scratch, i++, actual_bytes, &done);
            actual_bytes -= r;
        } while (actual_bytes && !done);

        if (done) {
            nitro_frame_destroy(fr);
            ++fwritten;
        } else {
            assert(!actual_bytes);
            *remain = nitro_frame_copy_partial(fr, scratch);
        }

        q->head++;

        if (q->head == q->end) {
            q->head = q->q;
        }

        q->count--;
    }

    nitro_queue_issue_callbacks(q, old_count);

    if (q->capacity && old_count == q->capacity && q->count < old_count) {
        pthread_cond_broadcast(&q->trigger);
    }

    if (q->count && ret > 0) {
        q->send_target = ret >  QUEUE_FD_BUFFER_GUESS ? QUEUE_FD_BUFFER_GUESS : ret;
    }

out:
    pthread_mutex_unlock(&q->lock);
    *frames_written = fwritten;
    return ret;
}

int nitro_queue_fd_write_encrypted(nitro_queue_t *q, int fd,
                                   nitro_frame_t *partial,
                                   nitro_frame_t **remain,
                                   int *frames_written,
                                   nitro_queue_encrypt_frame_cb encrypt, void *enc_baton) {
    *remain = NULL;
    int res = 0;
    int fwritten = 0;

    nitro_frame_t *current = partial;

    if (!current) {
        nitro_frame_t *clear = nitro_queue_pull(q, 0);

        if (clear) {
            current = encrypt(clear, enc_baton);

            if (!current) {
                res = -1;
            }
        }
    }

    while (current) {
        int num;
        struct iovec *f_vs = nitro_frame_iovs(current, &num);
        int bwrite = writev(fd, f_vs, num);

        if (bwrite == -1) {
            if (!OKAY_ERRNO) {
                res = -1;
                nitro_set_error(NITRO_ERR_ERRNO);
                nitro_frame_destroy(current);
            } else {
                *remain = current;
            }

            break;
        }

        // advance
        res += bwrite;
        int i;
        int done = 0;

        for (i = 0; bwrite > 0 && !done; ++i) {
            // modify in-place.. we own the frame privately
            bwrite -= nitro_frame_iovs_advance(
                          current, current->iovs, i, bwrite, &done);
        }

        if (done) {
            nitro_frame_destroy(current);
            ++fwritten;
            nitro_frame_t *clear = nitro_queue_pull(q, 0);

            if (clear) {
                current = encrypt(clear, enc_baton);

                if (!current) {
                    res = -1;
                }
            } else {
                current = NULL;
            }
        }
    }

    *frames_written = fwritten;

    return res;
}

#define MIN3(a, b, c) (\
                       (a < b) ? \
                       (a < c ? a : c) :\
                           (b < c ? b : c))

void nitro_queue_move(nitro_queue_t *src, nitro_queue_t *dst) {
    pthread_mutex_lock(&dst->lock);
    assert(!dst->capacity);

    /* Very quickly, let's take everything from the other queue */
    pthread_mutex_lock(&src->lock);

    int src_count = src->count;
    nitro_frame_t **src_q = NULL;
    nitro_frame_t **src_head = NULL;
    nitro_frame_t **src_end = NULL;

    if (src_count) {
        src_q = src->q;
        src_head = src->head;
        src_end = src->end;

        src->q = src->head = src->tail = src->end = NULL;
        src->count = src->size = 0;

        nitro_queue_issue_callbacks(src, src_count);
    }

    pthread_mutex_unlock(&src->lock);
    /* done! */

    if (!src_count) {
        goto out;
    }

    int final_size = dst->count + src_count;

    if (dst->size < final_size) {
        nitro_queue_grow(dst, final_size);
    }

    assert(dst->size >= final_size);

    int copy_left = src_count;

    nitro_frame_t **f_dst = dst->tail, **f_src = src_head;
    int dst_old_count = dst->count;

    while (1) {
        int copy_now = MIN3(
                           (dst->end - f_dst),
                           (src_end - f_src),
                           copy_left);

        memcpy(
            f_dst,
            f_src,
            copy_now * sizeof(void *));

        f_dst += copy_now;
        f_src += copy_now;
        copy_left -= copy_now;

        if (f_dst == dst->end) {
            f_dst = dst->q;
        }

        if (f_src == src_end) {
            f_src = src_q;
        }

        if (!copy_left) {
            break;
        }
    }

    dst->tail = f_dst;
    dst->count += src_count;

    nitro_queue_issue_callbacks(dst, dst_old_count);
    /* we now own this, so let's dealloc it */
    free(src_q);
out:
    pthread_mutex_unlock(&dst->lock);
}

void nitro_queue_destroy(nitro_queue_t *q) {
    while (q->head != q->tail) {
        nitro_frame_destroy(*q->head);
        q->head++;

        if (q->head == q->end) {
            q->head = q->q;
        }
    }

    free(q->q);
    free(q);
}

static void nitro_queue_issue_callbacks(nitro_queue_t *q,
                                        int old_count) {
    if (!q->state_callback) {
        return;
    }

    int old_state = (old_count == 0 ? NITRO_QUEUE_STATE_EMPTY :
                     ((q->capacity && old_count == q->capacity) ? NITRO_QUEUE_STATE_FULL :
                      NITRO_QUEUE_STATE_CONTENTS));

    /* 1. EMPTY to FULL|CONTENTS */
    if (old_count == 0 && q->count) {
        q->state_callback(
            (!q->capacity || q->count < q->capacity) ?
            NITRO_QUEUE_STATE_CONTENTS :
            NITRO_QUEUE_STATE_FULL, old_state, q->baton);
    }

    /* 2. FULL|CONTENTS to EMPTY */
    else if (old_count > 0 && !q->count) {
        q->state_callback(NITRO_QUEUE_STATE_EMPTY, old_state, q->baton);
    }

    else if (q->capacity) {
        /* 3. FULL to CONTENTS */
        if (old_count == q->capacity && q->count < q->capacity) {
            q->state_callback(NITRO_QUEUE_STATE_CONTENTS, old_state, q->baton);
        }
        /* 4. CONTENTS to FULL */
        else if (old_count < q->capacity && q->count == q->capacity) {
            q->state_callback(NITRO_QUEUE_STATE_FULL, old_state, q->baton);
        }
    }
}

void nitro_queue_consume(nitro_queue_t *q,
                         nitro_queue_frame_generator gen,
                         void *baton) {
    pthread_mutex_lock(&q->lock);

    int old_count = q->count;

    while (!q->capacity || q->count < q->capacity) {
        nitro_frame_t *fr = gen(baton);

        if (!fr) {
            break;
        }

        if (q->count == q->size) {
            nitro_queue_grow(q, 0);
        }

        *q->tail = fr;
        q->tail++;
        q->count++;

        if (q->tail == q->end) {
            q->tail = q->q;
        }
    }

    nitro_queue_issue_callbacks(q, old_count);

    if (old_count == 0 && q->count > 0) {
        pthread_cond_broadcast(&q->trigger);
    }

    pthread_mutex_unlock(&q->lock);
}
