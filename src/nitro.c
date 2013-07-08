/*
 * Nitro
 *
 * nitro.c - Public API
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
#include "socket.h"
#include "Stcp.h"
#include "Sinproc.h"

nitro_socket_t *nitro_socket_bind(char *location, nitro_sockopt_t *opt) {
    nitro_socket_t *s = nitro_socket_new(opt);

    if (!s) {
        return NULL;
    }

    char *next;
    s->trans = socket_parse_location(location, &next);

    if (s->trans == NITRO_SOCKET_NO_TRANSPORT) {
        nitro_socket_destroy(s);
        return NULL;
    }

    s->stype.univ.given_location = strdup(next);

    SOCKET_SET_PARENT(s);
    int r = SOCKET_CALL(s, bind, next);

    if (r) {
        nitro_socket_destroy(s);
        return NULL;
    }

    return s;
}

nitro_socket_t *nitro_socket_connect(char *location, nitro_sockopt_t *opt) {
    nitro_socket_t *s = nitro_socket_new(opt);

    if (!s) {
        return NULL;
    }

    char *next;
    s->trans = socket_parse_location(location, &next);

    if (s->trans == NITRO_SOCKET_NO_TRANSPORT) {
        nitro_socket_destroy(s);
        return NULL;
    }

    s->stype.univ.given_location = strdup(next);

    SOCKET_SET_PARENT(s);
    int r = SOCKET_CALL(s, connect, next);

    if (r) {
        nitro_socket_destroy(s);
        return NULL;
    }

    return s;
}

void nitro_socket_close(nitro_socket_t *s) {
    SOCKET_CALL(s, close);
}

