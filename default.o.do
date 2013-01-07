# on mac, this gets screwed up... 
path=`echo $1 | sed "s/\.o$//"`

redo-ifchange $path.c src/nitro.h src/nitro-private.h
gcc -O2 -Wall -Werror -DWVTEST_CONFIGURED -fPIC -g -Isrc -I$HERE/test/wvtest -c $path.c -o $3
