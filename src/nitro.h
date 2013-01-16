/*
 * Nitro
 *
 * nitro.h - Public API definition for nitro communication framework
 *
 *  -- LICENSE --
 *
 * Copyright 2013 Bump Technologies, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY BUMP TECHNOLOGIES, INC. ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BUMP TECHNOLOGIES, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Bump Technologies, Inc.
 *
 */
#ifndef NITRO_H
#define NITRO_H

#include "uv.h"
#include "uthash/uthash.h"

#define NITRO_KEY_LENGTH 1024
int nitro_start();
int nitro_stop();

typedef enum {
    NITRO_SOCKET_TCP,
    NITRO_SOCKET_INPROC,
} NITRO_SOCKET_TRANSPORT;

#define NITRO_FLAG_ID (1 << 0)

typedef enum {
    NITRO_ERR_ERRNO,
    NITRO_ERR_ALREADY_RUNNING,
    NITRO_ERR_NOT_RUNNING,
    NITRO_ERR_TCP_LOC_NOCOLON,
    NITRO_ERR_TCP_LOC_BADPORT,
    NITRO_ERR_PARSE_BAD_TRANSPORT,
} NITRO_ERROR;

typedef void (*nitro_free_function)(void *, void *);

typedef struct nitro_counted_buffer {
    void *ptr;
    int count;
    pthread_mutex_t lock;
    nitro_free_function ff;
    void *baton;
} nitro_counted_buffer;

typedef struct nitro_frame_t {
    void *buffer;
    uint32_t size;
    nitro_free_function ff;
    void *baton;

    int is_pub;
    char key[NITRO_KEY_LENGTH];

    pthread_mutex_t lock;

    // For UT_LIST
    struct nitro_frame_t *prev;
    struct nitro_frame_t *next;
} nitro_frame_t;

typedef struct nitro_key_t {
    char key[NITRO_KEY_LENGTH];
    struct nitro_key_t *prev;
    struct nitro_key_t *next;
} nitro_key_t;

// trie.c
typedef struct nitro_prefix_trie_mem {
    void *ptr;

    struct nitro_prefix_trie_mem *prev;
    struct nitro_prefix_trie_mem *next;
} nitro_prefix_trie_mem;

typedef struct nitro_prefix_trie_node {
    uint8_t *rep;
    uint8_t length;
    struct nitro_prefix_trie_node *subs[256];

    nitro_prefix_trie_mem *members;

} nitro_prefix_trie_node;

typedef struct nitro_pipe_t *nitro_pipe_t_p;

typedef struct nitro_pipe_t {

    uv_tcp_t *tcp_socket;

    uint8_t *buffer;
    uint32_t buf_alloc;
    uint32_t buf_bytes;

    void *the_socket;
    void *dest_socket;
    nitro_frame_t *outgoing;

    struct nitro_pipe_t *prev;
    struct nitro_pipe_t *next;
    int registered;

    void (*do_write)(nitro_pipe_t_p, nitro_frame_t *);
    void (*do_sub)(nitro_pipe_t_p, char *);
    void (*do_unsub)(nitro_pipe_t_p, char *);

    nitro_key_t *sub_keys;
    uint8_t last_sub_hash[20];

    UT_hash_handle hh;
} nitro_pipe_t;

typedef struct nitro_socket_t *nitro_socket_t_p;

typedef struct nitro_socket_t {
    NITRO_SOCKET_TRANSPORT trans;

    uv_tcp_t *tcp_bound_socket;
    uv_tcp_t *tcp_connecting_handle;
    uv_connect_t tcp_connect;
    uv_timer_t tcp_sub_timer;

    nitro_frame_t *q_recv;
    nitro_frame_t *q_send;

    // locks for sending and receiving from other threads
    // control locks for maniuplating counts, queues, or pipes
    pthread_mutex_t l_recv;
    pthread_mutex_t l_send;
    pthread_mutex_t l_sub;

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

    // Various TCP-related things
    struct sockaddr_in tcp_location;
    int is_connecting;
    int outbound;
    double close_time;
    int close_refs;

    /* Options */
    uint32_t capacity;

    /* Subscription trie */
    nitro_prefix_trie_node *subs;

    /* Local "want subscription" list */
    nitro_key_t *sub_keys;

    nitro_counted_buffer *sub_data;
    uint32_t sub_data_length;

    /* for tcp connect list */
    struct nitro_socket_t *prev;
    struct nitro_socket_t *next;

    /* tcp deferred flushing */
    struct nitro_socket_t *flush_next;
    int flush_pending;
    /* hash table for bound inproc sockets */
    UT_hash_handle hh;

    /* handle a subscription/unsubscription */
    void (*do_sub)(nitro_socket_t_p, char *);
    void (*do_unsub)(nitro_socket_t_p, char *);

} nitro_socket_t;

nitro_socket_t *nitro_socket_bind(char *location);
nitro_socket_t *nitro_socket_connect(char *location);
void nitro_socket_close(nitro_socket_t *s);

nitro_frame_t *nitro_recv(nitro_socket_t *s);
void nitro_send(nitro_frame_t *fr, nitro_socket_t *s);
void nitro_pub(nitro_frame_t *fr, nitro_socket_t *s, char *key);
void nitro_sub(nitro_socket_t *s, char *key);

nitro_frame_t *nitro_frame_new_copy(void *data, uint32_t size);
nitro_frame_t *nitro_frame_new(void *data, uint32_t size, nitro_free_function ff, void *baton);
void *nitro_frame_data(nitro_frame_t *fr);
uint32_t nitro_frame_size(nitro_frame_t *fr);
void nitro_frame_destroy(nitro_frame_t *fr);

char *nitro_errmsg(NITRO_ERROR error);
NITRO_ERROR nitro_error();

#define INPROC_PREFIX "inproc://"
#define TCP_PREFIX "tcp://"

#endif /* NITRO_H */
