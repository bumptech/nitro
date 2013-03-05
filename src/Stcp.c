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
    int revents) ;

static int Stcp_nonblocking_socket_new() {
    int s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(s >= 0);
    int flag = 1;
    int r = ioctl(s, FIONBIO, &flag);
    assert(r == 0);

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

    printf("location is: %s\n", location);

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

int Stcp_socket_connect(nitro_tcp_socket_t *s, char *location) {
    int r = Stcp_parse_location(location, &s->location);

    if (r) {
        /* Note - error detail set by parse_tcp_location */
        return r;
    }

    // XXX start async connect
    return 0;
}

int Stcp_socket_bind(nitro_tcp_socket_t *s, char *location) {
    int r = Stcp_parse_location(location, &s->location);

    if (r) {
        return r;
    }

    s->bound_fd = Stcp_nonblocking_socket_new();

    int t = 1;
    setsockopt(s->bound_fd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(int));
#ifdef SO_REUSEPORT
    setsockopt(s->bound_fd, SOL_SOCKET, SO_REUSEPORT, &t, sizeof(int));
#endif
    if (bind(s->bound_fd, 
        (struct sockaddr *)&s->location, 
        sizeof(s->location))){
        printf("bind error!\n");
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

void Stcp_socket_bind_listen(nitro_tcp_socket_t *s) {
    NITRO_THREAD_CHECK;
    ev_io_start(the_runtime->the_loop, 
    &s->bound_io);
}

void Stcp_pipe_send_queue(NITRO_QUEUE_STATE st, void *baton) {
    nitro_pipe_t *p = (nitro_pipe_t *)baton;

    if (st == NITRO_QUEUE_STATE_EMPTY) {
        ev_io_stop(the_runtime->the_loop,
        &p->iow);
    }
    else {
        ev_io_start(the_runtime->the_loop,
        &p->iow);
    }
}

void Stcp_make_pipe(nitro_tcp_socket_t *s, int fd) {
    nitro_pipe_t *p = nitro_pipe_new();
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
    Stcp_pipe_send_queue, p);

    socket_feed_pipe(SOCKET_UNIVERSAL(s), p);
}

typedef struct tcp_frame_parse_state {
    nitro_buffer_t *buf;
    nitro_counted_buffer_t *cbuf;
    char *cursor;
} tcp_frame_parse_state;

static nitro_frame_t *Stcp_parse_next_frame(void *baton) {
    tcp_frame_parse_state *st = (tcp_frame_parse_state *)baton;

    int size;
    char *start = nitro_buffer_data(st->buf, &size);

    if (!st->cursor) {
        st->cursor = start;
    }

    int taken = st->cursor - start;
    int left = size - taken;

    if (left < sizeof(nitro_protocol_header)) {
        return NULL;
    }

    nitro_protocol_header *hd = 
    (nitro_protocol_header *)st->cursor;
    left -= sizeof(nitro_protocol_header);

    // XXX error on protocol version
    // or other formatting
    // XXX this is BAD, crashy!
    // XXX maximum packet size, config based?
    // other safety/security/anti-dos audit
    // ... probably the socket closes on
    // rule violations
    assert(hd->protocol_version == 1);
    assert(hd->packet_type == NITRO_PACKET_FRAME);

    if (left < hd->frame_size) {
        return NULL;
    }

    if (!st->cbuf) {
        /* it is official, we will consume data... */
        st->cbuf = nitro_counted_buffer_new(
        st->buf, buffer_free, NULL);
    }
    else {
        nitro_counted_buffer_incref(st->cbuf);
    }

    nitro_frame_t *fr = 
        nitro_frame_new(
            st->cursor + sizeof(nitro_protocol_header),
            hd->frame_size,
            cbuffer_decref, st->cbuf);

    st->cursor += (sizeof(nitro_protocol_header) + hd->frame_size);
    return fr;
}

void Stcp_parse_socket_buffer(nitro_pipe_t *p) {
    /* now we parse */
    tcp_frame_parse_state parse_state = {0};
    parse_state.buf = p->in_buffer;

    nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)p->the_socket;

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
    }
}


/* LIBEV CALLBACKS */

void Stcp_bind_callback(
    struct ev_loop *loop,
    ev_io *bind_io,
    int revents) 
{
    NITRO_THREAD_CHECK;
    printf("should accept!\n");
    nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)bind_io->data;

    struct sockaddr addr;
    socklen_t len;

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
}

void Stcp_pipe_out_cb(
    struct ev_loop *loop,
    ev_io *pipe_iow,
    int revents) 
{
    NITRO_THREAD_CHECK;
    printf("try write!\n");
    nitro_pipe_t *p = (nitro_pipe_t *)pipe_iow->data;

    int r = nitro_queue_socket_write(
        p->q_send,
        p->fd);
    /* handle errno on socket send */
    (void)r;
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

    if (r <= 0) {
        /* okay, let's handle disconnection or errno etc */
        // XXX handle socket errors, shutdown, do whatever.

        assert(0); /* OH NOES */
    }

    nitro_buffer_extend(p->in_buffer, r);

    Stcp_parse_socket_buffer(p);

}
