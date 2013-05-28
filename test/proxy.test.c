#include "test.h"
#include "nitro.h"

struct t_1 {
    int got[10];
    pthread_t kids[10];
};

void *recipient(void *p) {
    char *bind_loc = (char *)p;
    nitro_socket_t *s = nitro_socket_bind(bind_loc, NULL);
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
    nitro_socket_t *inp = nitro_socket_bind("tcp://127.0.0.1:4443", NULL);
    outs[0] = nitro_socket_connect("tcp://127.0.0.1:4444", NULL);
    outs[1] = nitro_socket_connect("tcp://127.0.0.1:4445", NULL);

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

    nitro_socket_t *s = nitro_socket_connect("tcp://127.0.0.1:4443", NULL);
    if (!s) {
        printf("error on connect: %s\n", nitro_errmsg(nitro_error()));
        exit(1);
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
    nitro_runtime_start();

    pthread_t r1, r2, prox;

    struct t_1 acc1;
    bzero(&acc1, sizeof(acc1));

    pthread_create(&r1, NULL, recipient, "tcp://127.0.0.1:4444");
    pthread_create(&r2, NULL, recipient, "tcp://127.0.0.1:4445");
    pthread_create(&prox, NULL, proxy, "tcp://127.0.0.1:4445");

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
        fprintf(stderr, "thread got: %d\n", acc1.got[i]);
        TEST("proxy(sender) all 1,000 of mine matched", acc1.got[i] == 1000);
    }

    SUMMARY(0);
    return 1;
}
