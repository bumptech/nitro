#include "nitro.h"
#include "nitro-private.h"

static nitro_socket_t *bound_inproc_socks;



void inproc_sub(nitro_pipe_t *p, char *key) {
    nitro_socket_t *s = (nitro_socket_t *)p->dest_socket;
    nitro_pipe_t *return_pipe = NULL;
    CDL_FOREACH(s->pipes, return_pipe) {
        if (return_pipe->dest_socket == p->the_socket) {
            // we found the return pipe, let's add the sub key.
            add_pub_filter(s, return_pipe, key);
        }
    }
}
                      

void inproc_write(nitro_pipe_t *p, nitro_frame_t *f) {
    nitro_socket_t *s = (nitro_socket_t *)p->dest_socket;

    nitro_frame_t *fcopy = nitro_frame_copy(f);
    pthread_mutex_lock(&s->l_recv);
    DL_APPEND(s->q_recv, fcopy);
    pthread_cond_signal(&s->c_recv);
    s->count_recv++;
    pthread_mutex_unlock(&s->l_recv);
}

static nitro_pipe_t *new_inproc_pipe(nitro_socket_t *orig_socket, nitro_socket_t *dest_socket) {
    nitro_pipe_t *p = nitro_pipe_new();
    p->the_socket = (void *)orig_socket;
    p->dest_socket = (void *) dest_socket;
    p->do_write = &inproc_write;
    p->do_sub = &inproc_sub;

    return p;
}

nitro_socket_t * nitro_bind_inproc(char *location) {
    nitro_socket_t *s = nitro_socket_new();
    s->trans = NITRO_SOCKET_INPROC;
    nitro_socket_t *result;
    HASH_FIND(hh, bound_inproc_socks, location, strlen(location), result);
    /* XXX YOU SUCK FOR DOUBLE BINDING */
    assert(!result);
    HASH_ADD_KEYPTR(hh, bound_inproc_socks, location, strlen(location), (nitro_socket_t *)s);
    return s;
}

nitro_socket_t * nitro_connect_inproc(char *location) {
    nitro_socket_t *s = nitro_socket_new();
    s->trans = NITRO_SOCKET_INPROC;
    nitro_socket_t *result;
    HASH_FIND(hh, bound_inproc_socks, location, strlen(location), result);
    /* XXX YOU SUCK FOR LOOKING UP SOMETHING WRONG */
    assert(result);
    if (result) {
        nitro_pipe_t *pipe1 = new_inproc_pipe(s, result);
        CDL_PREPEND(s->pipes, pipe1);
        if (!s->next_pipe) {
            s->next_pipe = s->pipes;
        }
        nitro_pipe_t *pipe2 = new_inproc_pipe(result, s);
        CDL_PREPEND(result->pipes, pipe2);
        if (!result->next_pipe) {
            result->next_pipe = result->pipes;
        }
        
    }
    return s;
}

void nitro_close_inproc(nitro_socket_t *s) {
    nitro_pipe_t *p, *tmp;

    for (p=s->pipes; p; p = tmp) {
        tmp = p->next;
        destroy_pipe(p);
    }

    nitro_socket_destroy(s);
}
