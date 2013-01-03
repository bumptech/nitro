#include <unistd.h>

#include "nitro.h"
#include "nitro-private.h"

nitro_runtime *the_runtime;

static void *actual_run(void *unused) {
    printf("NITRO start!\n");
    while (the_runtime->run) {
        uv_run_once(the_runtime->the_loop);
    }

    uv_loop_delete(the_runtime->the_loop);

    printf("NITRO done!\n");

    pthread_cond_signal(&the_runtime->dc);
    return NULL;
}

void wake_and_die(uv_async_t *handle, int status) {
    the_runtime->run = 0;
}

int nitro_start() {
    int r;
    if (the_runtime)
        return nitro_set_error(NITRO_ERR_ALREADY_RUNNING);

    ZALLOC(the_runtime);

    the_runtime->run = 1;

    the_runtime->the_loop = uv_loop_new();
    pthread_mutex_init(&the_runtime->l_tcp_pair, NULL);
    uv_timer_init(the_runtime->the_loop, &the_runtime->tcp_timer);
    r = uv_timer_start(&the_runtime->tcp_timer, tcp_poll, 500, 500);
    assert(!r);
    r = uv_async_init(the_runtime->the_loop, &the_runtime->tcp_trigger, tcp_poll_cb);
    assert(!r);
    r = uv_async_init(the_runtime->the_loop, &the_runtime->done_wake, wake_and_die);
    assert(!r);

    pthread_mutex_init(&the_runtime->dm, NULL);
    pthread_cond_init(&the_runtime->dc, NULL);

    pthread_create(&the_runtime->the_thread, NULL, actual_run, NULL);

    return 0;
}

int nitro_stop() {
    if (!the_runtime)
        return nitro_set_error(NITRO_ERR_NOT_RUNNING);

    assert(the_runtime->num_sock == 0);

    uv_timer_stop(&the_runtime->tcp_timer);
    uv_unref((uv_handle_t *)&the_runtime->tcp_trigger);


    pthread_mutex_lock(&the_runtime->dm);
    uv_async_send(&the_runtime->done_wake);
    pthread_cond_wait(&the_runtime->dc, &the_runtime->dm);
    pthread_mutex_unlock(&the_runtime->dm);
    free(the_runtime);

    return 0;
}
