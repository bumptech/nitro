#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nitro.h"
#include "uthash/utlist.h"

#include <unistd.h>

int main(int argc, char **argv) {
    nitro_start();
    nitro_socket_t *s = nitro_bind_tcp("127.0.0.1:4444");
    assert(s);

    int x = 0;
    for (; x < 10000; x++) {
        nitro_frame_t *fr = nitro_recv(s);
        nitro_frame_destroy(fr);
    }

    nitro_frame_t *fr = nitro_frame_new_copy("world", 6);
    nitro_send(fr, s);

    nitro_socket_t *cs = nitro_connect_tcp("127.0.0.1:4444");
    fr = nitro_frame_new_copy("dogs!", 6);
    nitro_send(fr, cs);
    sleep(1);
    printf("yo\n");
    fr = nitro_frame_new_copy("dogs!", 6);

    nitro_socket_close(s);
    nitro_frame_t *in = nitro_recv(s);
    printf("yo, I got: %u %s\n", nitro_frame_size(in), (char *)nitro_frame_data(in));
    assert(!strcmp(nitro_frame_data(in), nitro_frame_data(fr)));
    printf("yo\n");
    nitro_socket_close(cs);

    sleep(100);

    return 0;
}
