# on mac, this gets screwed up... 
path=`echo $1 | sed "s/\.o$//"`

headers=`find src -name '*.h'`

redo-ifchange $path.c $headers
gcc -O2 -fno-strict-aliasing -Wall -Werror -DWVTEST_CONFIGURED -fPIC -g -Isrc -I$HERE/test/wvtest -c $path.c -o $3
