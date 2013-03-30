gcc -O2 -Wall -Werror -g -I../src -I$NACL_INC -L.. $1.c -lnitro -lev -lpthread -o $3 $EXTRA_LDFLAGS
#$VALGRIND ./$3
