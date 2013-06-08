redo-ifchange proxied.o ../libnitro.a
$CC -L.. proxied.o -lnitro -lev -pthread -o $3 $EXTRA_LDFLAGS
