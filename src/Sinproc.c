/*
 * Nitro
 *
 * tcp.c - TCP sockets are sockets designed to transmit frames between
 *         different machines on a TCP/IP network
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
#include "common.h"
#include "err.h"
#include "runtime.h"
#include "socket.h"
#include "nitro.h"


void Sinproc_socket_recv_queue_stat(NITRO_QUEUE_STATE st, NITRO_QUEUE_STATE last, void *p) {
//    nitro_inproc_socket_t *s = (nitro_inproc_socket_t *)p;

}

/* Sinproc_create_queues
 * ------------------
 *
 * Create the global in queue associated with a socket
 */
void Sinproc_create_queues(nitro_inproc_socket_t *s) {
    s->q_recv = nitro_queue_new(
        s->opt->hwm_in, Sinproc_socket_recv_queue_stat, (void*)s);
}

void Sinproc_socket_destroy(nitro_inproc_socket_t *s) {
    // free everything XXX
    nitro_queue_destroy(s->q_recv);
    nitro_socket_destroy(SOCKET_PARENT(s));
}

static int Sinproc_check_opt(nitro_inproc_socket_t *s) {
    if (s->opt->want_eventfd || s->opt->secure || s->opt->has_remote_ident) {
        return nitro_set_error(NITRO_ERR_BAD_INPROC_OPT);
    }

    return 0;
}

void Sinproc_socket_bound_add_conn(nitro_inproc_socket_t *b,
    nitro_inproc_socket_t *c) {

    pthread_rwlock_wrlock(&b->link_lock);

    CDL_PREPEND(b->links, c);
    if (!atomic_load(&b->current)) {
        atomic_init(&b->current, c);
    }
    c->links = b;
    nitro_counted_buffer_incref(b->bind_counter);
    pthread_rwlock_unlock(&b->link_lock);
}

void Sinproc_socket_bound_rm_conn(nitro_inproc_socket_t *b,
    nitro_inproc_socket_t *c) {

    pthread_rwlock_wrlock(&b->link_lock);
    if (atomic_load(&b->current) == c) {
        atomic_init(&b->current, (c->next == c) ? NULL : c->next);
    }
    CDL_DELETE(b->links, c);
    pthread_rwlock_unlock(&b->link_lock);
}

int Sinproc_socket_connect(nitro_inproc_socket_t *s, char *location) {
    if (Sinproc_check_opt(s) < 0) {
        return -1;
    }

    Sinproc_create_queues(s);

    pthread_mutex_lock(&the_runtime->l_inproc);
    nitro_inproc_socket_t *match;

    HASH_FIND(hh, the_runtime->inprocs, location, strlen(location), match);
    if (!match) {
        pthread_mutex_unlock(&the_runtime->l_inproc);
        return nitro_set_error(NITRO_ERR_INPROC_NOT_BOUND);
    }

    Sinproc_socket_bound_add_conn(match, s);
    pthread_mutex_unlock(&the_runtime->l_inproc);

    return 0;
}

static void free_inproc(void *item, void *unused) {
    nitro_inproc_socket_t *s = (nitro_inproc_socket_t *)item;
    Sinproc_socket_destroy(s);
}

int Sinproc_socket_bind(nitro_inproc_socket_t *s, char *location) {
    if (Sinproc_check_opt(s) < 0) {
        return -1;
    }

    Sinproc_create_queues(s);

    pthread_mutex_lock(&the_runtime->l_inproc);
    nitro_inproc_socket_t *match;

    s->bound = 1;
    s->bind_counter = nitro_counted_buffer_new(s, free_inproc, NULL);
    pthread_rwlock_init(&s->link_lock, NULL); 
    atomic_init(&s->current, NULL);

    HASH_FIND(hh, the_runtime->inprocs, location, strlen(location), match);
    if (match) {
        pthread_mutex_unlock(&the_runtime->l_inproc);
        return nitro_set_error(NITRO_ERR_INPROC_ALREADY_BOUND);
    }

    HASH_ADD_KEYPTR(hh, the_runtime->inprocs,
        s->given_location,
        strlen(s->given_location),
        s);
    HASH_FIND(hh, the_runtime->inprocs, location, strlen(location), match);
    assert(match == s);

    pthread_mutex_unlock(&the_runtime->l_inproc);
    return 0;
}

