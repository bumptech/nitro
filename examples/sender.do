redo-ifchange sender.o ../libnitro.a
$CC -L.. sender.o -lnitro -lev -pthread -o $3 $EXTRA_LDFLAGS
