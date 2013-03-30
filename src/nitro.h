#ifndef NITRO_H
#define NITRO_H

#include "socket.h"
#include "frame.h"
#include "runtime.h"
#include "err.h"
#include "Stcp.h"
#include "Sinproc.h"

#define nitro_send(fr, s) SOCKET_CALL(s, send, fr)
#define nitro_recv(s) SOCKET_CALL(s, recv)
#define nitro_reply(snd, fr, s) SOCKET_CALL(s, reply, snd, fr)

#endif
