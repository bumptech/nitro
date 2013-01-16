/*
 * Nitro
 *
 * setup.c - Implementation of start/stop and environment initialization
 *           for a nitro application
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

    if (the_runtime) {
        return nitro_set_error(NITRO_ERR_ALREADY_RUNNING);
    }

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
    if (!the_runtime) {
        return nitro_set_error(NITRO_ERR_NOT_RUNNING);
    }

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
