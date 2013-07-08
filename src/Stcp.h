/*
 * Nitro
 *
 * Stcp.h - TCP sockets are sockets designed to transmit frames between
 *         different machines on a TCP/IP network
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
#ifndef NITRO_STCP_H
#define NITRO_STCP_H

#include "socket.h"

int Stcp_socket_bind(nitro_tcp_socket_t *s, char *location);
int Stcp_socket_connect(nitro_tcp_socket_t *s, char *location);
void Stcp_socket_close(nitro_tcp_socket_t *s);
void Stcp_socket_bind_listen(nitro_tcp_socket_t *s);
void Stcp_socket_enable_writes(nitro_tcp_socket_t *s);
void Stcp_socket_enable_reads(nitro_tcp_socket_t *s);
void Stcp_socket_start_connect(nitro_tcp_socket_t *s);
void Stcp_socket_start_shutdown(nitro_tcp_socket_t *s);

int Stcp_socket_send(nitro_tcp_socket_t *s, nitro_frame_t **frp, int flags);
nitro_frame_t *Stcp_socket_recv(nitro_tcp_socket_t *s, int flags);
int Stcp_socket_reply(nitro_tcp_socket_t *s, nitro_frame_t *snd, nitro_frame_t **frp, int flags);
void Stcp_pipe_enable_write(nitro_pipe_t *p);
int Stcp_socket_relay_fw(nitro_tcp_socket_t *s, nitro_frame_t *snd, nitro_frame_t **frp, int flags);
int Stcp_socket_relay_bk(nitro_tcp_socket_t *s, nitro_frame_t *snd, nitro_frame_t **frp, int flags);
int Stcp_socket_sub(nitro_tcp_socket_t *s,
                    uint8_t *k, size_t length);
int Stcp_socket_unsub(nitro_tcp_socket_t *s,
                      uint8_t *k, size_t length);
int Stcp_socket_pub(nitro_tcp_socket_t *s,
                    nitro_frame_t **frp, const uint8_t *k,
                    size_t length, int flags);

void Stcp_register_pipe(nitro_tcp_socket_t *s, nitro_pipe_t *p);
nitro_pipe_t *Stcp_lookup_pipe(nitro_tcp_socket_t *s, uint8_t *ident);
void Stcp_unregister_pipe(nitro_tcp_socket_t *s, nitro_pipe_t *p);
nitro_pipe_t *Stcp_pipe_new(nitro_tcp_socket_t *s);
void Stcp_pipe_destroy(nitro_pipe_t *p, nitro_tcp_socket_t *s);
void Stcp_socket_describe(nitro_tcp_socket_t *s, nitro_buffer_t *buf);

#endif /* NITRO_STCP_H */
