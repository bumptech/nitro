#include "err.h"
#include "socket.h"
#include "runtime.h"
#include "crypto.h"

nitro_socket_t *nitro_socket_new() {
    nitro_socket_t *sock;
    ZALLOC(sock);

    nitro_universal_socket_t *us = &sock->stype.univ;

    // XXX TODO -- provided pub/sec key?  skip
    crypto_make_keypair(us->ident, us->pkey);

    pthread_mutex_init(&us->l_pipes, NULL);

 //   sock->sub_keys = NULL;
    // Change to stdatomic.h XXX
    __sync_add_and_fetch(&the_runtime->num_sock, 1);
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
    free(s);
    __sync_add_and_fetch(&the_runtime->num_sock, 1);
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
        HASH_DELETE(hh, s->pipes_by_session, 
            p);
        p->registered = 0;
    }
    pthread_mutex_unlock(&s->l_pipes);
}
