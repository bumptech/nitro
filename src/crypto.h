#ifndef CRYPTO_H
#define CRYPTO_H

#include "socket.h"

void crypto_make_keypair(uint8_t *pub, uint8_t *sec);
void crypto_make_pipe_cache(nitro_tcp_socket_t *s, nitro_pipe_t *p);
void crypto_generate_nonce(nitro_pipe_t *p, uint8_t *ptr);
nitro_frame_t *crypto_frame_encrypt(nitro_frame_t *fr, nitro_pipe_t *p);
uint8_t *crypto_decrypt_frame(const uint8_t *enc, size_t enc_len,
    nitro_pipe_t *p, size_t *out_len, nitro_counted_buffer_t **buf);

#endif /* CRYPTO_H */
