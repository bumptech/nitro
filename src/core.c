#include "nitro.h"
#include "nitro-private.h"

nitro_frame_t * nitro_recv(nitro_socket_t *s) {
    nitro_frame_t *out = NULL;
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
        uv_async_send(&s->tcp_flush_handle);

        break;
    case NITRO_SOCKET_INPROC:
         socket_flush(s);
        break;
    default:
        assert(0);
    }
}

/* Private API: _common_ socket initialization */
nitro_socket_t * nitro_socket_new() {
    nitro_socket_t *sock = calloc(1, sizeof(nitro_socket_t));

    pthread_mutex_init(&sock->l_recv, NULL);
    pthread_mutex_init(&sock->l_send, NULL);

    pthread_cond_init(&sock->c_recv, NULL);
    pthread_cond_init(&sock->c_send, NULL);
    
    sock->sub_keys = NULL;
    __sync_add_and_fetch(&the_runtime->num_sock, 1);

    return sock;
}

void nitro_socket_destroy(nitro_socket_t *s) {
    nitro_frame_t *f, *tmp;

    for (f=s->q_send; f; f = tmp) {
        tmp = f->next;
        nitro_frame_destroy(f);
    }
    for (f=s->q_recv; f; f = tmp) {
        tmp = f->next;
        nitro_frame_destroy(f);
    }
    free(s);
    __sync_sub_and_fetch(&the_runtime->num_sock, 1);
}

void socket_flush(nitro_socket_t *s) {
    pthread_mutex_lock(&s->l_send);
    while (1) {
        nitro_frame_t *f = s->q_send;
        if (!f) {
            break;
        }
        nitro_pipe_t *p = s->next_pipe;
        // Non- pub frames need a pipe to go to.
        if (!f->is_pub) {
            if (!p) break;
            p->do_write(p, f);
            s->next_pipe = p->next;
        } else {
        
            CDL_FOREACH(s->next_pipe,p) {
                int matched_pipe = 0;
                nitro_key_t *key;
                DL_FOREACH(p->sub_keys, key) {
                    if (strcmp(key->key, f->key) == 0) {
                        matched_pipe = 1;
                    }
                }
                if (matched_pipe) {
                    p->do_write(p, f);
                }
            }
        }
        DL_DELETE(s->q_send, f);
        nitro_frame_destroy(f);
    }
    pthread_mutex_unlock(&s->l_send);
}

void add_pub_filter(nitro_socket_t *s, nitro_pipe_t *p, char *key) {
    nitro_key_t *t = nitro_key_new(key);
    DL_APPEND(p->sub_keys, t);
    // XXX add trie insertion here
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




nitro_socket_t * nitro_socket_bind(char *location) {
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


nitro_socket_t * nitro_socket_connect(char *location) {
    char *next;
    NITRO_SOCKET_TRANSPORT trans = parse_location(location, &next);

    nitro_socket_t * ret = NULL;

    switch(trans) {
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
    CDL_FOREACH(s->pipes, p) {
        p->do_sub(p, key);
    }
    nitro_key_t *k = nitro_key_new(key);
    DL_APPEND(s->sub_keys, k);
}

nitro_key_t *nitro_key_new(char *key) {
    nitro_key_t *k = malloc(sizeof(nitro_key_t));
    k->next = NULL;
    k->prev = NULL;
    memmove(k->key, key, strlen(key));
    return k;
}
