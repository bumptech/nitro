platform=`uname`

EXTRA_LDFLAGS=""
if test $platform == "Darwin"; then 
    echo " ---> Configured for Darwin">&2;
    EXTRA_LDFLAGS="-framework CoreServices -framework -CoreFoundation $EXTRA_LDFLAGS"
elif test $platform == "Linux"; then
    echo " ---> Configured for Linux">&2;
fi
export EXTRA_LDFLAGS
