Nitro
=====

Nitro is a very fast, flexible, high-level network communication
library.

http://gonitro.io

Build
-----

You need:

 0. Linux, Mac OS X, FreeBSD... something Unix-y
 1. redo, a build tool ( https://github.com/apenwarr/redo )
 2. libev development libraries installed ( something like `apt-get install libev-dev` )
 3. libsodium ( https://github.com/jedisct1/libsodium )

Then:

**Build Nitro**

    $ redo

**Run the Nitro test suite**

    $ redo check

**Install Nitro**

    $ sudo redo install

Build Note
----------

If you need to specify a different gcc executable
besides just `gcc`, you can define $CC in the environment:

    CC=gcc-4.7 redo

Using Nitro
-----------

Nitro uses pkg-config; so after you're installed, just do something like:

    $ cat test.c
    #include <nitro.h>

    void main() {
            nitro_runtime_start();
    }
    $ gcc `pkg-config --cflags nitro` test.c `pkg-config --libs nitro`
    $ ./a.out
    $

Examples
--------

You can find examples in the `examples/` directory in the distribution.

Bindings, Docs, Etc.
--------

Check out the website:

http://docs.gonitro.io

Status
------

Nitro is beta software.  Submit bug reports, please!

Author
------

Jamie Turner <jamie@bu.mp> @jamwt
