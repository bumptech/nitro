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
    /* When we have partial output */
    nitro_frame_t *partial;

    nitro_buffer_t *in_buffer;

    void *the_socket;

    /* XXX for inproc, paired socket */
    void *dest_socket;

    struct nitro_pipe_t *prev;
    struct nitro_pipe_t *next;
    int registered;

    nitro_key_t *sub_keys;
    uint8_t last_sub_hash[20];

    UT_hash_handle hh;
} nitro_pipe_t;

nitro_pipe_t *nitro_pipe_new();
void nitro_pipe_destroy(nitro_pipe_t *p, void *s);

#endif /* PIPE_H */
