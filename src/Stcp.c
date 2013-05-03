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

#include "async.h"
#include "err.h"
#include "pipe.h"
#include "runtime.h"
#include "socket.h"

#include "nitro.h"

#define TCP_INBUF (64 * 1024)

/* EV callbacks, declaration */
void Stcp_bind_callback(
    struct ev_loop *loop, 
    ev_io *bind_io,
    int revents);
void Stcp_pipe_out_cb(
    struct ev_loop *loop,
    ev_io *pipe_iow,
    int revents);
void Stcp_pipe_in_cb(
    struct ev_loop *loop,
    ev_io *pipe_iow,
    int revents);
void Stcp_socket_connect_timer_cb(
    struct ev_loop *loop,
    ev_timer *connect_timer,
    int revents);
void Stcp_socket_check_sub(
    struct ev_loop *loop,
    ev_timer *connect_timer,
    int revents);
void Stcp_socket_close_cb(
    struct ev_loop *loop,
    ev_timer *close_timer,
    int revents);

/* various fw declaration */
void Stcp_socket_disable_reads(nitro_tcp_socket_t *s);
void Stcp_make_pipe(nitro_tcp_socket_t *s, int fd);
void Stcp_destroy_pipe(nitro_pipe_t *p);
void Stcp_socket_start_connect(nitro_tcp_socket_t *s);

static int Stcp_nonblocking_socket_new() {
    int s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(s >= 0);
    int flag = 1;
    int r = ioctl(s, FIONBIO, &flag);
    assert(r == 0);

    /* TCP NOWAIT */
    int state = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &state, sizeof(state));

    return s;
}

static int Stcp_parse_location(char *p_location,
                              struct sockaddr_in *addr) {
    char *location = alloca(strlen(p_location) + 1);
    strcpy(location, p_location);
    char *split = strchr(location, ':');


    if (!split) {
        return nitro_set_error(NITRO_ERR_TCP_LOC_NOCOLON);
    }


    *split = 0;
    errno = 0;
    int port = strtol(split + 1, NULL, 10);

    char buf[50] = {0};
    if (!strcmp(location, "*")) {
        strcpy(buf, "0.0.0.0");
    }
    else {
        strcpy(buf, location);
    }

    if (errno) {
        return nitro_set_error(NITRO_ERR_TCP_LOC_BADPORT);
    }

    bzero(addr, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    int r = inet_pton(AF_INET, buf,
    (void *)&addr->sin_addr);
    if (!r) {
        return nitro_set_error(NITRO_ERR_TCP_LOC_BADIPV4);
    }

    return 0;
}

void Stcp_socket_send_queue_stat(NITRO_QUEUE_STATE st, NITRO_QUEUE_STATE last, void *p) {
    if (last == NITRO_QUEUE_STATE_EMPTY) {
        nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)p;
        nitro_async_t *a = nitro_async_new(NITRO_ASYNC_ENABLE_WRITES);
        a->u.enable_writes.socket = SOCKET_PARENT(s);
        nitro_async_schedule(a);
    }
}

void Stcp_socket_recv_queue_stat(NITRO_QUEUE_STATE st, NITRO_QUEUE_STATE last, void *p) {
    nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)p;
    if (st == NITRO_QUEUE_STATE_FULL) {
        Stcp_socket_disable_reads(s);
    }
    else if (last == NITRO_QUEUE_STATE_FULL) {
        /* async, we're not on nitro thread */
        nitro_async_t *a = nitro_async_new(NITRO_ASYNC_ENABLE_READS);
        a->u.enable_reads.socket = SOCKET_PARENT(s);
        nitro_async_schedule(a);
    }
}

void Stcp_create_queues(nitro_tcp_socket_t *s) {
    s->q_send = nitro_queue_new(
        s->opt->hwm_out_general, Stcp_socket_send_queue_stat, (void*)s);
    s->q_recv = nitro_queue_new(
        s->opt->hwm_in, Stcp_socket_recv_queue_stat, (void*)s);
}

