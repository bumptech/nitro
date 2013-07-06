/*
 * Nitro
 *
 * socket.c - Common socket functions.
 *
 *  -- LICENSE --
 *
 * Copyright 2013 Bump Technologies, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY BUMP TECHNOLOGIES, INC. ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BUMP TECHNOLOGIES, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Bump Technologies, Inc.
 *
 */
#include "err.h"
#include "socket.h"
#include "runtime.h"
#include "crypto.h"

nitro_socket_t *nitro_socket_new(nitro_sockopt_t *opt) {
    nitro_socket_t *sock;
    ZALLOC(sock);

    nitro_universal_socket_t *us = &sock->stype.univ;
    opt = opt ? opt : nitro_sockopt_new();
    us->opt = opt;

    if (!us->opt->has_ident) {
        crypto_make_keypair(us->opt->ident, us->opt->pkey);
        us->opt->has_ident = 1;
    }

    if (us->opt->want_eventfd) {
#ifdef __linux__
        us->event_fd = eventfd(0, EFD_NONBLOCK);
#else
        int pipes[2];
        int r = pipe(pipes);
        if (r) {
            us->event_fd = -1;
        } else {
            us->event_fd = pipes[0];
            us->write_pipe = pipes[1];
            int flags = fcntl(us->event_fd, F_GETFL, 0);
            fcntl(us->event_fd, F_SETFL, flags | O_NONBLOCK);
            flags = fcntl(us->write_pipe, F_GETFL, 0);
            fcntl(us->write_pipe, F_SETFL, flags | O_NONBLOCK);
        }
#endif
        if (us->event_fd < 0) {
            nitro_set_error(NITRO_ERR_ERRNO);
            nitro_sockopt_destroy(opt);
            free(sock);
            return NULL;
        }
    }

    // XXX add socket to list for diagnostics
    __sync_fetch_and_add(&the_runtime->num_sock, 1);
    return sock;
}

NITRO_SOCKET_TRANSPORT socket_parse_location(char *location, char **next) {
    if (!strncmp(location, TCP_PREFIX, strlen(TCP_PREFIX))) {
        *next = location + strlen(TCP_PREFIX);
        return NITRO_SOCKET_TCP;
    }

    if (!strncmp(location, INPROC_PREFIX, strlen(INPROC_PREFIX))) {
        *next = location + strlen(INPROC_PREFIX);
        return NITRO_SOCKET_INPROC;
    }

    nitro_set_error(NITRO_ERR_PARSE_BAD_TRANSPORT);
    return -1;
}

void nitro_socket_destroy(nitro_socket_t *s) {
    nitro_universal_socket_t *us = &s->stype.univ;
    nitro_sockopt_destroy(us->opt);
    free(us->given_location);
    free(s);
    __sync_fetch_and_sub(&the_runtime->num_sock, 1);
}
