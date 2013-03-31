#include "test.h"
#include "trie.h"

struct test_data {
    int num_matches;
};

void trie_callback(uint8_t *pfx, uint8_t length,
    nitro_prefix_trie_mem *members, void *baton) {
    struct test_data *td = (struct test_data *)baton;

    nitro_prefix_trie_mem *m;

    for (m=members; m; m = m->next) {
        ++td->num_matches;
    }
}

int main(int argc, char **argv) {

    nitro_prefix_trie_node *root = NULL;

    struct test_data td = {0};
    int item_1 = 1;
    int item_2 = 2;
    int item_3 = 3;
    int item_4 = 4;
    int item_5 = 5;

    nitro_prefix_trie_search(root,
    (uint8_t *)"foo", 3, trie_callback, &td);

    TEST("empty no matches", td.num_matches == 0);

    nitro_prefix_trie_add(&root,
        (uint8_t *)"foo", 3, &item_1);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"foo", 3, trie_callback, &td);
    TEST("1 match exact", td.num_matches == 1);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"food", 4, trie_callback, &td);
    TEST("1 match sub", td.num_matches == 1);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"fo", 2, trie_callback, &td);
    TEST("0 match super", td.num_matches == 0);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"bright", 5, trie_callback, &td);
    TEST("0 match other", td.num_matches == 0);

    int r = nitro_prefix_trie_del(root,
        (uint8_t *)"foo", 3, &item_1);
    TEST("delete found a match", !r);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"foo", 3, trie_callback, &td);
    TEST("0 match exact", td.num_matches == 0);

    /* Two things on one entry */
    nitro_prefix_trie_add(&root,
        (uint8_t *)"foo", 3, &item_1);

    nitro_prefix_trie_add(&root,
        (uint8_t *)"food", 4, &item_2);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"foo", 3, trie_callback, &td);
    TEST("1 match short", td.num_matches == 1);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"food", 4, trie_callback, &td);
    TEST("2 match longer", td.num_matches == 2);

    r = nitro_prefix_trie_del(root,
        (uint8_t *)"foo", 3, &item_1);
    TEST("re-delete found a match", !r);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"foo", 3, trie_callback, &td);
    TEST("0 match exact after delete", td.num_matches == 0);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"foodie", 6, trie_callback, &td);
    TEST("1 match on longer (still)", td.num_matches == 1);

    nitro_prefix_trie_add(&root,
        (uint8_t *)"f", 1, &item_3);

    nitro_prefix_trie_add(&root,
        (uint8_t *)"foodie", 6, &item_4);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"foodie", 6, trie_callback, &td);
    TEST("3 match after two adds around (still)",
        td.num_matches == 3);

    nitro_prefix_trie_add(&root,
        (uint8_t *)"", 0, &item_5);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"foodie", 6, trie_callback, &td);
    TEST("4 match after wildcard",
        td.num_matches == 4);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"tango", 5, trie_callback, &td);
    TEST("1 match on just wildcard",
        td.num_matches == 1); 

    r = nitro_prefix_trie_del(root,
        (uint8_t *)"", 0, &item_1);

    TEST("no match on item ptr del", r == -1);

    r = nitro_prefix_trie_del(root,
        (uint8_t *)"", 0, &item_5);

    TEST("match on wildcard del", r == 0);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"foodie", 6, trie_callback, &td);
    TEST("3 match after wildcard",
        td.num_matches == 3);

    r = nitro_prefix_trie_del(root,
        (uint8_t *)"f", 1, &item_3);
    TEST("delete lower around match", r == 0);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"foodie", 6, trie_callback, &td);
    TEST("2 match after wildcard",
        td.num_matches == 2);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"fem", 3, trie_callback, &td);
    TEST("no short matches left",
        td.num_matches == 0);

    r = nitro_prefix_trie_del(root,
        (uint8_t *)"foodie", 6, &item_4);

    TEST("delete around long hits", !r);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"foodie", 6, trie_callback, &td);
    TEST("down to 1 match",
        td.num_matches == 1);

    nitro_prefix_trie_add(&root,
        (uint8_t *)"food", 4, &item_4);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"foodie", 6, trie_callback, &td);
    TEST("two matches on same item",
        td.num_matches == 2);

    r = nitro_prefix_trie_del(root,
        (uint8_t *)"food", 4, &item_3);
    TEST("del miss either valid",
        r == -1);

    r = nitro_prefix_trie_del(root,
        (uint8_t *)"food", 4, &item_2);
    TEST("del hit early one", r == 0);

    td.num_matches = 0;
    nitro_prefix_trie_search(root,
    (uint8_t *)"foodie", 6, trie_callback, &td);
    TEST("just one left", td.num_matches == 1);

    SUMMARY(0);
    return 1;
}
