#ifndef PIPE_H
#define PIPE_H

#include "queue.h"
#include "common.h"
#include "frame.h"
#include "buffer.h"

typedef struct nitro_pipe_t *nitro_pipe_t_p;

typedef struct nitro_pipe_t {

    /* Main send queue */
    nitro_queue_t *q_send;

    /* for TCP sockets */
    ev_io ior;
    ev_io iow;
    int fd;


    nitro_buffer_t *in_buffer;

    void *the_socket;
    void *dest_socket;

    struct nitro_pipe_t *prev;
    struct nitro_pipe_t *next;
    int registered;

    nitro_key_t *sub_keys;
    uint8_t last_sub_hash[20];

    UT_hash_handle hh;
} nitro_pipe_t;

nitro_pipe_t *nitro_pipe_new();

#endif /* PIPE_H */
