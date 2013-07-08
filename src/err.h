/*
 * Nitro
 *
 * err.h - Error messages exposed to public API
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
#ifndef NITRO_ERR_H
#define NITRO_ERR_H

#define NITRO_ERROR int

#define NITRO_ERR_NONE                  0
#define NITRO_ERR_ERRNO                 1
#define NITRO_ERR_ALREADY_RUNNING       2
#define NITRO_ERR_NOT_RUNNING           3
#define NITRO_ERR_TCP_LOC_NOCOLON       4
#define NITRO_ERR_TCP_LOC_BADPORT       5
#define NITRO_ERR_TCP_LOC_BADIPV4       6
#define NITRO_ERR_PARSE_BAD_TRANSPORT   7
#define NITRO_ERR_EAGAIN                8
#define NITRO_ERR_NO_RECIPIENT          9
#define NITRO_ERR_ENCRYPT               10
#define NITRO_ERR_DECRYPT               11
#define NITRO_ERR_INVALID_CLEAR         12
#define NITRO_ERR_MAX_FRAME_EXCEEDED    13
#define NITRO_ERR_BAD_PROTOCOL_VERSION  14
#define NITRO_ERR_DOUBLE_HANDSHAKE      15
#define NITRO_ERR_NO_HANDSHAKE          16
#define NITRO_ERR_BAD_SUB               17
#define NITRO_ERR_BAD_HANDSHAKE         18
#define NITRO_ERR_INVALID_CERT          19
#define NITRO_ERR_BAD_INPROC_OPT        20
#define NITRO_ERR_BAD_SECURE            21
#define NITRO_ERR_INPROC_ALREADY_BOUND  22
#define NITRO_ERR_INPROC_NOT_BOUND      23
#define NITRO_ERR_INPROC_NO_CONNECTIONS 24
#define NITRO_ERR_SUB_ALREADY           25
#define NITRO_ERR_SUB_MISSING           26
#define NITRO_ERR_TCP_BAD_ANY           27
#define NITRO_ERR_GAI                   28

int nitro_set_error(NITRO_ERROR e);
char *nitro_errmsg(NITRO_ERROR error);
NITRO_ERROR nitro_error();
void nitro_clear_error();
int nitro_has_error();
void nitro_error_log_handler(int err, void *baton);
void nitro_err_start();
void nitro_err_stop();
int nitro_set_gai_error(int e);

#endif /* ERR_H */
