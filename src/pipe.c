#include "nitro.h"
#include "nitro-private.h"

void destroy_pipe(nitro_pipe_t *p) {
    nitro_socket_t *s = (nitro_socket_t *)p->the_socket;
    pthread_mutex_lock(&s->l_sub);
    remove_pub_filters(s, p);
    pthread_mutex_unlock(&s->l_sub);
    assert(p->sub_keys == NULL);

    if (p->next == p) {
        s->next_pipe = NULL;
    } else {
        s->next_pipe = p->next;
    }

    CDL_DELETE(s->pipes, p);

    if (p->registered) {
        HASH_DEL(s->pipes_by_session, p);
    }

    free(p->buffer);
    free(p);
}

nitro_pipe_t *nitro_pipe_new() {
    nitro_pipe_t *p = calloc(1, sizeof(nitro_pipe_t));
    return p;
}
