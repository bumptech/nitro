redo-ifchange basic.o ../libnitro.a
$CC -L.. basic.o -lnitro -lev -lpthread -o $3 $EXTRA_LDFLAGS
