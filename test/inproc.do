redo-ifchange inproc.o ../libnitro.a
gcc -L.. inproc.o -lnitro -luv -lpthread -o $3 $EXTRA_LDFLAGS
