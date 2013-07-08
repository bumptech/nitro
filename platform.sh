platform=`uname`

export HERE=`pwd`

ARCH=`uname -m`

if [ "$ARCH" = "x86_64" ]; then
    ARCH=amd64
fi

EXTRA_LDFLAGS=""

if [ -z "$CC" ]; then
    export CC="gcc";
fi
if test $platform == "Darwin"; then 
    echo " ---> Configured for Darwin">&2;
    EXTRA_LDFLAGS="-L/usr/local/lib"
    CC="$CC -I/usr/local/include"
elif test $platform == "FreeBSD"; then 
    echo " ---> Configured for FreeBSD">&2;
    EXTRA_LDFLAGS="-L/usr/local/lib"
    CC="$CC -I/usr/local/include"
elif test $platform == "Linux"; then
    echo " ---> Configured for Linux">&2;
    EXTRA_LDFLAGS=""
fi
export EXTRA_LDFLAGS

if [ -e "/proc/cpuinfo" ]; then
    CORES=`grep '^processor' /proc/cpuinfo | wc -l`
    echo " ...  Building with $CORES cores..." 1>&2
    REDO_CMD="redo -j $CORES"
else
    REDO_CMD="redo"
fi
