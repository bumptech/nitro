redo-ifchange $NACL_LIB/libnacl.a $NACL_LIB/randombytes.o $NACL_LIB/cpucycles.o
cp $NACL_LIB/libnacl.a $3
ar rsc $3 $NACL_LIB/randombytes.o $NACL_LIB/cpucycles.o
