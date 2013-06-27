#ifndef ERR_H
#define ERR_H

#define NITRO_ERROR int

#define NITRO_ERR_NONE                  0
#define NITRO_ERR_ERRNO                 1
#define NITRO_ERR_ALREADY_RUNNING       2
#define NITRO_ERR_NOT_RUNNING           3
#define NITRO_ERR_TCP_LOC_NOCOLON       4
#define NITRO_ERR_TCP_LOC_BADPORT       5
#define NITRO_ERR_TCP_LOC_BADIPV4       6
#define NITRO_ERR_PARSE_BAD_TRANSPORT   7
#define NITRO_ERR_EAGAIN                8
#define NITRO_ERR_NO_RECIPIENT          9
#define NITRO_ERR_ENCRYPT               10
#define NITRO_ERR_DECRYPT               11
#define NITRO_ERR_INVALID_CLEAR         12
#define NITRO_ERR_MAX_FRAME_EXCEEDED    13
#define NITRO_ERR_BAD_PROTOCOL_VERSION  14
#define NITRO_ERR_DOUBLE_HANDSHAKE      15
#define NITRO_ERR_NO_HANDSHAKE          16
#define NITRO_ERR_BAD_SUB               17
#define NITRO_ERR_BAD_HANDSHAKE         18
#define NITRO_ERR_INVALID_CERT          19
#define NITRO_ERR_BAD_INPROC_OPT        20
#define NITRO_ERR_BAD_SECURE            21
#define NITRO_ERR_INPROC_ALREADY_BOUND  22
#define NITRO_ERR_INPROC_NOT_BOUND      23
#define NITRO_ERR_INPROC_NO_CONNECTIONS 24
#define NITRO_ERR_SUB_ALREADY           25
#define NITRO_ERR_SUB_MISSING           26
#define NITRO_ERR_TCP_BAD_ANY           27

int nitro_set_error(NITRO_ERROR e);
char *nitro_errmsg(NITRO_ERROR error);
NITRO_ERROR nitro_error();
void nitro_clear_error();
int nitro_has_error();
void nitro_error_log_handler(int err, void *baton);
void nitro_err_start();
void nitro_err_stop();

#endif /* ERR_H */
