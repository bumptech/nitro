#include "nitro.h"
#include "nitro-private.h"

nitro_frame_t * nitro_recv(nitro_socket_t *s) {
    nitro_frame_t *out = NULL;
    pthread_mutex_lock(&s->l_recv);

    while (!s->q_recv)
        pthread_cond_wait(&s->c_recv, &s->l_recv);
    out = s->q_recv;
    DL_DELETE(s->q_recv, out);
    pthread_mutex_unlock(&s->l_recv);
    assert(out); // why triggered with no frame?

    return out;
}

void nitro_send(nitro_frame_t *fr, nitro_socket_t *s) {
    pthread_mutex_lock(&s->l_send);
    while (s->capacity && s->count_send == s->capacity) {
        pthread_cond_wait(&s->c_send, &s->l_send);
    }
    DL_APPEND(s->q_send, fr);
    pthread_mutex_unlock(&s->l_send);

    // If we are a socket portal, use uv
    uv_async_send(&s->tcp_flush_handle);

    // else just ~ some list pointers
}

/* Private API: _common_ socket initialization */
nitro_socket_t * nitro_socket_new() {
    nitro_socket_t *sock = calloc(1, sizeof(nitro_socket_t));

    pthread_mutex_init(&sock->l_recv, NULL);
    pthread_mutex_init(&sock->l_send, NULL);

    pthread_cond_init(&sock->c_recv, NULL);
    pthread_cond_init(&sock->c_send, NULL);

    return sock;
}

// TODO common socket destroy
