#ifndef NITRO_STCP_H
#define NITRO_STCP_H

#include "socket.h"

int Stcp_socket_bind(nitro_tcp_socket_t *s, char *location);
int Stcp_socket_connect(nitro_tcp_socket_t *s, char *location);
void Stcp_socket_close(nitro_tcp_socket_t *s);
void Stcp_socket_bind_listen(nitro_tcp_socket_t *s);
void Stcp_socket_enable_writes(nitro_tcp_socket_t *s);
void Stcp_socket_enable_reads(nitro_tcp_socket_t *s);
void Stcp_socket_start_connect(nitro_tcp_socket_t *s);
void Stcp_socket_start_shutdown(nitro_tcp_socket_t *s);

int Stcp_socket_send(nitro_tcp_socket_t *s, nitro_frame_t **frp, int flags);
nitro_frame_t *Stcp_socket_recv(nitro_tcp_socket_t *s, int flags);
int Stcp_socket_reply(nitro_tcp_socket_t *s, nitro_frame_t *snd, nitro_frame_t **frp, int flags);
void Stcp_pipe_enable_write(nitro_pipe_t *p);
int Stcp_socket_relay_fw(nitro_tcp_socket_t *s, nitro_frame_t *snd, nitro_frame_t **frp, int flags);
int Stcp_socket_relay_bk(nitro_tcp_socket_t *s, nitro_frame_t *snd, nitro_frame_t **frp, int flags);
int Stcp_socket_sub(nitro_tcp_socket_t *s,
    uint8_t *k, size_t length);
int Stcp_socket_unsub(nitro_tcp_socket_t *s,
    uint8_t *k, size_t length);
int Stcp_socket_pub(nitro_tcp_socket_t *s,
    nitro_frame_t **frp, const uint8_t *k,
    size_t length, int flags);

#endif /* NITRO_STCP_H */
