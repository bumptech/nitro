/*
 * Nitro
 *
 * opt.h - Socket options.
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
#ifndef NITRO_OPT_H
#define NITRO_OPT_H

#include "common.h"
#include "cbuffer.h"

typedef void (*nitro_error_handler)(int nitro_error, void *baton);

typedef struct nitro_sockopt_t {
    int hwm_in;
    int hwm_out_general;
    int hwm_out_private;
    double close_linger;
    double reconnect_interval;
    uint32_t max_message_size;
    int want_eventfd;

    int has_ident;
    uint8_t *ident;
    nitro_counted_buffer_t *ident_buf;
    uint8_t pkey[crypto_box_SECRETKEYBYTES];

    int secure;
    int tcp_keep_alive;

    int has_remote_ident;
    uint8_t required_remote_ident[SOCKET_IDENT_LENGTH];

    nitro_error_handler error_handler;
    void *error_handler_baton;
} nitro_sockopt_t;

nitro_sockopt_t *nitro_sockopt_new();
void nitro_sockopt_set_hwm(nitro_sockopt_t *opt, int hwm);
void nitro_sockopt_set_hwm_detail(nitro_sockopt_t *opt, int hwm_in,
    int hwm_out_general, int hwm_out_private);
void nitro_sockopt_set_close_linger(nitro_sockopt_t *opt,
    double close_linger);
void nitro_sockopt_set_reconnect_interval(nitro_sockopt_t *opt,
    double reconnect_interval);
void nitro_sockopt_set_max_message_size(nitro_sockopt_t *opt,
    uint32_t max_message_size);
void nitro_sockopt_set_secure_identity(nitro_sockopt_t *opt,
    uint8_t *ident, size_t ident_length,
    uint8_t *pkey, size_t pkey_length);
void nitro_sockopt_set_secure(nitro_sockopt_t *opt, int enabled);
void nitro_sockopt_set_required_remote_ident(nitro_sockopt_t *opt,
    uint8_t *ident, size_t ident_length);
void nitro_sockopt_set_want_eventfd(nitro_sockopt_t *opt, int want_eventfd);
void nitro_sockopt_set_tcp_keep_alive(nitro_sockopt_t *opt, int alive_time);
void nitro_sockopt_set_error_handler(nitro_sockopt_t *opt,
    nitro_error_handler handler, void *baton);
void nitro_sockopt_disable_error_handler(nitro_sockopt_t *opt);
void nitro_sockopt_destroy(nitro_sockopt_t *opt);

#endif /* NITRO_OPT_H */
