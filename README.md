Nitro
=====

Nitro is a very fast, flexible, high-level network communication
library.

Build
-----

You need:

 0. Linux (platforms besides linux/amd64 not tested yet)
 1. redo ( https://github.com/apenwarr/redo )
 2. gcc-4.7 ( Ubuntu 12.04 does not have this by default! )
 3. libev development libraries installed

Then:

**Create a build of nacl on your machine (nacl tarball included)**

    $ tar jxvf nacl-20110221.tar.bz2
    $ cd nacl-20110221
    $ ./do

     .. wait 5-20 minutes ..

    $ cd ..

**Build Nitro**

    $ redo

**Run the Nitro test suite**

    $ redo check

Build Note
----------

If you need to specify a different gcc executable
besides just `gcc`, you can define $CC in the environment:

    CC=gcc-4.7 redo

Examples
--------

You can find examples in the `examples/` directory in the distribution.

Bindings
--------

Bindings for Python and Haskell coming soon.

Status
------

Nitro is alpha software.  Submit bug reports, please!  There is no
global installer, so check out the `examples/*.do` files to see
what a build looks like.

Author
------

Jamie Turner <jamie@bu.mp> @jamwt
