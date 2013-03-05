#ifndef NITRO_STCP_H
#define NITRO_STCP_H

int Stcp_socket_bind(nitro_tcp_socket_t *s, char *location);
int Stcp_socket_connect(nitro_tcp_socket_t *s, char *location);
void Stcp_socket_bind_listen(nitro_tcp_socket_t *s);

#endif /* NITRO_STCP_H */
