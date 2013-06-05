#include <nitro.h>

void *sender(void *unused) {
    int x = 0;
    nitro_socket_t *sock =
        nitro_socket_connect("tcp://127.0.0.1:4444", NULL);
    for (; x < 100000; ++x) {
        nitro_frame_t *fr = nitro_frame_new_copy("hello", 6);
        nitro_send(&fr, sock, 0);
        fr = nitro_recv(sock, 0);
        nitro_frame_destroy(fr);
    }
    return NULL;
}

void *echoer(void *unused) {
    nitro_socket_t *sock =
        nitro_socket_bind("tcp://*:4444", NULL);
    while (1) {
        nitro_frame_t *fr = nitro_recv(sock, 0);
        nitro_reply(fr, &fr, sock, 0);
    }
    return NULL;
}


int main(int argc, char **argv) {
    pthread_t t1, t2;
    nitro_runtime_start();
    pthread_create(&t1, NULL, sender, NULL);
    pthread_create(&t2, NULL, echoer, NULL);
    void *res;
    pthread_join(t1, &res);
    return 0;
}
