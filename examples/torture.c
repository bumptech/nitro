#include "nitro.h"

int count_backend;
int count_balance_out;
int count_balance_in;
int count_send;
int count_recv;

struct backend_state {
    nitro_socket_t *s;
    int double_back;
};

void * do_backend(void *p) {
    struct backend_state *st = (struct backend_state *)p;
    nitro_socket_t *s = st->s;

    while (1) {
        nitro_frame_t *fr = nitro_recv(s, 0);

        if (st->double_back) {
            uint64_t i = *(uint64_t *)nitro_frame_data(fr);
            i <<= 1;
            nitro_frame_t *out = nitro_frame_new_copy(&i, sizeof(uint64_t));
            int r = nitro_reply(fr, &out,  s, 0);
            nitro_frame_destroy(fr);
            assert(!r);
        } else {
            int r = nitro_reply(fr, &fr,  s, 0);
            assert(!r);
        }
        __sync_fetch_and_add(&count_backend, 1);
    }

    return NULL;
}

struct balancer_state {
    nitro_socket_t *f;
    nitro_socket_t *b;
    int double_out;
    int double_back;
};

void * do_balancer_out(void *p) {
    struct balancer_state *st = (struct balancer_state *)p;

    while (1) {
        nitro_frame_t *fr = nitro_recv(st->f, 0);

        if (st->double_out) {
            uint64_t i = *(uint64_t *)nitro_frame_data(fr);
            i <<= 1;
            nitro_frame_t *out = nitro_frame_new_copy(&i, sizeof(uint64_t));
            int r = nitro_relay_fw(fr, &out, st->b, 0);
            nitro_frame_destroy(fr);
            assert(!r);
        } else {
            int r = nitro_relay_fw(fr, &fr, st->b, 0);
            assert(!r);
        }
        __sync_fetch_and_add(&count_balance_out, 1);
    }

    return NULL;
}

void * do_balancer_in(void *p) {
    struct balancer_state *st = (struct balancer_state *)p;

    while (1) {
        nitro_frame_t *fr = nitro_recv(st->b, 0);

        if (st->double_back) {
            uint64_t i = *(uint64_t *)nitro_frame_data(fr);
            i <<= 1;
            nitro_frame_t *out = nitro_frame_new_copy(&i, sizeof(uint64_t));
            int r = nitro_relay_bk(fr, &out, st->f, 0);
            nitro_frame_destroy(fr);
            assert(!r);
        } else {
            int r = nitro_relay_bk(fr, &fr, st->f, 0);
            assert(!r);
        }
        __sync_fetch_and_add(&count_balance_in, 1);
    }

    return NULL;
}

struct sender_state {
    nitro_socket_t *s[3];
    uint64_t base;
    uint64_t factor;
    int stoptime;
    int *done;
};

void * do_sender(void *p) {
    struct sender_state *st = (struct sender_state *)p;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    uint64_t count = st->base;

    for (; tv.tv_sec < st->stoptime; ++count) {
        nitro_socket_t *s = st->s[count % 3];
        nitro_frame_t *fr = nitro_frame_new_copy(&count, sizeof(uint64_t));
        nitro_send(&fr, s, 0);
        __sync_fetch_and_add(&count_send, 1);
        fr = nitro_recv(s, 0);
        __sync_fetch_and_add(&count_recv, 1);

        uint64_t result = *(uint64_t *)nitro_frame_data(fr);
        assert(result == count * st->factor);
        __sync_fetch_and_add(st->done, 1);

        gettimeofday(&tv, NULL);
    }

    nitro_socket_close(st->s[0]);
    nitro_socket_close(st->s[1]);
    nitro_socket_close(st->s[2]);

    return NULL;
}

#define BIND(s, l) {\
    s = nitro_socket_bind(l, NULL);\
    assert(s);\
}
#define CONNECT(s, l) {\
    s = nitro_socket_connect(l, NULL);\
    assert(s);\
}

#define BIND_STCP(s, l) {\
    nitro_sockopt_t *_o = nitro_sockopt_new();\
    nitro_sockopt_set_secure(_o, 1);\
    s = nitro_socket_bind(l, _o);\
    assert(s);\
}
#define CONNECT_STCP(s, l) {\
    nitro_sockopt_t *_o = nitro_sockopt_new();\
    nitro_sockopt_set_secure(_o, 1);\
    s = nitro_socket_connect(l, _o);\
    assert(s);\
}


