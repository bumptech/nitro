/*
 * Nitro
 *
 * pool.c - A lock-free (atomic CAS etc) object pool
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
#include "nitro.h"
#include "pool.h"

/* Implied: NULL on program load */
static void *lockfree_pool_null;

lockfree_pool_t *lockfree_pool_init(
    int max, lockfree_pool_free_func free_it,
    lockfree_pool_reset_func reset_it) {
    lockfree_pool_t *pool = calloc(1, sizeof(lockfree_pool_t));
    pool->max = max;
    pool->free_it = free_it;
    pool->reset_it = reset_it;
    pool->cache = calloc(1, max * sizeof(_Atomic(void *)));

    int i;
    for(i = 0; i < max; i++) {
        atomic_init(&pool->cache[i], NULL);
    }

    atomic_init(&pool->count, 0);

    return pool;
}

void *lockfree_pool_get(lockfree_pool_t *pool) {
    int index = atomic_load(&pool->count) - 1;
    while (index >= 0) {
        volatile void * exist = (void *)atomic_load(&pool->cache[index]);
        int swapped = 0;
        if (exist) {
            swapped = atomic_compare_exchange_strong(
                &pool->cache[index],
                &exist,
                NULL);
            if (swapped) {
                atomic_fetch_add(&pool->count, -1);
                return (void *)exist;
            }
        } else {
            /* We only execute this on the "else" so that we try
               the same slot again in case a read/write happened
               between the load and the swap; we don't decrement the 
               index until it comes back NULL  */
            index--;
        }
    }
    return NULL;
}

void lockfree_pool_add(lockfree_pool_t *pool, void *p) {
    int index = atomic_fetch_add(&pool->count, 1);
    if (index < pool->max) {
        do {
            int swapped = atomic_compare_exchange_strong(
                &pool->cache[index],
                &lockfree_pool_null,
                p);
            if (swapped) {
                pool->reset_it(p);
                return;
            }
            index--;
        } while (index >=0);
    }
    atomic_fetch_add(&pool->count, -1);
    pool->free_it(p);
}

void lockfree_pool_destroy(lockfree_pool_t *pool) {
    int i;
    for (i = 0; i < pool->max; i++) {
        volatile void *item = atomic_load(&pool->cache[i]);
        if (item) {
            pool->free_it((void *)item);
        }
    }
    free(pool->cache);
    free(pool);
}
