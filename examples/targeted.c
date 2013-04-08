#include "nitro.h"
#include <unistd.h>

#define MESSAGES 10000

void recipient() {
    nitro_socket_t *s = nitro_socket_bind("tcp://127.0.0.1:4444");
    if (!s) {
        printf("error on bind: %s\n", nitro_errmsg(nitro_error()));
        exit(1);
    }

    int i;
    for (i=0; i < MESSAGES; ++i) {
        nitro_frame_t *fr = nitro_recv(s, 0);
        int p = nitro_reply(fr, fr, s, 0);
        assert(!p);
        nitro_frame_destroy(fr);
    }
}

void *sender(void *p) {
    int id = *(int*)p;
    nitro_socket_t *s = nitro_socket_connect("tcp://127.0.0.1:4444");
    if (!s) {
        printf("error on connect: %s\n", nitro_errmsg(nitro_error()));
        exit(1);
    }

    char buf[5];
    snprintf(buf, 5, "%d", id);
    int l = strlen(buf) + 1;

    int i;
    for (i=0; i < MESSAGES + 5; ++i) {
        nitro_frame_t *fr = nitro_frame_new_copy(buf, l);
        nitro_send(fr, s, 0);
        nitro_frame_destroy(fr);
        fr = nitro_recv(s, 0);
        assert(!strcmp(nitro_frame_data(fr), buf));
        nitro_frame_destroy(fr);
    }

    return NULL;
}

int main(int argc, char **argv) {
    nitro_runtime_start();

    pthread_t t1, t2;

    int id1 = 1, id2 = 2;
    pthread_create(&t1, NULL, sender, &id1);
    pthread_create(&t2, NULL, sender, &id2);

    recipient();

    sleep(1);
    return 0;
}
