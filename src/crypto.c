#include "common.h"

void crypto_make_keypair(uint8_t *pub, uint8_t *sec) {
    crypto_box_keypair(pub, sec);
}