int main(int argc, char **argv) {
    unsigned int base;
    nitro_runtime_start();
    if (argc > 1) {
        base = atol(argv[1]);
    } else {
        struct timeval tv;

        gettimeofday(&tv, NULL);
        base = tv.tv_usec;
    }
    srandom(base);


    int job_time = random() % 20 + 2;
    int launch_pause = random() % 200; // ms
    int num_jobs = (random() % 100) + 20;
    int backend_threads = (random() % 15) + 1;
    char balancer_protocol[50];
    char backend_protocol[50];
    char sender_location[50];

    nitro_socket_t *backend = NULL;
    nitro_socket_t *balancers[3] = {0};
    nitro_socket_t *balancers_out[3] = {0};
    int sender_secure = 0;

    switch(random() % 3) {
    case 0:
        strcpy(balancer_protocol, "tcp");
        BIND(balancers[0], "tcp://*:4450");
        BIND(balancers[1], "tcp://*:4451");
        BIND(balancers[2], "tcp://*:4452");
        strcpy(sender_location, "tcp://127.0.0.1:445%d");
        break;
    case 1:
        strcpy(balancer_protocol, "stcp");
        BIND_STCP(balancers[0], "tcp://*:4450");
        BIND_STCP(balancers[1], "tcp://*:4451");
        BIND_STCP(balancers[2], "tcp://*:4452");
        strcpy(sender_location, "tcp://127.0.0.1:445%d");
        sender_secure = 1;
        break;
    case 2:
        strcpy(balancer_protocol, "inproc");
        BIND(balancers[0], "inproc://bal0");
        BIND(balancers[1], "inproc://bal1");
        BIND(balancers[2], "inproc://bal2");
        strcpy(sender_location, "inproc://bal%d");
        break;
    }

    switch(random() % 3) {
    case 0:
        strcpy(backend_protocol, "tcp");
        BIND(backend, "tcp://*:4440");
        CONNECT(balancers_out[0], "tcp://127.0.0.1:4440");
        CONNECT(balancers_out[1], "tcp://127.0.0.1:4440");
        CONNECT(balancers_out[2], "tcp://127.0.0.1:4440");
        break;

    case 1:
        strcpy(backend_protocol, "stcp");
        BIND_STCP(backend, "tcp://*:4440");
        CONNECT_STCP(balancers_out[0], "tcp://127.0.0.1:4440");
        CONNECT_STCP(balancers_out[1], "tcp://127.0.0.1:4440");
        CONNECT_STCP(balancers_out[2], "tcp://127.0.0.1:4440");
        break;

    case 2:
        strcpy(backend_protocol, "inproc");
        BIND(backend, "inproc://backend");
        CONNECT(balancers_out[0], "inproc://backend");
        CONNECT(balancers_out[1], "inproc://backend");
        CONNECT(balancers_out[2], "inproc://backend");
        break;
    }

    uint64_t double_proxy_fw = random() % 2;
    uint64_t double_proxy_bk = random() % 2;
    uint64_t double_backend = random() % 2;

    uint64_t factor = 1 << (double_proxy_fw + double_proxy_bk + double_backend);

    printf("Run id is %u\n", base);
    printf("Balancer protocol is %s\n", balancer_protocol);
    printf("Backend protocol is %s\n", backend_protocol);
    printf("Doubling: fw=%d, be=%d, bk=%d factor=%d\n",
        (int)double_proxy_fw, (int)double_backend, (int)double_proxy_bk, (int)factor);
    printf("Jobs=%d, pause=%dms, runtime=%ds, backend threads=%d\n", 
        num_jobs, launch_pause, job_time, backend_threads);

    int i = 0;
    struct backend_state be_st = {backend, double_backend};
    pthread_t *backends = alloca(backend_threads * sizeof(pthread_t));
    for (; i < backend_threads; i++) {
        pthread_create(&backends[i], NULL, do_backend, &be_st);
    }
    pthread_t balancer_out_threads[3];
    pthread_t balancer_in_threads[3];
    struct balancer_state balancer_states[] = {
        {balancers[0], balancers_out[0], double_proxy_fw, double_proxy_bk},
        {balancers[1], balancers_out[1], double_proxy_fw, double_proxy_bk},
        {balancers[2], balancers_out[2], double_proxy_fw, double_proxy_bk},
    };

    for (i=0; i < 3; ++i) {
        pthread_create(&balancer_out_threads[i], NULL, do_balancer_out, &balancer_states[i]);
        pthread_create(&balancer_in_threads[i], NULL, do_balancer_in, &balancer_states[i]);
    }

    int all_done = 0;

    pthread_t *job_threads = alloca(num_jobs * sizeof(pthread_t));

    sleep(2);
    printf("Spawning %d jobs:\n", num_jobs);
    for (i=0; i < num_jobs; ++i) {
        struct timespec pause_time = {0, launch_pause * 1000000};
        nanosleep(&pause_time, NULL);
        struct sender_state *send_st = alloca(sizeof(struct sender_state));
        char buf[50];
        if (sender_secure) {
            snprintf(buf, 50, sender_location, 0);
            CONNECT_STCP(send_st->s[0], buf);
            snprintf(buf, 50, sender_location, 1);
            CONNECT_STCP(send_st->s[1], buf);
            snprintf(buf, 50, sender_location, 2);
            CONNECT_STCP(send_st->s[2], buf);
        } else {
            snprintf(buf, 50, sender_location, 0);
            CONNECT(send_st->s[0], buf);
            snprintf(buf, 50, sender_location, 1);
            CONNECT(send_st->s[1], buf);
            snprintf(buf, 50, sender_location, 2);
            CONNECT(send_st->s[2], buf);
        }
        send_st->base = random() % (1 << 31);
        send_st->factor = factor;
        send_st->done = &all_done;
        struct timeval tv;
        gettimeofday(&tv, NULL);
        send_st->stoptime = tv.tv_sec + job_time;
        pthread_create(&job_threads[i], NULL, do_sender, send_st);
        write(1, ".", 1);
    }

    void *res;
    printf("\nWaiting for %d jobs:\n", num_jobs);
    for (i=0; i < num_jobs; ++i) {
        pthread_join(job_threads[i], &res);
        write(1, ".", 1);
    }
    printf("\n ~~ successfully processed %d RPCs ~~\n", all_done);

    return 0;
}
