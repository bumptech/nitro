#include "common.h"
#include "crypto.h"
#include "err.h"
#include "runtime.h"

void crypto_make_keypair(uint8_t *pub, uint8_t *sec) {
    crypto_box_keypair(pub, sec);
}

void crypto_make_pipe_cache(nitro_tcp_socket_t *s, nitro_pipe_t *p) {
    int r = crypto_box_beforenm(
        p->crypto_cache,
        p->remote_ident,
        s->opt->pkey);
    assert(!r);

    r = read(the_runtime->random_fd,
        p->nonce_gen, crypto_box_NONCEBYTES);
    assert(r == crypto_box_NONCEBYTES);

    p->nonce_incr = (uint64_t *)p->nonce_gen;
    *(p->nonce_incr) = 0;
}

void crypto_generate_nonce(nitro_pipe_t *p, uint8_t *ptr) {
    NITRO_THREAD_CHECK;

    (*p->nonce_incr)++;

    /* Note: Cannot send more than (1 << 64) */
    assert(*p->nonce_incr != 0);

    memcpy(ptr, p->nonce_gen, crypto_box_NONCEBYTES);
}

nitro_frame_t *crypto_frame_encrypt(nitro_frame_t *fr, nitro_pipe_t *p) {
    nitro_buffer_t *buf = nitro_buffer_new();
    int want_growth = crypto_box_ZEROBYTES + crypto_box_NONCEBYTES;
    uint8_t *ptr = (uint8_t *)nitro_buffer_prepare(buf, &want_growth);
    crypto_generate_nonce(p, ptr);
    bzero(ptr + crypto_box_NONCEBYTES, crypto_box_ZEROBYTES);
    nitro_buffer_extend(buf, crypto_box_ZEROBYTES + crypto_box_NONCEBYTES);

    int count;

    struct iovec *iovs = nitro_frame_iovs(fr, &count);

    int i;

    for (i=0; i < count; i++) {
        nitro_buffer_append(buf, iovs[i].iov_base, iovs[i].iov_len);
    }

    nitro_frame_destroy(fr);

    int size;
    uint8_t *clear = (uint8_t *)nitro_buffer_data(buf, &size);

    uint8_t *enc = malloc(size);
    memcpy(enc, clear, crypto_box_NONCEBYTES);
    int r = crypto_box_afternm(enc + crypto_box_NONCEBYTES, clear + crypto_box_NONCEBYTES, 
        size - crypto_box_NONCEBYTES, clear, p->crypto_cache);

    nitro_buffer_destroy(buf);
    if (r != 0) {
        return NULL;
    }


    fr = nitro_frame_new(enc, size, just_free, NULL);
    fr->type = NITRO_FRAME_SECURE;

    return fr;
}

uint8_t *crypto_decrypt_frame(const uint8_t *enc, size_t enc_len,
    nitro_pipe_t *p, size_t *out_len, nitro_counted_buffer_t **buf) {

    if (!(enc_len >= crypto_box_NONCEBYTES + crypto_box_ZEROBYTES)) {
        nitro_set_error(NITRO_ERR_DECRYPT);
        return NULL;
    }
    uint8_t *clear = malloc(enc_len - crypto_box_NONCEBYTES);

    int r = crypto_box_open_afternm(clear, enc + crypto_box_NONCEBYTES,
    enc_len - crypto_box_NONCEBYTES, enc, p->crypto_cache);
    if (r != 0) {
        free(clear);
        nitro_set_error(NITRO_ERR_DECRYPT);
        return NULL;
    }

    *out_len = enc_len - crypto_box_NONCEBYTES - crypto_box_ZEROBYTES;
    *buf = nitro_counted_buffer_new(clear, just_free, NULL);

    return clear + crypto_box_ZEROBYTES;
}
