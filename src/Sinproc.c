/*
 * Nitro
 *
 * Sinproc.c - Inproc sockets are in-process sockets, thin wrappers
 *             around thread-safe queues that are API-compatible
 *             with TCP sockets.
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
    nitro_inproc_socket_t *s = (nitro_inproc_socket_t *)p;

    if (s->opt->want_eventfd) {
        if (last == NITRO_QUEUE_STATE_EMPTY) {
            /* empty to non-empty */
            uint64_t inc = 1;
            int evwrote = write(s->event_fd, (char *)(&inc), sizeof(inc));
            assert(evwrote == sizeof(inc));
        } else if (st == NITRO_QUEUE_STATE_EMPTY) {
            /* non-empty to empty */
            uint64_t buf;
            int evread = read(s->event_fd, &buf, sizeof(uint64_t));
            assert(evread == sizeof(uint64_t));
        }
    }
}

/* Sinproc_create_queues
 * ------------------
 *
 * Create the global in queue associated with a socket
 */
void Sinproc_create_queues(nitro_inproc_socket_t *s) {
    s->q_recv = nitro_queue_new(
                    s->opt->hwm_in, Sinproc_socket_recv_queue_stat, (void *)s);
}

void Sinproc_socket_destroy(nitro_inproc_socket_t *s) {
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

    HASH_ADD_KEYPTR(bound_hh,
                    b->registry, c->opt->ident,
                    SOCKET_IDENT_LENGTH, c);

    CDL_PREPEND(b->links, c);
    ++b->num_links;

    if (!b->current) {
        b->current = c;
    }

    c->links = b;
    nitro_counted_buffer_incref(b->bind_counter);
    pthread_rwlock_unlock(&b->link_lock);
}

