platform=`uname`

export HERE=`pwd`
export NACL_LIB=$(dirname `find $HERE/nacl-* -name "libnacl.a"`)
export NACL_INC=$(dirname `find $HERE/nacl-* -name crypto_box.h`)

EXTRA_LDFLAGS=""

if [ -z "$CC" ]; then
    export CC="gcc";
fi
if test $platform == "Darwin"; then 
    echo " ---> Configured for Darwin">&2;
    EXTRA_LDFLAGS="-framework CoreServices -framework CoreFoundation $EXTRA_LDFLAGS"
elif test $platform == "Linux"; then
    echo " ---> Configured for Linux">&2;
    EXTRA_LDFLAGS="$NACL_LIB/libnacl.a $NACL_LIB/randombytes.o $NACL_LIB/cpucycles.o"
fi
export EXTRA_LDFLAGS
