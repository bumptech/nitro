/*
 * Nitro
 *
 * cbuffer.h - Refcounted Objects
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
#ifndef NITRO_CBUFFER_H
#define NITRO_CBUFFER_H

#include "common.h"

#include "util.h"

typedef struct nitro_counted_buffer_t {
    void *ptr;
    int count;
    nitro_free_function ff;
    void *baton;
} nitro_counted_buffer_t;

nitro_counted_buffer_t *nitro_counted_buffer_new(void *backing, nitro_free_function ff, void *baton);

#define nitro_counted_buffer_decref(buf) {\
        nitro_counted_buffer_t *__tmp_buf = (buf);\
        if (__sync_fetch_and_sub(&__tmp_buf->count, 1) == 1) {\
            if ((__tmp_buf)->ff) {\
                (__tmp_buf)->ff((__tmp_buf)->ptr, (__tmp_buf)->baton);\
            }\
            free((__tmp_buf));\
        }\
    }

#define nitro_counted_buffer_incref(buf) {\
        nitro_counted_buffer_t *__tmp_buf = (buf);\
        __sync_fetch_and_add(&(__tmp_buf)->count, 1);\
    }

//inline void nitro_counted_buffer_incref(nitro_counted_buffer_t *buf);
//inline void nitro_counted_buffer_decref(nitro_counted_buffer_t *buf);

#endif /* CBUFFER_H */
