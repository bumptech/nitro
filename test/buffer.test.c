#include "test.h"
#include "buffer.h"

int main(int argc, char **argv) {
    nitro_buffer_t *buf = nitro_buffer_new();
    TEST("buffer non-null", buf);

    char *ex1 = "this is a string";
    int ex1_size = strlen(ex1) + 1;
    nitro_buffer_append(buf, ex1, ex1_size);
    int size;
    char *ret = nitro_buffer_data(buf, &size);
    TEST("simple append length", size == ex1_size);
    TEST("simple append strcmp", !strcmp(ret, ex1));
    TEST("simple append diff ptr", ret != ex1);

    char *ex2 = "this is another string";
    int ex2_size = strlen(ex1) + 1;
    nitro_buffer_append(buf, ex2, ex2_size);

    ret = nitro_buffer_data(buf, &size);
    TEST("follow append length", size == (ex1_size + ex2_size));

    char lbuf[50000];
    memcpy(lbuf, ex1, ex1_size);
    memcpy(lbuf + ex1_size, ex2, ex2_size);
    TEST("follow append memcmp", !memcmp(ret, lbuf, ex1_size + ex2_size));

    int resized = (1013 * 1013);
    char *write = nitro_buffer_prepare(buf, &resized);
    TEST("prepare resize exceeds ask", resized >= (1013 * 1013));
    TEST("prepare loc is non-null", write);

    memset(write, 'X', 40000);
    nitro_buffer_extend(buf, 40000);
    memset(lbuf + ex1_size + ex2_size, 'X', 40000);
    
    ret = nitro_buffer_data(buf, &size);
    TEST("extend size is sum", size == (ex1_size + ex2_size + 40000));
    TEST("total value memcmp", !memcmp(ret, lbuf, size));

    nitro_buffer_destroy(buf);

    SUMMARY(0);
    return 1;
}
