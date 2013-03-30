#ifndef SOCKET_H
#define SOCKET_H

#include "common.h"

#include "buffer.h"
#include "cbuffer.h"
#include "frame.h"
#include "pipe.h"
#include "queue.h"
#include "trie.h"

#define INPROC_PREFIX "inproc://"
#define TCP_PREFIX "tcp://"

#define SOCKET_CALL(s, name, args...) \
    (s->trans == NITRO_SOCKET_TCP ? \
        Stcp_socket_##name(&(s->stype.tcp), ## args) : \
        Sinproc_socket_##name(&(s->stype.inproc), ## args));

#define SOCKET_SET_PARENT(s) {\
    if(s->trans == NITRO_SOCKET_TCP)\
        s->stype.tcp.parent = s;\
    else\
        s->stype.inproc.parent = s;\
    }

#define SOCKET_PARENT(s) ((nitro_socket_t *)s->parent)
#define SOCKET_UNIVERSAL(s) (&(SOCKET_PARENT(s)->stype.univ))

typedef enum {
    NITRO_SOCKET_TCP,
    NITRO_SOCKET_INPROC,
} NITRO_SOCKET_TRANSPORT;


#define SOCKET_COMMON_FIELDS\
    /* Incoming messages */\
    nitro_queue_t *q_recv;\
    /* Outgoing _general_ messages */\
    nitro_queue_t *q_send;\
    \
    /* locks for sending and receiving from other threads\
       control locks for maniuplating counts, queues, or pipes*/\
    pthread_mutex_t l_sub;\
    \
    /* Pipes need to be locked during map
       lookup, mutation by libev thread, etc */\
    pthread_mutex_t l_pipes;\
    /* for reply-style session mapping\
       UT Hash.  Pipes that have not yet registered are not in here */\
    nitro_pipe_t *pipes_by_session;\
    \
    /* Circular List of all connected pipes (can use for round robining, or broadcast with pub)*/\
    nitro_pipe_t *pipes;\
    nitro_pipe_t *next_pipe;\
    int num_pipes;\
    \
    /* Subscription trie */\
    nitro_prefix_trie_node *subs;\
    /* Socket identity/crypto */\
    uint8_t ident[SOCKET_IDENT_LENGTH];\
    uint8_t pkey[crypto_box_SECRETKEYBYTES];\
    \
    /* Local "want subscription" list */\
    nitro_key_t *sub_keys;\
    /* Parent socket */\
    void *parent;\

typedef struct nitro_universal_socket_t {
    SOCKET_COMMON_FIELDS
} nitro_universal_socket_t;

typedef struct nitro_tcp_socket_t *nitro_tcp_socket_t_p;

typedef struct nitro_tcp_socket_t {
    SOCKET_COMMON_FIELDS

    ev_io bound_io;
    int bound_fd;

    ev_io connect_io;
    int connect_fd;
    ev_timer connect_timer;

    struct sockaddr_in location;

    nitro_counted_buffer_t *sub_data;
    uint32_t sub_data_length;
} nitro_tcp_socket_t;

typedef struct nitro_inproc_socket_t {
    SOCKET_COMMON_FIELDS

    /* hash table for bound sockets */
    UT_hash_handle hh;

} nitro_inproc_socket_t;

typedef struct nitro_socket_t {
    NITRO_SOCKET_TRANSPORT trans;

    union {
        nitro_universal_socket_t univ;
        nitro_tcp_socket_t tcp;
        nitro_inproc_socket_t inproc;
    } stype;

} nitro_socket_t;

nitro_socket_t *nitro_socket_new();
void nitro_socket_destroy();
nitro_socket_t *nitro_socket_bind(char *location);
nitro_socket_t *nitro_socket_connect(char *location);
void nitro_socket_close(nitro_socket_t *s);
NITRO_SOCKET_TRANSPORT socket_parse_location(char *location, char **next);
void socket_register_pipe(nitro_universal_socket_t *s, nitro_pipe_t *p);
nitro_pipe_t *socket_lookup_pipe(nitro_universal_socket_t *s, uint8_t *ident);
void socket_unregister_pipe(nitro_universal_socket_t *s, nitro_pipe_t *p);


#endif /* SOCKET_H */