void Sinproc_socket_bound_rm_conn(nitro_inproc_socket_t *b,
                                  nitro_inproc_socket_t *c) {

    pthread_rwlock_wrlock(&b->link_lock);

    HASH_DELETE(bound_hh, b->registry, c);

    if (b->current == c) {
        b->current =  (c->next == c) ? NULL : c->next;
    }

    CDL_DELETE(b->links, c);
    --b->num_links;
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

    pthread_rwlock_init(&s->link_lock, NULL);
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
    s->current = NULL;

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

void Sinproc_socket_enable_writes(nitro_inproc_socket_t *s) {
    INPROC_NOTIMPL;
}

void Sinproc_socket_enable_reads(nitro_inproc_socket_t *s) {
    INPROC_NOTIMPL;
}

void Sinproc_socket_start_connect(nitro_inproc_socket_t *s) {
    INPROC_NOTIMPL;
}

void Sinproc_socket_start_shutdown(nitro_inproc_socket_t *s) {
    INPROC_NOTIMPL;
}

void Sinproc_socket_close(nitro_inproc_socket_t *s) {
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

static int Sinproc_socket_send_general(nitro_inproc_socket_t *s,  nitro_frame_t *fr, int flags) {
    int ret = -1;

    if (s->bound) {
        pthread_rwlock_rdlock(&s->link_lock);

        volatile nitro_inproc_socket_t *try = s->current;

        if (try == NULL) {
                nitro_set_error(NITRO_ERR_INPROC_NO_CONNECTIONS);
            }
        else {
            int ok = 0;

            while (!ok) {
                try = s->current;

                ok = __sync_bool_compare_and_swap(

                         &s->current, (nitro_inproc_socket_t *)try, try->next);
            }

            ret = nitro_queue_push(try->q_recv, fr,
                                       !(flags & NITRO_NOWAIT));

            INCR_STAT((nitro_inproc_socket_t *)try, try->stat_recv, 1);
        }

        pthread_rwlock_unlock(&s->link_lock);
    } else {
        assert(s->links);

        if (s->links->dead) {
            nitro_set_error(NITRO_ERR_INPROC_NO_CONNECTIONS);
        } else {
            ret = nitro_queue_push(s->links->q_recv, fr,
                                   !(flags & NITRO_NOWAIT));
            INCR_STAT(s->links, s->links->stat_recv, 1);
        }
    }

    return ret;
}

static int Sinproc_socket_send_to_ident(nitro_inproc_socket_t *s, uint8_t *ident, nitro_frame_t *fr, int flags) {
    int ret = -1;

    nitro_inproc_socket_t *try;

    if (s->bound) {
        pthread_rwlock_rdlock(&s->link_lock);
        HASH_FIND(bound_hh, s->registry, ident,
                  SOCKET_IDENT_LENGTH, try);

        if (try == NULL) {
                nitro_set_error(NITRO_ERR_NO_RECIPIENT);
            }
        else {

            ret = nitro_queue_push(try->q_recv, fr, 0);

            INCR_STAT(try, try->stat_recv, 1);
        }

        pthread_rwlock_unlock(&s->link_lock);
    } else {
        assert(s->links);

        if (s->links->dead) {
            nitro_set_error(NITRO_ERR_NO_RECIPIENT);
        } else {
            ret = nitro_queue_push(s->links->q_recv, fr,
                                   !(flags & NITRO_NOWAIT));
            INCR_STAT(s->links, s->links->stat_recv, 1);
        }
    }

    return ret;
}

int Sinproc_socket_send(nitro_inproc_socket_t *s, nitro_frame_t **frp, int flags) {
    nitro_frame_t *fr = *frp;

    if (flags & NITRO_REUSE) {
        fr = nitro_frame_copy_partial(fr, NULL);
    } else {
        *frp = NULL;
    }

    nitro_frame_set_sender(fr, s->opt->ident, s->opt->ident_buf);

    int ret = Sinproc_socket_send_general(s, fr, flags);

    if (ret) {
        nitro_frame_destroy(fr);
    }

    return ret;
}

nitro_frame_t *Sinproc_socket_recv(nitro_inproc_socket_t *s, int flags) {
    return nitro_queue_pull(s->q_recv, !(flags & NITRO_NOWAIT));
}

int Sinproc_socket_reply(nitro_inproc_socket_t *s, nitro_frame_t *snd, nitro_frame_t **frp, int flags) {
    int ret = -1;
    nitro_frame_t *fr = *frp;
    fr = nitro_frame_copy_partial(fr, NULL);
    nitro_frame_clone_stack(snd, fr);
    nitro_frame_set_sender(fr, s->opt->ident, s->opt->ident_buf);

    ret = Sinproc_socket_send_to_ident(s, snd->sender, fr, flags);

    if (!(flags & NITRO_REUSE)) {
        nitro_frame_destroy(*frp);
        *frp = NULL;
    }

    if (ret) {
        nitro_frame_destroy(fr);
    }

    return ret;
}

int Sinproc_socket_relay_fw(nitro_inproc_socket_t *s, nitro_frame_t *snd, nitro_frame_t **frp, int flags) {
    int ret = -1;
    nitro_frame_t *fr = *frp;
    fr = nitro_frame_copy_partial(fr, NULL);

    nitro_frame_set_sender(fr, s->opt->ident, s->opt->ident_buf);
    nitro_frame_extend_stack(snd, fr);

    ret = Sinproc_socket_send_general(s, fr, flags);

    if (!(flags & NITRO_REUSE)) {
        nitro_frame_destroy(*frp);
        *frp = NULL;
    }

    if (ret) {
        nitro_frame_destroy(fr);
    }

    return ret;
}

int Sinproc_socket_relay_bk(nitro_inproc_socket_t *s, nitro_frame_t *snd, nitro_frame_t **frp, int flags) {
    int ret = -1;
    nitro_frame_t *fr = *frp;
    fr = nitro_frame_copy_partial(fr, NULL);
    nitro_frame_set_sender(fr, s->opt->ident, s->opt->ident_buf);

    uint8_t *top_of_stack = snd->ident_data + (
                                (snd->num_ident - 1) * SOCKET_IDENT_LENGTH);

    nitro_frame_clone_stack(snd, fr);
    nitro_frame_stack_pop(fr);

    ret = Sinproc_socket_send_to_ident(s, top_of_stack, fr, flags);

    if (!(flags & NITRO_REUSE)) {
        nitro_frame_destroy(*frp);
        *frp = NULL;
    }

    if (ret) {
        nitro_frame_destroy(fr);
    }

    return ret;
}

int Sinproc_socket_sub(nitro_inproc_socket_t *s,
                       uint8_t *k, size_t length) {
    uint8_t *cp = memdup(k, length);
    nitro_key_t *search;

    nitro_counted_buffer_t *buf =
        nitro_counted_buffer_new(
            cp, just_free, NULL);

    nitro_key_t *key = nitro_key_new(cp, length,
                                     buf);

    int ret = 0;
    pthread_rwlock_wrlock(&s->link_lock);

    DL_FOREACH(s->sub_keys, search) {
        if (!nitro_key_compare(search, key)) {
            ret = -1;
            break;
        }
    }

    if (!ret) {
        DL_APPEND(s->sub_keys, key);
        pthread_rwlock_unlock(&s->link_lock);

        if (s->bound) {
            pthread_rwlock_rdlock(&s->link_lock);
            nitro_inproc_socket_t *iter;
            CDL_FOREACH(s->links, iter) {
                nitro_prefix_trie_add(
                    &iter->subs,
                    k, length, s);

            }
            pthread_rwlock_unlock(&s->link_lock);
        } else {
            assert(s->links);

            if (s->links->dead) {
                ret = nitro_set_error(NITRO_ERR_NO_RECIPIENT);
            } else {
                ret = 0;
                nitro_inproc_socket_t *remote = s->links;
                pthread_rwlock_wrlock(&remote->link_lock);
                nitro_prefix_trie_add(
                    &remote->subs,
                    k, length, s);
                pthread_rwlock_unlock(&remote->link_lock);
            }
        }
    } else {
        nitro_key_destroy(key);
        nitro_set_error(NITRO_ERR_SUB_ALREADY);
        pthread_rwlock_unlock(&s->link_lock);
    }

    return ret;
}

int Sinproc_socket_unsub(nitro_inproc_socket_t *s,
                         uint8_t *k, size_t length) {
    uint8_t *cp = memdup(k, length);
    nitro_key_t *search;

    nitro_counted_buffer_t *buf =
        nitro_counted_buffer_new(
            cp, just_free, NULL);

    nitro_key_t *tmp = nitro_key_new(cp, length,
                                     buf);

    int ret = -1;
    pthread_rwlock_wrlock(&s->link_lock);

    DL_FOREACH(s->sub_keys, search) {
        if (!nitro_key_compare(search, tmp)) {
            ret = 0;
            break;
        }
    }

    nitro_key_destroy(tmp);

    if (!ret) {
        assert(search);
        DL_DELETE(s->sub_keys, search);
        pthread_rwlock_unlock(&s->link_lock);

        if (s->bound) {
            pthread_rwlock_rdlock(&s->link_lock);
            nitro_inproc_socket_t *iter;
            CDL_FOREACH(s->links, iter) {
                nitro_prefix_trie_del(
                    iter->subs,
                    k, length, s);
            }
            pthread_rwlock_unlock(&s->link_lock);
        } else {
            assert(s->links);

            if (s->links->dead) {
                ret = nitro_set_error(NITRO_ERR_NO_RECIPIENT);
            } else {
                nitro_inproc_socket_t *remote = s->links;
                pthread_rwlock_wrlock(&remote->link_lock);
                nitro_prefix_trie_del(
                    remote->subs,
                    k, length, s);
                pthread_rwlock_unlock(&remote->link_lock);
            }
        }
    } else {
        nitro_set_error(NITRO_ERR_SUB_MISSING);
        pthread_rwlock_unlock(&s->link_lock);
    }

    return ret;
}

/* State during trie walk for socket delivery */
typedef struct Sinproc_pub_state {
    int count;
    nitro_frame_t *fr;
} Sinproc_pub_state;

/*
 * Stcp_deliver_pub_frame
 * ----------------------
 *
 * Callback given to trie walk function by pub() function;
 * will be invoked for all prefixes that match.
 *
 * It walks the list of pipes subscribed to that prefix
 * and puts the message on the direct queue for each.
 */
static void Sinproc_deliver_pub_frame(
    const uint8_t *pfx, uint8_t length, nitro_prefix_trie_mem *mems,
    void *ptr) {

    Sinproc_pub_state *st = (Sinproc_pub_state *)ptr;

    nitro_prefix_trie_mem *m;

    DL_FOREACH(mems, m) {
        nitro_inproc_socket_t *s = (nitro_inproc_socket_t *)m->ptr;

        nitro_frame_t *fr = st->fr;
        fr = nitro_frame_copy_partial(fr, NULL);
        int r = nitro_queue_push(s->q_recv, fr, 0);

        if (r) {
            nitro_frame_destroy(fr);
        } else {
            ++st->count;
        }
    }
}

int Sinproc_socket_pub(nitro_inproc_socket_t *s,
                       nitro_frame_t **frp, uint8_t *k, size_t length, int flags) {
    nitro_frame_t *fr = *frp;

    if (flags & NITRO_REUSE) {
        fr = nitro_frame_copy_partial(fr, NULL);
    } else {
        *frp = NULL;
    }

    nitro_frame_set_sender(fr, s->opt->ident, s->opt->ident_buf);
    Sinproc_pub_state st = {0, fr};

    pthread_rwlock_rdlock(&s->link_lock);

    nitro_prefix_trie_search(s->subs,
                             k, length, Sinproc_deliver_pub_frame, &st);

    pthread_rwlock_unlock(&s->link_lock);

    nitro_frame_destroy(fr);

    return st.count;
}

void Sinproc_socket_describe(nitro_inproc_socket_t *s, nitro_buffer_t *buf) {
    int amt = 500;
    char *ptr = nitro_buffer_prepare(buf, &amt);

    pthread_rwlock_rdlock(&s->link_lock);

    int written;

    if (!s->bound) {
        written = snprintf(ptr, amt, "C-%02x%02x%02x%02x inproc://%s (recv_q=%u, recv_tot=%" PRIu64 ")\n",
                           SOCKET_UNIVERSAL(s)->opt->ident[0],
                           SOCKET_UNIVERSAL(s)->opt->ident[1],
                           SOCKET_UNIVERSAL(s)->opt->ident[2],
                           SOCKET_UNIVERSAL(s)->opt->ident[3],
                           s->given_location,
                           nitro_queue_count(s->q_recv),
                           s->stat_recv
                          );
        nitro_buffer_extend(buf, written);
    } else {
        written = snprintf(ptr, amt, "B-%02x%02x%02x%02x  inproc://%s (peers=%d, recv_q=%u, recv_tot=%" PRIu64 ")\n",
                           SOCKET_UNIVERSAL(s)->opt->ident[0],
                           SOCKET_UNIVERSAL(s)->opt->ident[1],
                           SOCKET_UNIVERSAL(s)->opt->ident[2],
                           SOCKET_UNIVERSAL(s)->opt->ident[3],
                           s->given_location,
                           s->num_links,
                           nitro_queue_count(s->q_recv),
                           s->stat_recv
                          );
        nitro_buffer_extend(buf, written);
    }

    pthread_rwlock_unlock(&s->link_lock);
}
