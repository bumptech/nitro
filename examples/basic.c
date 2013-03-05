#include "nitro.h"
#include <unistd.h>


int main(int argc, char **argv) {
    nitro_start();

    nitro_socket_t *s = nitro_socket_bind("tcp://127.0.0.1:4444");
    if (!s) {
        printf("error on bind: %s\n", nitro_errmsg(nitro_error()));
        exit(1);
    }

    sleep(20);


    return 0;
}
