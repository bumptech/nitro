Nitro
=====

Nitro is a very fast, flexible, high-level network communication
library.

http://gonitro.io

Build
-----

You need:

 0. Linux or Mac OS X
 1. redo ( https://github.com/apenwarr/redo )
 2. libev development libraries installed

Then:

**Build Nacl**

    $ redo nacl

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

Bindings
--------

Bindings for Python and Haskell coming soon.

Docs
----

Find everything you need at the website.  http://gonitro.io

Status
------

Nitro is beta software.  Submit bug reports, please!

Author
------

Jamie Turner <jamie@bu.mp> @jamwt
