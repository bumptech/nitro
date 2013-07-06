/*
 * Nitro
 *
 * nitro.h - Public API and macros.
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
#ifndef NITRO_H
#define NITRO_H

#include "socket.h"
#include "frame.h"
#include "runtime.h"
#include "err.h"
#include "Stcp.h"
#include "Sinproc.h"
#include "log.h"
#include "stat.h"

#define NITRO_REUSE (1 << 0)
#define NITRO_NOWAIT (1 << 1)

#define nitro_send(fr, s, flags) SOCKET_CALL(s, send, fr, flags)
#define nitro_recv(s, flags) SOCKET_CALL(s, recv, flags)
#define nitro_reply(snd, fr, s, flags) SOCKET_CALL(s, reply, snd, fr, flags)
#define nitro_relay_fw(snd, fr, s, flags) SOCKET_CALL(s, relay_fw, snd, fr, flags)
#define nitro_relay_bk(snd, fr, s, flags) SOCKET_CALL(s, relay_bk, snd, fr, flags)
#define nitro_sub(s, k, l) SOCKET_CALL(s, sub, k, l)
#define nitro_unsub(s, k, l) SOCKET_CALL(s, unsub, k, l)
#define nitro_pub(fr, k, l, s, f) SOCKET_CALL(s, pub, fr, k, l, f)
#define nitro_eventfd(s) ((s)->stype.univ.event_fd)

#define nitro_enable_stats stat_register_handler

#endif