int Stcp_socket_connect(nitro_tcp_socket_t *s, char *location) {
    int r = Stcp_parse_location(location, &s->location);

    if (r) {
        /* Note - error detail set by parse_tcp_location */
        return r;
    }
    s->outbound = 1;

    Stcp_create_queues(s);

    ev_timer_init(
        &s->sub_send_timer,
        Stcp_socket_check_sub,
        1.0, 1.0);
    s->sub_send_timer.data = s;

    ev_timer_init(
        &s->connect_timer,
        Stcp_socket_connect_timer_cb,
        s->opt->reconnect_interval, 0);
    s->connect_timer.data = s;

    nitro_async_t *a = nitro_async_new(NITRO_ASYNC_CONNECT);
    a->u.connect.socket = SOCKET_PARENT(s);
    nitro_async_schedule(a);

    return 0;
}

int Stcp_socket_bind(nitro_tcp_socket_t *s, char *location) {
    int r = Stcp_parse_location(location, &s->location);
    s->outbound = 0;

    if (r) {
        return r;
    }

    Stcp_create_queues(s);
    ev_timer_init(
        &s->sub_send_timer,
        Stcp_socket_check_sub,
        1.0, 1.0);
    s->sub_send_timer.data = s;

    s->bound_fd = Stcp_nonblocking_socket_new();

    int t = 1;
    setsockopt(s->bound_fd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(int));
#ifdef SO_REUSEPORT
    setsockopt(s->bound_fd, SOL_SOCKET, SO_REUSEPORT, &t, sizeof(int));
#endif
    if (bind(s->bound_fd, 
        (struct sockaddr *)&s->location, 
        sizeof(s->location))){
        return nitro_set_error(NITRO_ERR_ERRNO);
    }

    listen(s->bound_fd, 512);

    ev_io_init(&s->bound_io,
    Stcp_bind_callback, s->bound_fd,
    EV_READ);
    s->bound_io.data = s;

    /* we need to do the ev work on the ev thread */
    nitro_async_t *bg = nitro_async_new(NITRO_ASYNC_BIND_LISTEN);
    bg->u.bind_listen.socket = (nitro_socket_t *)s->parent;
    nitro_async_schedule(bg);

    return 0;
}

void Stcp_socket_start_shutdown(nitro_tcp_socket_t *s) {
    ev_timer_init(
        &s->close_timer,
        Stcp_socket_close_cb,
        s->opt->close_linger, 0);
    s->close_timer.data = s;
    ev_timer_start(the_runtime->the_loop, &s->close_timer);
}

void Stcp_socket_shutdown(nitro_tcp_socket_t *s) {
    nitro_pipe_t *tmp1, *tmp2, *p = NULL;

    CDL_FOREACH_SAFE(s->pipes,p,tmp1,tmp2) {
        Stcp_destroy_pipe(p);
    }

    nitro_queue_destroy(s->q_send);
    nitro_queue_destroy(s->q_recv);
    ev_timer_stop(the_runtime->the_loop, &s->connect_timer);
    ev_io_stop(the_runtime->the_loop, &s->connect_io);
    ev_io_stop(the_runtime->the_loop, &s->bound_io);
    if (s->bound_fd > 0) {
        close(s->bound_fd);
    }
    if (s->connect_fd > 0) {
        close(s->connect_fd);
    }
    nitro_socket_destroy(SOCKET_PARENT(s));
}

void Stcp_socket_close_cb(
    struct ev_loop *loop,
    ev_timer *close_timer,
    int revents) {
        nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)close_timer->data;
        Stcp_socket_shutdown(s);
}

void Stcp_socket_bind_listen(nitro_tcp_socket_t *s) {
    NITRO_THREAD_CHECK;
    ev_io_start(the_runtime->the_loop, 
    &s->bound_io);
}

void Stcp_pipe_send_sub(nitro_tcp_socket_t *s,
    nitro_pipe_t *p) {
    // NOTE: assumed l_pipes is held

    nitro_counted_buffer_incref(s->sub_data);
    nitro_frame_t *fr = nitro_frame_new_prealloc(
        s->sub_data->ptr,
        s->sub_data_length,
        s->sub_data);
    fr->type = NITRO_FRAME_SUB;

    int r = nitro_queue_push(p->q_send, fr, 0); // NO WAIT on nitro thread
    if (r) {
        /* cannot get our sub frame out b/c output is full */
        nitro_frame_destroy(fr);
    }
}

