#ifndef XQ_H
#define XQ_H

#include "uv.h"
#include "uthash/uthash.h"

void xq_init();
void xq_run();
void xq_stop();

typedef enum {
    XQ_SOCKET_PUSH,
    XQ_SOCKET_PULL,
    XQ_SOCKET_REQ,
    XQ_SOCKET_REP,
    XQ_SOCKET_PAIR,
    XQ_SOCKET_DEALER,
    XQ_SOCKET_ROUTER,
    XQ_SOCKET_PUB,
    XQ_SOCKET_SUB
} XQ_SOCKET_TYPE;

typedef enum {
    XQ_SOCKET_NONE,
    XQ_SOCKET_TCP,
    XQ_SOCKET_INPROC
} XQ_SOCKET_TRANSPORT;

typedef void (*xq_socket_bind_function)(void *s, char *location);

typedef void (*xq_free_function)(void *, void *);

typedef struct xq_frame_t {
    void *data;
    uint32_t size;
    xq_free_function ff;
    void *baton;

    uint32_t ref_count;
    pthread_mutex_t lock;

    // For UT_LIST
    struct xq_frame_t *prev;
    struct xq_frame_t *next;
} xq_frame_t;
typedef struct xq_pipe_t* xq_pipe_t_p;


typedef struct xq_pipe_t {

    uv_tcp_t *tcp_socket;

    uint8_t *buffer;
    uint32_t buf_alloc;
    uint32_t buf_bytes;

    void *the_socket;
    void *dest_socket;
    xq_frame_t *outgoing;

    struct xq_pipe_t *prev;
    struct xq_pipe_t *next;
    int registered;

    void (*do_write)(xq_pipe_t_p, xq_frame_t *);
    void (*destroy)(xq_pipe_t_p);
    UT_hash_handle hh;
} xq_pipe_t;

typedef struct xq_socket_t {
    XQ_SOCKET_TYPE typ;
    XQ_SOCKET_TRANSPORT trans;
    xq_socket_bind_function do_bind;

    uv_tcp_t tcp_socket;
    uv_async_t tcp_flush_handle;
    uv_connect_t tcp_connect;
//    uv_async_t close_handle;

    xq_frame_t *q_recv;
    xq_frame_t *q_send;

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
    xq_pipe_t *pipes_by_session;
    // Circular List of all connected pipes (can use for round robining, or broadcast with pub)
    xq_pipe_t *pipes;

    xq_pipe_t *next_pipe; // for RR

    struct sockaddr_in connect_location;
    int is_connecting;
    int reconnect;

    /* Options */
    uint32_t capacity;

    /* for tcp connect list */
    struct xq_socket_t *prev;
    struct xq_socket_t *next;

    /* hash table for bound inproc sockets */
    UT_hash_handle hh;
} xq_socket_t;

xq_socket_t * xq_socket_new(XQ_SOCKET_TYPE socktype);
int xq_socket_bind(xq_socket_t *s, char *location);
//int xq_socket_connect(xq_socket_t *s, char *location);

xq_frame_t * xq_frame_new_copy(void *data, uint32_t size);
xq_frame_t * xq_frame_new(void *data, uint32_t size, xq_free_function ff, void *baton);
void * xq_frame_data(xq_frame_t *fr);
uint32_t xq_frame_size(xq_frame_t *fr);
void xq_frame_destroy(xq_frame_t *fr);
xq_frame_t * xq_frame_recv(xq_socket_t *s);
void xq_frame_send(xq_frame_t *fr, xq_socket_t *s);
void xq_frame_retain(xq_frame_t *f);
int xq_socket_connect(xq_socket_t *s, char *location);

#endif /* XQ_H */
