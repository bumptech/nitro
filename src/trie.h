/*
 * Nitro
 *
 * trie.h - Prefix trie for efficient frame dispatch to sub()'d sockets
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
#ifndef NITRO_TRIE_H
#define NITRO_TRIE_H

#include "common.h"

typedef struct nitro_prefix_trie_mem {
    void *ptr;

    struct nitro_prefix_trie_mem *prev;
    struct nitro_prefix_trie_mem *next;
} nitro_prefix_trie_mem;

typedef struct nitro_prefix_trie_node {
    uint8_t *rep;
    uint8_t length;
    struct nitro_prefix_trie_node *subs[256];

    nitro_prefix_trie_mem *members;

} nitro_prefix_trie_node;

typedef void (*nitro_prefix_trie_search_callback)
(const uint8_t *pfx, uint8_t length, nitro_prefix_trie_mem *members, void *baton);


void nitro_prefix_trie_search(
    nitro_prefix_trie_node *t, const uint8_t *rep, uint8_t length,
    nitro_prefix_trie_search_callback cb, void *baton);
void nitro_prefix_trie_add(nitro_prefix_trie_node **t,
    const uint8_t *rep, uint8_t length, void *ptr);
int nitro_prefix_trie_del(nitro_prefix_trie_node *t,
    const uint8_t *rep, uint8_t length, void *ptr);

#endif /* TRIE_H */
