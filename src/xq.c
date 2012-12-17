#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xq.h"
#include "uthash/utlist.h"

static uv_loop_t *the_loop;
static uv_timer_t connect_timer;
static uv_async_t connect_trigger;
static pthread_t the_thread;
static int run_started;
static xq_socket_t *want_tcp_connect;
static pthread_mutex_t l_tcp_connect;

static xq_socket_t *bound_inproc_socks;


static void destroy_tcp_pipe(xq_pipe_t *p);

typedef struct xq_protocol_header {
    char protocol_version;
    uint32_t frame_size;
} xq_protocol_header;

typedef struct xq_counted_buffer {
    void *ptr;
    int count;
    pthread_mutex_t lock;
} xq_counted_buffer;

#define TCP_PREFIX "tcp://"
#define INPROC_PREFIX "inproc://"

static void fatal(char *why) {
    fprintf(stderr, "fatal error: %s\n", why);
}

static void poll_timers(uv_timer_t *handle, int status);

static char * parse_location(char *location, XQ_SOCKET_TRANSPORT *trans) {
    if (!strncmp(location, TCP_PREFIX, strlen(TCP_PREFIX))) {
        *trans = XQ_SOCKET_TCP;
        return location + strlen(TCP_PREFIX);
    }
    if (!strncmp(location, INPROC_PREFIX, strlen(TCP_PREFIX))) {
        *trans = XQ_SOCKET_INPROC;
        return location + strlen(INPROC_PREFIX);
    }

    fatal("invalid transport type for socket");

    return NULL;
}

xq_socket_t * xq_socket_new(XQ_SOCKET_TYPE socktype) {
    xq_socket_t *sock = calloc(1, sizeof(xq_socket_t));

    sock->typ = socktype;
    sock->trans = XQ_SOCKET_NONE;
    pthread_mutex_init(&sock->l_recv, NULL);
    pthread_mutex_init(&sock->l_send, NULL);

    pthread_cond_init(&sock->c_recv, NULL);
    pthread_cond_init(&sock->c_send, NULL);

    return sock;
}

static uv_buf_t pipe_allocate(uv_handle_t *handle, size_t suggested_size) {
    xq_pipe_t *p = (xq_pipe_t *)handle->data;
    size_t avail = p->buf_alloc - p->buf_bytes;
    if (suggested_size > ((double)avail * 1.2)) {
        p->buf_alloc = suggested_size + p->buf_bytes;
        p->buffer = realloc(p->buffer, p->buf_alloc);
        avail = p->buf_alloc - p->buf_bytes;
    }
    printf("avail is: %zu at %p\n", avail, p->buffer);
    return uv_buf_init((char *)p->buffer + p->buf_bytes, avail);
}

typedef struct tcp_write_request {
    xq_protocol_header header;
    xq_frame_t *frame;
    xq_socket_t *socket;
} tcp_write_request;

static void socket_flush(xq_socket_t *s);

static void tcp_write_finished(uv_write_t *w, int status) {
    // for now, we're ignoring status
    tcp_write_request *req = (tcp_write_request *)w->data;
    xq_socket_t *s = req->socket;
    if (status>=0) {
        xq_frame_destroy(req->frame);
        pthread_mutex_lock(&s->l_send);
        req->socket->count_send--;
        if (req->socket->capacity && req->socket->count_send ==
        (req->socket->capacity - 1))
            pthread_cond_signal(&req->socket->c_send);
        pthread_mutex_unlock(&s->l_send);
    }
    else {
        pthread_mutex_lock(&s->l_send);
        DL_PREPEND(req->socket->q_send, req->frame);
        pthread_mutex_unlock(&s->l_send);
        socket_flush(req->socket);
    }
    free(req);
    free(w);
}

void inproc_write(xq_pipe_t *p, xq_frame_t *f) {
    xq_socket_t *s = (xq_socket_t *)p->dest_socket;

    xq_frame_retain(f);
    pthread_mutex_lock(&s->l_recv);
    DL_APPEND(s->q_recv, f);
    pthread_cond_signal(&s->c_recv);
    s->count_recv++;
    pthread_mutex_unlock(&s->l_recv);
}

