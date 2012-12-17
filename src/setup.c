#include "nitro.h"
#include "nitro-private.h"

nitro_runtime *the_runtime;

static void *actual_run(void *unused) {
    uv_run(the_runtime->the_loop);

    return NULL;
}

int nitro_start() {
    int r;
    if (the_runtime)
        return nitro_set_error(NITRO_ERR_ALREADY_RUNNING);

    ZALLOC(the_runtime);

    the_runtime->the_loop = uv_loop_new();
    pthread_mutex_init(&the_runtime->l_tcp_pair, NULL);
    uv_timer_init(the_runtime->the_loop, &the_runtime->tcp_timer);
    r = uv_timer_start(&the_runtime->tcp_timer, tcp_poll, 500, 500);
    assert(!r);
    r = uv_async_init(the_runtime->the_loop, &the_runtime->tcp_trigger, tcp_poll_cb);
    assert(!r);

    pthread_create(&the_runtime->the_thread, NULL, actual_run, NULL);
    return 0;
}
