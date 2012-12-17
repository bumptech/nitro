#include "nitro.h"
#include "nitro-private.h"

typedef struct nitro_protocol_header {
    char protocol_version;
    uint32_t frame_size;
} nitro_protocol_header;

static int parse_tcp_location(char *p_location,
    struct sockaddr_in *addr) {
    char *location = alloca(strlen(p_location)+1);
    strcpy(location, p_location);

    char *split = strchr(location, ':');

    if (!split) 
        return nitro_set_error(NITRO_ERR_TCP_LOC_NOCOLON);

    *split = 0;

    errno = 0;
    int port = strtol(split + 1, NULL, 10);
    if (errno) 
        return nitro_set_error(NITRO_ERR_TCP_LOC_BADPORT);

    *addr = uv_ip4_addr(location, port);
    return 0;
}

static uv_buf_t pipe_allocate(uv_handle_t *handle, size_t suggested_size) {
    nitro_pipe_t *p = (nitro_pipe_t *)handle->data;
    size_t avail = p->buf_alloc - p->buf_bytes;
    if (suggested_size > ((double)avail * 1.2)) {
        p->buf_alloc = suggested_size + p->buf_bytes;
        p->buffer = realloc(p->buffer, p->buf_alloc);
        avail = p->buf_alloc - p->buf_bytes;
    }
    return uv_buf_init((char *)p->buffer + p->buf_bytes, avail);
}

typedef struct tcp_write_request {
    nitro_protocol_header header;
    nitro_frame_t *frame;
    nitro_socket_t *socket;
} tcp_write_request;