void Stcp_scan_subs(nitro_tcp_socket_t *s) {
    // NOTE: assumed l_pipes is held
    nitro_pipe_t *p;

    CDL_FOREACH(s->pipes, p) {
        if (p->sub_state_sent != s->sub_keys_state) {
            Stcp_pipe_send_sub(s, p);
        }
    }
}

void Stcp_socket_check_sub(
    struct ev_loop *loop,
    ev_timer *connect_timer,
    int revents) {
    nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)connect_timer->data;

    pthread_mutex_lock(&s->l_pipes);
    Stcp_scan_subs(s);
    pthread_mutex_unlock(&s->l_pipes);
}

void Stcp_socket_connect_timer_cb(
    struct ev_loop *loop,
    ev_timer *connect_timer,
    int revents) {
    nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)connect_timer->data;
    ev_timer_stop(the_runtime->the_loop, connect_timer);

    Stcp_socket_start_connect(s);
}

void Stcp_connect_cb(
    struct ev_loop *loop,
    ev_io *connect_io,
    int revents) {
    nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)connect_io->data;

    int t = connect(s->connect_fd,
        (struct sockaddr *)&s->location,
        sizeof(s->location));

    if (!t || errno == EISCONN || !errno) {
        ev_io_stop(the_runtime->the_loop, &s->connect_io);
        Stcp_make_pipe(s, s->connect_fd);
        s->connect_fd = -1;
    }
    else if (errno == EINPROGRESS || errno == EINTR || errno == EALREADY) {
        /* do nothing, we'll get phoned home again... */
    }
    else {
        /* let's restart the timer */
        ev_io_stop(the_runtime->the_loop, &s->connect_io);
        close(s->connect_fd);
        ev_timer_start(the_runtime->the_loop, &s->connect_timer);
    }
}

void Stcp_socket_start_connect(nitro_tcp_socket_t *s) {
    NITRO_THREAD_CHECK;

    s->connect_fd = Stcp_nonblocking_socket_new();

    ev_io_init(&s->connect_io,
    Stcp_connect_cb, s->connect_fd, EV_WRITE);
    s->connect_io.data = s;


    int t = connect(s->connect_fd,
        (struct sockaddr *)&s->location,
        sizeof(s->location));

    if (t == 0 || errno == EINPROGRESS || errno == EINTR) {
        ev_io_start(the_runtime->the_loop, &s->connect_io);
    }
    else {
        close(s->connect_fd);
        ev_timer_start(the_runtime->the_loop, &s->connect_timer);
    }
}

void Stcp_pipe_send_queue_stat(NITRO_QUEUE_STATE st, NITRO_QUEUE_STATE last, void *baton) {
    if (last == NITRO_QUEUE_STATE_EMPTY) {
        nitro_pipe_t *p = (nitro_pipe_t *)baton;
        nitro_async_t *a = nitro_async_new(NITRO_ASYNC_ENABLE_WRITES);
        a->u.enable_writes.pipe = p;
        nitro_async_schedule(a);
    }
}

void Stcp_destroy_pipe(nitro_pipe_t *p) {
    nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)p->the_socket;
    ev_io_stop(the_runtime->the_loop, &p->iow);
    ev_io_stop(the_runtime->the_loop, &p->ior);
    nitro_buffer_destroy(p->in_buffer);
    nitro_queue_destroy(p->q_send);
    close(p->fd);
    if (p->partial) {
        nitro_frame_destroy(p->partial);
    }
    nitro_pipe_destroy(p,
        SOCKET_UNIVERSAL(s));

    if (s->outbound) {
        ev_timer_start(the_runtime->the_loop, &s->connect_timer);
    }
}

void Stcp_make_pipe(nitro_tcp_socket_t *s, int fd) {
    NITRO_THREAD_CHECK;
    nitro_pipe_t *p = nitro_pipe_new(SOCKET_UNIVERSAL(s));
    p->fd = fd;
    p->in_buffer = nitro_buffer_new();
    p->the_socket = s;

    ev_io_init(&p->iow, Stcp_pipe_out_cb,
    p->fd, EV_WRITE);
    p->iow.data = p;

    ev_io_init(&p->ior, Stcp_pipe_in_cb,
    p->fd, EV_READ);
    p->ior.data = p;

    p->q_send = nitro_queue_new(
        s->opt->hwm_out_private,
        Stcp_pipe_send_queue_stat, p);

    ev_io_start(the_runtime->the_loop,
    &p->iow);
    ev_io_start(the_runtime->the_loop,
    &p->ior);

}

