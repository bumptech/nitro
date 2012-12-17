#include "nitro.h"
#include "nitro-private.h"

void destroy_pipe(nitro_pipe_t *p) {
    nitro_socket_t *s = (nitro_socket_t *)p->the_socket;

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
