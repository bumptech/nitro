#ifndef RUNTIME_H
#define RUNTIME_H
#include "common.h"
#include "async.h"

typedef struct nitro_runtime {
    struct ev_loop *the_loop;
    pthread_t the_thread;

    /* TCP specific things */
    nitro_socket_t *want_tcp_connect;
    pthread_mutex_t l_tcp_connect;

    /* CAS stack */
    _Atomic (nitro_async_t *)async_stack;

    pthread_mutex_t l_die;
    pthread_cond_t c_die;

    ev_async thread_wake;

    uint32_t num_sock;

} nitro_runtime;

extern nitro_runtime *the_runtime;

int nitro_runtime_start();
int nitro_runtime_stop();

#define NITRO_THREAD_CHECK {\
    assert(pthread_equal(pthread_self(), the_runtime->the_thread));\
}

#endif /* RUNTIME_H */
