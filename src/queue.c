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
#include "queue.h"

static void nitro_queue_issue_callbacks(nitro_queue_t *q, int old_count);

static void nitro_queue_grow(nitro_queue_t *q, int suggestion) {
    /* Assumed 
      1. lock is held 
      2. suggestion <= capacity
    */

    int size;
    do {
        if (!q->size) {
            size = INITIAL_QUEUE_SZ;
        } else {
            size = (q->capacity && (q->size << 3) > q->capacity) ?
                q->capacity : (q->size << 3);
        }
    } while (size < suggestion);

    nitro_frame_t **old_q = q->q;
    nitro_frame_t **old_head = q->head;
    q->q = realloc(q->q, size * sizeof(nitro_frame_t *));

    q->head = q->q + (old_head - old_q);
    q->tail = q->head + q->count;
    q->size = size;
    q->end = q->q + q->size;

    /* copy the wrap-around */
    int got = (q->end - q->head);

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

nitro_frame_t *nitro_queue_pull(nitro_queue_t *q) {
    nitro_frame_t *ptr = NULL;
    pthread_mutex_lock(&q->lock);

    while (q->count == 0) {
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
        pthread_cond_signal(&q->trigger);
    }
    pthread_mutex_unlock(&q->lock);
    return ptr;
}

void nitro_queue_push(nitro_queue_t *q, nitro_frame_t *f) {
    pthread_mutex_lock(&q->lock);
    while (q->capacity && q->count == q->capacity) {
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
        pthread_cond_signal(&q->trigger);
    }

    pthread_mutex_unlock(&q->lock);
}

/* "internal" functions, mass population and eviction */
int nitro_queue_fd_write(nitro_queue_t *q, int fd) {
    /* Does gather IO to avoid copying buffers around */
    pthread_mutex_lock(&q->lock);
    int actual_iovs = 0;
    int accum_bytes = 0;
    int ret = 0;
    nitro_frame_t **iter = q->head;
    struct iovec vectors[IOV_MAX];

    int old_count = q->count;
    while (accum_bytes < (32 * 1024) && actual_iovs < IOV_MAX 
        && iter != q->tail) {
        int num;
        struct iovec *f_vs = nitro_frame_iovs(*iter, &num);
        assert(num == 2);
        vectors[actual_iovs++] = f_vs[0];
        accum_bytes += f_vs[0].iov_len;
        vectors[actual_iovs++] = f_vs[1];
        accum_bytes += f_vs[1].iov_len;
        iter++;
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
       state */
    int i;
    for (i=0; i < actual_iovs; i++) {
        nitro_frame_t *fr = *q->head;
        if (vectors[i].iov_len > actual_bytes) {
            nitro_frame_iovs_advance(fr, 0, actual_bytes);
            break;
        }
        actual_bytes -= vectors[i].iov_len;
        i++;

        if (vectors[i].iov_len > actual_bytes) {
            nitro_frame_iovs_advance(fr, 1, actual_bytes);
            break;
        }
        actual_bytes -= vectors[i].iov_len;
        nitro_frame_destroy(fr);
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
        
void nitro_queue_move(nitro_queue_t *src, nitro_queue_t *dst, int max) {
    pthread_mutex_lock(&src->lock);
    pthread_mutex_lock(&dst->lock);

    int available = dst->capacity ? (dst->capacity - dst->count) : max;
    int to_copy = max < available ? max : available;
    int final_size = dst->count + to_copy;

    if (dst->size < final_size) {
        nitro_queue_grow(dst, final_size);
    }
    assert(dst->size >= final_size);

    int copy_left = to_copy;

    nitro_frame_t **f_dst = dst->tail, **f_src = src->head;
    int dst_old_count = dst->count;
    int src_old_count = src->count;

    while (1) {
        int copy_now = MIN3(
            (dst->end - f_dst),
            (src->end - f_src),
            copy_left);

        memcpy(
            f_dst,
            f_src,
            copy_now * sizeof(void *));

        f_dst += copy_now;
        f_src += copy_now;
        copy_left -= copy_now;

        if (!copy_left) {
            break;
        }

        if (f_dst == dst->end) {
            f_dst = dst->q;
        }
        if (f_src == src->end) {
            f_src = src->q;
        }
    }

    dst->tail = f_dst;
    dst->count += to_copy;

    nitro_queue_issue_callbacks(dst, dst_old_count);

    src->head = f_src;
    src->count -= to_copy;

    nitro_queue_issue_callbacks(src, src_old_count);

    pthread_mutex_unlock(&dst->lock);
    pthread_mutex_unlock(&src->lock);
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

    /* 1. EMPTY to FULL|CONTENTS */
    if (old_count == 0 && q->count) {
        q->state_callback(
            (!q->capacity || q->count < q->capacity) ?
                NITRO_QUEUE_STATE_CONTENTS :
                NITRO_QUEUE_STATE_FULL, q->baton);
    } 
    
    /* 2. FULL|CONTENTS to EMPTY */
    else if (old_count > 0 && !q->count) {
        q->state_callback(NITRO_QUEUE_STATE_EMPTY, q->baton);
    } 
    
    else if (q->capacity) {
        /* 3. FULL to CONTENTS */
        if (old_count == q->capacity && q->count < q->capacity) {
            q->state_callback(NITRO_QUEUE_STATE_CONTENTS, q->baton);
        }
        /* 4. CONTENTS to FULL */
        else if (old_count < q->capacity && q->count == q->capacity) {
            q->state_callback(NITRO_QUEUE_STATE_FULL, q->baton);
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
            q->tail = q->head;
        }
    }

    nitro_queue_issue_callbacks(q, old_count);

    pthread_mutex_unlock(&q->lock);
}
