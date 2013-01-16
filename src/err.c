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
#include <nitro.h>
#include <nitro-private.h>

// XXX: this is not thread safe, therefore incorrect. __thread not supported on mac
static int nitro_errno;

char *nitro_errmsg(NITRO_ERROR error) {
    switch (error) {
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

    case NITRO_ERR_PARSE_BAD_TRANSPORT:
        return "invalid transport type for socket";
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