static void tcp_write_finished(uv_write_t *w, int status) {
    // for now, we're ignoring status
    tcp_write_request *req = (tcp_write_request *)w->data;
    nitro_socket_t *s = req->socket;
    if (status>=0) {
        nitro_frame_destroy(req->frame);
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

void tcp_write(nitro_pipe_t *p, nitro_frame_t *frame) {
    nitro_frame_t *f = nitro_frame_copy(frame);
    tcp_write_request *req = calloc(1, sizeof(tcp_write_request));
    uv_write_t *w = calloc(1, sizeof(uv_write_t));
    w->data = req;
    req->header.protocol_version = 1;
    req->header.frame_size = f->size;
    req->frame = f;
    req->socket = p->the_socket;

    uv_buf_t out[] = {
        { .base = (void *)&req->header, .len = sizeof(nitro_protocol_header)},
        { .base = (void *)((nitro_counted_buffer*)f->buffer)->ptr, .len = f->size}
    };

    uv_write(w, (uv_stream_t *)p->tcp_socket,
    out, 2, tcp_write_finished);
}

static void destroy_tcp_pipe(nitro_pipe_t *p) {
    uv_close((uv_handle_t *)p->tcp_socket, NULL);
    destroy_pipe(p);
}

static void on_tcp_read(uv_stream_t *peer, ssize_t nread, uv_buf_t unused) {
    nitro_pipe_t *p = (nitro_pipe_t *)peer->data;
    nitro_socket_t *s = (nitro_socket_t *)p->the_socket;

    if (nread == -1) {
        p->destroy(p);
        if (s->outbound) {
            uv_tcp_init(the_runtime->the_loop, &s->tcp_socket);
            s->is_connecting = 0;
            pthread_mutex_lock(&the_runtime->l_tcp_pair);
            DL_APPEND(the_runtime->want_tcp_pair, s);
            pthread_mutex_unlock(&the_runtime->l_tcp_pair);
            tcp_poll(NULL, 0);
        }

        return;
    }

    if (nread == 0) {
        assert(0); // wha?
    }

    p->buf_bytes += nread;

    if (p->buf_bytes < sizeof(nitro_protocol_header))
        return;

    nitro_counted_buffer *buf = NULL;
    uint8_t *region = p->buffer;
    nitro_protocol_header *header = (nitro_protocol_header *)region;
    uint32_t current_frame_size = header->frame_size;
    uint32_t whole_size = sizeof(nitro_protocol_header) + current_frame_size;
    while (p->buf_bytes >= whole_size) {
        assert(header->protocol_version == 1);
        /* we have the whole frame! */
        if (!buf)
            buf = nitro_counted_buffer_new(p->buffer, &just_free, NULL);
        buffer_incref(buf);
        
        nitro_frame_t *fr = nitro_frame_new(region + sizeof(nitro_protocol_header),
        current_frame_size, buffer_decref, buf);
        pthread_mutex_lock(&s->l_recv);
        DL_APPEND(s->q_recv, fr);
        s->count_recv++;
        pthread_cond_signal(&s->c_recv);
        pthread_mutex_unlock(&s->l_recv);

        p->buf_bytes -= whole_size;
        if (p->buf_bytes < sizeof(nitro_protocol_header))
            break;
        region += whole_size;
        header = (nitro_protocol_header *)region;
        current_frame_size = header->frame_size;
        whole_size = sizeof(nitro_protocol_header) + current_frame_size;
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

static nitro_pipe_t *new_tcp_pipe(nitro_socket_t *s, uv_tcp_t *tcp_socket) {
    nitro_pipe_t *p = calloc(1, sizeof(nitro_pipe_t));
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

    nitro_socket_t *s = (nitro_socket_t *)peer->data;
    uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
    uv_tcp_init(the_runtime->the_loop, client);
    if (uv_accept(peer, (uv_stream_t*) client) == 0) {
        nitro_pipe_t *pipe = new_tcp_pipe(s, client);
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


void tcp_poll_cb(uv_async_t *handle, int status) {
    tcp_poll(NULL, 0);
}

static void tcp_flush_cb(uv_async_t *handle, int status) {
    nitro_socket_t *s = (nitro_socket_t *)handle->data;
    socket_flush(s);
}

static void on_tcp_connectresult(uv_connect_t *handle, int status) {
    pthread_mutex_lock(&the_runtime->l_tcp_pair);
    nitro_socket_t *s = (nitro_socket_t *)handle->data;
    s->is_connecting = 0;
    // XXX handle disconnect

    if (!status) {
        DL_DELETE(the_runtime->want_tcp_pair, s);
        nitro_pipe_t *pipe = new_tcp_pipe(s, &s->tcp_socket);
        CDL_PREPEND(s->pipes, pipe);
        if (!s->next_pipe) {
            s->next_pipe = s->pipes;
            socket_flush(s); // if there's anything waiting, give it to this guy
        }
        s->tcp_socket.data = pipe;
        uv_read_start((uv_stream_t*) &s->tcp_socket, pipe_allocate, on_tcp_read);
    }
    pthread_mutex_unlock(&the_runtime->l_tcp_pair);
}

void tcp_poll(uv_timer_t *handle, int status) {
    nitro_socket_t *s = the_runtime->want_tcp_pair;
    pthread_mutex_lock(&the_runtime->l_tcp_pair);

    while (s) {
        if (s->outbound) {
            if (!s->is_connecting) {
                uv_tcp_connect(&s->tcp_connect, &s->tcp_socket,
                s->tcp_location, on_tcp_connectresult);
                s->is_connecting = 1;
            }
        }
        else {
            // do the bind/listen
            int r = uv_listen((uv_stream_t *)&s->tcp_socket, 512, on_tcp_connection);
            if (!r) {
                DL_DELETE(the_runtime->want_tcp_pair, s);
            }
        }
        s = s->next;
    }
    pthread_mutex_unlock(&the_runtime->l_tcp_pair);
}

/* Okay, make sockets of this type! */

nitro_socket_t * nitro_connect_tcp(char *location) {
    nitro_socket_t *s = nitro_socket_new();
    s->trans = NITRO_SOCKET_TCP;

    s->tcp_flush_handle.data = s;
    s->outbound = 1;
    uv_async_init(the_runtime->the_loop, &s->tcp_flush_handle, tcp_flush_cb);

    int r = parse_tcp_location(location, &s->tcp_location);
    if (r) goto errout;

    uv_tcp_init(the_runtime->the_loop, &s->tcp_socket);
    s->tcp_connect.data = s;

    pthread_mutex_lock(&the_runtime->l_tcp_pair);
    DL_APPEND(the_runtime->want_tcp_pair, s);
    pthread_mutex_unlock(&the_runtime->l_tcp_pair);
    uv_async_send(&the_runtime->tcp_trigger);

    return s;

errout:
    // XXX destroy socket
    return NULL;
}

nitro_socket_t * nitro_bind_tcp(char *location) {
    int r;
    nitro_socket_t *s = nitro_socket_new();
    s->trans = NITRO_SOCKET_TCP;
    s->tcp_flush_handle.data = s;
    uv_async_init(the_runtime->the_loop, &s->tcp_flush_handle, tcp_flush_cb);

    r = parse_tcp_location(location, &s->tcp_location);
    if (r)
        goto errout;

    s->tcp_socket.data = s;
    uv_tcp_init(the_runtime->the_loop, &s->tcp_socket);
    r = uv_tcp_bind(&s->tcp_socket, s->tcp_location);

    if (r) {
        nitro_set_error(NITRO_ERR_ERRNO);
        goto errout;
    }

    pthread_mutex_lock(&the_runtime->l_tcp_pair);
    DL_APPEND(the_runtime->want_tcp_pair, s);
    pthread_mutex_unlock(&the_runtime->l_tcp_pair);

    // XXX debug
    printf("did bind to: %s\n", location);

    return s;

errout:
    // XXX socket destroy
    return NULL;
}
