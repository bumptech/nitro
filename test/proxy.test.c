#include "test.h"
#include "nitro.h"

static int mode;

struct t_1 {
    int got[10];
    pthread_t kids[10];
};

void *recipient(void *p) {
    char *bind_loc = (char *)p;
    nitro_socket_t *s;
    nitro_sockopt_t *opt = nitro_sockopt_new();

    if (mode == 1) {
        nitro_sockopt_set_secure(opt, 1);
    }

    s = nitro_socket_bind(bind_loc, opt);
    if (!s) {
        printf("error on bind: %s\n", nitro_errmsg(nitro_error()));
        exit(1);
    }

    int i;
    for (i=0; i < 5000; i++) {
        nitro_frame_t *fr = nitro_recv(s, 0);
        int p = nitro_reply(fr, &fr, s, 0);
        assert(!p);
    }

    return NULL;
}

void *proxy(void *ptr) {
    nitro_socket_t *outs[2];
    nitro_socket_t *inp = NULL;

    nitro_sockopt_t *opt = nitro_sockopt_new();
    nitro_sockopt_t *opt1 = nitro_sockopt_new();
    nitro_sockopt_t *opt2 = nitro_sockopt_new();

    switch (mode) {
    case 0:
        inp = nitro_socket_bind("tcp://127.0.0.1:4443", opt);
        outs[0] = nitro_socket_connect("tcp://127.0.0.1:4444", opt1);
        outs[1] = nitro_socket_connect("tcp://127.0.0.1:4445", opt2);
        break;
    case 1:
        nitro_sockopt_set_secure(opt, 1);
        nitro_sockopt_set_secure(opt1, 1);
        nitro_sockopt_set_secure(opt2, 1);
        inp = nitro_socket_bind("tcp://127.0.0.1:4443", opt);
        outs[0] = nitro_socket_connect("tcp://127.0.0.1:4444", opt1);
        outs[1] = nitro_socket_connect("tcp://127.0.0.1:4445", opt2);
        break;
    case 2:
        inp = nitro_socket_bind("inproc://front", opt);
        outs[0] = nitro_socket_connect("inproc://back1", opt1);
        outs[1] = nitro_socket_connect("inproc://back2", opt2);
        break;
    }

    int p;
    int i;
    for (i=0; i < 10000; i++) {
        nitro_socket_t *outp = outs[i % 2];
        nitro_frame_t *fr = nitro_recv(inp, 0);
        p = nitro_relay_fw(fr, &fr, outp, 0);
        assert(!p);
        fr = nitro_recv(outp, 0);
        nitro_relay_bk(fr, &fr, inp, 0);
        assert(!p);
    }
    return NULL;
}

void *sender(void *p) {
    struct t_1 *acc = (struct t_1 *)p;

    int i;
    for (i=0; i < 10; i++) {
        if (pthread_self() == acc->kids[i]) {
            break;
        }
    }

    int id = i;

    nitro_socket_t *s = NULL;
    nitro_sockopt_t *opt = nitro_sockopt_new();

    switch (mode) {
    case 0:
        s = nitro_socket_connect("tcp://127.0.0.1:4443", opt);
        break;
    case 1:
        nitro_sockopt_set_secure(opt, 1);
        s = nitro_socket_connect("tcp://127.0.0.1:4443", opt);
        break;
    case 2:
        s = nitro_socket_connect("inproc://front", opt);
        break;
    }

    int base = id * 1000;

    int count = 0;
    /* pipeline... */
    for (i=base; i < base + 1000; ++i) {
        nitro_frame_t *fr = nitro_frame_new_copy(&i, sizeof(int));
        nitro_send(&fr, s, 0);
    }

    /* pipeline... */
    for (i=base; i < base + 1000; ++i, ++count) {
        nitro_frame_t *fr = nitro_recv(s, 0);
        if (*(int*)nitro_frame_data(fr) != i) {
            break;
        }
        nitro_frame_destroy(fr);
    }

    acc->got[id] = count;

    return NULL;
}

int main(int argc, char **argv) {
    if (argc > 1) {
        mode = atoi(argv[1]);
    }
    nitro_runtime_start();

    pthread_t r1, r2, prox;

    struct t_1 acc1;
    bzero(&acc1, sizeof(acc1));

    switch(mode) {
    case 0:
    case 1:
        pthread_create(&r1, NULL, recipient, "tcp://127.0.0.1:4444");
        pthread_create(&r2, NULL, recipient, "tcp://127.0.0.1:4445");
        break;
    case 2:
        pthread_create(&r1, NULL, recipient, "inproc://back1");
        pthread_create(&r2, NULL, recipient, "inproc://back2");
        break;
    }
    sleep(1);
    pthread_create(&prox, NULL, proxy, NULL);
    sleep(1);

    int i;
    for (i=0; i < 10; i++) {
        pthread_create(&acc1.kids[i], NULL, sender, &acc1);

    }

    void *res = NULL;

    pthread_join(r1, res);
    pthread_join(r2, res);
    pthread_join(prox, res);

    for (i=0; i < 10; i++) {
        pthread_join(acc1.kids[i], res);
        TEST("proxy(sender) all 1,000 of mine matched", acc1.got[i] == 1000);
    }

    SUMMARY(0);
    return 1;
}
