/*
 * Nitro
 *
 * Sinproc.h - Inproc sockets are in-process sockets, thin wrappers
 *             around thread-safe queues that are API-compatible
 *             with TCP sockets.
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
#ifndef NITRO_SINPROC_H
#define NITRO_SINPROC_H

#include "socket.h"

int Sinproc_socket_connect(nitro_inproc_socket_t *s, char *location);
int Sinproc_socket_bind(nitro_inproc_socket_t *s, char *location);
void Sinproc_socket_bind_listen(nitro_inproc_socket_t *s);
void Sinproc_socket_enable_writes(nitro_inproc_socket_t *s);
void Sinproc_socket_enable_reads(nitro_inproc_socket_t *s);
void Sinproc_socket_start_connect(nitro_inproc_socket_t *s);
void Sinproc_socket_start_shutdown(nitro_inproc_socket_t *s);
void Sinproc_socket_close(nitro_inproc_socket_t *s);

int Sinproc_socket_send(nitro_inproc_socket_t *s, nitro_frame_t **frp, int flags);
nitro_frame_t *Sinproc_socket_recv(nitro_inproc_socket_t *s, int flags);
int Sinproc_socket_reply(nitro_inproc_socket_t *s, nitro_frame_t *snd, nitro_frame_t **frp, int flags);
int Sinproc_socket_relay_fw(nitro_inproc_socket_t *s, nitro_frame_t *snd, nitro_frame_t **frp, int flags);
int Sinproc_socket_relay_bk(nitro_inproc_socket_t *s, nitro_frame_t *snd, nitro_frame_t **frp, int flags);
int Sinproc_socket_sub(nitro_inproc_socket_t *s,
                       uint8_t *k, size_t length);
int Sinproc_socket_unsub(nitro_inproc_socket_t *s,
                         uint8_t *k, size_t length);
int Sinproc_socket_pub(nitro_inproc_socket_t *s,
                       nitro_frame_t **frp, uint8_t *k, size_t length, int flags);

#endif /* NITRO_SINPROC_H */
