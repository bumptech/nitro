#include "test.h"
#include "cbuffer.h"

typedef struct whatever {
    char *alloc;
    char gone;
} whatever;

void my_free(void *main, void *ctx) {
    whatever *w = (whatever *)ctx;
    TEST("buffer unfreed", w->alloc);
    TEST("alloc eq region", w->alloc == main);
    free(w->alloc);
    w->gone = 1;
}

int main(int argc, char **argv) {
    whatever w = {0};
    w.alloc = malloc(5);
    nitro_counted_buffer_t *buf = nitro_counted_buffer_new(
        w.alloc, my_free, &w);
    TEST("cbuffer non-null", buf);
    nitro_counted_buffer_decref(buf);
    TEST("marked done", w.gone);

    whatever w2 = {0};
    w2.alloc = malloc(5);
    buf = nitro_counted_buffer_new(
        w2.alloc, my_free, &w2);
    int i = 0;
    for (i=0; i < 200; i++) {
        nitro_counted_buffer_incref(buf);
        nitro_counted_buffer_decref(buf);
    }
    TEST("not marked done", !w2.gone);
    for (i=0; i < 200; i++) {
        nitro_counted_buffer_incref(buf);
    }
    TEST("not marked done", !w2.gone);
    for (i=0; i < 200; i++) {
        nitro_counted_buffer_decref(buf);
    }
    TEST("not marked done", !w2.gone);
    nitro_counted_buffer_decref(buf);
    TEST("marked done", w2.gone);

    SUMMARY(0);
    return 1;
}
