redo-ifchange inprocpubsub.o ../libnitro.a
gcc -L.. inprocpubsub.o -lnitro -luv -lpthread -o $3 $EXTRA_LDFLAGS
