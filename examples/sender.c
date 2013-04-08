#include "nitro.h"
#include <unistd.h>

#define MESSAGES 10000000

int main(int argc, char **argv) {
    nitro_runtime_start();

    nitro_socket_t *s = nitro_socket_connect("tcp://127.0.0.1:4444", NULL);

    if (!s) {
        printf("error on connect: %s\n", nitro_errmsg(nitro_error()));
        exit(1);
    }

    sleep(1);

    int i = 0;
    nitro_frame_t *out = nitro_frame_new_copy("hello", 6);

    struct timeval tstart;

    gettimeofday(&tstart, NULL);
    for (i=0; i < MESSAGES; ++i) {
        nitro_send(out, s, 0);
    }
    printf("started %d messages: %d.%d\n", MESSAGES,
        (int)tstart.tv_sec, (int)tstart.tv_usec);

    sleep(10);


    return 0;
}
