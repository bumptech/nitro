/*
 * Nitro
 *
 * inproc.c - inproc sockets are high-performance, thread-safe in-process
 *            queues
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
#include "nitro.h"
#include "nitro-private.h"

// XXX move to the_runtime
static nitro_socket_t *bound_inproc_socks;

void inproc_socket_sub(nitro_socket_t *s, char *key) {
    /* NO-OP */
}

void inproc_pipe_sub(nitro_pipe_t *p, char *key) {
    nitro_socket_t *s = (nitro_socket_t *)p->dest_socket;
    nitro_pipe_t *return_pipe = NULL;
    CDL_FOREACH(s->pipes, return_pipe) {
        if (return_pipe->dest_socket == p->the_socket) {
            // we found the return pipe, let's add the sub key.
            add_pub_filter(s, return_pipe, key);
        }
    }
}

void inproc_write(nitro_pipe_t *p, nitro_frame_t *f) {
    nitro_socket_t *s = (nitro_socket_t *)p->dest_socket;
    nitro_frame_t *fcopy = nitro_frame_copy(f);
    pthread_mutex_lock(&s->l_recv);
    DL_APPEND(s->q_recv, fcopy);
    pthread_cond_signal(&s->c_recv);
    s->count_recv++;
    pthread_mutex_unlock(&s->l_recv);
}

static nitro_pipe_t *new_inproc_pipe(nitro_socket_t *orig_socket, nitro_socket_t *dest_socket) {
    nitro_pipe_t *p = nitro_pipe_new();
    p->the_socket = (void *)orig_socket;
    p->dest_socket = (void *) dest_socket;
    p->do_write = &inproc_write;
    p->do_sub = &inproc_pipe_sub;
    return p;
}

nitro_socket_t *nitro_bind_inproc(char *location) {
    // XXX not thread safe -- global lock on the_runtime
    nitro_socket_t *s = nitro_socket_new();
    s->trans = NITRO_SOCKET_INPROC;
    s->do_sub = inproc_socket_sub;
    nitro_socket_t *result;
    HASH_FIND(hh, bound_inproc_socks, location, strlen(location), result);
    /* XXX YOU SUCK FOR DOUBLE BINDING */
    assert(!result);
    HASH_ADD_KEYPTR(hh, bound_inproc_socks, location, strlen(location), (nitro_socket_t *)s);
    return s;
}

nitro_socket_t *nitro_connect_inproc(char *location) {
    // XXX not thread safe -- need some global lock on the_runtime
    nitro_socket_t *s = nitro_socket_new();
    s->trans = NITRO_SOCKET_INPROC;
    s->do_sub = inproc_socket_sub;
    s->outbound = 1;
    nitro_socket_t *result;
    HASH_FIND(hh, bound_inproc_socks, location, strlen(location), result);
    /* XXX YOU SUCK FOR LOOKING UP SOMETHING WRONG */
    assert(result);

    if (result) {
        nitro_pipe_t *pipe1 = new_inproc_pipe(s, result);
        CDL_PREPEND(s->pipes, pipe1);

        if (!s->next_pipe) {
            s->next_pipe = s->pipes;
        }

        nitro_pipe_t *pipe2 = new_inproc_pipe(result, s);
        CDL_PREPEND(result->pipes, pipe2);

        if (!result->next_pipe) {
            result->next_pipe = result->pipes;
        }
    }

    return s;
}

void nitro_close_inproc(nitro_socket_t *s) {
    nitro_pipe_t *p, *tmp;

    // destroy_pipe will NULL out s->pipes
    // when all are gone
    for (p = s->pipes; s->pipes;) {
        tmp = p->next;
        destroy_pipe(p);

        p = tmp;
    }

    if (!s->outbound) {
        HASH_DEL(bound_inproc_socks, s);
    }

    nitro_socket_destroy(s);
}
