/*
 * Nitro
 *
 * runtime.h - Implementation of start/stop and environment initialization
 *             for a nitro application
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
#ifndef NITRO_RUNTIME_H
#define NITRO_RUNTIME_H
#include "common.h"
#include "async.h"

typedef struct nitro_runtime {
    struct ev_loop *the_loop;
    pthread_t the_thread;

    nitro_async_t *async_queue;
    pthread_mutex_t l_async;

    pthread_mutex_t l_inproc;
    nitro_inproc_socket_t *inprocs;

    pthread_mutex_t l_socks;
    nitro_socket_t *socks;

    ev_async thread_wake;

    int random_fd;

    int num_sock;
} nitro_runtime;

extern nitro_runtime *the_runtime;

int nitro_runtime_start();
int nitro_runtime_stop();

#define NITRO_THREAD_CHECK {\
        assert(pthread_equal(pthread_self(), the_runtime->the_thread));\
    }

#endif /* RUNTIME_H */
