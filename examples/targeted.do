redo-ifchange targeted.o ../libnitro.a
$CC -L.. targeted.o -lnitro -lev -lpthread -o $3 $EXTRA_LDFLAGS
