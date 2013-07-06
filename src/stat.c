#include "socket.h"
#include "runtime.h"
#include "Stcp.h"
#include "Sinproc.h"

void stat_handle_usr1(int sig) {
    if (the_runtime) {
        nitro_buffer_t *buf = nitro_buffer_new();
        char *header = "~~~ NITRO SOCKET REPORT ~~~\n";
        nitro_buffer_append(buf, header, strlen(header));
        pthread_mutex_lock(&the_runtime->l_socks);

        nitro_socket_t *iter;
        DL_FOREACH(the_runtime->socks, iter) {
            SOCKET_CALL(iter, describe, buf);
        };
        pthread_mutex_unlock(&the_runtime->l_socks);
        char *footer = "~~~ END NITRO SOCKET REPORT ~~~\n";
        nitro_buffer_append(buf, footer, strlen(footer));
        int sz;
        char *ptr = nitro_buffer_data(buf, &sz);
        fwrite(ptr, sz, 1, stderr);
        nitro_buffer_destroy(buf);
    }
}

void stat_register_handler() {
    signal(SIGUSR1, stat_handle_usr1);
}
