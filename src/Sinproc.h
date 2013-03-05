#ifndef NITRO_SINPROC_H
#define NITRO_SINPROC_H
int Sinproc_socket_connect(nitro_inproc_socket_t *s, char *location);
int Sinproc_socket_bind(nitro_inproc_socket_t *s, char *location);
void Sinproc_socket_bind_listen(nitro_inproc_socket_t *s);

#endif /* NITRO_SINPROC_H */
