#ifndef UTIL_H
#define UTIL_H

#define ZALLOC(p) {p = (typeof(p))calloc(1, sizeof(*p));}

typedef void (*nitro_free_function)(void *, void *);
void just_free(void *data, void *unused);
double now_double();
void buffer_free(void *data, void *bufptr);
void cbuffer_decref(void *data, void *bufptr);


#endif /* UTIL_H */
