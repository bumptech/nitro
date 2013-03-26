redo-ifchange sender.o ../libnitro.a
gcc -L.. sender.o -lnitro -lev -lpthread -o $3 $EXTRA_LDFLAGS
