#ifndef POOL_H
#define POOL_H

typedef void (*lockfree_pool_free_func)(void *);
typedef void (*lockfree_pool_reset_func)(void *);

typedef struct lockfree_pool_t {
    int max;
    _Atomic (int) count;
    lockfree_pool_free_func free_it;
    lockfree_pool_reset_func reset_it;
    _Atomic(void *) *cache;
} lockfree_pool_t;


lockfree_pool_t *lockfree_pool_init(
    int max, lockfree_pool_free_func free_it,
    lockfree_pool_reset_func);
void *lockfree_pool_get(lockfree_pool_t *pool);
void lockfree_pool_add(lockfree_pool_t *pool, void *p);
void lockfree_pool_destroy(lockfree_pool_t *pool);

#endif /* POOL_H */
