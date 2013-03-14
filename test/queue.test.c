#include "test.h"

#include "queue.h"
#include "frame.h"

/* 
Test Notes:

3. destroying with frames still in
   the queue
4. check queue count
5. resizing the queue on push
6. queue move
7. queue move w/resize
8. capacity on push (with pull on another thread)?
9. capacity on resize
10. pull on empty (with wakeup in another
    thread?)
11. queue consume
12. fd write to /tmp file

*/

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
    void *baton) {
    test_state *tstate = (test_state *)baton;
    tstate->got_state_change = 1;
    tstate->state_was = st;
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

    world = nitro_frame_new(
    "hello", 6, my_free, &dt);

    nitro_queue_push(q, world);

    TEST("delete outstanding.. not deleted",
    dt.done == 0);

    nitro_queue_destroy(q);

    TEST("post-queue-delete.. frame free",
    dt.done);

    SUMMARY(0);
    return 1;
}
