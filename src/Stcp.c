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

    if (errno) {
        return nitro_set_error(NITRO_ERR_TCP_LOC_BADPORT);
    }

    bzero(addr, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    int r = inet_pton(AF_INET, location, 
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
        0, Stcp_socket_send_queue_stat, (void*)s);
    s->q_recv = nitro_queue_new(
        0, Stcp_socket_recv_queue_stat, (void*)s);
}

int Stcp_socket_connect(nitro_tcp_socket_t *s, char *location) {
    int r = Stcp_parse_location(location, &s->location);

    if (r) {
        /* Note - error detail set by parse_tcp_location */
        return r;
    }

    Stcp_create_queues(s);

    ev_timer_init(
        &s->connect_timer,
        Stcp_socket_connect_timer_cb,
        0.5, 0);
    s->connect_timer.data = s;

    nitro_async_t *a = nitro_async_new(NITRO_ASYNC_CONNECT);
    a->u.connect.socket = SOCKET_PARENT(s);
    nitro_async_schedule(a);

    return 0;
}

int Stcp_socket_bind(nitro_tcp_socket_t *s, char *location) {
    int r = Stcp_parse_location(location, &s->location);

    if (r) {
        return r;
    }

    Stcp_create_queues(s);

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

void Stcp_socket_bind_listen(nitro_tcp_socket_t *s) {
    NITRO_THREAD_CHECK;
    ev_io_start(the_runtime->the_loop, 
    &s->bound_io);
}

void Stcp_socket_connect_timer_cb(
    struct ev_loop *loop,
    ev_timer *connect_timer,
    int revents) {
    nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)connect_timer->data;

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
        return ;
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
    // XXX why is this partial set, crash?
    if (p->partial) {
        nitro_frame_destroy(p->partial);
    }
    nitro_pipe_destroy(p,
        SOCKET_UNIVERSAL(s));
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

    // eventually, use socket settings
    // for capacity
    p->q_send = nitro_queue_new(0,
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
            }
        } else {
            // XXX again, assert is not right here
            assert(st->p->them_handshake);
            if (!st->cbuf) {
                /* it is official, we will consume data... */

                /* first incref is for the socket itself, not done copying yet */
                st->cbuf = nitro_counted_buffer_new(
                (nitro_buffer_t *)buf, buffer_free, NULL);
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

    if (parse_state.cbuf) {
        /* if this buffer contained at least one whole frame,
        we're going to disown it and create another one
        with whatever fractional data remain (if any) */

        int size;
        char *start = nitro_buffer_data(p->in_buffer, &size);
        int to_copy = size - (parse_state.cursor - start);
        p->in_buffer = nitro_buffer_new();

        if (to_copy) {
            int writable = TCP_INBUF > to_copy ? TCP_INBUF : to_copy;
            char *write = nitro_buffer_prepare(p->in_buffer, &writable);

            memcpy(write, parse_state.cursor, to_copy);
            nitro_buffer_extend(p->in_buffer, to_copy);
        }
        /* release our copy of the backing buffer */
        nitro_counted_buffer_decref(parse_state.cbuf);
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
        s->ident, SOCKET_IDENT_LENGTH);
        hello->type = NITRO_FRAME_HELLO;
        nitro_queue_push(p->q_send, hello);
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

void Stcp_socket_send(nitro_tcp_socket_t *s, nitro_frame_t *fr) {
    nitro_queue_push(s->q_send, nitro_frame_copy(fr));
}

nitro_frame_t *Stcp_socket_recv(nitro_tcp_socket_t *s) {
    return nitro_queue_pull(s->q_recv);
}

int Stcp_socket_reply(nitro_tcp_socket_t *s, nitro_frame_t *snd, nitro_frame_t *fr) {
    int ret = -1;
    pthread_mutex_lock(&s->l_pipes);
    nitro_pipe_t *p = socket_lookup_pipe(
        SOCKET_UNIVERSAL(s), snd->sender);

    if (p) {
        nitro_frame_t *out = nitro_frame_copy(fr);
        nitro_frame_clone_stack(snd, out);
        nitro_queue_push(p->q_send, out);
        ret = 0;
    }

    pthread_mutex_unlock(&s->l_pipes);
    return ret;
}

int Stcp_socket_relay_fw(nitro_tcp_socket_t *s, nitro_frame_t *snd, nitro_frame_t *fr) {
    nitro_frame_t *out = nitro_frame_copy(fr);
    nitro_frame_clone_stack(snd, out);
    nitro_frame_set_sender(out, snd->sender, snd->sender_buffer);
    nitro_frame_stack_push_sender(out);
    nitro_queue_push(s->q_send, out);

    return 0;
}

int Stcp_socket_relay_bk(nitro_tcp_socket_t *s, nitro_frame_t *snd, nitro_frame_t *fr) {
    int ret = -1;
    pthread_mutex_lock(&s->l_pipes);

    assert(snd->num_ident);
    uint8_t *top_of_stack = snd->ident_data + (
        (snd->num_ident - 1) * SOCKET_IDENT_LENGTH);

    nitro_pipe_t *p = socket_lookup_pipe(
        SOCKET_UNIVERSAL(s), top_of_stack);

    if (p) {
        nitro_frame_t *out = nitro_frame_copy(fr);
        nitro_frame_clone_stack(snd, out);
        nitro_frame_stack_pop(out);
        nitro_queue_push(p->q_send, out);
        ret = 0;
    }

    pthread_mutex_unlock(&s->l_pipes);
    return ret;
}
