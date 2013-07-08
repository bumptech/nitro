$CC -O2 -Wall -Werror -std=gnu99 -g -I../src -L.. $1.c -lnitro -lev -lsodium -pthread -o $3 $EXTRA_LDFLAGS
#$VALGRIND ./$3
