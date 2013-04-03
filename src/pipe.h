#ifndef PIPE_H
#define PIPE_H

#include "queue.h"
#include "common.h"
#include "frame.h"
#include "buffer.h"

typedef struct nitro_pipe_t *nitro_pipe_t_p;

typedef struct nitro_pipe_t {

    /* Direct send queue */
    nitro_queue_t *q_send;

    /* for TCP sockets */
    ev_io ior;
    ev_io iow;
    int fd;
    uint64_t sub_state_sent;
    uint64_t sub_state_recv;

    /* When we have partial output */
    nitro_frame_t *partial;
    uint8_t *remote_ident;
    nitro_counted_buffer_t *remote_ident_buf;
    char us_handshake;
    char them_handshake;
    char registered;

    nitro_buffer_t *in_buffer;

    void *the_socket;

    /* XXX for inproc, paired socket */
    void *dest_socket;

    struct nitro_pipe_t *prev;
    struct nitro_pipe_t *next;

    nitro_key_t *sub_keys;

    UT_hash_handle hh;
} nitro_pipe_t;

nitro_pipe_t *nitro_pipe_new(void *s);
void nitro_pipe_destroy(nitro_pipe_t *p, void *s);

#endif /* PIPE_H */
