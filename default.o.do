path=`echo $1 | sed "s/\.o$//"`

headers=`find src -name '*.h'`

redo-ifchange $path.c $headers
gcc -O2 -fno-strict-aliasing -Wall -Werror -fPIC -g -Isrc -I$NACL_INC -c $path.c -o $3

# for profiling
#gcc -O2 -fno-strict-aliasing -Wall -Werror -fPIC -g -Isrc -I$NACL_INC -c $path.c -o $3 -pg
