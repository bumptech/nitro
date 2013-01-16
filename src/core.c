/*
 * Nitro
 *
 * core.c - Public API implementations, that often do switching out to tcp/inproc.c on the
 *          basis of socket type
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

nitro_frame_t *nitro_recv(nitro_socket_t *s) {
    nitro_frame_t *out = NULL;
    pthread_mutex_lock(&s->l_recv);

    while (!s->q_recv) {
        pthread_cond_wait(&s->c_recv, &s->l_recv);
    }

    out = s->q_recv;
    DL_DELETE(s->q_recv, out);
    s->count_recv--;
    pthread_mutex_unlock(&s->l_recv);
    assert(out); // why triggered with no frame?
    return out;
}

void socket_schedule_tcp_flush(nitro_socket_t *s) {
    pthread_mutex_lock(&s->l_send);
    if (!s->flush_pending) {
        nitro_socket_t *trial = NULL, *actual = NULL;
        while (1) {
            actual = __sync_val_compare_and_swap(
                &the_runtime->want_flush,
                trial, s);
            if (actual == trial) {
                s->flush_next = actual;
                break;
            }
            trial = actual;
        }
        uv_async_send(&the_runtime->tcp_flush);
        s->flush_pending = 1;
    }
    pthread_mutex_unlock(&s->l_send);
}

void nitro_send(nitro_frame_t *fr, nitro_socket_t *s) {
    nitro_frame_t *f = nitro_frame_copy(fr);
    pthread_mutex_lock(&s->l_send);

    while (s->capacity && s->count_send == s->capacity) {
        pthread_cond_wait(&s->c_send, &s->l_send);
    }

    DL_APPEND(s->q_send, f);
    pthread_mutex_unlock(&s->l_send);

    // If we are a socket portal, use uv
    switch (s->trans) {
    case NITRO_SOCKET_TCP:
        socket_schedule_tcp_flush(s);
        break;

    case NITRO_SOCKET_INPROC:
        socket_flush(s);
        break;

    default:
        assert(0);
    }
}

/* Private API: _common_ socket initialization */
nitro_socket_t *nitro_socket_new() {
    nitro_socket_t *sock = calloc(1, sizeof(nitro_socket_t));
    pthread_mutex_init(&sock->l_recv, NULL);
    pthread_mutex_init(&sock->l_send, NULL);
    pthread_mutex_init(&sock->l_sub, NULL);
    pthread_cond_init(&sock->c_recv, NULL);
    pthread_cond_init(&sock->c_send, NULL);
    sock->sub_keys = NULL;
    __sync_add_and_fetch(&the_runtime->num_sock, 1);
    return sock;
}

void nitro_socket_destroy(nitro_socket_t *s) {
    nitro_frame_t *f, *tmp;

    for (f = s->q_send; f; f = tmp) {
        tmp = f->next;
        nitro_frame_destroy(f);
    }

    for (f = s->q_recv; f; f = tmp) {
        tmp = f->next;
        nitro_frame_destroy(f);
    }

    nitro_prefix_trie_destroy(s->subs);
    free(s);
    __sync_sub_and_fetch(&the_runtime->num_sock, 1);
}

void pub_trie_callback(uint8_t *pfx,
                       uint8_t length, nitro_prefix_trie_mem *members,
                       void *baton) {
    nitro_frame_t *f = (nitro_frame_t *)baton;
    nitro_prefix_trie_mem *m = NULL;

    for (m = members; m; m = m->next) {
        nitro_pipe_t *p =
            (nitro_pipe_t *)m->ptr;
        printf("callback for %s and %p!\n", pfx, p);
        p->do_write(p, f);
    }
}

void nitro_flush() {
    while (1) {
        nitro_socket_t *maybe_flush = NULL, *wanted_flush = NULL;
        while (1) {
            wanted_flush = __sync_val_compare_and_swap(
                &the_runtime->want_flush,
                maybe_flush, NULL);
            if (wanted_flush == maybe_flush)
                break;
            maybe_flush = wanted_flush;
        }
        if (!wanted_flush)
            break;

        for (; wanted_flush; wanted_flush = wanted_flush->flush_next) {
            socket_flush(wanted_flush);
        }
    }
}

void socket_flush(nitro_socket_t *s) {
    pthread_mutex_lock(&s->l_send);
    s->flush_pending = 0;

    while (1) {
        nitro_frame_t *f = s->q_send;

        if (!f) {
            break;
        }

        nitro_pipe_t *p;

        // Non- pub frames need a pipe to go to.
        if (!f->is_pub) {
            p = s->next_pipe;

            if (!p) {
                break;
            }

            p->do_write(p, f);
            s->next_pipe = p->next;
        } else {
            pthread_mutex_lock(&s->l_sub);
            nitro_prefix_trie_search(
                s->subs, (uint8_t *)f->key, strlen(f->key),
                pub_trie_callback, (void *)f);
            pthread_mutex_unlock(&s->l_sub);
        }

        DL_DELETE(s->q_send, f);
        nitro_frame_destroy(f);
    }

    pthread_mutex_unlock(&s->l_send);
}

