#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nitro.h"
#include "../src/uthash/utlist.h"

#include <unistd.h>

int main(int argc, char **argv) {
    nitro_start();

    nitro_socket_t *ins = nitro_socket_bind("inproc://inbox");

    nitro_socket_t *inout = nitro_socket_connect("inproc://inbox");
    int i;


    nitro_frame_t *fra = nitro_frame_new_copy("world", 6);
    for (i = 0; i < 70000; i++) {
        nitro_send(fra, inout);
    }
    nitro_frame_destroy(fra);
    for (i = 0; i < 70000; i++) {

        nitro_frame_t * f = nitro_recv(ins);
        printf("got frame length = %u and content = '%s' count = %d!\n", nitro_frame_size(f), (char *)nitro_frame_data(f), 1);
        nitro_frame_destroy(f);

    }
    

    
   nitro_frame_t * f = nitro_frame_new_copy("hello", 6);

    nitro_send(f, ins);
    f = nitro_recv(inout);
    printf("got frame length = %u and content = '%s' count = %d!\n", nitro_frame_size(f), (char *)nitro_frame_data(f), 1);

    nitro_frame_destroy(f);
    return 0;
}