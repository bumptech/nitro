/*
 * Nitro
 *
 * frame.h - The frame object is the refcounted representation of a chunk
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
#ifndef NITRO_FRAME_H
#define NITRO_FRAME_H

#include "cbuffer.h"
#include "common.h"

#include "util.h"

#define NITRO_FRAME_DATA 0
#define NITRO_FRAME_SUB  1
#define NITRO_FRAME_HELLO 2
#define NITRO_FRAME_SECURE 3

#define NITRO_MAX_FRAME (1024 * 1024 * 1024)

/* Used for publishing */
typedef struct nitro_key_t {
    const uint8_t *data;
    uint8_t length;
    nitro_counted_buffer_t *buf;

    struct nitro_key_t *prev;
    struct nitro_key_t *next;
} nitro_key_t;

typedef struct nitro_protocol_header {
    char protocol_version;
    char packet_type;
    uint8_t num_ident;
    uint8_t flags;
    uint32_t frame_size;
} nitro_protocol_header;

#define FRAME_BZERO_SIZE \
    ((sizeof(void *) * 3) + \
     (sizeof(char) * 4))

typedef struct nitro_frame_t {
    /* NOTE: careful about order here!
    fields that must be zeroed should
    be first.. adjust FRAME_BZERO_SIZE
    above if you add fields */

    /* START bzero() region */
    /* num idents to send on end of buffer */
    nitro_counted_buffer_t *ident_buffer;
    nitro_counted_buffer_t *sender_buffer;
    void *ident_data;

    uint8_t num_ident;
    char iovec_set;
    char type;
    /* send self ident? */
    char push_sender;

    /* END bzero() region */

    nitro_counted_buffer_t *buffer;
    void *data;
    uint32_t size;

    uint8_t *sender;
    nitro_counted_buffer_t *myref;

    // TCP
    nitro_protocol_header tcp_header;
    struct iovec iovs[4];

} nitro_frame_t;

void nitro_frame_set_iovec(nitro_frame_t *f, struct iovec *vecs);
nitro_frame_t *nitro_frame_copy_partial(nitro_frame_t *f, struct iovec *vecs);
nitro_frame_t *nitro_frame_new(void *data, uint32_t size, nitro_free_function ff, void *baton);
nitro_frame_t *nitro_frame_new_prealloc(void *data, uint32_t size, nitro_counted_buffer_t *buffer);
nitro_frame_t *nitro_frame_new_copy(void *data, uint32_t size);
void nitro_frame_clear(nitro_frame_t *fr);
#define nitro_frame_new_heap(d, size) nitro_frame_new(d, size, just_free, NULL)

struct iovec *nitro_frame_iovs(nitro_frame_t *fr, int *num);
int nitro_frame_iovs_advance(nitro_frame_t *fr,
                             struct iovec *vecs, int index, int offset, int *done);
void nitro_frame_iovs_reset(nitro_frame_t *fr);
void nitro_frame_set_sender(nitro_frame_t *f,
                            uint8_t *sender, nitro_counted_buffer_t *buf);
void nitro_frame_clone_stack(nitro_frame_t *fr, nitro_frame_t *to);
void nitro_frame_set_stack(nitro_frame_t *f, const uint8_t *data,
                           nitro_counted_buffer_t *buf, uint8_t num);
void nitro_frame_extend_stack(nitro_frame_t *fr, nitro_frame_t *to);

#define nitro_frame_destroy(f) {\
        nitro_counted_buffer_decref((f)->myref);\
    }

#define nitro_frame_incref(f) {\
        nitro_counted_buffer_incref((f)->myref);\
    }

nitro_key_t *nitro_key_new(const uint8_t *data, uint8_t length,
                           nitro_counted_buffer_t *buf);
int nitro_key_compare(nitro_key_t *k1, nitro_key_t *k2);
void nitro_key_destroy(nitro_key_t *k);

inline void nitro_frame_stack_pop(nitro_frame_t *f) {
    if (f->num_ident > 0) {
        --(f->num_ident);
    }
}

inline void nitro_frame_stack_push_sender(nitro_frame_t *f) {
    f->push_sender = 1;
}

inline void *nitro_frame_data(nitro_frame_t *fr) {
    return fr->data;
}

inline uint32_t nitro_frame_size(nitro_frame_t *fr) {
    return fr->size;
}

#endif /* FRAME_H */
