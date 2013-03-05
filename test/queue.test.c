#include "test.h"

#include "queue.h"
#include "frame.h"

/* 
Test Notes:

1. wrapping is working for push/pull
2. incref decref of frames
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

*/

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

    SUMMARY(0);
    return 1;
}
