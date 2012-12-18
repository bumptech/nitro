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
    
    nitro_socket_t *pub = nitro_socket_bind("inproc://inbox");
    
    nitro_socket_t *sub1 = nitro_socket_connect("inproc://inbox");
    nitro_socket_t *sub2 = nitro_socket_connect("inproc://inbox");
    
    
    nitro_frame_t *fra1 = nitro_frame_new_copy("hello", 6);
    nitro_frame_t *fra2 = nitro_frame_new_copy("cruel", 6);
    nitro_frame_t *fra3 = nitro_frame_new_copy("world", 6);
    nitro_sub(sub1, "key1");
    nitro_sub(sub1, "key3");
    
    nitro_sub(sub2, "key1");
    nitro_sub(sub2, "key2");
    nitro_sub(sub2, "key3");
    nitro_pub(fra1, pub, "key1");
    nitro_pub(fra2, pub, "key2");
    nitro_pub(fra3, pub, "key3");
    
    nitro_frame_t * f = nitro_recv(sub1);
    printf("got frame length = %u and content = '%s' count = %d!\n", nitro_frame_size(f), (char *)nitro_frame_data(f), 1);
    nitro_frame_destroy(f);
    f = nitro_recv(sub1);
    printf("got frame length = %u and content = '%s' count = %d!\n", nitro_frame_size(f), (char *)nitro_frame_data(f), 1);
    nitro_frame_destroy(f);
    
    f = nitro_recv(sub2);
    printf("got frame length = %u and content = '%s' count = %d!\n", nitro_frame_size(f), (char *)nitro_frame_data(f), 1);
    nitro_frame_destroy(f);
    f = nitro_recv(sub2);
    printf("got frame length = %u and content = '%s' count = %d!\n", nitro_frame_size(f), (char *)nitro_frame_data(f), 1);
    nitro_frame_destroy(f);
    f = nitro_recv(sub2);
    printf("got frame length = %u and content = '%s' count = %d!\n", nitro_frame_size(f), (char *)nitro_frame_data(f), 1);
    nitro_frame_destroy(f);
    
    
    nitro_frame_destroy(fra1);
    nitro_frame_destroy(fra2);
    nitro_frame_destroy(fra3);
    
    
    //
    //    for (i = 0; i < 70000; i++) {
    //        nitro_send(fra, inout);
    //    }
    //    nitro_frame_destroy(fra);
    //    for (i = 0; i < 70000; i++) {
    //
    //        nitro_frame_t * f = nitro_recv(ins);
    //        printf("got frame length = %u and content = '%s' count = %d!\n", nitro_frame_size(f), (char *)nitro_frame_data(f), 1);
    //        nitro_frame_destroy(f);
    //
    //    }
    //
    //
    //
    //   nitro_frame_t * f = nitro_frame_new_copy("hello", 6);
    //
    //    nitro_send(f, ins);
    //    f = nitro_recv(inout);
    //    printf("got frame length = %u and content = '%s' count = %d!\n", nitro_frame_size(f), (char *)nitro_frame_data(f), 1);
    //
    //    nitro_frame_destroy(f);
    return 0;
}