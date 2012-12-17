#ifndef NITRO_H
#define NITRO_H

#include "uv.h"
#include "uthash/uthash.h"

int nitro_start();
int nitro_stop();

typedef enum {
    NITRO_SOCKET_TCP,
    NITRO_SOCKET_INPROC,
} NITRO_SOCKET_TRANSPORT;

typedef enum {
    NITRO_ERR_ERRNO,
    NITRO_ERR_ALREADY_RUNNING,
    NITRO_ERR_TCP_LOC_NOCOLON,
    NITRO_ERR_TCP_LOC_BADPORT
} NITRO_ERROR;


typedef void (*nitro_free_function)(void *, void *);

typedef struct nitro_frame_t {
    void *data;
    uint32_t size;
    nitro_free_function ff;
    void *baton;

    uint32_t ref_count;
    pthread_mutex_t lock;

    // For UT_LIST
    struct nitro_frame_t *prev;
    struct nitro_frame_t *next;
} nitro_frame_t;

typedef struct nitro_pipe_t {

    uv_tcp_t *tcp_socket;

    uint8_t *buffer;
    uint32_t buf_alloc;
    uint32_t buf_bytes;

    void *the_socket;
    nitro_frame_t *outgoing;

    struct nitro_pipe_t *prev;
    struct nitro_pipe_t *next;
    int registered;

    UT_hash_handle hh;
} nitro_pipe_t;

typedef struct nitro_socket_t {
    NITRO_SOCKET_TRANSPORT trans;

    uv_tcp_t tcp_socket;
    uv_async_t tcp_flush_handle;
    uv_connect_t tcp_connect;
//    uv_async_t close_handle;

    nitro_frame_t *q_recv;
    nitro_frame_t *q_send;

    // locks for sending and receiving from other threads
    // control locks for maniuplating counts, queues, or pipes
    pthread_mutex_t l_recv;
    pthread_mutex_t l_send;

    // condition for when we are blocking waiting for sending or receiving
    // Buffer is full, or there are no incoming packets ready
    pthread_cond_t c_recv;
    pthread_cond_t c_send;

    uint32_t count_send;
    uint32_t count_recv;

    // for reply-style session mapping
    // UT Hash.  Pipes that have not yet registered are not in here
    nitro_pipe_t *pipes_by_session;
    // Circular List of all connected pipes (can use for round robining, or broadcast with pub)
    nitro_pipe_t *pipes;

    nitro_pipe_t *next_pipe; // for RR

    struct sockaddr_in tcp_location;
    int is_connecting;
    int outbound;

    /* Options */
    uint32_t capacity;

    struct nitro_socket_t *prev;
    struct nitro_socket_t *next;
} nitro_socket_t;

nitro_socket_t * nitro_bind_tcp(char *location);
nitro_socket_t * nitro_connect_tcp(char *location);
nitro_socket_t * nitro_bind_inproc(char *location);
nitro_socket_t * nitro_connect_inproc(char *location);

nitro_frame_t * nitro_recv(nitro_socket_t *s);
void nitro_send(nitro_frame_t *fr, nitro_socket_t *s);

nitro_frame_t * nitro_frame_new_copy(void *data, uint32_t size);
nitro_frame_t * nitro_frame_new(void *data, uint32_t size, nitro_free_function ff, void *baton);
void * nitro_frame_data(nitro_frame_t *fr);
uint32_t nitro_frame_size(nitro_frame_t *fr);
void nitro_frame_destroy(nitro_frame_t *fr);

char *nitro_errmsg(NITRO_ERROR error);
NITRO_ERROR nitro_error();

#endif /* NITRO_H */
