#include "test.h"

#include "queue.h"
#include "frame.h"
#include "util.h"

#include <fcntl.h>
#include <sys/stat.h>

/*
Test Notes:

12. fd write to /tmp file

*/

#define CONSUME_COUNT (100000)
typedef struct my_frame_data {
    int done;
} my_frame_data;

void my_free(void *p, void *data) {
    my_frame_data *md = (my_frame_data *)data;
    md->done = 1;
}

typedef struct test_state {
    int got_state_change;
    NITRO_QUEUE_STATE state_was;
} test_state;

void test_state_callback(NITRO_QUEUE_STATE st,
    NITRO_QUEUE_STATE last,
    void *baton) {
    test_state *tstate = (test_state *)baton;
    tstate->got_state_change = 1;
    tstate->state_was = st;
}

nitro_frame_t *gframe;

void *take_item(void *p) {
    nitro_queue_t *q = (nitro_queue_t *)p;
    sleep(1);
    nitro_frame_t *fr = nitro_queue_pull(q);

    assert(*((int *)nitro_frame_data(fr)) == 0);
    nitro_frame_destroy(fr);
    return NULL;
}

void *put_item(void *p) {
    nitro_queue_t *q = (nitro_queue_t *)p;
    sleep(1);
    int i = 1337;
    nitro_frame_t *fr = nitro_frame_new_copy(
    (void *)&i, sizeof(int));

    nitro_queue_push(q, fr);

    return NULL;
}

typedef struct pipe_pass {
    char *out;
    int pread;
} pipe_pass;

