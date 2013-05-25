#include "err.h"
#include "socket.h"
#include "runtime.h"
#include "crypto.h"

nitro_socket_t *nitro_socket_new(nitro_sockopt_t *opt) {
    nitro_socket_t *sock;
    ZALLOC(sock);

    nitro_universal_socket_t *us = &sock->stype.univ;
    opt = opt ? opt : nitro_sockopt_new();
    us->opt = opt;

    if (!us->opt->has_ident) {
        crypto_make_keypair(us->opt->ident, us->opt->pkey);
        us->opt->has_ident = 1;
    }

    if (us->opt->want_eventfd) {
        us->event_fd = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE);
        assert(us->event_fd >= 0);
    }

    pthread_mutex_init(&us->l_pipes, NULL);

    // XXX add socket to list for diagnostics
    atomic_fetch_add(&the_runtime->num_sock, 1);
    return sock;
}

NITRO_SOCKET_TRANSPORT socket_parse_location(char *location, char **next) {
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

void nitro_socket_destroy(nitro_socket_t *s) {
    nitro_universal_socket_t *us = &s->stype.univ;
    free(us->opt);
    free(us->given_location);
    free(s);
    atomic_fetch_sub(&the_runtime->num_sock, 1);
}

void socket_register_pipe(nitro_universal_socket_t *s, nitro_pipe_t *p) {
    pthread_mutex_lock(&s->l_pipes);
    HASH_ADD_KEYPTR(hh, s->pipes_by_session, 
        p->remote_ident, SOCKET_IDENT_LENGTH, p);
    
    nitro_pipe_t *p2 = socket_lookup_pipe(s, p->remote_ident);
    assert(p2);
    p->registered = 1;
    pthread_mutex_unlock(&s->l_pipes);
}

nitro_pipe_t *socket_lookup_pipe(nitro_universal_socket_t *s, uint8_t *ident) {
    // assumed -- lock held
    nitro_pipe_t *p;
    HASH_FIND(hh, s->pipes_by_session, 
        ident, SOCKET_IDENT_LENGTH, p);
    return p;
}

void socket_unregister_pipe(nitro_universal_socket_t *s, nitro_pipe_t *p) {
    pthread_mutex_lock(&s->l_pipes);
    if (p->registered) {
        HASH_DELETE(hh, s->pipes_by_session, p);
        p->registered = 0;
    }
    pthread_mutex_unlock(&s->l_pipes);
}
