#ifndef FRAME_H
#define FRAME_H

#include "common.h"

#include "util.h"

#define NITRO_KEY_LENGTH 1024

typedef enum {
    NITRO_FRAME_DATA,
    NITRO_FRAME_SUB
} NITRO_FRAME_TYPE;

typedef enum {
    NITRO_PACKET_FRAME,
    NITRO_PACKET_SUB
} NITRO_PACKET_TYPE;


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

typedef struct nitro_frame_t {
    void *buffer;
    uint32_t size;
    nitro_free_function ff;
    void *baton;

    int is_pub;
    NITRO_FRAME_TYPE type;
    char key[NITRO_KEY_LENGTH];

    pthread_mutex_t lock;

    // TCP
    int iovec_set;
    nitro_protocol_header tcp_header;
    struct iovec iovs[2];

    // For UT_LIST on pipe or socket
    struct nitro_frame_t *prev;
    struct nitro_frame_t *next;
} nitro_frame_t;

nitro_frame_t *nitro_frame_copy(nitro_frame_t *f);
nitro_frame_t *nitro_frame_new(void *data, uint32_t size, nitro_free_function ff, void *baton);
void nitro_frame_destroy(nitro_frame_t *f);
nitro_frame_t *nitro_frame_new_copy(void *data, uint32_t size);
inline void *nitro_frame_data(nitro_frame_t *fr);
inline uint32_t nitro_frame_size(nitro_frame_t *fr);
inline struct iovec *nitro_frame_iovs(nitro_frame_t *fr, int *num);
void nitro_frame_iovs_advance(nitro_frame_t *fr, int index, int offset);

#endif /* FRAME_H */
