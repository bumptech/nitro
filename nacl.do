tar jxvf nacl-20110221.tar.bz2
sed -i.orig 's/$/ -fPIC/' nacl-20110221/okcompilers/c
sed -i.orig 's/$/ -fPIC/' nacl-20110221/okcompilers/cpp
cd nacl-20110221 && ./do
