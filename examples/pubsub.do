redo-ifchange pubsub.o ../libnitro.a
$CC -L.. pubsub.o -lnitro -lev -pthread -o $3 $EXTRA_LDFLAGS
