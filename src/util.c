/*
 * Nitro
 *
 * util.c - Various utlity functions used throughout nitro
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
#include "util.h"
#include "cbuffer.h"
#include "buffer.h"

void fatal(char *why) {
    fprintf(stderr, "fatal error: %s\n", why);
}

void just_free(void *data, void *unused) {
    free(data);
}

void free_nothing(void *unused1, void *unused2) {}

double now_double() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((double)tv.tv_sec +
            ((double)tv.tv_usec / 1000000));
}

/* a free function */
void cbuffer_decref(void *data, void *bufptr) {
    nitro_counted_buffer_t *buf = (nitro_counted_buffer_t *)bufptr;
    nitro_counted_buffer_decref(buf);
}

void buffer_free(void *data, void *bufptr) {
    nitro_buffer_t *buf = (nitro_buffer_t *)bufptr;
    nitro_buffer_destroy(buf);
}

uint8_t *memdup(uint8_t *src, size_t size) {
    uint8_t *out = malloc(size);
    memcpy(out, src, size);
    return out;
}
