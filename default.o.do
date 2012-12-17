# on mac, this gets screwed up... 
path=`echo $1 | sed "s/\.o$//"`

redo-ifchange $path.c src/nitro.h src/nitro-private.h
gcc -O2 -Wall -Werror -fPIC -g -Isrc -c $path.c -o $3
