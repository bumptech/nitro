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
#include "crypto.h"
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

/* Various FW declaration */
void Stcp_socket_disable_reads(nitro_tcp_socket_t *s);
void Stcp_make_pipe(nitro_tcp_socket_t *s, int fd);
void Stcp_destroy_pipe(nitro_pipe_t *p);
void Stcp_socket_start_connect(nitro_tcp_socket_t *s);

/*
 * Stcp_nonblocking_socket_new
 * ---------------------------
 *
 * Create a new IPv4/TCP socket and setup nonblocking I/O
 */
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

/*
 * Stcp_parse_location
 * -------------------
 *
 * Parse the given tcp nitro location of the form "<ip>:port",
 * and set the result in a BSD sockaddr_in.  '*' is supported
 * for "all interfaces" (0.0.0.0)
 */
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

/*
 * Stcp_socket_send_queue_stat
 * ---------------------------
 *
 * Callback from queue library for when the send queue changes state
 * EMPTY|CONTENTS|FULL.  A non-empty send queue on any pipe associated
 * with a socket means we can enable writes for the socket
 */
void Stcp_socket_send_queue_stat(NITRO_QUEUE_STATE st, NITRO_QUEUE_STATE last, void *p) {
    if (last == NITRO_QUEUE_STATE_EMPTY) {
        nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)p;
        nitro_async_t *a = nitro_async_new(NITRO_ASYNC_ENABLE_WRITES);
        a->u.enable_writes.socket = SOCKET_PARENT(s);
        nitro_async_schedule(a);
    }
}

/*
 * Stcp_socket_recv_queue_stat
 * ---------------------------
 *
 * Callback from queue library for when a recv queue changes state
 * EMPTY|CONTENTS|FULL.
 *
 * EMPTY means there is nothing to read, so clear the eventfd if one
 * exists
 *
 * FULL means we need to disable reads from the network.
 *
 * no longer FULL (FULL = last) means we can re-eanble writes
 */
void Stcp_socket_recv_queue_stat(NITRO_QUEUE_STATE st, NITRO_QUEUE_STATE last, void *p) {
    nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)p;
    if (s->opt->want_eventfd && st == NITRO_QUEUE_STATE_EMPTY) {
        /* clear the "has data" bit */
        uint64_t buf;
        int evread = read(s->event_fd, &buf, sizeof(uint64_t));
        assert(evread == sizeof(uint64_t));
    }
    else if (st == NITRO_QUEUE_STATE_FULL) {
        Stcp_socket_disable_reads(s);
    }
    else if (last == NITRO_QUEUE_STATE_FULL) {
        /* async, we're not on nitro thread */
        nitro_async_t *a = nitro_async_new(NITRO_ASYNC_ENABLE_READS);
        a->u.enable_reads.socket = SOCKET_PARENT(s);
        nitro_async_schedule(a);
    }
}

/* Stcp_create_queues
 * ------------------
 *
 * Create the global in/out queues associated with a socket
 */
void Stcp_create_queues(nitro_tcp_socket_t *s) {
    s->q_send = nitro_queue_new(
        s->opt->hwm_out_general, Stcp_socket_send_queue_stat, (void*)s);
    s->q_recv = nitro_queue_new(
        s->opt->hwm_in, Stcp_socket_recv_queue_stat, (void*)s);
}

/* Stcp_socket_connect
 * -------------------
 *
 * Turn a newly-created socket into a TCP/connect socket.
 * In addition to the usual TCP socket init, we need to schedule
 * the connect callback to attempt connections to the remote
 * location.
 */
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

/* Stcp_socket_bind
 * ----------------
 *
 * Turn a newly-created socket into a TCP/bind socket.
 */
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

/*
 * Stcp_socket_start_shutdown
 * --------------------------
 *
 * Schedule a socket shutdown event.  This uses a timer in order to
 * honor "linger" settings.  When the timer expires, the socket will
 * be closed and destroyed.
 */
void Stcp_socket_start_shutdown(nitro_tcp_socket_t *s) {
    ev_timer_init(
        &s->close_timer,
        Stcp_socket_close_cb,
        s->opt->close_linger, 0);
    s->close_timer.data = s;
    ev_timer_start(the_runtime->the_loop, &s->close_timer);
}

/*
 * Stcp_socket_shutdown
 * --------------------
 *
 * Close all pipes and destroy the socket.
 *
 * Typically called as a result of close_timer firing after the
 * "linger" period has elapsed.
 */
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