void Stcp_pipe_enable_write(nitro_pipe_t *p) {
    NITRO_THREAD_CHECK;
    ev_io_start(the_runtime->the_loop,
    &p->iow);
}

void Stcp_socket_enable_writes(nitro_tcp_socket_t *s) {
    NITRO_THREAD_CHECK;
    nitro_pipe_t *p;

    CDL_FOREACH(s->pipes, p) {
        ev_io_start(the_runtime->the_loop,
        &p->iow);
    }
}

void Stcp_socket_enable_reads(nitro_tcp_socket_t *s) {
    NITRO_THREAD_CHECK;
    nitro_pipe_t *p;

    CDL_FOREACH(s->pipes, p) {
        ev_io_start(the_runtime->the_loop,
        &p->ior);
    }
}

void Stcp_socket_disable_reads(nitro_tcp_socket_t *s) {
    NITRO_THREAD_CHECK;
    nitro_pipe_t *p;

    CDL_FOREACH(s->pipes, p) {
        ev_io_stop(the_runtime->the_loop,
        &p->ior);
    }
}

void Stcp_socket_close(nitro_tcp_socket_t *s) {
    nitro_async_t *a = nitro_async_new(NITRO_ASYNC_CLOSE);
    a->u.close.socket = SOCKET_PARENT(s);
    nitro_async_schedule(a);
}

typedef struct tcp_frame_parse_state {
    nitro_buffer_t *buf;
    nitro_counted_buffer_t *cbuf;
    char *cursor;
    nitro_pipe_t *p;
    nitro_tcp_socket_t *s;
} tcp_frame_parse_state;


static void Stcp_socket_parse_subs(nitro_tcp_socket_t *s, nitro_pipe_t *p,
    uint64_t state, uint8_t *data, size_t size, nitro_counted_buffer_t *buf) {

    /* We don't need the lock for this part */
    nitro_key_t *old = p->sub_keys;
    p->sub_keys = NULL;

    while (1) {
        if (size < sizeof(uint8_t)) {
            break;
        }

        uint8_t length = *(uint8_t *)data;

        size -= sizeof(uint8_t);
        if (size < length)
            break;
        data += sizeof(uint8_t);

        nitro_key_t *key = nitro_key_new(data, length, buf);
        DL_APPEND(p->sub_keys, key);

        size -= length;
        data += length;
    }

    /* Okay, now let's modify the shared trie */
    DL_SORT(p->sub_keys, nitro_key_compare);

    // XXX asserts on del()

    nitro_key_t *tkey;
    DL_FOREACH(p->sub_keys, tkey) {
        char buf[4];
        bzero(buf, 4);
        memcpy(buf, tkey->data, 3);
        fprintf(stderr, " ~ %s\n", buf);
    }

    pthread_mutex_lock(&s->l_pipes);

    nitro_key_t *walk_old = old, *tmp=NULL, *walk_new = p->sub_keys;

    while (1) {
        if (walk_old == NULL) {
            if (walk_new == NULL) {
                /* end of both lists */
                break;
            }
            fprintf(stderr, "add new at end\n");

            nitro_prefix_trie_add(&s->subs,
                walk_new->data, walk_new->length,
                p);

            walk_new = walk_new->next;
        } else if (walk_new == NULL) {
            /* End of new, walk old and remove */
            nitro_prefix_trie_del(s->subs,
                walk_old->data, walk_old->length,
                p);
            tmp = walk_old;
            walk_old = walk_old->next;
            nitro_key_destroy(tmp);
            fprintf(stderr, "rm old at end\n");
        }

        else {
            /* they're both still in progress */
            int comp = nitro_key_compare(walk_old, walk_new);
            if (comp < 0) {
                /* new has jumped ahead, old key is missing */
                nitro_prefix_trie_del(s->subs,
                    walk_old->data, walk_old->length,
                    p);

                tmp = walk_old;
                walk_old = walk_old->next;
                nitro_key_destroy(tmp);
                fprintf(stderr, "rm old at skip\n");

            } else if (comp > 0) {
                /* old has jumped ahead, need to add key */
                nitro_prefix_trie_add(&s->subs,
                    walk_new->data, walk_new->length,
                    p);

                walk_new = walk_new->next;
                fprintf(stderr, "add new at skip\n");
            } else {
                /* key still exists.. move both forward, don't alter trie */
                tmp = walk_old;
                walk_old = walk_old->next;
                nitro_key_destroy(tmp);

                walk_new = walk_new->next;
                fprintf(stderr, "same -- skip both\n");
            }
        }
    }

    pthread_mutex_unlock(&s->l_pipes);

    p->sub_state_recv = state;
}

