if [[ -z $PREFIX ]]; then
    export PREFIX="/usr/local";
fi

hash pkg-config || (echo "install pkg-config first" && exit 1)

export HERE=`pwd`
export NACL_INC=$(dirname `find $HERE/nacl-* -name crypto_box.h`)

mkdir -p $PREFIX/include/nitro
mkdir -p $PREFIX/include/nitro/nitronacl
cp src/*.h $PREFIX/include/nitro
cp -r src/uthash $PREFIX/include/nitro
cp -r $NACL_INC/* $PREFIX/include/nitro/nitronacl
mkdir -p $PREFIX/lib/pkgconfig
cp libnitro*.a $PREFIX/lib

(
echo "prefix=$PREFIX";
echo "exec_prefix=\${prefix}";
echo "includedir=\${prefix}/include";
echo "libdir=\${exec_prefix}/lib";
echo "";
echo "Name: nitro";
echo "Description: The nitro library";
echo "Version: 0.1";
echo "Cflags: -I\${includedir}/nitro -I\${includedir}/nitro/nitronacl";
echo "Libs: -L\${libdir} -lnitro -lnitronacl -lev -pthread";
) > $PREFIX/lib/pkgconfig/nitro.pc
