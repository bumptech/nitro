path=`echo $1 | sed "s/\.o$//"`

headers=`find src -name '*.h'`

redo-ifchange $path.c $headers
gcc -O2 -fno-strict-aliasing -Wall -Werror -fPIC -g -Isrc -c $path.c -o $3
