#include "nitro.h"
#include <unistd.h>
#include "test.h"

static int mode;

int main(int argc, char **argv) {
    if (argc > 1) {
        mode = atoi(argv[1]);
    }
    nitro_runtime_start();
    nitro_sockopt_t *opt = nitro_sockopt_new();
    nitro_sockopt_t *opt1 = nitro_sockopt_new();
    nitro_socket_t *s = NULL;
    nitro_socket_t *c = NULL;

    switch (mode) {
    case 0:
        s = nitro_socket_bind("tcp://127.0.0.1:4444", opt);
        c = nitro_socket_connect("tcp://127.0.0.1:4444", opt1);
        break;
    case 1:
        nitro_sockopt_set_secure(opt, 1);
        nitro_sockopt_set_secure(opt1, 1);
        s = nitro_socket_bind("tcp://127.0.0.1:4444", opt);
        c = nitro_socket_connect("tcp://127.0.0.1:4444", opt1);
        break;
    case 2:
        s = nitro_socket_bind("inproc://foobar", opt);
        c = nitro_socket_connect("inproc://foobar", opt1);
        break;
    }

    sleep(1);

    nitro_frame_t *fr = nitro_frame_new_copy("dog", 4);
    int sent = nitro_pub(&fr, (uint8_t *)"foxy", 4, s, 0);
    TEST("No hits on 'foxy'", sent == 0);

    nitro_sub(c, (uint8_t *)"fox", 3);

    sleep(1);
    nitro_sub(c, (uint8_t *)"box", 3);

    sleep(1);
    fr = nitro_frame_new_copy("frog", 5);
    sent = nitro_pub(&fr, (uint8_t *)"boxy", 4, s, 0);
    TEST("1 hits on 'boxy'", sent == 1);
    fr = nitro_recv(c, 0);
    TEST("Boxy frame was as expected", !strcmp((char *)nitro_frame_data(fr), "frog"));
    nitro_frame_destroy(fr);

    sleep(1);
    nitro_sub(c, (uint8_t *)"lox", 3);

    int r = nitro_unsub(c, (uint8_t *)"box", 3);
    TEST("unsub worked #1", !r);

    r = nitro_unsub(c, (uint8_t *)"lox", 3);
    TEST("unsub worked #2", !r);

    sleep(1);

    fr = nitro_frame_new_copy("dog", 4);
    sent = nitro_pub(&fr, (uint8_t *)"foxy", 4, s, 0);
    TEST("One hit on 'foxy'", sent == 1);

    fr = nitro_recv(c, 0);
    TEST("Foxy frame was as expected", !strcmp((char *)nitro_frame_data(fr), "dog"));
    nitro_frame_destroy(fr);

    fr = nitro_frame_new_copy("frog", 5);
    sent = nitro_pub(&fr, (uint8_t *)"boxy", 4, s, 0);
    TEST("(Post unsub) No hits on 'boxy'", sent == 0);

    return 0;
}
