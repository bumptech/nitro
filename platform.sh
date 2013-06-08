platform=`uname`

export HERE=`pwd`
if [ -z "`find $HERE/nacl-* -name "libnacl.a"`" ]; then
    echo '"redo nacl" first!' 1>&2;
    exit 1;
fi

export NACL_LIB=$(dirname `find $HERE/nacl-* -name "libnacl.a" | head -1`)
export NACL_INC=$(dirname `find $HERE/nacl-* -name crypto_box.h | head -1`)

EXTRA_LDFLAGS=""

if [ -z "$CC" ]; then
    export CC="gcc";
fi
if test $platform == "Darwin"; then 
    echo " ---> Configured for Darwin">&2;
    EXTRA_LDFLAGS="-L/usr/local/lib $NACL_LIB/libnacl.a $NACL_LIB/randombytes.o $NACL_LIB/cpucycles.o"
    CC="$CC -I/usr/local/include"
elif test $platform == "Linux"; then
    echo " ---> Configured for Linux">&2;
    EXTRA_LDFLAGS="$NACL_LIB/libnacl.a $NACL_LIB/randombytes.o $NACL_LIB/cpucycles.o"
fi
export EXTRA_LDFLAGS

echo "-I$HERE/src -I$NACL_INC" > NITRO_CFLAGS
echo "-L$HERE -lnitro -lev -lpthread $EXTRA_LDFLAGS" > NITRO_LDFLAGS

if [ -e "/proc/cpuinfo" ]; then
    CORES=`grep '^processor' /proc/cpuinfo | wc -l`
    echo " ...  Building with $CORES cores..." 1>&2
    REDO_CMD="redo -j $CORES"
else
    REDO_CMD="redo"
fi
