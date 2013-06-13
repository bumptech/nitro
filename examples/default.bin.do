redo-ifchange $1.o ../libnitro.a

$CC -L.. $1.o -lnitro -lev -pthread -o $3 $EXTRA_LDFLAGS
#$CC -L.. -pg $1.o -lnitro -lev -pthread -o $3 $EXTRA_LDFLAGS
