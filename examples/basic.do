redo-ifchange basic.o ../libnitro.a
$CC -L.. basic.o -lnitro -lev -pthread -o $3 $EXTRA_LDFLAGS
#$CC -L.. basic.o -lnitro -lev -pthread -pg -o $3 $EXTRA_LDFLAGS