static nitro_frame_t *Stcp_parse_next_frame(void *baton) {
    tcp_frame_parse_state *st = (tcp_frame_parse_state *)baton;
    nitro_frame_t *fr = NULL;

    int size;
    nitro_buffer_t const *buf = st->buf;
    char const *start = nitro_buffer_data((nitro_buffer_t *)buf, &size);

    if (!st->cursor) {
        st->cursor = (char *)start;
    }

    while (!fr) {

        char const *cursor = st->cursor;

        int taken = cursor - start;
        int left = size - taken;

        if (left < sizeof(nitro_protocol_header)) {
            break;
        }

        nitro_protocol_header const *hd = 
        (nitro_protocol_header *)cursor;
        left -= sizeof(nitro_protocol_header);

        if (hd->frame_size > st->s->opt->max_message_size) {
            assert(0);
            // XXX handle more gracefully
        }

        // XXX error on protocol version
        // XXX this is BAD, crashy!
        // XXX maximum packet size, config based?
        // other safety/security/anti-dos audit
        // ... probably the socket closes on
        // rule violations

        assert(hd->protocol_version == 1);
        int ident_size = hd->num_ident * SOCKET_IDENT_LENGTH;
        if (left < hd->frame_size + ident_size) {
            break;
        }
        if (hd->packet_type != NITRO_FRAME_DATA) {
            /* It's a control frame; handle appropriately */
            if (hd->packet_type == NITRO_FRAME_HELLO) {
                // XXX better error
                assert(hd->frame_size == SOCKET_IDENT_LENGTH);
                st->p->remote_ident = malloc(SOCKET_IDENT_LENGTH);
                memcpy(st->p->remote_ident,
                    (char *)cursor + sizeof(nitro_protocol_header),
                    SOCKET_IDENT_LENGTH);
                st->p->remote_ident_buf = nitro_counted_buffer_new(
                    st->p->remote_ident, just_free, NULL);
                socket_register_pipe(SOCKET_UNIVERSAL(st->s), st->p);
                st->p->them_handshake = 1;
            } else if (hd->packet_type == NITRO_FRAME_SUB) {
                assert(hd->frame_size >= sizeof(uint64_t));
                uint64_t state = *(uint64_t *)(st->cursor + sizeof(nitro_protocol_header));
                if (st->p->sub_state_recv != state) {
                    if (!st->cbuf) {
                        /* we need to retain the backing buffer for the sub data */

                        st->cbuf = nitro_counted_buffer_new(
                        NULL, buffer_free, (void *)buf);
                    } else {fprintf(stderr, "nope, got it already\n");}
                    Stcp_socket_parse_subs(st->s, st->p, state,
                        (uint8_t *)(st->cursor + sizeof(nitro_protocol_header) + sizeof(uint64_t)),
                        hd->frame_size - sizeof(uint64_t), st->cbuf);
                }
            }
        } else {
            // XXX again, assert is not right here
            assert(st->p->them_handshake);
            if (!st->cbuf) {
                /* it is official, we will consume data... */

                /* first incref is for the socket itself, not done copying yet */
                st->cbuf = nitro_counted_buffer_new(
                NULL, buffer_free, (void *)buf);
            }
            nitro_counted_buffer_t const *cbuf = st->cbuf;

            /* incref for the eventual recipient */
            nitro_counted_buffer_incref((nitro_counted_buffer_t *)cbuf);

            fr = nitro_frame_new_prealloc(
                    (char *)cursor + sizeof(nitro_protocol_header),
                    hd->frame_size, (nitro_counted_buffer_t *)cbuf);
            nitro_frame_set_sender(fr,
            st->p->remote_ident, st->p->remote_ident_buf);
            if (hd->num_ident) {
                nitro_counted_buffer_incref((nitro_counted_buffer_t *)cbuf);
                nitro_frame_set_stack(fr,
                st->cursor + (sizeof(nitro_protocol_header) + hd->frame_size),
                (nitro_counted_buffer_t *)cbuf, hd->num_ident);
            }
        }
        st->cursor += (sizeof(nitro_protocol_header) + hd->frame_size + ident_size);
    }
    return fr;
}

