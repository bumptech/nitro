#ifndef NITRO_H
#define NITRO_H

#include "socket.h"
#include "frame.h"
#include "runtime.h"
#include "err.h"
#include "Stcp.h"
#include "Sinproc.h"
#include "log.h"

#define NITRO_NOCOPY (1 << 0)
#define NITRO_NOWAIT (1 << 1)

#define nitro_send(fr, s, flags) SOCKET_CALL(s, send, fr, flags)
#define nitro_recv(s, flags) SOCKET_CALL(s, recv, flags)
#define nitro_reply(snd, fr, s, flags) SOCKET_CALL(s, reply, snd, fr, flags)
#define nitro_relay_fw(snd, fr, s, flags) SOCKET_CALL(s, relay_fw, snd, fr, flags)
#define nitro_relay_bk(snd, fr, s, flags) SOCKET_CALL(s, relay_bk, snd, fr, flags)
#define nitro_sub(s, k, l) SOCKET_CALL(s, sub, k, l)
#define nitro_unsub(s, k, l) SOCKET_CALL(s, unsub, k, l)
#define nitro_pub(fr, k, l, s) SOCKET_CALL(s, pub, fr, k, l)

#endif
