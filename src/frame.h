#ifndef FRAME_H
#define FRAME_H

#include "cbuffer.h"
#include "common.h"

#include "util.h"

#define NITRO_FRAME_DATA 0
#define NITRO_FRAME_SUB  1
#define NITRO_FRAME_HELLO  2

#define NITRO_KEY_LENGTH (64)

/* Used for publishing */
typedef struct nitro_key_t {
    char key[NITRO_KEY_LENGTH];
    struct nitro_key_t *prev;
    struct nitro_key_t *next;
} nitro_key_t;


typedef struct nitro_protocol_header {
    char protocol_version;
    char packet_type;
    uint8_t num_ident;
    uint8_t flags;
    uint32_t frame_size;
} nitro_protocol_header;

#define FRAME_BZERO_SIZE \
    ((sizeof(void *) * 3) + \
    (sizeof(char) * 4))

typedef struct nitro_frame_t {
    /* NOTE: careful about order here!  
    fields that must be zeroed should
    be first.. adjust FRAME_BZERO_SIZE 
    above if you add fields */

    /* START bzero() region */
    nitro_key_t *key;
    /* num idents to send on end of buffer */
    nitro_counted_buffer_t *ident_buffer;
    void *ident_data;

    uint8_t num_ident;
    char iovec_set;
    char type;
    /* send self ident? */
    char skip_self;

    /* END bzero() region */

    nitro_counted_buffer_t *buffer;
    void *data;
    uint32_t size;

    // TCP
    nitro_protocol_header tcp_header;
    struct iovec iovs[3];

} nitro_frame_t;

nitro_frame_t *nitro_frame_copy(nitro_frame_t *f);
nitro_frame_t *nitro_frame_new(void *data, uint32_t size, nitro_free_function ff, void *baton);
nitro_frame_t *nitro_frame_new_prealloc(void *data, uint32_t size, nitro_counted_buffer_t *buffer);
nitro_frame_t *nitro_frame_new_copy(void *data, uint32_t size);
inline void *nitro_frame_data(nitro_frame_t *fr);
inline uint32_t nitro_frame_size(nitro_frame_t *fr);
inline struct iovec *nitro_frame_iovs(nitro_frame_t *fr, int *num);
void nitro_frame_iovs_advance(nitro_frame_t *fr, int index, int offset);
void nitro_frame_iovs_reset(nitro_frame_t *fr);
#define nitro_frame_destroy(f) {\
    nitro_counted_buffer_decref((f)->buffer);\
    if ((f)->ident_buffer) {\
        nitro_counted_buffer_decref((f)->ident_buffer);\
    }\
    free(f);\
}


#endif /* FRAME_H */
