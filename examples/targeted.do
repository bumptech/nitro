redo-ifchange targeted.o ../libnitro.a
gcc -L.. targeted.o -lnitro -lev -lpthread -o $3 $EXTRA_LDFLAGS
