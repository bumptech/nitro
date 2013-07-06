if [[ -z $PREFIX ]]; then
    export PREFIX="/usr/local";
fi

hash pkg-config || (echo "install pkg-config first" && exit 1)

export HERE=`pwd`

mkdir -p $PREFIX/include/nitro
cp src/*.h $PREFIX/include/nitro
cp -r src/uthash $PREFIX/include/nitro
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
echo "Cflags: -std=gnu99 -I\${includedir}/nitro"
echo "Libs: -L\${libdir} -lnitro -lev -pthread";
) > $PREFIX/lib/pkgconfig/nitro.pc
