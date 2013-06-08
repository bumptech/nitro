$CC -O2 -Wall -Werror -g -I../src -I$NACL_INC -L.. $1.c -lnitro -lev -pthread -o $3 $EXTRA_LDFLAGS
#$VALGRIND ./$3
