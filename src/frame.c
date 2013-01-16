/*
 * Nitro
 *
 * frame.c - The frame object is the refcounted representation of a chunk
 *           of data sent from one nitro socket to another
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
#include "nitro.h"
#include "nitro-private.h"

nitro_frame_t *nitro_frame_copy(nitro_frame_t *f) {
    nitro_frame_t *result = malloc(sizeof(nitro_frame_t));
    memcpy(result, f, sizeof(nitro_frame_t));
    nitro_counted_buffer_incref(f->buffer);
    return result;
}

nitro_frame_t *nitro_frame_new(void *data, uint32_t size, nitro_free_function ff, void *baton) {
    nitro_frame_t *f;
    ZALLOC(f);
    nitro_counted_buffer *buffer = nitro_counted_buffer_new(data, ff, baton);
    f->buffer = buffer;
    f->size = size;
    f->prev = f->next = NULL;
    pthread_mutex_init(&f->lock, NULL);
    return f;
}

void nitro_frame_destroy(nitro_frame_t *f) {
    nitro_counted_buffer_decref(f->buffer);
    free(f);
}

nitro_frame_t *nitro_frame_new_copy(void *data, uint32_t size) {
    char *n = malloc(size);
    memmove(n, data, size);
    return nitro_frame_new(n, size, just_free, NULL);
}

inline void *nitro_frame_data(nitro_frame_t *fr) {
    return ((nitro_counted_buffer *)fr->buffer)->ptr;
}

inline uint32_t nitro_frame_size(nitro_frame_t *fr) {
    return fr->size;
}
