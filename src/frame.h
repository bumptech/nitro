#ifndef FRAME_H
#define FRAME_H

#include "cbuffer.h"
#include "common.h"

#include "util.h"

#define NITRO_FRAME_DATA 0
#define NITRO_FRAME_SUB  1

typedef enum {
    NITRO_PACKET_FRAME,
    NITRO_PACKET_SUB
} NITRO_PACKET_TYPE;

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
    uint16_t flags;
    uint32_t frame_size;
} nitro_protocol_header;

#define FRAME_BZERO_SIZE \
    (sizeof(char) + sizeof(char) + \
     sizeof(nitro_key_t *) + \
     sizeof (nitro_frame_t *) +\
     sizeof(nitro_frame_t *))

typedef struct nitro_frame_t {
    /* NOTE: careful about order here!  
    fields that must be zeroed should
    be first.. adjust FRAME_BZERO_SIZE 
    above if you add fields */

    /* START bzero() region */
    char iovec_set;
    char type;
    nitro_key_t *key;

    // For UT_LIST on pipe or socket
    struct nitro_frame_t *prev;
    struct nitro_frame_t *next;
    /* END bzero() region */

    nitro_counted_buffer_t *buffer;
    void *data;
    uint32_t size;

    // TCP
    nitro_protocol_header tcp_header;
    struct iovec iovs[2];

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
    free(f);\
}


#endif /* FRAME_H */
