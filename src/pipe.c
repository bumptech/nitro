/*
 * Nitro
 *
 * pipe.c - Pipes represent (one half of ) a socket to socket link
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
#include "pipe.h"
#include "socket.h"

void nitro_pipe_destroy(nitro_pipe_t *p, void *ptr) {
    nitro_universal_socket_t *s = (nitro_universal_socket_t *)ptr;
    /*pthread_mutex_lock(&s->l_sub);*/
    /*remove_pub_filters(s, p);*/
    /*pthread_mutex_unlock(&s->l_sub);*/
    /*assert(p->sub_keys == NULL);*/

    pthread_mutex_lock(&s->l_pipes);
    if (s->num_pipes == 1) {
       s->next_pipe = NULL;
    } else if (p == s->next_pipe) {
       s->next_pipe = p->next;
    }
    --s->num_pipes;
    pthread_mutex_unlock(&s->l_pipes);

    CDL_DELETE(s->pipes, p);

    if (p->registered) {
       HASH_DEL(s->pipes_by_session, p);
    }

    free(p);
}

nitro_pipe_t *nitro_pipe_new(void *ptr) {
    nitro_universal_socket_t *s = (nitro_universal_socket_t *)ptr;
    nitro_pipe_t *p = calloc(1, sizeof(nitro_pipe_t));

    pthread_mutex_lock(&s->l_pipes);
    CDL_PREPEND(s->pipes, p);
    if (!s->next_pipe) {
        s->next_pipe = p;
    }
    ++s->num_pipes;
    pthread_mutex_unlock(&s->l_pipes);

    return p;
}
