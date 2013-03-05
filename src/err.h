#ifndef ERR_H
#define ERR_H

typedef enum {
    NITRO_ERR_ERRNO,
    NITRO_ERR_ALREADY_RUNNING,
    NITRO_ERR_NOT_RUNNING,
    NITRO_ERR_TCP_LOC_NOCOLON,
    NITRO_ERR_TCP_LOC_BADPORT,
    NITRO_ERR_TCP_LOC_BADIPV4,
    NITRO_ERR_PARSE_BAD_TRANSPORT,
} NITRO_ERROR;

int nitro_set_error(NITRO_ERROR e);
char *nitro_errmsg(NITRO_ERROR error);
NITRO_ERROR nitro_error();

#endif /* ERR_H */
