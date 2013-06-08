redo-ifchange targeted.o ../libnitro.a
$CC -L.. targeted.o -lnitro -lev -pthread -o $3 $EXTRA_LDFLAGS