#define INPROC_NOTIMPL (assert(0))
void Sinproc_socket_bind_listen(nitro_inproc_socket_t *s) {
    INPROC_NOTIMPL;
}

void Sinproc_socket_enable_writes(nitro_inproc_socket_t *s){
    INPROC_NOTIMPL;
}

void Sinproc_socket_enable_reads(nitro_inproc_socket_t *s){
    INPROC_NOTIMPL;
}

void Sinproc_socket_start_connect(nitro_inproc_socket_t *s){
    INPROC_NOTIMPL;
}

void Sinproc_socket_start_shutdown(nitro_inproc_socket_t *s){
    INPROC_NOTIMPL;
}

void Sinproc_socket_close(nitro_inproc_socket_t *s){
    nitro_counted_buffer_t *cleanup = NULL;
    pthread_mutex_lock(&the_runtime->l_inproc);
    s->dead = 1;
    if (s->bound) {
        HASH_DEL(the_runtime->inprocs, s);
        cleanup = s->bind_counter;
    } else {
        assert(s->links);
        Sinproc_socket_bound_rm_conn(s->links, s);
        cleanup = s->links->bind_counter;
        Sinproc_socket_destroy(s);
    }
    pthread_mutex_unlock(&the_runtime->l_inproc);
    nitro_counted_buffer_decref(cleanup);
}

int Sinproc_socket_send(nitro_inproc_socket_t *s, nitro_frame_t **frp, int flags) {
    nitro_frame_t *fr = *frp;
    if (flags & NITRO_REUSE) {
        nitro_frame_incref(fr);
    } else {
        *frp = NULL;
    }

    int r = -1;
    if (s->bound) {
        pthread_rwlock_rdlock(&s->link_lock);
        volatile nitro_inproc_socket_t *try = atomic_load(&s->current);
        if (try == NULL) {
            nitro_set_error(NITRO_ERR_INPROC_NO_CONNECTIONS);
        } else {
            int ok = 0;
            while (!ok) {
                ok = atomic_compare_exchange_strong(
                    &s->current, &try, try->next);
            }
            r = nitro_queue_push(try->q_recv, fr,
               !(flags & NITRO_NOWAIT));
        }
        pthread_rwlock_unlock(&s->link_lock);
    } else {
        assert(s->links);
        if (s->links->dead) {
            nitro_set_error(NITRO_ERR_INPROC_NO_CONNECTIONS);
        } else {
            r = nitro_queue_push(s->links->q_recv, fr,
               !(flags & NITRO_NOWAIT));
        }
    }

    if (r) {
        nitro_frame_destroy(fr);
    }
    return r;
}

nitro_frame_t *Sinproc_socket_recv(nitro_inproc_socket_t *s, int flags) {
    return nitro_queue_pull(s->q_recv, !(flags & NITRO_NOWAIT));
}

int Sinproc_socket_reply(nitro_inproc_socket_t *s, nitro_frame_t *snd, nitro_frame_t **frp, int flags) {
    return -1;
}

int Sinproc_socket_relay_fw(nitro_inproc_socket_t *s, nitro_frame_t *snd, nitro_frame_t **frp, int flags) {
    return -1;
}

int Sinproc_socket_relay_bk(nitro_inproc_socket_t *s, nitro_frame_t *snd, nitro_frame_t **frp, int flags) {
    return -1;
}

int Sinproc_socket_sub(nitro_inproc_socket_t *s,
    uint8_t *k, size_t length) {
    return -1;
}

int Sinproc_socket_unsub(nitro_inproc_socket_t *s,
    uint8_t *k, size_t length) {
    return -1;
}

int Sinproc_socket_pub(nitro_inproc_socket_t *s,
    nitro_frame_t **frp, uint8_t *k, size_t length, int flags) {
    return 0;
}
