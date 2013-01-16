#ifndef NITRO_PRIV_H
#define NITRO_PRIV_H

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "uthash/utlist.h"

#define ZALLOC(p) {p = (typeof(p))calloc(1, sizeof(*p));}

enum {
    NITRO_PACKET_FRAME,
    NITRO_PACKET_SUB
};

typedef struct nitro_runtime {
    uv_loop_t *the_loop;
    pthread_t the_thread;

    /* TCP specific things */
    nitro_socket_t *want_tcp_pair;
    pthread_mutex_t l_tcp_pair;

    pthread_mutex_t dm;
    pthread_cond_t dc;

    uv_timer_t tcp_timer;
    uv_async_t tcp_trigger;
    uv_async_t done_wake;

    int run;
    uint32_t num_sock;

} nitro_runtime;

extern nitro_runtime *the_runtime;

// setup.c
inline uv_loop_t *nitro_loop();

// err.c
int nitro_set_error(NITRO_ERROR e);

// tcp.c
void tcp_poll(uv_timer_t *handle, int status);
void tcp_poll_cb(uv_async_t *handle, int status);
nitro_socket_t *nitro_bind_tcp(char *location);
nitro_socket_t *nitro_connect_tcp(char *location);
void nitro_close_tcp(nitro_socket_t *s);

// inproc.c
nitro_socket_t *nitro_bind_inproc(char *location);
nitro_socket_t *nitro_connect_inproc(char *location);
void nitro_close_inproc(nitro_socket_t *s);

// util.c
nitro_counted_buffer *nitro_counted_buffer_new(void *backing, nitro_free_function ff, void *baton);
void nitro_counted_buffer_incref(nitro_counted_buffer *buf);
void nitro_counted_buffer_decref(nitro_counted_buffer *buf);

void buffer_decref(void *data, void *bufptr);
void just_free(void *data, void *unused);
double now_double();

// core.c
nitro_socket_t *nitro_socket_new();
void nitro_socket_destroy();

void socket_flush(nitro_socket_t *s);
nitro_frame_t *nitro_frame_copy(nitro_frame_t *f);
void destroy_pipe(nitro_pipe_t *p);
nitro_socket_t *nitro_connect_inproc(char *location);

nitro_key_t *nitro_key_new(char *key);
void add_pub_filter(nitro_socket_t *s, nitro_pipe_t *p, char *key);
void remove_pub_filters(nitro_socket_t *s, nitro_pipe_t *p);
nitro_pipe_t *nitro_pipe_new();

typedef void (*nitro_prefix_trie_search_callback)
(uint8_t *pfx, uint8_t length, nitro_prefix_trie_mem *members, void *baton);

// trie.c
void nitro_prefix_trie_search(
    nitro_prefix_trie_node *t, uint8_t *rep, uint8_t length,
    nitro_prefix_trie_search_callback cb, void *baton);
void nitro_prefix_trie_add(nitro_prefix_trie_node **t,
                           uint8_t *rep, uint8_t length, void *ptr);
int nitro_prefix_trie_del(nitro_prefix_trie_node *t,
                          uint8_t *rep, uint8_t length, void *ptr);
void nitro_prefix_trie_destroy(nitro_prefix_trie_node *t);

// sha1.c

/*
SHA-1 in C
By Steve Reid <steve@edmweb.com>
100% Public Domain
*/

typedef struct {
    u_int32_t state[5];
    u_int32_t count[2];
    unsigned char buffer[64];
} SHA1_CTX;

void SHA1Transform(u_int32_t state[5], const unsigned char buffer[64]);
void SHA1Init(SHA1_CTX *context);
void SHA1Update(SHA1_CTX *context, const unsigned char *data, u_int32_t len);
void SHA1Final(unsigned char digest[20], SHA1_CTX *context);

#endif /* NITRO_PRIV_H */