void Stcp_parse_socket_buffer(nitro_pipe_t *p) {
    /* now we parse */
    nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)p->the_socket;

    tcp_frame_parse_state parse_state = {0};
    parse_state.buf = p->in_buffer;
    parse_state.p = p;
    parse_state.s = s;

    nitro_queue_consume(s->q_recv,
        Stcp_parse_next_frame,
        &parse_state);

    int size;
    char *start = nitro_buffer_data(p->in_buffer, &size);
    if (parse_state.cursor != start) {
        /* if this buffer contained at least one whole frame,
        we're going to disown it and create another one
        with whatever fractional data remain (if any) */

        int to_copy = size - (parse_state.cursor - start);
        nitro_buffer_t *tmp = p->in_buffer;
        p->in_buffer = nitro_buffer_new();

        if (to_copy) {
            int writable = TCP_INBUF > to_copy ? TCP_INBUF : to_copy;
            char *write = nitro_buffer_prepare(p->in_buffer, &writable);

            memcpy(write, parse_state.cursor, to_copy);
            nitro_buffer_extend(p->in_buffer, to_copy);
        }
        /* release our copy of the backing buffer */
        if (parse_state.cbuf) {
            nitro_counted_buffer_decref(parse_state.cbuf);
        }
        else {
            nitro_buffer_destroy(tmp);
        }
    }
}

/* LIBEV CALLBACKS */

void Stcp_bind_callback(
    struct ev_loop *loop,
    ev_io *bind_io,
    int revents)
{
    NITRO_THREAD_CHECK;
    nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)bind_io->data;

    struct sockaddr addr;
    socklen_t len = sizeof(struct sockaddr);

    int fd = accept(s->bound_fd,
    &addr, &len);
    if (fd < 0) {
        switch(fd) {
            case EAGAIN:
#if (EAGAIN != EWOULDBLOCK)
            case EWOULDBLOCK:
#endif
            case EINTR:
            case ECONNABORTED:
                return;
            case EMFILE:
            case ENFILE:
                perror("{nitro} file descriptor max reached:"); 
                return;
            default:
                /* the stipulation is we have our logic screwed
                   up if we get anything else */
                assert(0); // unusual error on accept()
        }
    }
    Stcp_make_pipe(s, fd);
}
#define OKAY_ERRNO (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == EINPROGRESS || errno == EALREADY)
void Stcp_pipe_out_cb(
    struct ev_loop *loop,
    ev_io *pipe_iow,
    int revents)
{
    NITRO_THREAD_CHECK;
    nitro_pipe_t *p = (nitro_pipe_t *)pipe_iow->data;
    nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)p->the_socket;

    int r = 0;
    int tried = 0;

    if (!p->us_handshake) {
        tried = 1;
        assert(nitro_queue_count(p->q_send) == 0);
        nitro_frame_t *hello = nitro_frame_new_copy(
        s->opt->ident, SOCKET_IDENT_LENGTH);
        hello->type = NITRO_FRAME_HELLO;
        r = nitro_queue_push(p->q_send, hello, 0);

        /* Cannot get hello out because q_send full?? not okay */
        assert(!r);

        pthread_mutex_lock(&s->l_pipes);
        if (s->sub_data) {
            Stcp_pipe_send_sub(s, p);
        }
        pthread_mutex_unlock(&s->l_pipes);
        p->us_handshake = 1;
    }

    /* Local queue takes precedence */
    if (nitro_queue_count(p->q_send)) {
        tried = 1;
        r = nitro_queue_fd_write(
            p->q_send,
            p->fd, p->partial, &(p->partial));
        if (r < 0 && OKAY_ERRNO)
            return;
        if (r < 0) {
            Stcp_destroy_pipe(p);
            return;
        }
    }

    /* Note -- using truncate frame as heuristic
       to guess kernel buffer is full*/
    if ((!tried || !p->partial) && nitro_queue_count(s->q_send)) {
        tried = 1;
        r = nitro_queue_fd_write(
            s->q_send,
            p->fd, p->partial, &(p->partial));
        if (r < 0 && OKAY_ERRNO)
            return;
        if (r < 0) {
            Stcp_destroy_pipe(p);
            return;
        }
    }

    if (!tried) {
        // wait until we're enabled
        ev_io_stop(the_runtime->the_loop,
        pipe_iow);
    }
}

