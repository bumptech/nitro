#include "common.h"

#include "err.h"
#include "frame.h"
#include "queue.h"


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
        size = (q->capacity && (q->size << 3) > q->capacity) ?
            q->capacity : (q->size << 3);
    }

    while (size < suggestion) {
        size <<= 3;
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
    for (i=0; i < extra; i++) {
        q->head[got + i] = q->q[i];
    }
}

inline int nitro_queue_count(
    nitro_queue_t *q)
{
    return q->count;
}

nitro_queue_t *nitro_queue_new(int capacity,
    nitro_queue_state_changed queue_cb, void *baton)
{
    nitro_queue_t *q;
    ZALLOC(q);
    q->capacity = capacity;
    q->state_callback = queue_cb;
    q->baton = baton;
    pthread_mutex_init(&q->lock, NULL);
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

/* "internal" functions, mass population and eviction */
int nitro_queue_fd_write(nitro_queue_t *q, int fd, 
    nitro_frame_t *partial,
    nitro_frame_t **remain) {
    /* Does gather IO to avoid copying buffers around */
    pthread_mutex_lock(&q->lock);
    int actual_iovs = 0;
    int accum_bytes = 0;
    int ret = 0;
    nitro_frame_t **iter = q->head;
    struct iovec vectors[IOV_MAX];

    if (partial) {
        int num;
        struct iovec *f_vs = nitro_frame_iovs(partial, &num);
        memcpy(&(vectors[0]), f_vs, num * sizeof(struct iovec));
        accum_bytes += 
            (f_vs[0].iov_len + f_vs[1].iov_len + 
             f_vs[2].iov_len + f_vs[3].iov_len);
        actual_iovs += num;
    }

    int old_count = q->count;
    int temp_count = old_count;
    while (accum_bytes < (32 * 1024) && actual_iovs < (IOV_MAX - 4) && temp_count) {
        int num;
        struct iovec *f_vs = nitro_frame_iovs(*iter, &num);
        memcpy(&(vectors[actual_iovs]), f_vs, num * sizeof(struct iovec));
        accum_bytes += 
            (f_vs[0].iov_len + f_vs[1].iov_len + 
             f_vs[2].iov_len + f_vs[3].iov_len);
        actual_iovs += num;
        ++iter;
        --temp_count;
        if (iter == q->end)
            iter = q->q;
    }

    if (!accum_bytes) {
        goto out;
    }

    int actual_bytes = writev(fd, (const struct iovec *)vectors, actual_iovs);

    /* On error, we don't move the queue pointers at all. 
       We'll let the caller sort out the errno. */
    if (actual_bytes == -1) {
        ret = -1;
        goto out;
    }

    ret = actual_bytes;

    /* Sweep up *wholly* sent things by destroying the frames and
       advancing the queue; if a frame is left partially sent
       at the end, update its iovectors to represent the fractional
       state and return it as a "remainder" (but still pop it off
       this queue) */
    int i = 0, r = 0, done=0;
    *remain = NULL;
    if (partial) {
        i = 0;
        do {
            r = nitro_frame_iovs_advance(partial, i++, actual_bytes, &done);
            actual_bytes -= r;
        } while (actual_bytes && !done);

        if (done) {
            nitro_frame_destroy(partial);
        } else {
            assert(!actual_bytes);
             *remain = partial;
        }
    }
    while (actual_bytes) {
        nitro_frame_t *fr = *q->head;
        i = 0;
        do {
            r = nitro_frame_iovs_advance(fr, i++, actual_bytes, &done);
            actual_bytes -= r;
        } while (actual_bytes && !done);

        if (done) {
            nitro_frame_destroy(fr);
        } else {
            assert(!actual_bytes);
            *remain = fr;
        }

        q->head++;
        if (q->head == q->end)
            q->head = q->q;
        q->count--;
    }

    nitro_queue_issue_callbacks(q, old_count);

out:
    pthread_mutex_unlock(&q->lock);
    return ret;
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
        if (q->head == q->end)
            q->head = q->q;
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