/*
 * Stcp_socket_close_cb
 * --------------------
 *
 * Callback (shim) for when the linger has expired.
 * Just calls through to Stcp_socket_shutdown()
 */
void Stcp_socket_close_cb(
    struct ev_loop *loop,
    ev_timer *close_timer,
    int revents) {
        nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)close_timer->data;
        Stcp_socket_shutdown(s);
}

/*
 * Stcp_socket_bind_listen
 * -----------------------
 *
 * Enable the libev watcher for read I/O on a bound socket
 * for accept(), which will create a new pipe
 */
void Stcp_socket_bind_listen(nitro_tcp_socket_t *s) {
    NITRO_THREAD_CHECK;
    ev_io_start(the_runtime->the_loop, 
    &s->bound_io);
}

/*
 * Stcp_pipe_send_sub
 * ------------------
 *
 * Send the socket's current list of subscriptions
 * to the other end of this pipe
 */
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

/*
 * Stcp_scan_subs
 * --------------
 *
 * For each pipe that does not have the current version
 * of the sub list, send the current version along.
 */
void Stcp_scan_subs(nitro_tcp_socket_t *s) {
    // NOTE: assumed l_pipes is held
    nitro_pipe_t *p;

    CDL_FOREACH(s->pipes, p) {
        if (p->sub_state_sent != s->sub_keys_state) {
            Stcp_pipe_send_sub(s, p);
        }
    }
}

/*
 * Stcp_socket_check_sub
 * ---------------------
 *
 * Shim callback, fired on a timer, which makes sure all pipes
 * have the correct version of the sub list
 */
void Stcp_socket_check_sub(
    struct ev_loop *loop,
    ev_timer *connect_timer,
    int revents) {
    nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)connect_timer->data;

    pthread_mutex_lock(&s->l_pipes);
    Stcp_scan_subs(s);
    pthread_mutex_unlock(&s->l_pipes);
}

/*
 * Stcp_socket_connect_timer_cb
 * ----------------------------
 *
 * Attempt a connection for a disassociated connect socket.
 * Called by a libev timer.
 */
void Stcp_socket_connect_timer_cb(
    struct ev_loop *loop,
    ev_timer *connect_timer,
    int revents) {
    nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)connect_timer->data;
    ev_timer_stop(the_runtime->the_loop, connect_timer);

    Stcp_socket_start_connect(s);
}

/*
 * Stcp_connect_cb
 * ---------------
 *
 * I/O callback for a socket doing connect() to a remote socket.
 * libev calls this when the socket is writable, and we then
 * check that the socket is, in fact, connected (and the nonblocking
 * connect is done).  If it is, the fd is promoted to a nitro pipe,
 * and the nitro socket leaves the connecting state.
 */
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

/*
 * Stcp_socket_start_connect
 * -------------------------
 *
 * Start attemping a nonblocking connect with a new fd
 * for a nitro socket.  This is invoked by the connect_timer's callback,
 * when the nitro socket is in the disconnected state.
 */
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

/*
 * Stcp_pipe_send_queue_stat
 * -------------------------
 *
 * The queue state change callback for a _direct_ send queue associated
 * with a particular pipe.
 *
 * A move from EMPTY to any non-empty state should trigger the pipe to make
 * sure writing is enabled (and re-enable it if necessary).
 */
void Stcp_pipe_send_queue_stat(NITRO_QUEUE_STATE st, NITRO_QUEUE_STATE last, void *baton) {
    if (last == NITRO_QUEUE_STATE_EMPTY) {
        nitro_pipe_t *p = (nitro_pipe_t *)baton;
        nitro_async_t *a = nitro_async_new(NITRO_ASYNC_ENABLE_WRITES);
        a->u.enable_writes.pipe = p;
        nitro_async_schedule(a);
    }
}

/*
 * Stcp_destroy_pipe
 * -----------------
 *
 * Close a pipe and clean up all resources associated with it.
 */
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

/*
 * Stcp_make_pipe
 * --------------
 *
 * Create a new pipe from a successfully connected fd.
 * This fd could ahve been created via an accept (on
 * bound nitro sockets) or via a connect() (on connected
 * nitro sockets).
 */
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

/*
 * Stcp_pipe_enable_write
 * ----------------------
 *
 * Enable the libev write callback on a particular pipe.
 */
void Stcp_pipe_enable_write(nitro_pipe_t *p) {
    NITRO_THREAD_CHECK;
    ev_io_start(the_runtime->the_loop,
    &p->iow);
}