void tcp_write(xq_pipe_t *p, xq_frame_t *f) {
    xq_frame_retain(f);
    tcp_write_request *req = calloc(1, sizeof(tcp_write_request));
    uv_write_t *w = calloc(1, sizeof(uv_write_t));
    w->data = req;
    req->header.protocol_version = 1;
    req->header.frame_size = f->size;
    req->frame = f;
    req->socket = p->the_socket;

    uv_buf_t out[] = {
        { .base = (void *)&req->header, .len = sizeof(xq_protocol_header)},
        { .base = (void *)f->data, .len = f->size}
    };

    uv_write(w, (uv_stream_t *)p->tcp_socket,
    out, 2, tcp_write_finished);
}

static void socket_flush(xq_socket_t *s) {
    pthread_mutex_lock(&s->l_send);
    while (1) {
        if (!s->q_send || !s->next_pipe)
            break;

        xq_pipe_t *p = s->next_pipe;
        xq_frame_t *f = s->q_send;

        DL_DELETE(s->q_send, f);

        p->do_write(p, f);
        xq_frame_destroy(f);

        s->next_pipe = s->next_pipe->next;
    }
    pthread_mutex_unlock(&s->l_send);
}


xq_frame_t *xq_frame_new(void *data, uint32_t size, xq_free_function ff, void *baton) {
    xq_frame_t *f = malloc(sizeof(xq_frame_t));

    f->data = data;
    f->size = size;
    f->ff = ff;
    f->baton = baton;
    f->prev = f->next = NULL;
    pthread_mutex_init(&f->lock, NULL);
    f->ref_count = 1;
    return f;
}

void xq_frame_retain(xq_frame_t *f) {
    pthread_mutex_lock(&f->lock);
    f->ref_count++;
    pthread_mutex_unlock(&f->lock);
}

void xq_frame_destroy(xq_frame_t *f) {
    // This decrements the ref_count and frees if necessary
    pthread_mutex_lock(&f->lock);
    f->ref_count--;
    pthread_mutex_unlock(&f->lock);

    assert(f->ref_count >= 0);
    if (f->ref_count == 0)
    {
        f->ff(f->data, f->baton);
        free(f);
    }
}

static void just_free(void *data, void *unused) {
    free(data);
}

xq_frame_t * xq_frame_new_copy(void *data, uint32_t size) {
    char *n = malloc(size);
    memmove(n, data, size);
    return xq_frame_new(n, size, just_free, NULL);
}

static void buffer_decref(void *data, void *bufptr) {
    xq_counted_buffer *buf = (xq_counted_buffer *)bufptr;

    pthread_mutex_lock(&buf->lock);
    buf->count--;
    if (!buf->count) {
        printf("free!\n");
        free(buf->ptr);
        free(buf);
    }
    else
        pthread_mutex_unlock(&buf->lock);
}

static void destroy_pipe(xq_pipe_t *p) {
    xq_socket_t *s = (xq_socket_t *)p->the_socket;

    if (p->next == p) {
        s->next_pipe = NULL;
    }
    else {
        s->next_pipe = p->next;
    }

    CDL_DELETE(s->pipes, p);
    if (p->registered)
        HASH_DEL(s->pipes_by_session, p);
    free(p->buffer);
    free(p);
}

static void destroy_tcp_pipe(xq_pipe_t *p) {
    uv_close((uv_handle_t *)p->tcp_socket, NULL);
    destroy_pipe(p);
}


