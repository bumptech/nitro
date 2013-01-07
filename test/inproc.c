#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nitro.h"
#include "wvtest.h"

#include <unistd.h>

WVTEST_MAIN("basic_inproc") {
    nitro_start();

    nitro_socket_t *ins = nitro_socket_bind("inproc://inbox");

    nitro_socket_t *inout = nitro_socket_connect("inproc://inbox");
    int i;

    nitro_frame_t *fra = nitro_frame_new_copy("world", 6);
    for (i = 0; i < 70; i++) {
        nitro_send(fra, inout);
    }
    nitro_frame_destroy(fra);
    for (i = 0; i < 70; i++) {

        nitro_frame_t * f = nitro_recv(ins);
        WVPASS(nitro_frame_size(f) == 6 && !memcmp(nitro_frame_data(f), "world", 5));
        nitro_frame_destroy(f);
    }
    
    nitro_frame_t * f = nitro_frame_new_copy("hello", 6);

    nitro_send(f, ins);
    f = nitro_recv(inout);
    WVPASS(nitro_frame_size(f) == 6 && !memcmp(nitro_frame_data(f), "hello", 5));

    nitro_frame_destroy(f);
}