/*
 * Stcp_socket_enable_writes
 * -------------------------
 *
 * Enable the libev write callbacks on all connected pipes.
 */
void Stcp_socket_enable_writes(nitro_tcp_socket_t *s) {
    NITRO_THREAD_CHECK;
    nitro_pipe_t *p;

    CDL_FOREACH(s->pipes, p) {
        ev_io_start(the_runtime->the_loop,
        &p->iow);
    }
}

/*
 * Stcp_socket_enable_reads
 * ------------------------
 *
 * Enable reads on all connected pipes.
 */
void Stcp_socket_enable_reads(nitro_tcp_socket_t *s) {
    NITRO_THREAD_CHECK;
    nitro_pipe_t *p;

    CDL_FOREACH(s->pipes, p) {
        ev_io_start(the_runtime->the_loop,
        &p->ior);
    }
}

/*
 * Stcp_socket_disable_reads
 * -------------------------
 *
 * Disable reads on all connected pipes.
 *
 * (Done when recv queue is full).
 */
void Stcp_socket_disable_reads(nitro_tcp_socket_t *s) {
    NITRO_THREAD_CHECK;
    nitro_pipe_t *p;

    CDL_FOREACH(s->pipes, p) {
        ev_io_stop(the_runtime->the_loop,
        &p->ior);
    }
}

/*
 * Stcp_socket_close
 * -----------------
 *
 * Schedule a socket close after the linger time for this socket.
 *
 * (PUBLIC API)
 */
void Stcp_socket_close(nitro_tcp_socket_t *s) {
    nitro_async_t *a = nitro_async_new(NITRO_ASYNC_CLOSE);
    a->u.close.socket = SOCKET_PARENT(s);
    nitro_async_schedule(a);
}

/* state used during frame parse callbacks */
typedef struct tcp_frame_parse_state {
    nitro_buffer_t *buf;
    nitro_counted_buffer_t *cbuf;
    char *cursor;
    nitro_pipe_t *p;
    nitro_tcp_socket_t *s;
    int got_data_frames;
    int pipe_error;
} tcp_frame_parse_state;


/*
 * Stcp_socket_parse_subs
 * ----------------------
 *
 * Parse subscription data and save it as the pipe's particular
 * set of subscriptions.  Update the socket trie for routing
 * pub messages to this pipe.
 */
static void Stcp_socket_parse_subs(nitro_tcp_socket_t *s, nitro_pipe_t *p,
    uint64_t state, const uint8_t *data, size_t size, nitro_counted_buffer_t *buf) {

    /* We don't need the lock for this part */
    nitro_key_t *old = p->sub_keys;
    p->sub_keys = NULL;

    /* Build up a list of all the keys that came in on the SUB
       frame from the network */
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

    /* Sort the keys so that we can do a sorted walk to
       find things added and removed vs. last iteration */
    DL_SORT(p->sub_keys, nitro_key_compare);

    pthread_mutex_lock(&s->l_pipes);

    nitro_key_t *walk_old = old, *tmp=NULL, *walk_new = p->sub_keys;

    /* Now, we're going to do a sorted walk to find new
       and old keys that need to be added/removed from the trie */
    while (1) {
        if (walk_old == NULL) {
            if (walk_new == NULL) {
                /* End of both lists, so we're done */
                break;
            }

            /* If the old is done, but not new we can
               assume all remaining new items need to be added */
            nitro_prefix_trie_add(&s->subs,
                walk_new->data, walk_new->length,
                p);

            walk_new = walk_new->next;
        } else if (walk_new == NULL) {
            /* If the old has more, but the new is done, the
               rest of the old are now gone.  Delete 'em */
            nitro_prefix_trie_del(s->subs,
                walk_old->data, walk_old->length,
                p);
            tmp = walk_old;
            walk_old = walk_old->next;
            nitro_key_destroy(tmp);
        }

        else {
            /* They're both still non-null so we ened to figure out
               if they're the same or not. */
            int comp = nitro_key_compare(walk_old, walk_new);
            if (comp < 0) {
                /* New has jumped ahead, old key is missing */
                nitro_prefix_trie_del(s->subs,
                    walk_old->data, walk_old->length,
                    p);

                tmp = walk_old;
                walk_old = walk_old->next;
                nitro_key_destroy(tmp);

            } else if (comp > 0) {
                /* Old has jumped ahead, need to add new key */
                nitro_prefix_trie_add(&s->subs,
                    walk_new->data, walk_new->length,
                    p);

                walk_new = walk_new->next;
            } else {
                /* Key still exists.. move both forward, don't alter trie */
                tmp = walk_old;
                walk_old = walk_old->next;
                nitro_key_destroy(tmp);

                walk_new = walk_new->next;
            }
        }
    }

    pthread_mutex_unlock(&s->l_pipes);

    p->sub_state_recv = state;
}

