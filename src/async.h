/*
 * Nitro
 *
 * async.h - Commands to be run on the libev thread.
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

#ifndef ASYNC_H
#define ASYNC_H
#include "common.h"

#include "socket.h"

enum {
    NITRO_ASYNC_DIE,
    NITRO_ASYNC_BIND_LISTEN,
    NITRO_ASYNC_TCP_FLUSH
};

typedef struct nitro_async_tcp_flush {
    nitro_socket_t *socket;
} nitro_async_tcp_flush;

typedef struct nitro_async_bind_listen {
    nitro_socket_t *socket;
} nitro_async_bind_listen;

typedef struct nitro_async {
    int type;
    union {
        nitro_async_bind_listen bind_listen;
        nitro_async_tcp_flush tcp_flush;
    } u;
    struct nitro_async *next;
} nitro_async_t;

void nitro_async_cb(struct ev_loop *loop, ev_async *a, int revents);
nitro_async_t *nitro_async_new(int type);
void nitro_async_schedule(nitro_async_t *a);


#endif /* ASYNC_H */
