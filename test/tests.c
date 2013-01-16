#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nitro.h"
#include "uthash/utlist.h"
#include "wvtest.h"

#include <unistd.h>

WVTEST_MAIN("basic_tcp") {
    WVPASS(!nitro_start());
    nitro_socket_t *s = nitro_socket_bind("tcp://127.0.0.1:4444");
    WVPASS(s);

    nitro_socket_t *cs = nitro_socket_connect("tcp://127.0.0.1:4444");
    WVPASS(cs);

    nitro_frame_t *fr = nitro_frame_new_copy("dogs!", 6);
    nitro_send(fr, cs);
    nitro_frame_destroy(fr);
    sleep(1);
    fr = nitro_frame_new_copy("dogs!", 6);

    nitro_socket_close(s);
    nitro_frame_t *in = nitro_recv(s);
    WVPASS(!strcmp(nitro_frame_data(in), nitro_frame_data(fr)));
    nitro_socket_close(cs);
    nitro_frame_destroy(fr);
    nitro_frame_destroy(in);

    sleep(1);
    WVPASS(!nitro_stop());
}

WVTEST_MAIN("basic_inproc") {
    WVPASS(!nitro_start());

    nitro_socket_t *ins = nitro_socket_bind("inproc://inbox");
    WVPASS(ins);

    nitro_socket_t *inout = nitro_socket_connect("inproc://inbox");
    WVPASS(inout);
    int i;
    fprintf(stderr, "waiting on this 0\n");

    nitro_frame_t *fra = nitro_frame_new_copy("world", 6);
    WVPASS(fra);
    fprintf(stderr, "waiting on this 1\n");
    for (i = 0; i < 70; i++) {
        nitro_send(fra, inout);
    }
    nitro_frame_destroy(fra);
    fprintf(stderr, "waiting on this 2\n");
    for (i = 0; i < 70; i++) {
        fprintf(stderr, "iter is %d\n", i);

        nitro_frame_t * f = nitro_recv(ins);
        WVPASS(nitro_frame_size(f) == 6 && !memcmp(nitro_frame_data(f), "world", 5));
        nitro_frame_destroy(f);
    }

    nitro_frame_t * f = nitro_frame_new_copy("hello", 6);

    nitro_send(f, ins);
    nitro_frame_destroy(f);
    f = nitro_recv(inout);
    WVPASS(nitro_frame_size(f) == 6 && !memcmp(nitro_frame_data(f), "hello", 5));

    nitro_frame_destroy(f);

    nitro_socket_close(inout);
    nitro_socket_close(ins);

    sleep(1);
    WVPASS(!nitro_stop());
}

WVTEST_MAIN("pubsub_basic_tcp") {
    WVPASS(!nitro_start());
    
    nitro_socket_t *pub = nitro_socket_bind("tcp://127.0.0.1:4444");
    
    nitro_socket_t *sub1 = nitro_socket_connect("tcp://127.0.0.1:4444");
    nitro_socket_t *sub2 = nitro_socket_connect("tcp://127.0.0.1:4444");

    WVPASS(pub);
    WVPASS(sub1);
    WVPASS(sub2);
    
    
    nitro_frame_t *fra1 = nitro_frame_new_copy("hello", 6);
    nitro_frame_t *fra2 = nitro_frame_new_copy("cruel", 6);
    nitro_frame_t *fra3 = nitro_frame_new_copy("world", 6);
    nitro_sub(sub1, "key1");
    nitro_sub(sub1, "key3");
    
    nitro_sub(sub2, "key");
    sleep(1);
    nitro_pub(fra1, pub, "key1");
    nitro_pub(fra2, pub, "key2");
    nitro_pub(fra3, pub, "key3");
    
    nitro_frame_t * f = nitro_recv(sub1);
    WVPASS(!strcmp(nitro_frame_data(f), "hello"));
    nitro_frame_destroy(f);
    f = nitro_recv(sub1);
    WVPASS(!strcmp(nitro_frame_data(f), "world"));
    nitro_frame_destroy(f);

    
    f = nitro_recv(sub2);
    WVPASS(!strcmp(nitro_frame_data(f), "hello"));
    nitro_frame_destroy(f);
    f = nitro_recv(sub2);
    WVPASS(!strcmp(nitro_frame_data(f), "cruel"));
    nitro_frame_destroy(f);
    f = nitro_recv(sub2);
    WVPASS(!strcmp(nitro_frame_data(f), "world"));
    nitro_frame_destroy(f);
    
    
    nitro_frame_destroy(fra1);
    nitro_frame_destroy(fra2);
    nitro_frame_destroy(fra3);

    nitro_socket_close(pub);
    nitro_socket_close(sub1);
    nitro_socket_close(sub2);

    sleep(1);
    WVPASS(!nitro_stop());
}

WVTEST_MAIN("pubsub_basic_inproc") {
    WVPASS(!nitro_start());
    
    nitro_socket_t *pub = nitro_socket_bind("inproc://pubber");
    
    nitro_socket_t *sub1 = nitro_socket_connect("inproc://pubber");
    nitro_socket_t *sub2 = nitro_socket_connect("inproc://pubber");

    WVPASS(pub);
    WVPASS(sub1);
    WVPASS(sub2);
    
    
    nitro_frame_t *fra1 = nitro_frame_new_copy("hello", 6);
    nitro_frame_t *fra2 = nitro_frame_new_copy("cruel", 6);
    nitro_frame_t *fra3 = nitro_frame_new_copy("world", 6);
    nitro_sub(sub1, "key1");
    nitro_sub(sub1, "key3");
    
    nitro_sub(sub2, "key");
    sleep(1);
    nitro_pub(fra1, pub, "key1");
    nitro_pub(fra2, pub, "key2");
    nitro_pub(fra3, pub, "key3");
    
    nitro_frame_t * f = nitro_recv(sub1);
    WVPASS(!strcmp(nitro_frame_data(f), "hello"));
    nitro_frame_destroy(f);
    f = nitro_recv(sub1);
    WVPASS(!strcmp(nitro_frame_data(f), "world"));
    nitro_frame_destroy(f);

    
    f = nitro_recv(sub2);
    WVPASS(!strcmp(nitro_frame_data(f), "hello"));
    nitro_frame_destroy(f);
    f = nitro_recv(sub2);
    WVPASS(!strcmp(nitro_frame_data(f), "cruel"));
    nitro_frame_destroy(f);
    f = nitro_recv(sub2);
    WVPASS(!strcmp(nitro_frame_data(f), "world"));
    nitro_frame_destroy(f);
    
    
    nitro_frame_destroy(fra1);
    nitro_frame_destroy(fra2);
    nitro_frame_destroy(fra3);

    fprintf(stderr, "one\n?");
    nitro_socket_close(pub);
    fprintf(stderr, "two\n?");
    nitro_socket_close(sub1);
    nitro_socket_close(sub2);

    sleep(1);
    WVPASS(!nitro_stop());
}