static void on_tcp_read(uv_stream_t *peer, ssize_t nread, uv_buf_t unused) {
    xq_pipe_t *p = (xq_pipe_t *)peer->data;
    xq_socket_t *s = (xq_socket_t *)p->the_socket;

    if (nread == -1) {
        p->destroy(p);
        if (s->reconnect) {
            uv_tcp_init(the_loop, &s->tcp_socket);
            s->is_connecting = 0;
            pthread_mutex_lock(&l_tcp_connect);
            DL_APPEND(want_tcp_connect, s);
            pthread_mutex_unlock(&l_tcp_connect);
            poll_timers(NULL, 0);
        }

        return;
    }

    if (nread == 0) {
        assert(0); // wha?
    }

    p->buf_bytes += nread;

    if (p->buf_bytes < sizeof(xq_protocol_header))
        return;

    xq_counted_buffer *buf = NULL;
    uint8_t *region = p->buffer;
    xq_protocol_header *header = (xq_protocol_header *)region;
    uint32_t current_frame_size = header->frame_size;
    uint32_t whole_size = sizeof(xq_protocol_header) + current_frame_size;
    while (p->buf_bytes >= whole_size) {
        assert(header->protocol_version == 1);
        /* we have the whole frame! */
        if (!buf) {
            buf = malloc(sizeof(xq_counted_buffer));
            buf->ptr = p->buffer;
            buf->count = 1;
            pthread_mutex_init(&buf->lock, NULL);
        }
        pthread_mutex_lock(&buf->lock);
        buf->count++;
        pthread_mutex_unlock(&buf->lock);

        xq_frame_t *fr = xq_frame_new(region + sizeof(xq_protocol_header),
        current_frame_size, buffer_decref, buf);
        pthread_mutex_lock(&s->l_recv);
        DL_APPEND(s->q_recv, fr);
        pthread_cond_signal(&s->c_recv);
        s->count_recv++;
        pthread_mutex_unlock(&s->l_recv);

        p->buf_bytes -= whole_size;
        if (p->buf_bytes < sizeof(xq_protocol_header))
            break;
        region += whole_size;
        header = (xq_protocol_header *)region;
        current_frame_size = header->frame_size;
        whole_size = sizeof(xq_protocol_header) + current_frame_size;
    }

    if (buf) {
        pthread_mutex_lock(&buf->lock);
        buf->count--;
        pthread_mutex_unlock(&buf->lock);
        if (p->buf_bytes) {
            p->buf_alloc = (uint32_t)(whole_size * 1.2);
            p->buffer = malloc(p->buf_alloc);
            memmove(p->buffer, region, p->buf_bytes);
        }
        else {
            p->buf_alloc = 0;
            p->buffer = NULL;
        }
    }
}

xq_frame_t * xq_frame_recv(xq_socket_t *s) {
    xq_frame_t *out = NULL;
    pthread_mutex_lock(&s->l_recv);

    while (!s->q_recv)
        pthread_cond_wait(&s->c_recv, &s->l_recv);
    out = s->q_recv;
    DL_DELETE(s->q_recv, out);
    s->count_recv--;

    pthread_mutex_unlock(&s->l_recv);
    assert(out); // why triggered with no frame?

    return out;
}

static xq_pipe_t *new_inproc_pipe(xq_socket_t *orig_socket, xq_socket_t *dest_socket) {
    xq_pipe_t *p = calloc(1, sizeof(xq_pipe_t));
    p->the_socket = (void *)orig_socket;
    p->dest_socket = (void *) dest_socket;
    p->do_write = &inproc_write;
    p->destroy = &destroy_pipe;

    return p;
}


static xq_pipe_t *new_tcp_pipe(xq_socket_t *s, uv_tcp_t *tcp_socket) {
    xq_pipe_t *p = calloc(1, sizeof(xq_pipe_t));
    p->tcp_socket = tcp_socket;
    p->the_socket = (void *)s;
    p->do_write = &tcp_write;
    p->destroy = &destroy_tcp_pipe;

    return p;
}

static void on_tcp_connection(uv_stream_t *peer, int status) {
    if (status == -1) {
        // error!
        return;
    }

    xq_socket_t *s = (xq_socket_t *)peer->data;
    uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
    uv_tcp_init(the_loop, client);
    if (uv_accept(peer, (uv_stream_t*) client) == 0) {
        xq_pipe_t *pipe = new_tcp_pipe(s, client);
        CDL_PREPEND(s->pipes, pipe);
        if (!s->next_pipe) {
            s->next_pipe = s->pipes;
            socket_flush(s); // if there's anything waiting, give it to this guy
        }
        client->data = pipe;
        uv_read_start((uv_stream_t*) client, pipe_allocate, on_tcp_read);
    }
    else {
        uv_close((uv_handle_t*) client, NULL);
    }
}


static void tcp_connect_cb(uv_async_t *handle, int status) {
    poll_timers(NULL, 0);
}

static void tcp_flush_cb(uv_async_t *handle, int status) {
    xq_socket_t *s = (xq_socket_t *)handle->data;
    socket_flush(s);
}

static struct sockaddr_in parse_tcp_location(char *p_location) {
    char *location = alloca(strlen(p_location)+1);
    strcpy(location, p_location);

    char *split = strchr(location, ':');

