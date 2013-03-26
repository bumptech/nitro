/*
 * Nitro
 *
 * async.c - Commands to be run on the libev thread.
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

#include "async.h"
#include "runtime.h"
#include "Stcp.h"
#include "Sinproc.h"

nitro_async_t *nitro_async_new(int type) {
    /* eventually.. pool? */
    nitro_async_t *a;
    ZALLOC(a);
    a->type = type;
    return a;
}

static void nitro_async_destroy(nitro_async_t *a) {
    /* eventually.. pool? */
    free(a);
}

void nitro_async_schedule(nitro_async_t *a) {
    volatile nitro_async_t *trial = NULL;
    while (1) {
        int swapped = atomic_compare_exchange_strong(
            &the_runtime->async_stack,
            (nitro_async_t **)&trial, a);
        if (swapped) {
            a->next = (nitro_async_t *)trial;
            break;
        }
    }
    ev_async_send(the_runtime->the_loop,
        &the_runtime->thread_wake);
}

static void nitro_async_handle(nitro_async_t *a) {
    switch (a->type) {
    case NITRO_ASYNC_ENABLE_WRITES:
        SOCKET_CALL(a->u.bind_listen.socket, enable_writes);
        break;
    case NITRO_ASYNC_ENABLE_READS:
        SOCKET_CALL(a->u.bind_listen.socket, enable_reads);
        break;
    case NITRO_ASYNC_DIE:
        ev_break(the_runtime->the_loop, EVBREAK_ALL);
        break;
    case NITRO_ASYNC_BIND_LISTEN:
        SOCKET_CALL(a->u.bind_listen.socket, bind_listen);
        break;
    case NITRO_ASYNC_CONNECT:
        SOCKET_CALL(a->u.connect.socket, start_connect);
        break;
    case NITRO_ASYNC_CLOSE:
        SOCKET_CALL(a->u.close.socket, shutdown);
        break;
    }
}

void nitro_async_cb(struct ev_loop *loop, ev_async *a, int revents) {
    volatile nitro_async_t *head = NULL, *next = NULL;
    while (1) {
        int swapped = atomic_compare_exchange_strong(
            &the_runtime->async_stack,
            (nitro_async_t **)&head, NULL);
        if (swapped)
            break;
    }

    for (; head; head = next) {
        next = head->next;
        nitro_async_handle((nitro_async_t *)head);
        nitro_async_destroy((nitro_async_t *)head);
    }
}
