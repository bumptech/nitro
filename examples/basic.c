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

    int i;
    for (i=0; i < MESSAGES; ++i) {
        nitro_frame_t *fr = nitro_recv(s, 0);
        nitro_frame_destroy(fr);
    }
    struct timeval tend;
    gettimeofday(&tend, NULL);

    printf("ended %d messages: %d.%d\n", MESSAGES,
        (int)tend.tv_sec, (int)tend.tv_usec);

    return 0;
}