/*
 * Stcp_parse_next_frame
 * ---------------------
 *
 * This is the callback given to queue_consume().  It is invoked
 * repeatedly until it returns NULL (and does not return a frame).
 * This particular callback is parsing the buffer accumulated from
 * the network and yielding each data frame as it encouters it
 * (as well as processing control frames).  Ergo, it is *the* way
 * messages are received and placed onto the nitro socket's recv()
 * queue.
 *
 * Security note: this is the most dangerous function is nitro, since
 * it directly processes data from the network.  Having it crash safe
 * and buffer overrun safe is important.
 */
static nitro_frame_t *Stcp_parse_next_frame(void *baton) {
    tcp_frame_parse_state *st = (tcp_frame_parse_state *)baton;
    nitro_frame_t *fr = NULL;

    int size;
    nitro_buffer_t const *buf = st->buf;
    char const *start = nitro_buffer_data((nitro_buffer_t *)buf, &size);

    if (!st->cursor) {
        st->cursor = (char *)start;
    }

    /* This loop processes one frame at a time, parsing bytes from
       the network.  It can exit for a few reasons:

       1. It has found a full, whole data frame.  Then fr != NULL,
          and the function will return the data frame.  (Note:
          processing control frames iterates, but does not set fr,
          so control frames do not cause this function to yield a
          frame onto the queue).
       2. A protocol error occurs.  The associated pipe should be
          closed and destroyed for protection from a noncompliant
          client.
       3. Not enough bytes are unconsumed in the network buffer to
          read an entire frame header.  The loop is broken with
          fr = NULL.  This will cause queue_consume() to stop calling.
       4. A frame header was read, but not enough bytes remain
          unconsumed in the network buffer to satisfy the frame size.
          The loop is broken with fr = NULL like case (3).
    */
    while (!fr) {
        char const *cursor = st->cursor;

        size_t taken = cursor - start;
        size_t left = size - taken;

        if (left < sizeof(nitro_protocol_header)) {
            break;
        }

        nitro_protocol_header const *hd =
            (nitro_protocol_header *)cursor;
        left -= sizeof(nitro_protocol_header);

        if (hd->frame_size > st->s->opt->max_message_size) {
            nitro_set_error(NITRO_ERR_MAX_FRAME_EXCEEDED);
            st->pipe_error = 1;
            return NULL;
        }

        if (hd->protocol_version != 1) {
            nitro_set_error(NITRO_ERR_BAD_PROTOCOL_VERSION);
            st->pipe_error = 1;
            return NULL;
        }

        size_t ident_size = hd->num_ident * SOCKET_IDENT_LENGTH;
        if (left < hd->frame_size + ident_size) {
            break;
        }

        /* "Parsing" protocol header */
        const nitro_protocol_header *phd = hd;
        const uint8_t *frame_data = (uint8_t *)cursor + sizeof(nitro_protocol_header);
        nitro_counted_buffer_t **bbuf_p = &(st->cbuf);
        nitro_counted_buffer_t *bbuf = NULL;

        /* First order, unwrap secure */
        if (hd->packet_type == NITRO_FRAME_SECURE) {
            if (!st->s->opt->secure) {
                nitro_set_error(NITRO_ERR_BAD_SECURE);
                st->pipe_error = 1;
                return NULL;
            }
            size_t final_size;
            uint8_t *clear = crypto_decrypt_frame(
                frame_data, hd->frame_size, st->p, &final_size, &bbuf);
            if (!clear) {
                assert(nitro_has_error());
                st->pipe_error = 1;
                return NULL;
            }
            bbuf_p = &bbuf;
            phd = (nitro_protocol_header *)clear;
            frame_data = clear + sizeof(nitro_protocol_header);
        } else if (st->s->opt->secure && hd->packet_type != NITRO_FRAME_HELLO) {
            nitro_set_error(NITRO_ERR_INVALID_CLEAR);
            st->pipe_error = 1;
            return NULL;
        }

        if (phd->packet_type != NITRO_FRAME_DATA) {
            /* It's a control frame; handle appropriately */
            if (phd->packet_type == NITRO_FRAME_HELLO) {
                if (st->p->them_handshake) {
                    nitro_set_error(NITRO_ERR_DOUBLE_HANDSHAKE);
                    st->pipe_error = 1;
                    return NULL;
                }
                if (phd->frame_size != SOCKET_IDENT_LENGTH) {
                    nitro_set_error(NITRO_ERR_DOUBLE_HANDSHAKE);
                    st->pipe_error = 1;
                    return NULL;
                }

                /* Check the remote identity if config has one */
                if (st->s->opt->has_remote_ident && 
                    memcmp(st->s->opt->required_remote_ident, frame_data,
                        SOCKET_IDENT_LENGTH) != 0) {

                    nitro_set_error(NITRO_ERR_INVALID_CERT);
                    st->pipe_error = 1;
                    return NULL;
                }

                /* Copy the identity of the remote socket */
                st->p->remote_ident = malloc(SOCKET_IDENT_LENGTH);
                memcpy(st->p->remote_ident,
                    frame_data,
                    SOCKET_IDENT_LENGTH);
                st->p->remote_ident_buf = nitro_counted_buffer_new(
                    st->p->remote_ident, just_free, NULL);

                /* Register the pipe id into the routing table (for replies and pub) */
                socket_register_pipe(SOCKET_UNIVERSAL(st->s), st->p);

                /* If this is a secure socket, cache crypto information associated
                   with the ident (which is actually an NaCl public key */
                if (st->s->opt->secure) {
                    crypto_make_pipe_cache(st->s, st->p);
                    Stcp_pipe_enable_write(st->p);
                }
                pthread_mutex_lock(&st->s->l_pipes);
                if (st->s->sub_data) {
                    Stcp_pipe_send_sub(st->s, st->p);
                }
                pthread_mutex_unlock(&st->s->l_pipes);

                /* Mark the handshake done */
                st->p->them_handshake = 1;

            } else if (phd->packet_type == NITRO_FRAME_SUB) {
                if (!st->p->them_handshake) {
                    nitro_set_error(NITRO_ERR_NO_HANDSHAKE);
                    st->pipe_error = 1;
                    return NULL;
                }

                if (phd->frame_size < sizeof(uint64_t)) {
                    nitro_set_error(NITRO_ERR_BAD_SUB);
                    st->pipe_error = 1;
                    return NULL;
                }
                uint64_t state = *(uint64_t *)frame_data;

                /* Check the state counter to see if we've already processed this
                   version of the pipe's sub state */
                if (st->p->sub_state_recv != state) {
                    if (*bbuf_p) {
                        /* we need to retain the backing buffer for the sub data */
                        *bbuf_p = nitro_counted_buffer_new(
                            NULL, buffer_free, (void *)buf);
                    } else {fprintf(stderr, "nope, got it already\n");}
                    /* Parse the sub data from the socket and update the socket
                       sub trie as appropriate */
                    Stcp_socket_parse_subs(st->s, st->p, state,
                        frame_data + sizeof(uint64_t),
                        phd->frame_size - sizeof(uint64_t), *bbuf_p);
                }
            }
        } else {
            /* Data frame.  This is meat and potatos user data.
               Parse it and return it so that queue_consume() adds
               it to the end of the queue */

            /* NOTE: This is an especially busy corner of a nitro socket */
            if (!st->p->them_handshake) {
                nitro_set_error(NITRO_ERR_NO_HANDSHAKE);
                st->pipe_error = 1;
                return NULL;
            }
            if (!*bbuf_p) {
                /* it is official, we will consume data... */

                /* first incref is for the socket itself, not done copying yet */
                *bbuf_p = nitro_counted_buffer_new(
                NULL, buffer_free, (void *)buf);
            }
            nitro_counted_buffer_t const *cbuf = *bbuf_p;

            /* Incref for the eventual recipient */
            nitro_counted_buffer_incref((nitro_counted_buffer_t *)cbuf);

            fr = nitro_frame_new_prealloc(
                    (char *)frame_data,
                    phd->frame_size, (nitro_counted_buffer_t *)cbuf);
            nitro_frame_set_sender(fr,
                st->p->remote_ident, st->p->remote_ident_buf);

            /* If this has a ident stack that's been routed, copy/retain it */
            if (phd->num_ident) {
                nitro_frame_set_stack(fr,
                    frame_data + phd->frame_size,
                    (nitro_counted_buffer_t *)cbuf, phd->num_ident);
            }
        }
        if (bbuf) {
            nitro_counted_buffer_decref(bbuf);
        }
        /* Increment cursor using original frame information */
        st->cursor += (sizeof(nitro_protocol_header) + hd->frame_size + ident_size);
    }

    if (fr) {
        st->got_data_frames = 1;
    }
    return fr;
}

