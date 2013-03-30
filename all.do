export HERE=`pwd`
export NACL_LIB=$(dirname `find $HERE/nacl-* -name "libnacl.a"`)
export NACL_INC=$(dirname `find $HERE/nacl-* -name crypto_box.h`)
source ./platform.sh
export LIBOBJ="`find $HERE/src -name '*.c' | sed 's/\.c/\.o/g'`"
redo-ifchange libnitro.a
redo examples/basic
redo examples/sender