void Stcp_pipe_in_cb(
    struct ev_loop *loop,
    ev_io *pipe_iow,
    int revents)
{
    /* NOTE: this is on the security critical path */
    NITRO_THREAD_CHECK;
    nitro_pipe_t *p = (nitro_pipe_t *)pipe_iow->data;


    int sz = TCP_INBUF;
    char *append_ptr = nitro_buffer_prepare(p->in_buffer, &sz);

    int r = read(p->fd, append_ptr, sz);

    if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        return;
    if (r <= 0) {
        Stcp_destroy_pipe(p);
        return;
    }

    nitro_buffer_extend(p->in_buffer, r);

    Stcp_parse_socket_buffer(p);

}

int Stcp_socket_send(nitro_tcp_socket_t *s, nitro_frame_t *fr, int flags) {
    if (!(flags & NITRO_NOCOPY)) {
        fr = nitro_frame_copy(fr);
    }
    int r = nitro_queue_push(s->q_send, fr, !(flags & NITRO_NOWAIT));
    if (r && !(flags & NITRO_NOCOPY)) {
        nitro_frame_destroy(fr);
    }
    return r;
}

nitro_frame_t *Stcp_socket_recv(nitro_tcp_socket_t *s, int flags) {
    return nitro_queue_pull(s->q_recv, !(flags & NITRO_NOWAIT));
}

int Stcp_socket_reply(nitro_tcp_socket_t *s, 
    nitro_frame_t *snd, nitro_frame_t *fr, int flags) {
    int ret = -1;
    pthread_mutex_lock(&s->l_pipes);
    nitro_pipe_t *p = socket_lookup_pipe(
        SOCKET_UNIVERSAL(s), snd->sender);

    if (p) {
        nitro_frame_t *out = (flags & NITRO_NOCOPY) ?
            fr : nitro_frame_copy(fr);
        nitro_frame_clone_stack(snd, out);
        ret = nitro_queue_push(p->q_send, out, !(flags & NITRO_NOWAIT));
        if (ret && !(flags & NITRO_NOCOPY)) {
            nitro_frame_destroy(out);
        }
    } else {
        nitro_set_error(NITRO_ERR_NO_RECIPIENT);
    }

    pthread_mutex_unlock(&s->l_pipes);
    return ret;
}

int Stcp_socket_relay_fw(nitro_tcp_socket_t *s, 
    nitro_frame_t *snd, nitro_frame_t *fr, int flags) {

    nitro_frame_t *out = (flags & NITRO_NOCOPY) ? fr : nitro_frame_copy(fr);
    nitro_frame_clone_stack(snd, out);
    nitro_frame_set_sender(out, snd->sender, snd->sender_buffer);
    nitro_frame_stack_push_sender(out);
    int r = nitro_queue_push(s->q_send, out, !(flags & NITRO_NOWAIT));
    if (r && !(flags & NITRO_NOCOPY)) {
        nitro_frame_destroy(out);
    }

    return r;
}

int Stcp_socket_relay_bk(nitro_tcp_socket_t *s, 
    nitro_frame_t *snd, nitro_frame_t *fr, int flags) {
    int ret = -1;
    pthread_mutex_lock(&s->l_pipes);

    assert(snd->num_ident);
    uint8_t *top_of_stack = snd->ident_data + (
        (snd->num_ident - 1) * SOCKET_IDENT_LENGTH);

    nitro_pipe_t *p = socket_lookup_pipe(
        SOCKET_UNIVERSAL(s), top_of_stack);

    if (p) {
        nitro_frame_t *out = (flags & NITRO_NOCOPY) ? 
            fr : nitro_frame_copy(fr);
        nitro_frame_clone_stack(snd, out);
        nitro_frame_stack_pop(out);
        ret = nitro_queue_push(p->q_send, out, !(flags & NITRO_NOWAIT));
        if (ret && !(flags & NITRO_NOCOPY)) {
            nitro_frame_destroy(out);
        }
    } else {
        nitro_set_error(NITRO_ERR_NO_RECIPIENT);
    }

    pthread_mutex_unlock(&s->l_pipes);
    return ret;
}

