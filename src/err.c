/*
 * Nitro
 *
 * err.c - Error messages exposed to public API
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
#include "common.h"
#include "err.h"
#include "log.h"

// XXX: this is not thread safe, therefore incorrect. __thread not supported on mac
static int nitro_errno;

char *nitro_errmsg(NITRO_ERROR error) {
    switch (error) {
    case NITRO_ERR_NONE:
        return "(no error)";
        break;
    case NITRO_ERR_EAGAIN:
        return "socket queue operation would block";
        break;

    case NITRO_ERR_NO_RECIPIENT:
        return "specified frame recipient not found in socket table";
        break;

    case NITRO_ERR_ERRNO:
        return strerror(errno);
        break;

    case NITRO_ERR_ALREADY_RUNNING:
        return "nitro is already running; cannot call nitro_start twice";
        break;

    case NITRO_ERR_NOT_RUNNING:
        return "nitro is not running";
        break;

    case NITRO_ERR_TCP_LOC_NOCOLON:
        return "TCP socket location did not contain a colon";
        break;

    case NITRO_ERR_TCP_LOC_BADPORT:
        return "TCP socket location did not contain an integer port number";
        break;

    case NITRO_ERR_TCP_LOC_BADIPV4:
        return "TCP socket location was not a valid IPv4 address (a.b.c.d)";
        break;

    case NITRO_ERR_PARSE_BAD_TRANSPORT:
        return "invalid transport type for socket";
        break;

    case NITRO_ERR_ENCRYPT:
        return "(pipe) frame encryption failed";
        break;

    case NITRO_ERR_DECRYPT:
        return "(pipe) frame decryption failed";
        break;
        
    case NITRO_ERR_MAX_FRAME_EXCEEDED:
        return "(pipe) remote tried to send a frame larger than the maximum allowable size";
        break;

    case NITRO_ERR_BAD_PROTOCOL_VERSION:
        return "(pipe) remote sent a nitro protocol version that is unsupported by this application";
        break;

    case NITRO_ERR_INVALID_CLEAR:
        return "(pipe) remote sent a unencrypted message over a secure socket";
        break;

    case NITRO_ERR_DOUBLE_HANDSHAKE:
        return "(pipe) remote sent two HELLO packets on same connection";
        break;

    case NITRO_ERR_INVALID_CERT:
        return "(pipe) remote identity/public key does not match the one required by this socket";
        break;

    case NITRO_ERR_NO_HANDSHAKE:
        return "(pipe) remote sent a non-HELLO packet before HELLO";
        break;

    case NITRO_ERR_BAD_SUB:
        return "(pipe) remote sent a SUB packet that is too short to be valid";
        break;

    case NITRO_ERR_BAD_HANDSHAKE:
        return "(pipe) remote sent a HELLO packet that is too short to be valid";
        break;

    case NITRO_ERR_BAD_INPROC_OPT:
        return "inproc socket creation was given an unsupported socket option";
        break;

    case NITRO_ERR_INPROC_ALREADY_BOUND:
        return "another inproc socket is already bound to that location";
        break;

    case NITRO_ERR_BAD_SECURE:
        return "(pipe) remote sent a secure envelope on an insecure connection";
        break;

    case NITRO_ERR_INPROC_NOT_BOUND:
        return "cannot connect to inproc: not bound";
        break;

    case NITRO_ERR_INPROC_NO_CONNECTIONS:
        return "cannot send() on inproc without any established connections";
        break;

    default:
        assert(0);
        break;
    }

    return NULL;
}

NITRO_ERROR nitro_error() {
    return nitro_errno;
}

int nitro_set_error(NITRO_ERROR e) {
    nitro_errno = e;
    return -1;
}

void nitro_clear_error() {
    nitro_errno = NITRO_ERR_NONE;
}

int nitro_has_error() {
    return nitro_errno != NITRO_ERR_NONE;
}

void nitro_error_log_handler(int err, void *baton) {
    nitro_log_err("error-logger", nitro_errmsg(err));
}