    if (!split) {
        fatal("tcp bind location must contain a ':'");
    }

    *split = 0;

    errno = 0;
    int port = strtol(split + 1, NULL, 10);
    if (errno) {
        fatal("tcp bind port is not an integer");
    }

    return uv_ip4_addr(location, port);
}

void inproc_bind(void *s, char *location) {
    xq_socket_t *result;
    HASH_FIND(hh, bound_inproc_socks, location, strlen(location), result);
    /* XXX YOU SUCK FOR DOUBLE BINDING */
    assert(!result);
    HASH_ADD_KEYPTR(hh, bound_inproc_socks, location, strlen(location), (xq_socket_t *)s);
}

void tcp_bind(void *p, char *p_location) {
    xq_socket_t *s = (xq_socket_t *)p;

    s->tcp_flush_handle.data = s;
    uv_async_init(the_loop, &s->tcp_flush_handle, tcp_flush_cb);

    struct sockaddr_in bind_loc = parse_tcp_location(p_location);
    s->tcp_socket.data = p;
    uv_tcp_init(the_loop, &s->tcp_socket);
    int r = uv_tcp_bind(&s->tcp_socket, bind_loc);


    if (r) {
        fatal("tcp bind error"); // why?
    }

    r = uv_listen((uv_stream_t *) &s->tcp_socket, 128, on_tcp_connection);

    printf("did bind to: %s\n", p_location);
}

int xq_socket_bind(xq_socket_t *s, char *location) {
    // clean up bind failures through the stack
    assert(!run_started);
    char *next = parse_location(location, &s->trans);

    switch (s->trans) {
    case XQ_SOCKET_TCP:
        s->do_bind = tcp_bind;
        break;

    case XQ_SOCKET_INPROC:
        s->do_bind = inproc_bind;
        break;

   default:
        assert(0);
        break;
    }

    s->do_bind(s, next);

    return 0;
}
static void on_tcp_connectresult(uv_connect_t *handle, int status) {
    pthread_mutex_lock(&l_tcp_connect);
    xq_socket_t *s = (xq_socket_t *)handle->data;
    s->is_connecting = 0;
    // XXX handle disconnect

    if (!status) {
        DL_DELETE(want_tcp_connect, s);
        xq_pipe_t *pipe = new_tcp_pipe(s, &s->tcp_socket);
        CDL_PREPEND(s->pipes, pipe);
        if (!s->next_pipe) {
            s->next_pipe = s->pipes;
            socket_flush(s); // if there's anything waiting, give it to this guy
        }
        s->tcp_socket.data = pipe;
        uv_read_start((uv_stream_t*) &s->tcp_socket, pipe_allocate, on_tcp_read);
    }
    pthread_mutex_unlock(&l_tcp_connect);
}

static void poll_timers(uv_timer_t *handle, int status) {
    xq_socket_t *s = want_tcp_connect;
    pthread_mutex_lock(&l_tcp_connect);

    while (s) {
        if (!s->is_connecting) {
            uv_tcp_connect(&s->tcp_connect, &s->tcp_socket,
            s->connect_location, on_tcp_connectresult);
            s->is_connecting = 1;
        }
        s = s->next;
    }
    pthread_mutex_unlock(&l_tcp_connect);
}

void xq_init() {
    the_loop = uv_loop_new();
    pthread_mutex_init(&l_tcp_connect, NULL);
    uv_timer_init(the_loop, &connect_timer);
    int r = uv_timer_start(&connect_timer, poll_timers, 500, 500);
    assert(!r);
    r = uv_async_init(the_loop, &connect_trigger, tcp_connect_cb);
    assert(!r);
}

static void *actual_run(void *unused) {
    uv_run(the_loop);

    return NULL;
}

inline void * xq_frame_data(xq_frame_t *fr) {
    return fr->data;
}

inline uint32_t xq_frame_size(xq_frame_t *fr) {
    return fr->size;
}


// This puts the lotion in the basket.
// puts the packets on the socket.
void xq_frame_send(xq_frame_t *fr, xq_socket_t *s) {
    pthread_mutex_lock(&s->l_send);
    while (s->capacity && s->count_send == s->capacity) {
        pthread_cond_wait(&s->c_send, &s->l_send);
    }
    DL_APPEND(s->q_send, fr);
    pthread_mutex_unlock(&s->l_send);


    // If we are a socket portal, use uv
    switch (s->trans) {
    case XQ_SOCKET_TCP:
        uv_async_send(&s->tcp_flush_handle);

        break;
    case XQ_SOCKET_INPROC:
         socket_flush(s);
        break;
    default:
        assert(0);
    }
}