static void Stcp_socket_reindex_subs(nitro_tcp_socket_t *s) {

    nitro_key_t *key;
    ++s->sub_keys_state;
    fprintf(stderr, "state changed to: %llu\n", (unsigned long long) s->sub_keys_state);
    ev_timer_start(the_runtime->the_loop,
        &s->sub_send_timer);
    if (s->sub_data) {
        nitro_counted_buffer_decref(s->sub_data);
    }
    nitro_buffer_t *buf = 
        nitro_buffer_new();

    nitro_buffer_append(
        buf,
        (const char *)(&s->sub_keys_state),
        sizeof(uint64_t));

    DL_FOREACH(s->sub_keys, key) {
        nitro_buffer_append(
        buf,
        (const char *)(&key->length),
        sizeof(uint8_t));
        nitro_buffer_append(
        buf,
        (const char *)key->data,
        key->length);
    }

    s->sub_data = nitro_counted_buffer_new(
        buf->area, buffer_free, buf);
    s->sub_data_length = buf->size;
    Stcp_scan_subs(s);

}

int Stcp_socket_sub(nitro_tcp_socket_t *s,
    uint8_t *k, size_t length) {
    uint8_t *cp = memdup(k, length);

    nitro_counted_buffer_t *buf = 
        nitro_counted_buffer_new(
            cp, just_free, NULL);

    nitro_key_t *key = nitro_key_new(cp, length,
        buf);

    int ret = 0;
    pthread_mutex_lock(&s->l_pipes);
    nitro_key_t *search;

    DL_FOREACH(s->sub_keys, search) {
        if (!nitro_key_compare(search, key)) {
            ret = -1;
            break;
        }
    }

    if (!ret) {
        DL_APPEND(s->sub_keys, key);
        Stcp_socket_reindex_subs(s);
    }

    pthread_mutex_unlock(&s->l_pipes);

    return ret;
}

int Stcp_socket_unsub(nitro_tcp_socket_t *s,
    uint8_t *k, size_t length) {

    nitro_counted_buffer_t *buf =
        nitro_counted_buffer_new(
            k, free_nothing, NULL);

    nitro_key_t *tmp = nitro_key_new(
        k, length, buf);

    int ret = -1;

    pthread_mutex_lock(&s->l_pipes);
    nitro_key_t *search = NULL;

    DL_FOREACH(s->sub_keys, search) {
        if (!nitro_key_compare(search, tmp)) {
            ret = 0;
            break;
        }
    }

    nitro_key_destroy(tmp);

    if (search) {
        DL_DELETE(s->sub_keys, search);
        nitro_key_destroy(search);
        Stcp_socket_reindex_subs(s);
    }

    pthread_mutex_unlock(&s->l_pipes);

    return ret;
}

typedef struct Stcp_pub_state {
    int count;
    nitro_frame_t *fr;
} Stcp_pub_state;

static void Stcp_deliver_pub_frame(
    uint8_t *pfx, uint8_t length, nitro_prefix_trie_mem *mems,
    void *ptr) {
    // XXX use nonblocking push

    Stcp_pub_state *st = (Stcp_pub_state *)ptr;

    nitro_prefix_trie_mem *m;

    DL_FOREACH(mems, m) {
        nitro_pipe_t *p = (nitro_pipe_t *)m->ptr;

        nitro_frame_t *fr = nitro_frame_copy(st->fr);

        /* we'll *try* to pub, but if queue is full, then
           we're just gonna have to drop it (pub will not
           block the caller) */
        int r = nitro_queue_push(p->q_send, fr, 0);
        if (r) {
            nitro_frame_destroy(fr);
        } else {
            ++st->count;
        }
    }
}

int Stcp_socket_pub(nitro_tcp_socket_t *s,
    nitro_frame_t *fr, uint8_t *k, size_t length) {

    Stcp_pub_state st = {0};

    st.fr = fr;

    pthread_mutex_lock(&s->l_pipes);

    nitro_prefix_trie_search(s->subs,
        k, length, Stcp_deliver_pub_frame, &st);

    pthread_mutex_unlock(&s->l_pipes);

    return st.count;
}
