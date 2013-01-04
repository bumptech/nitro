#include "nitro.h"
#include "nitro-private.h"

void nitro_prefix_trie_search(
    nitro_prefix_trie_node *t, uint8_t *rep, uint8_t length,
    nitro_prefix_trie_search_callback cb, void *baton) {
    if (!t || t->length > length || memcmp(t->rep, rep, t->length))
        return;

    if (t->members) {
        cb(t->rep, t->length, t->members, baton);
    }

    if (t->length < length) {
        uint8_t n = rep[t->length];
        nitro_prefix_trie_node *next = t->subs[n];
        nitro_prefix_trie_search(next, rep, length, cb, baton);
    }
}

void nitro_prefix_trie_add(nitro_prefix_trie_node **t,
    uint8_t *rep, uint8_t length, void *ptr) {
    nitro_prefix_trie_node *n, *on;
    if (!*t) {
        printf("add: create\n");
        ZALLOC(*t);
        if (length) {
            (*t)->length = length;
            (*t)->rep = malloc(length);
            memmove((*t)->rep, rep, length);
        }
    }
    on = n = *t;

    if (n->length < length && !memcmp(n->rep, rep, n->length)) {
        printf("add: recurse %s %d %s\n", rep, n->length, n->rep);
        uint8_t c = rep[n->length];
        nitro_prefix_trie_add(&n->subs[c], rep, length, ptr);
    }
    else {
        // alloc
        nitro_prefix_trie_mem *m;
        ZALLOC(m);
        m->ptr = ptr;

        if (n->length == length && !memcmp(n->rep, rep, length)) {
            printf("add: match %d %d %s %s\n", 
            n->length, length, n->rep, rep);
            DL_APPEND(n->members, m);
        }
        else {
            ZALLOC(n);
            n->length = length;
            n->rep = malloc(length);
            memmove(n->rep, rep, length);
            DL_APPEND(n->members, m);
            if (n->length < on->length && !memcmp(on->rep, n->rep, n->length)) {
                *t = n;
                printf("add: insertion\n");
                n->subs[on->rep[length]] = on;
            }
            else if (n->length > on->length && !memcmp(on->rep, n->rep, on->length)) {
                *t = on;
                printf("add: other insertion\n");
                on->subs[n->rep[on->length]] = n;
            }
            else {
                printf("add: intermediate\n");
                int i;
                for(i=0; i < length && on->rep[i] == n->rep[i]; i++) {}
                nitro_prefix_trie_node *parent;
                ZALLOC(parent);
                parent->length = i;
                parent->rep = malloc(parent->length);
                memmove(parent->rep, rep, parent->length);
                parent->subs[rep[parent->length]] = n;
                parent->subs[on->rep[parent->length]] = on;
                *t = parent;
            }
        }
    }
}

int nitro_prefix_trie_del(nitro_prefix_trie_node *t,
    uint8_t *rep, uint8_t length, void *ptr) {
    if (!t || t->length > length || memcmp(t->rep, rep, t->length))
        return 0;

    if (t->length < length) {
        uint8_t n = rep[t->length];
        nitro_prefix_trie_node *next = t->subs[n];
        return nitro_prefix_trie_del(next, rep, length, ptr);
    }
    else {
        nitro_prefix_trie_mem *m;
        for (m=t->members; m && ptr != m->ptr; m = m->next) {}
        if (m) {
            DL_DELETE(t->members, m);
            free(m);
            return 1;
        }
    }
    return 0;
}

void nitro_prefix_trie_destroy(nitro_prefix_trie_node *t) {
    if (!t) return;
    assert(t->members == NULL);

    int i;
    for (i=0; i < 256; i++) {
        if (t->subs[i])
            nitro_prefix_trie_destroy(t->subs[i]);
    }

    free(t->rep);
    free(t);
}

#if 0
static void print_trie(nitro_prefix_trie_node *t, int c) {
    int x;
    for (x=0; x < c; x++) {
        printf(" ");
    }
    printf("%s:%d:%d", t->rep, t->length, t->members ? 1 : 0);
    printf("\n");

    int i;
    for(i=0; i < 256; i++) {
        if (t->subs[i]) {
            for (x=0; x < c + 1; x++) {
                printf(" ");
            }

            printf("%d:\n", i);
            print_trie(t->subs[i], c + 2);
        }
    }
}


void callback(uint8_t *pfx, uint8_t length, nitro_prefix_trie_mem *members, void *baton) {
    printf("got: %s\n", (char *)pfx);
}

int main(int argc, char **argv) {
    nitro_prefix_trie_node *root = NULL;

    nitro_prefix_trie_search(root, (uint8_t *)"bar", 3, callback, NULL);
    nitro_prefix_trie_add(&root, (uint8_t *)"bar", 3, NULL);
    printf("after adding bar ---\n");
    print_trie(root, 0);
    nitro_prefix_trie_add(&root, (uint8_t *)"b", 1, NULL);
    printf("after adding b ---\n");
    print_trie(root, 0);
    nitro_prefix_trie_add(&root, (uint8_t *)"bark", 4, NULL);
    printf("after adding bark---\n");
    print_trie(root, 0);
    nitro_prefix_trie_add(&root, (uint8_t *)"baz", 3, NULL);
    printf("after adding baz---\n");
    print_trie(root, 0);
    printf("search for bark ===\n");
    nitro_prefix_trie_search(root, (uint8_t *)"bark", 4, callback, NULL);
    printf("search for baz ===\n");
    nitro_prefix_trie_search(root, (uint8_t *)"baz", 3, callback, NULL);

    nitro_prefix_trie_del(root, (uint8_t *)"bar", 3, NULL);

    printf("search for bark (post delete) ===\n");
    nitro_prefix_trie_search(root, (uint8_t *)"bark", 4, callback, NULL);

    return 0;
}
#endif /* 0 */
