redo-ifchange $1.c ../libnitro.a
gcc -Wall -Werror -g -I../src -L.. $1.c -lnitro -lev -lpthread -o $3 $EXTRA_LDFLAGS
