redo-ifchange torture.o ../libnitro.a
$CC -L.. torture.o -lnitro -lev -pthread -o $3 $EXTRA_LDFLAGS
