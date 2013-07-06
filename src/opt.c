/*
 * Nitro
 *
 * opt.c - Socket options.
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
#include "opt.h"
#include "err.h"
#include "util.h"
#include "frame.h"

#define NITRO_MB (1024 * 1024)

nitro_sockopt_t *nitro_sockopt_new() {
    nitro_sockopt_t *opt;
    ZALLOC(opt);

    /* defaults (otherwise, 0) */
    opt->ident = malloc(SOCKET_IDENT_LENGTH);
    opt->ident_buf = nitro_counted_buffer_new(opt->ident, just_free, NULL);
    opt->close_linger = 1.0;
    opt->reconnect_interval = 0.2; /* seconds */
    opt->max_message_size = 16 * NITRO_MB;
    opt->tcp_keep_alive = 5; /* seconds */

    opt->error_handler = nitro_error_log_handler;
    return opt;
}

void nitro_sockopt_set_hwm(nitro_sockopt_t *opt, int hwm) {
    opt->hwm_in = opt->hwm_out_general = opt->hwm_out_private = hwm;
}

void nitro_sockopt_set_want_eventfd(nitro_sockopt_t *opt, int want_eventfd) {
    opt->want_eventfd = want_eventfd;
}

void nitro_sockopt_set_hwm_detail(nitro_sockopt_t *opt, int hwm_in,
    int hwm_out_general, int hwm_out_private) {
    opt->hwm_in = hwm_in;
    opt->hwm_out_general = hwm_out_general;
    opt->hwm_out_private = hwm_out_private;
}

void nitro_sockopt_set_close_linger(nitro_sockopt_t *opt,
    double close_linger) {
    opt->close_linger = close_linger;
}

void nitro_sockopt_set_reconnect_interval(nitro_sockopt_t *opt,
    double reconnect_interval) {
    opt->reconnect_interval = reconnect_interval;
}

void nitro_sockopt_set_max_message_size(nitro_sockopt_t *opt,
    uint32_t max_message_size) {
    // For performance and security reasons,
    // we do not allow this to exceed 1GB If 
    // we keep this away from the 4GB limit, 
    // we minimize our chances of overflowing integers
    // (and size_t on 32-bit systems)
    // when we parse the frame coming from
    // the network
    assert(max_message_size <= NITRO_MAX_FRAME);
    opt->max_message_size = max_message_size;
}

void nitro_sockopt_set_secure_identity(nitro_sockopt_t *opt,
    uint8_t *ident, size_t ident_length,
    uint8_t *pkey, size_t pkey_length) {

    assert(ident_length == SOCKET_IDENT_LENGTH);
    assert(pkey_length == crypto_box_SECRETKEYBYTES);

    memcpy(opt->ident, ident, SOCKET_IDENT_LENGTH);
    memcpy(opt->pkey, pkey, crypto_box_SECRETKEYBYTES);
    opt->has_ident = 1;
}

void nitro_sockopt_set_secure(nitro_sockopt_t *opt,
    int enabled) {
    opt->secure = enabled;
}

void nitro_sockopt_set_tcp_keep_alive(nitro_sockopt_t *opt, int alive_time) {
    opt->tcp_keep_alive = alive_time;
}

void nitro_sockopt_set_required_remote_ident(nitro_sockopt_t *opt,
    uint8_t *ident, size_t ident_length) {
    assert(ident_length == SOCKET_IDENT_LENGTH);

    memcpy(opt->required_remote_ident, ident, SOCKET_IDENT_LENGTH);
    opt->has_remote_ident = 1;
}

void nitro_sockopt_set_error_handler(nitro_sockopt_t *opt,
    nitro_error_handler handler, void *baton) {
    opt->error_handler_baton = baton;
    opt->error_handler = handler;
}

void nitro_sockopt_disable_error_handler(nitro_sockopt_t *opt) {
    opt->error_handler = NULL;
    opt->error_handler_baton = NULL;
}

void nitro_sockopt_destroy(nitro_sockopt_t *opt) {
    nitro_counted_buffer_decref(opt->ident_buf);
    free(opt);
}

