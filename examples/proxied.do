redo-ifchange proxied.o ../libnitro.a
gcc -L.. proxied.o -lnitro -lev -lpthread -o $3 $EXTRA_LDFLAGS
