redo-ifchange tcppubsub.o ../libnitro.a
gcc -L.. tcppubsub.o -lnitro -luv -lpthread -o $3 $EXTRA_LDFLAGS
