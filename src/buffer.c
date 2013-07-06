/*
 * Nitro
 *
 * buffer.c - String buffers
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

// think through max size of buffer, and preventing
// integer overruns

#include "common.h"
#include "buffer.h"
#include "util.h"

#define START_SIZE 1024

static void nitro_buffer_grow(nitro_buffer_t *buf) {
    while (buf->alloc < buf->size) {
        if (!buf->alloc) {
            buf->alloc = START_SIZE;
        } else {
            buf->alloc <<= 3;
        }
    }

    buf->area = realloc(buf->area, buf->alloc);
}

nitro_buffer_t *nitro_buffer_new() {
    nitro_buffer_t *buf;
    ZALLOC(buf);
    return buf;
}

void nitro_buffer_append(nitro_buffer_t *buf, const char *s, int bytes) {
    int old_size = buf->size;
    buf->size += bytes;

    if (buf->size > buf->alloc) {
        nitro_buffer_grow(buf);
    }

    memcpy(buf->area + old_size, s, bytes);
}

char *nitro_buffer_data(nitro_buffer_t *buf, int *size) {
    *size = buf->size;
    return buf->area;
}

void nitro_buffer_destroy(nitro_buffer_t *buf) {
    free(buf->area);
    free(buf);
}

char *nitro_buffer_prepare(nitro_buffer_t *buf, int *growth) {
    int proposed = *growth;
    buf->size += proposed;
    nitro_buffer_grow(buf);
    buf->size -= proposed;

    *growth = buf->alloc - buf->size;

    return buf->area + buf->size;
}

void nitro_buffer_extend(nitro_buffer_t *buf, int bytes) {
    buf->size += bytes;
    assert(buf->size <= buf->alloc);
}