void *pipe_consume(void *p) {
    pipe_pass *pp = (pipe_pass *)p;

    char *ptr = pp->out;

    int bytes = 0;
    int r = 0;

    int rfd = open("/dev/urandom", O_RDONLY);
    do {
        uint8_t ms;
        struct timespec ts;
        read(rfd, (void *)&ms, sizeof(uint8_t));
        ts.tv_sec = 0;
        ts.tv_nsec = 10000 * ms;
        // introduce some latency to test various boundary conditions
        nanosleep(&ts, NULL);
        do {
            r = read(pp->pread, ptr, 1000);
        } while (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
        if (r > 0) {
            bytes += r;
            ptr += r;
        }
    } while (r > 0 && bytes != 550000);
    assert(bytes == 550000);

    close(rfd);
    return NULL;
}

inline nitro_frame_t *make_frames(void *p) {
    int *i = (int *)p;
    if (*i == CONSUME_COUNT)
        return NULL;
    ++(*i);

    return nitro_frame_copy(gframe);
}

int main(int argc, char **argv) {
    test_state tstate = {0};
    nitro_queue_t *q = nitro_queue_new(0, 
    test_state_callback, &tstate);
    TEST("new non-null", q);

    nitro_frame_t *hello = nitro_frame_new_copy(
        "hello", 6);

    nitro_queue_push(q, hello);
    TEST("queue went contents", 
        tstate.got_state_change 
        && tstate.state_was == NITRO_QUEUE_STATE_CONTENTS);
    tstate.got_state_change = 0;

    nitro_frame_t *back = nitro_queue_pull(q);
    TEST("got frame back", 
        back == hello);
    TEST("queue went empty", 
        tstate.got_state_change 
        && tstate.state_was == NITRO_QUEUE_STATE_EMPTY);
    tstate.got_state_change = 0;


    /* Ordering */
    nitro_frame_t *world = nitro_frame_new_copy(
        "world", 6);
    nitro_queue_push(q, hello);
    nitro_queue_push(q, world);

    back = nitro_queue_pull(q);
    TEST("got hello (ordering)", 
        back == hello);
    back = nitro_queue_pull(q);
    TEST("got world (ordering)", 
        back == world);

    nitro_queue_destroy(q);
    q = nitro_queue_new(0, 
    test_state_callback, &tstate);

    /* Wrapping */
    TEST("pre-wrapping empty queue", nitro_queue_count(q) == 0);
    TEST("pre-wrapping internal assumption", q->size == 1024);

    int i;
    for (i=0; i < 1023; i++) {
        nitro_frame_t *hcopy = nitro_frame_copy(hello);
        nitro_queue_push(q, hcopy);
        back = nitro_queue_pull(q);
        assert(back == hcopy);
        nitro_frame_destroy(back);
    }


    nitro_queue_push(q, hello);
    nitro_queue_push(q, world);

    TEST("wrapping internals, wrap occurred",
    q->tail < q->head);

    back = nitro_queue_pull(q);
    TEST("got hello (wrapping)", 
        back == hello);
    back = nitro_queue_pull(q);
    TEST("got world (wrapping)", 
        back == world);

    TEST("wrapping internals, wrap back to even",
    q->tail == q->head);

    /* Destroy with frames in */

    my_frame_data dt = {0};

    nitro_frame_destroy(world);
    world = nitro_frame_new(
    "hello", 6, my_free, &dt);

    nitro_queue_push(q, world);

    TEST("delete outstanding.. not deleted",
    dt.done == 0);

    nitro_queue_destroy(q);

    TEST("post-queue-delete.. frame free",
    dt.done);

    nitro_frame_destroy(hello);

    /* Queue count */

    q = nitro_queue_new(0,
    test_state_callback, &tstate);

    hello = nitro_frame_new_copy("hello", 6);

    for (i=0; i < 313; i++) {
        back = nitro_frame_copy(hello);
        nitro_queue_push(q, back);
    }

    nitro_frame_destroy(hello);


    TEST("count basic", nitro_queue_count(q) == 313);

    /* resize */
    nitro_queue_destroy(q);
    q = nitro_queue_new(0,
    test_state_callback, &tstate);
    assert(q->size == 1024);

    for (i=0; i < 1050; i++) {
        nitro_queue_push(q,
            nitro_frame_new_copy((void*)&i, sizeof(int)));
    }

    TEST("was resized", q->size > 1024);

    for (i=0; i < 1050; i++) {
        back = nitro_queue_pull(q);
        if ( *((int*)nitro_frame_data(back)) != i)
            break;
        nitro_frame_destroy(back);
    }

    TEST("order after resize", i == 1050);

    nitro_queue_destroy(q);

    /* Test queue moving */

    /* move 0 -> 0 */
    q = nitro_queue_new(0,
    test_state_callback, &tstate);
    nitro_queue_t *dst = nitro_queue_new(0,
    test_state_callback, &tstate);

    nitro_queue_move(
    q, dst, 0);
    TEST("move(0->0) src empty",
    nitro_queue_count(q) == 0);
    TEST("move(0->0) dest empty",
    nitro_queue_count(dst) == 0);

    /* move N -> 0 */

    for (i=0; i < 5; i++) {
        nitro_queue_push(q,
            nitro_frame_new_copy(
            (void *)&i, sizeof(int)));
    }

    TEST("load 5 into src",
    nitro_queue_count(q) == 5);
    nitro_queue_move(q, dst, 5);
    TEST("move(0->0) src empty",
    nitro_queue_count(q) == 0);
    TEST("move(0->0) dest 5",
    nitro_queue_count(dst) == 5);

    for (i=0; i < 5; i++) {
        back = nitro_queue_pull(dst);
        if ( *((int*)nitro_frame_data(back)) != i)
            break;
        nitro_frame_destroy(back);
    }

    TEST("order after move", i == 5);

    nitro_queue_destroy(q);
    nitro_queue_destroy(dst);

    /* move M:N -> N */
    q = nitro_queue_new(0,
    test_state_callback, &tstate);
    dst = nitro_queue_new(0,
    test_state_callback, &tstate);

    /* zero through 5 into src */
    for (i=0; i < 5; i++) {
        nitro_queue_push(q,
            nitro_frame_new_copy(
            (void *)&i, sizeof(int)));
    }

    /* 5 through 10 into dst */
    for (; i < 10; i++) {
        nitro_queue_push(dst,
            nitro_frame_new_copy(
            (void *)&i, sizeof(int)));
    }
    TEST("load 5 into src",
    nitro_queue_count(q) == 5);
    TEST("load 5 into dst",
    nitro_queue_count(dst) == 5);
    nitro_queue_move(q, dst, 3);
    TEST("(3/5->5) 2 src",
    nitro_queue_count(q) == 2);
    TEST("(3/5->5) 8 dst",
    nitro_queue_count(dst) == 8);

    for (i=3; i < 5; i++) {
        back = nitro_queue_pull(q);
        if ( *((int*)nitro_frame_data(back)) != i)
            break;
        nitro_frame_destroy(back);
    }
    TEST("(3/5->5) ordering src", i == 5);
    for (i=5; i < 10; i++) {
        back = nitro_queue_pull(dst);
        if ( *((int*)nitro_frame_data(back)) != i)
            break;
        nitro_frame_destroy(back);
    }
    TEST("(3/5->5) ordering dst (1)", i == 10);
    for (i=0; i < 3; i++) {
        back = nitro_queue_pull(dst);
        if ( *((int*)nitro_frame_data(back)) != i)
            break;
        nitro_frame_destroy(back);
    }
    TEST("(3/5->5) ordering dst (2)", i == 3);

    nitro_queue_destroy(q);
    nitro_queue_destroy(dst);

    /* move with fractional wraparound, ugly copies */
    q = nitro_queue_new(0,
    test_state_callback, &tstate);
    dst = nitro_queue_new(0,
    test_state_callback, &tstate);

    for (i=0; i < 300; i++) {
        nitro_queue_push(q,
            nitro_frame_new_copy(
            (void *)&i, sizeof(int)));
    }
    for (i=0; i < 300; i++) {
        nitro_frame_destroy(nitro_queue_pull(q));
    }
    for (i=0; i < 700; i++) {
        nitro_queue_push(q,
            nitro_frame_new_copy(
            (void *)&i, sizeof(int)));
    }

    for (i=0; i < 324; i++) {
        nitro_queue_push(dst,
            nitro_frame_new_copy(
            (void *)&i, sizeof(int)));
    }
    for (i=0; i < 324; i++) {
        nitro_frame_destroy(nitro_queue_pull(dst));
    }
    for (i=0; i < 324; i++) {
        nitro_queue_push(dst,
            nitro_frame_new_copy(
            (void *)&i, sizeof(int)));
    }

    TEST("(ugly move) count 700 src",
    nitro_queue_count(q) == 700);
    TEST("(ugly move) count 324 dst",
    nitro_queue_count(dst) == 324);

    nitro_queue_move(q, dst, 700);
    TEST("(ugly move post) count 0 src",
    nitro_queue_count(q) == 0);
    TEST("(ugly move post) count 1024 dst",
    nitro_queue_count(dst) == 1024);
    assert(q->size == 1024); /* no resizing! */
    TEST("(ugly move post) head == tail",
    dst->head == dst->tail);

    for (i=0; i < 324; i++) {
        back = nitro_queue_pull(dst);
        if ( *((int*)nitro_frame_data(back)) != i)
            break;
        nitro_frame_destroy(back);
    }
    TEST("(ugly move post) first 324 still there",
    i == 324);
    for (i=0; i < 700; i++) {
        back = nitro_queue_pull(dst);
        if ( *((int*)nitro_frame_data(back)) != i)
            break;
        nitro_frame_destroy(back);
    }
    TEST("(ugly move post) appended 700 there",
    i == 700);
    TEST("(ugly move post) dst emptied",
    nitro_queue_count(dst) == 0);

    nitro_queue_destroy(q);
    nitro_queue_destroy(dst);

    /* move with resize */
    q = nitro_queue_new(0,
    test_state_callback, &tstate);
    dst = nitro_queue_new(0,
    test_state_callback, &tstate);

    for (i=0; i < 1000; i++) {
        nitro_queue_push(q,
            nitro_frame_new_copy(
            (void *)&i, sizeof(int)));
    }

    for (i=0; i < 324; i++) {
        nitro_queue_push(dst,
            nitro_frame_new_copy(
            (void *)&i, sizeof(int)));
    }

    TEST("(resize move) count 1000 src",
    nitro_queue_count(q) == 1000);
    TEST("(resize move) count 324 dst",
    nitro_queue_count(dst) == 324);
    assert(dst->size == 1024);
    nitro_queue_move(q, dst, nitro_queue_count(q));
    TEST("(resize post-move) size dst is sum",
    nitro_queue_count(dst) == 1324);
    assert(dst->size > 1024);

    for (i=0; i < 324; i++) {
        back = nitro_queue_pull(dst);
        if ( *((int*)nitro_frame_data(back)) != i)
            break;
        nitro_frame_destroy(back);
    }
    TEST("(resize post-move) first 324 still there",
    i == 324);
    for (i=0; i < 1000; i++) {
        back = nitro_queue_pull(dst);
        if ( *((int*)nitro_frame_data(back)) != i)
            break;
        nitro_frame_destroy(back);
    }
    TEST("(resize post-move) appended 1000 there",
    i == 1000);
    TEST("(resize post-move) dst cleared",
    nitro_queue_count(dst) == 0);

    nitro_queue_destroy(q);
    nitro_queue_destroy(dst);

    /* Capacity tests */

    q = nitro_queue_new(5,
    test_state_callback, &tstate);

    for (i=0; i < 5; i++) {
        nitro_queue_push(q,
            nitro_frame_new_copy(
            (void *)&i, sizeof(int)));
    }

    double d1 = now_double();
    pthread_t t1;
    pthread_create(&t1, NULL, take_item, (void*)q);
    nitro_queue_push(q,
        nitro_frame_new_copy(
        (void *)&i, sizeof(int)));
    double d2 = now_double();
    TEST("(capacity) got blocked on push",
    (d2 - d1 > 0.7) && (d2 - d1 < 1.5));

    for (i=1; i < 6; i++) {
        back = nitro_queue_pull(q);
        if ( *((int*)nitro_frame_data(back)) != i)
            break;
        nitro_frame_destroy(back);
    }
    TEST("(capacity) have rest",
    i == 6);
    TEST("(capacity) emptied",
    nitro_queue_count(q) == 0);
    pthread_t t2;

    d1 = now_double();
    pthread_create(&t2, NULL, put_item, (void*)q);
    back = nitro_queue_pull(q);
    d2 = now_double();
    TEST("(capacity) got blocked on empty pull",
    (d2 - d1 > 0.7) && (d2 - d1 < 1.5));
    TEST("(capacity) empty pull got right frame",
    *((int*)nitro_frame_data(back)) == 1337);
    nitro_frame_destroy(back);

    nitro_queue_destroy(q);

    void *unused;
    pthread_join(t1, &unused);
    pthread_join(t2, &unused);


    /* Consume */
    q = nitro_queue_new(0,
    test_state_callback, &tstate);

    gframe = nitro_frame_new_copy("hello", 6);

    /* Traditional test */
    d1 = now_double();
    for (i=0; i < CONSUME_COUNT; i++) {
        nitro_queue_push(q,
            nitro_frame_copy(gframe));
    }
    d2 = now_double();

    char buf[50];
    snprintf(buf, 50, "(consume-manual) in %.4f",
    d2 - d1);
    TEST(buf, nitro_queue_count(q) == CONSUME_COUNT);
    nitro_queue_destroy(q);

    // vs consume
    i = 0;
    q = nitro_queue_new(0,
    test_state_callback, &tstate);
    d1 = now_double();
    nitro_queue_consume(q,
        make_frames, (void *)&i);
    d2 = now_double();
    snprintf(buf, 50, "(consume-stream) in %.4f",
    d2 - d1);
    TEST(buf, nitro_queue_count(q) == CONSUME_COUNT);
    nitro_queue_destroy(q);

    nitro_frame_destroy(gframe);

    /* Write to file */

    q = nitro_queue_new(0,
    test_state_callback, &tstate);
    for (i=0; i < 50000; i++) {
        nitro_queue_push(q,
            nitro_frame_new_copy(
                "dog", 3));
    }

    int ps[2];
    int r = pipe(ps);
    assert(!r);

    int pread = ps[0];
    int pwrite = ps[1];

    int flag = 1;
    r = ioctl(pread, FIONBIO, &flag);
    assert(r == 0);
    r = ioctl(pwrite, FIONBIO, &flag);
    assert(r == 0);

    int bytes = 0;
    int total = 0;
    nitro_frame_t *remain = NULL;
    pthread_t reader;
    char out[550000];
    pipe_pass pp = {out, pread};
    pthread_create(&reader, NULL, pipe_consume, &pp);
    do {
        errno = 0;
        if (bytes > 0)
            total += bytes;
        bytes = nitro_queue_fd_write(
            q, pwrite, remain, &remain);
    } while (bytes > 0 || errno == EAGAIN || errno == EWOULDBLOCK);
    if (bytes < 0) {
        perror("write to pipe:");
        assert(bytes == 0);
    }

    TEST("(fd write) correct write byte count",
    total == 550000);

    void *t_ret;
    /* wait for reader to finish */
    pthread_join(reader, &t_ret);

    char *ptr = out;
    nitro_protocol_header target_head = {
        1,
        0,
        0,
        3};

    for (i=0; i < 50000; i++) {
        if (memcmp(ptr, &target_head, sizeof(target_head)) ||
            memcmp(ptr + sizeof(target_head), "dog", 3)) {
            break;
        }
        ptr += 11;
    }

    TEST("(fd write) read data was correct",
    ptr == out + 550000);

    close(pwrite);
    close(pread);


    nitro_queue_destroy(q);

    SUMMARY(0);
    return 1;
}
