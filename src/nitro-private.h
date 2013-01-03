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
void nitro_close_tcp(nitro_socket_t *s);

// inproc.c
nitro_socket_t * nitro_bind_inproc(char *location);
void nitro_close_inproc(nitro_socket_t *s);

// util.c
typedef struct nitro_counted_buffer {
    void *ptr;
    int count;
    pthread_mutex_t lock;
    nitro_free_function ff;
    void *baton;
} nitro_counted_buffer;
nitro_counted_buffer * nitro_counted_buffer_new(void *backing, nitro_free_function ff, void *baton);

void buffer_decref(void *data, void *bufptr);
void buffer_incref(void *bufptr);
void just_free(void *data, void *unused);
double now_double();

// core.c
nitro_socket_t * nitro_socket_new();
void nitro_socket_destroy();

void socket_flush(nitro_socket_t *s);
nitro_frame_t *nitro_frame_copy(nitro_frame_t *f);
void destroy_pipe(nitro_pipe_t *p);
nitro_socket_t * nitro_connect_inproc(char *location);

nitro_key_t *nitro_key_new(char *key);
void add_pub_filter(nitro_socket_t *s, nitro_pipe_t *p, char *key);
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

#endif /* NITRO_PRIV_H */
