#include "nitro.h"
#include <unistd.h>

#define MESSAGES 10000000

int main(int argc, char **argv) {
    nitro_runtime_start();

    nitro_socket_t *s = nitro_socket_bind("tcp://127.0.0.1:4444", NULL);
    if (!s) {
        printf("error on bind: %s\n", nitro_errmsg(nitro_error()));
        exit(1);
    }

    nitro_socket_t *c = nitro_socket_connect("tcp://127.0.0.1:4444", NULL);

    if (!c) {
        printf("error on connect: %s\n", nitro_errmsg(nitro_error()));
        exit(1);
    }

    nitro_sub(c, (uint8_t *)"fox", 3);

    sleep(1);
    nitro_sub(c, (uint8_t *)"box", 3);

    sleep(1);
    nitro_sub(c, (uint8_t *)"lox", 3);

    sleep(1);
    int r = nitro_unsub(c, (uint8_t *)"box", 3);
    assert(!r);

    sleep(1);

    r = nitro_unsub(c, (uint8_t *)"lox", 3);
    assert(!r);

    sleep(1);

    nitro_frame_t *fr = nitro_frame_new_copy("dog", 4);
    int sent = nitro_pub(fr, (uint8_t *)"foxy", 4, s);
    printf("sent was: %d\n", sent);
    assert(sent == 1);

    nitro_frame_destroy(fr);

    fr = nitro_recv(c, 0);

    assert(!strcmp((char *)nitro_frame_data(fr), "dog"));

    printf("done!!!\n");

    return 0;
}