void xq_run() {
    /* XXX flawed run model.  we need to
       allow uv_bind etc to be run in other
       threads.  we'll need more async handles. */
    run_started = 1;
    pthread_create(&the_thread, NULL, actual_run, NULL);

    /* XXX also, we need stop/restart and stuff; maybe
    */
}

int tcp_connect(xq_socket_t *s, char *location) {

    s->tcp_flush_handle.data = s;
    s->reconnect = 1;
    uv_async_init(the_loop, &s->tcp_flush_handle, tcp_flush_cb);

    s->connect_location = parse_tcp_location(location);
    uv_tcp_init(the_loop, &s->tcp_socket);
    s->tcp_connect.data = s;

    pthread_mutex_lock(&l_tcp_connect);
    DL_APPEND(want_tcp_connect, s);
    pthread_mutex_unlock(&l_tcp_connect);
    uv_async_send(&connect_trigger);

    return 0;
}

int inproc_connect(xq_socket_t *s, char *location) {
    xq_socket_t *result;
    HASH_FIND(hh, bound_inproc_socks, location, strlen(location), result);
    /* XXX YOU SUCK FOR LOOKING UP SOMETHING WRONG */
    assert(result);
    if (result) {
        xq_pipe_t *pipe1 = new_inproc_pipe(s, result);
        CDL_PREPEND(s->pipes, pipe1);
        if (!s->next_pipe) {
            s->next_pipe = s->pipes;
        }
        xq_pipe_t *pipe2 = new_inproc_pipe(result, s);
        CDL_PREPEND(result->pipes, pipe2);
        if (!result->next_pipe) {
            result->next_pipe = result->pipes;
        }
        
    }

    return 0;
}

int xq_socket_connect(xq_socket_t *s, char *location) {
    char *next = parse_location(location, &s->trans);

    int ret = -1;

    switch(s->trans) {
    case XQ_SOCKET_TCP:
        ret = tcp_connect(s, next);
        break;
    case XQ_SOCKET_INPROC:
        ret = inproc_connect(s, next);
        break;

    default:
        assert(0);
    }
            
    return ret;
}

#include <unistd.h>

int main(int argc, char **argv) {
    xq_init();
    // xq_socket_t *s = xq_socket_new(XQ_SOCKET_PULL);

    // xq_socket_bind(s, "tcp://127.0.0.1:4444");


    xq_socket_t *ins = xq_socket_new(XQ_SOCKET_PULL);
    xq_socket_bind(ins, "inproc://inbox");





    xq_socket_t *inout = xq_socket_new(XQ_SOCKET_PUSH);
    xq_socket_connect(inout, "inproc://inbox");

    xq_run();


    xq_frame_t *fra = xq_frame_new_copy("world", 6);
    xq_frame_send(fra, inout);




    return 0;
//     int x = 0;
//     for (; x < 10000; x++) {
//         xq_frame_t *fr = xq_frame_recv(s);
// //        printf("got frame length = %u and content = '%s' count = %d!\n",
// ////        xq_frame_size(fr), (char *)xq_frame_data(fr), x);
//         xq_frame_destroy(fr);
//     }

//     xq_frame_t *fr = xq_frame_new_copy("world", 6);
//     xq_frame_send(fr, s);

//     xq_socket_t *cs = xq_socket_new(XQ_SOCKET_PULL);

//     xq_socket_connect(cs, "tcp://127.0.0.1:4444");
//     fr = xq_frame_new_copy("dogs!", 6);
//     xq_frame_send(fr, cs);
//     sleep(1);
//     printf("yo\n");
//     fr = xq_frame_new_copy("dogs!", 6);

//     xq_frame_t *in = xq_frame_recv(s);
//     printf("yo, I got: %u %s\n", xq_frame_size(in), (char *)xq_frame_data(in));
//     assert(!strcmp(xq_frame_data(in), xq_frame_data(fr)));
//     printf("yo\n");

//     sleep(1);

//     return 0;
}