/*
 * Stcp_parse_socket_buffer
 * ------------------------
 *
 * After having received a chunk of data from the network, attempt to
 * split it up into frames.
 *
 * If this function finds any whole frames it will retain the buffer as
 * a zero-copy refcounted backing buffer for the frame data, and it creates
 * an new empty buffer for the next recv from the network.
 *
 * If it does not find any whole frames, it keeps the network buffer for appending
 * more data.
 */
void Stcp_parse_socket_buffer(nitro_pipe_t *p) {
    /* now we parse */
    nitro_tcp_socket_t *s = (nitro_tcp_socket_t *)p->the_socket;

    tcp_frame_parse_state parse_state = {0};
    parse_state.buf = p->in_buffer;
    parse_state.p = p;
    parse_state.s = s;

    nitro_clear_error();
    nitro_queue_consume(s->q_recv,
        Stcp_parse_next_frame,
        &parse_state);

    if (parse_state.pipe_error) {
        if (s->opt->error_handler) {
            s->opt->error_handler(nitro_error(),
                s->opt->error_handler_baton);
        }
        Stcp_destroy_pipe(p);
        return;
    }

    int size;
    char *start = nitro_buffer_data(p->in_buffer, &size);
    if (parse_state.cursor != start) {
        /* If this buffer contained at least one whole frame,
        we're going to disown it and create another one
        with whatever fractional data remain (if any) */

        /* If we got some data frames, and we're using an eventfd
           for embedding, make sure it gets triggered as readable */
        if (parse_state.got_data_frames && s->opt->want_eventfd) {
            uint64_t inc = 1;
            int evwrote = write(s->event_fd, (char *)(&inc), sizeof(inc));
            assert(evwrote == sizeof(inc));
        }

        int to_copy = size - (parse_state.cursor - start);
        nitro_buffer_t *tmp = p->in_buffer;
        p->in_buffer = nitro_buffer_new();

        if (to_copy) {
            int writable = TCP_INBUF > to_copy ? TCP_INBUF : to_copy;
            char *write = nitro_buffer_prepare(p->in_buffer, &writable);

            memcpy(write, parse_state.cursor, to_copy);
            nitro_buffer_extend(p->in_buffer, to_copy);
        }
        /* Release our copy of the backing buffer */
        if (parse_state.cbuf) {
            nitro_counted_buffer_decref(parse_state.cbuf);
        }
        else {
            nitro_buffer_destroy(tmp);
        }
    }
}