void add_pub_filter(nitro_socket_t *s, nitro_pipe_t *p, char *key) {
    nitro_key_t *t = nitro_key_new(key);
    DL_APPEND(p->sub_keys, t);
    nitro_prefix_trie_add(&s->subs,
                          (uint8_t *)key, strlen(key), p);
}

void remove_pub_filters(nitro_socket_t *s, nitro_pipe_t *p) {
    nitro_key_t *key, *tmp;

    for (key = p->sub_keys; key; key = tmp) {
        tmp = key->next;
        nitro_prefix_trie_del(s->subs,
                              (uint8_t *)key->key, strlen(key->key), p);
        free(key);
    }

    p->sub_keys = NULL;
}

NITRO_SOCKET_TRANSPORT parse_location(char *location, char **next) {
    if (!strncmp(location, TCP_PREFIX, strlen(TCP_PREFIX))) {
        *next = location + strlen(TCP_PREFIX);
        return NITRO_SOCKET_TCP;
    }

    if (!strncmp(location, INPROC_PREFIX, strlen(INPROC_PREFIX))) {
        *next = location + strlen(INPROC_PREFIX);
        return NITRO_SOCKET_INPROC;
    }

    nitro_set_error(NITRO_ERR_PARSE_BAD_TRANSPORT);
    return -1;
}

nitro_socket_t *nitro_socket_bind(char *location) {
    // clean up bind failures through the stack
    char *next;
    NITRO_SOCKET_TRANSPORT trans = parse_location(location, &next);

    switch (trans) {
    case NITRO_SOCKET_TCP:
        return nitro_bind_tcp(next);

    case NITRO_SOCKET_INPROC:
        return nitro_bind_inproc(next);

    default:
        assert(0);
        break;
    }

    return NULL;
}

nitro_socket_t *nitro_socket_connect(char *location) {
    char *next;
    NITRO_SOCKET_TRANSPORT trans = parse_location(location, &next);
    nitro_socket_t *ret = NULL;

    switch (trans) {
    case NITRO_SOCKET_TCP:
        ret = nitro_connect_tcp(next);
        break;

    case NITRO_SOCKET_INPROC:
        ret = nitro_connect_inproc(next);
        break;

    default:
        assert(0);
    }

    return ret;
}

void nitro_socket_close(nitro_socket_t *s) {
    switch (s->trans) {
    case NITRO_SOCKET_TCP:
        nitro_close_tcp(s);
        break;

    case NITRO_SOCKET_INPROC:
        nitro_close_inproc(s);
        break;

    default:
        assert(0);
    }
}

void nitro_pub(nitro_frame_t *fr, nitro_socket_t *s, char *key) {
    // XXX We are incurring an etra copy here
    nitro_frame_t *f = nitro_frame_copy(fr);
    f->is_pub = 1;
    memmove(f->key, key, strlen(key));
    nitro_send(f, s);
    nitro_frame_destroy(f);
}

void nitro_sub(nitro_socket_t *s, char *key) {
    nitro_pipe_t *p;
    nitro_key_t *k = nitro_key_new(key);
    pthread_mutex_lock(&s->l_sub);
    DL_APPEND(s->sub_keys, k);
    s->do_sub(s, key);
    CDL_FOREACH(s->pipes, p) {
        p->do_sub(p, key);
    }
    pthread_mutex_unlock(&s->l_sub);
}

void nitro_unsub(nitro_socket_t *s, char *key) {
    nitro_pipe_t *p;
    nitro_key_t *k = NULL, *target = NULL;
    pthread_mutex_lock(&s->l_sub);

    for (k = s->sub_keys; k; k = k->next) {
        if (!strcmp(k->key, key)) {
            target = k;
            break;
        }
    }

    if (target) {
        DL_DELETE(s->sub_keys, target);
        s->do_unsub(s, key);
        CDL_FOREACH(s->pipes, p) {
            p->do_unsub(p, key);
        }
    }

    pthread_mutex_unlock(&s->l_sub);
}

nitro_key_t *nitro_key_new(char *key) {
    nitro_key_t *k = malloc(sizeof(nitro_key_t));
    k->next = NULL;
    k->prev = NULL;
    memmove(k->key, key, strlen(key));
    return k;
}
