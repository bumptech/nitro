#ifndef NITRO_SINPROC_H
#define NITRO_SINPROC_H

#include "socket.h"

int Sinproc_socket_connect(nitro_inproc_socket_t *s, char *location);
int Sinproc_socket_bind(nitro_inproc_socket_t *s, char *location);
void Sinproc_socket_bind_listen(nitro_inproc_socket_t *s);
void Sinproc_socket_enable_writes(nitro_inproc_socket_t *s);
void Sinproc_socket_enable_reads(nitro_inproc_socket_t *s);
void Sinproc_socket_start_connect(nitro_inproc_socket_t *s);
void Sinproc_socket_shutdown(nitro_inproc_socket_t *s);
void Sinproc_socket_close(nitro_inproc_socket_t *s);

#endif /* NITRO_SINPROC_H */
