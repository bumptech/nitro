redo-ifchange torture.o ../libnitro.a
$CC -L.. torture.o -lnitro -lev -lpthread -o $3 $EXTRA_LDFLAGS