/*
 * Stcp_bind_callback
 * ------------------
 *
 * Bound fd is readable on a bound nitro socket.  Time to
 * accept() a fd and set up a new pipe.
 */
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

/*
 * Stcp_encrypt_frame
 * ------------------
 *
 * Encrypt a frame using the pipe's nacl cache.
 *
 * (Callback for nitro_queue_fd_write_encrypted())
 */
nitro_frame_t *Stcp_encrypt_frame(nitro_frame_t *fr, void *baton) {
    nitro_pipe_t *p = (nitro_pipe_t *)baton;
    nitro_frame_t *out = crypto_frame_encrypt(fr, p);

    if (!out) {
        nitro_set_error(NITRO_ERR_ENCRYPT);
    }
    return out;
}

/*
 * Stcp_pipe_out_cb
 * ----------------
 *
 * Called when libev says a pipe fd is writable.  If we have
 * any frames ready in either the direct queue or the common
 * queue, try to send 'em.
 *
 * If we have no data waiting anywhere, disable writes on this
 * fd until things are re-enabled via a queue append.
 */
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
    int doing_hello = 0;

    if (!p->us_handshake) {
        tried = 1;
        assert(nitro_queue_count(p->q_send) == 0);
        nitro_frame_t *hello = nitro_frame_new_copy(
        s->opt->ident, SOCKET_IDENT_LENGTH);
        hello->type = NITRO_FRAME_HELLO;
        r = nitro_queue_push(p->q_send, hello, 0);

        /* Cannot get hello out because q_send full?? not okay */
        assert(!r);

        p->us_handshake = 1;
        doing_hello = 1;
    }

    /* Local queue takes precedence */
    if (nitro_queue_count(p->q_send)) {
        tried = 1;
        if (s->opt->secure && !doing_hello) {
            r = nitro_queue_fd_write_encrypted(
                p->q_send,
                p->fd, p->partial, &(p->partial),
                Stcp_encrypt_frame, p);

        } else {
            r = nitro_queue_fd_write(
                p->q_send,
                p->fd, p->partial, &(p->partial));
        }
        if (r < 0 && OKAY_ERRNO)
            return;
        if (r < 0) {
            if (s->opt->error_handler) {
                s->opt->error_handler(nitro_error(),
                    s->opt->error_handler_baton);
            }
            Stcp_destroy_pipe(p);
            return;
        }
        if (doing_hello && s->opt->secure) {
            ev_io_stop(the_runtime->the_loop,
            pipe_iow);
            return;
        }
    }

    /* Note -- using truncate frame as heuristic
       to guess kernel buffer is full*/
    if ((!tried || !p->partial) && nitro_queue_count(s->q_send)) {
        tried = 1;
        if (s->opt->secure) {
            r = nitro_queue_fd_write_encrypted(
                s->q_send,
                p->fd, p->partial, &(p->partial),
                Stcp_encrypt_frame, p);

        } else {
            r = nitro_queue_fd_write(
                s->q_send,
                p->fd, p->partial, &(p->partial));
        }
        if (r < 0 && OKAY_ERRNO)
            return;
        if (r < 0) {
            if (s->opt->error_handler) {
                s->opt->error_handler(nitro_error(),
                    s->opt->error_handler_baton);
            }
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

/*
 * Stcp_pipe_in_cb
 * ---------------
 *
 * A pipe fd has some data ready for reading.
 *
 * Read it, then call the frame parsing functions on
 * the accumulated buffer.
 */
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

/*
 * Stcp_socket_send
 * ----------------
 *
 * Send frame `fr` on socket `s`.  Effectively, this pushes the frame
 * onto the common queue for the next pipe that comes up write ready.
 *
 * (PUBLIC API)
 */
int Stcp_socket_send(nitro_tcp_socket_t *s, nitro_frame_t **frp, int flags) {
    nitro_frame_t *fr = *frp;
    if (flags & NITRO_REUSE) {
        nitro_frame_incref(fr);
    } else {
        *frp = NULL;
    }

    int r = nitro_queue_push(s->q_send, fr, !(flags & NITRO_NOWAIT));
    if (r) {
        nitro_frame_destroy(fr);
    }
    return r;
}

/*
 * Stcp_socket_recv
 * ----------------
 *
 * Receive a frame on the socket if one has been queued from the
 * network.
 *
 * (PUBLIC API)
 */
nitro_frame_t *Stcp_socket_recv(nitro_tcp_socket_t *s, int flags) {
    return nitro_queue_pull(s->q_recv, !(flags & NITRO_NOWAIT));
}

/*
 * Stcp_socket_reply
 * -----------------
 *
 * Reply to frame `snd` with frame `fr` on socket `s`.
 *
 * Internally, this means look up a pipe in the routing
 * table that sent frame `snd`, and put frame `fr` on its
 * private outbound queue.
 */
int Stcp_socket_reply(nitro_tcp_socket_t *s,
    nitro_frame_t *snd, nitro_frame_t **frp, int flags) {
    assert(!(flags & NITRO_REUSE));
    nitro_frame_t *fr = *frp;
    *frp = NULL;
    
    int ret = -1;
    pthread_mutex_lock(&s->l_pipes);
    nitro_pipe_t *p = socket_lookup_pipe(
        SOCKET_UNIVERSAL(s), snd->sender);

    if (p) {
        nitro_frame_clone_stack(snd, fr);
        ret = nitro_queue_push(p->q_send, fr, !(flags & NITRO_NOWAIT));
    } else {
        nitro_set_error(NITRO_ERR_NO_RECIPIENT);
    }

    pthread_mutex_unlock(&s->l_pipes);

    if (ret) {
        nitro_frame_destroy(fr);
    }

    return ret;
}

/*
 * Stcp_socket_relay_fw
 * --------------------
 *
 * Relay a packet to another node.
 *
 * This is like a normal send, routing stack from `snd`
 * is retained so that the packet can find its way back
 * to the origin through all intermediate hops (if necessary).
 */
int Stcp_socket_relay_fw(nitro_tcp_socket_t *s, 
    nitro_frame_t *snd, nitro_frame_t **frp, int flags) {
    assert(!(flags & NITRO_REUSE));
    nitro_frame_t *fr = *frp;
    *frp = NULL;

    nitro_frame_clone_stack(snd, fr);
    nitro_frame_set_sender(fr, snd->sender, snd->sender_buffer);
    nitro_frame_stack_push_sender(fr);
    int r = nitro_queue_push(s->q_send, fr, !(flags & NITRO_NOWAIT));
    if (r) {
        nitro_frame_destroy(fr);
    }

    return r;
}

/*
 * Stcp_socket_relay_bk
 * --------------------
 *
 * Relay a packet back to a node which is earlier in
 * the routing stack.
 *
 * This is like relay_fw, but instead of sending a packet
 * _deeper_ and increasing the routing stack, it is relaying
 * a reply _shallower_ and decreasing the routing stack so
 * the reply finds the original sender() (even over several hops).
 */
int Stcp_socket_relay_bk(nitro_tcp_socket_t *s, 
    nitro_frame_t *snd, nitro_frame_t **frp, int flags) {
    assert(!(flags & NITRO_REUSE));
    nitro_frame_t *fr = *frp;
    *frp = NULL;

    int ret = -1;
    pthread_mutex_lock(&s->l_pipes);

    if (!snd->num_ident) {
        nitro_set_error(NITRO_ERR_NO_RECIPIENT);
    } else {
        uint8_t *top_of_stack = snd->ident_data + (
            (snd->num_ident - 1) * SOCKET_IDENT_LENGTH);

        nitro_pipe_t *p = socket_lookup_pipe(
            SOCKET_UNIVERSAL(s), top_of_stack);

        if (p) {
            nitro_frame_clone_stack(snd, fr);
            nitro_frame_stack_pop(fr);
            ret = nitro_queue_push(p->q_send, fr, !(flags & NITRO_NOWAIT));
        } else {
            nitro_set_error(NITRO_ERR_NO_RECIPIENT);
        }
    }
    pthread_mutex_unlock(&s->l_pipes);

    if (ret) {
        nitro_frame_destroy(fr);
    }
    return ret;
}

/*
 * Stcp_socket_reindex_subs
 * ------------------------
 *
 * When a new subscription is added to this socket, re-build the pre-calculated
 * outgoing SUB frame for sending to all conected peers.
 */
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

/*
 * Stcp_socket_sub
 * ---------------
 *
 * Add a new subscription to this socket's subscription list.
 *
 * This subscription list will be relayed to the peer socket so
 * that pub() messages sent on that socket that match the subscription
 * patterns will find their way back.
 *
 * (PUBLIC API)
 */
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

/*
 * Stcp_socket_unsub
 *
 * Remove a subscription from the socket's subscription list.
 *
 * (see Stcp_socket_sub for more info about the significance of this
 * list.
 *
 * (PUBLIC API)
 */
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

/* State during trie walk for socket delivery */
typedef struct Stcp_pub_state {
    int count;
    nitro_frame_t *fr;
} Stcp_pub_state;

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
static void Stcp_deliver_pub_frame(
    const uint8_t *pfx, uint8_t length, nitro_prefix_trie_mem *mems,
    void *ptr) {

    Stcp_pub_state *st = (Stcp_pub_state *)ptr;

    nitro_prefix_trie_mem *m;

    DL_FOREACH(mems, m) {
        nitro_pipe_t *p = (nitro_pipe_t *)m->ptr;

        nitro_frame_t *fr = st->fr;
        nitro_frame_incref(fr);

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

/*
 * Stcp_socket_pub
 * ---------------
 *
 * Use the socket sub trie to find all prefixes that
 * are matching subscriptions to key `k`.  Deliver the
 * frame `fr` to each of them.
 *
 * (PUBLIC API)
 */
int Stcp_socket_pub(nitro_tcp_socket_t *s,
    nitro_frame_t **frp, const uint8_t *k,
    size_t length, int flags) {

    nitro_frame_t *fr = *frp;
    if (flags & NITRO_REUSE) {
        nitro_frame_incref(fr);
    } else {
        *frp = NULL;
    }

    Stcp_pub_state st = {0};

    st.fr = fr;

    pthread_mutex_lock(&s->l_pipes);

    nitro_prefix_trie_search(s->subs,
        k, length, Stcp_deliver_pub_frame, &st);

    pthread_mutex_unlock(&s->l_pipes);

    nitro_frame_destroy(fr);

    return st.count;
}
