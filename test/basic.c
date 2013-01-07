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
    nitro_start();
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

    sleep(2);

    nitro_stop();

    sleep(1);
}
