#ifndef TRIE_H
#define TRIE_H


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
(uint8_t *pfx, uint8_t length, nitro_prefix_trie_mem *members, void *baton);


void nitro_prefix_trie_search(
    nitro_prefix_trie_node *t, uint8_t *rep, uint8_t length,
    nitro_prefix_trie_search_callback cb, void *baton);
void nitro_prefix_trie_add(nitro_prefix_trie_node **t,
    uint8_t *rep, uint8_t length, void *ptr);
int nitro_prefix_trie_del(nitro_prefix_trie_node *t,
    uint8_t *rep, uint8_t length, void *ptr);

#endif /* TRIE_H */
