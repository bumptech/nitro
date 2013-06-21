%Nitro Docs
%Jamie Turner
%June 20th, 2013

Introducing Nitro
=================

Nitro is a library for writing scalable, flexible,
and secure network applications.  Nitro applications
create bound sockets ("nitro sockets" here being a
different and higher-level abstraction than BSD sockets)
that listen at a certain location so that other
Nitro sockets can connect and exchange messages.

A message, wrapped in a "frame", is the fundamental
unit of communication between Nitro sockets.  When
a frame is sent, it is either received completely
by another party or not at all.  Application
developers don't need to worry about boundary
conditions, message delimiting, etc.

Sockets can also be set to be "secure", in which
case NaCl-based public-key encryption is used.
These secure sockets can be given a required
public key the peer socket must present to
authenticate the connection (and protect against
man-in-the-middle attacks).

Sockets can either be protocol "tcp" or "inproc".
TCP sockets actually push the message frames between
hosts over a TCP/IP network.
Inproc sockets are thin (but API-compatible) wrappers
on top of thread-safe message queues to help with
multithreaded in-process message passing or to provide
abstraction/generalization of local vs. remote services.

Some use cases for nitro:

 * Private network services, like caches, proxies
   (dispatchers/routers), job queues, database
   communication, distributed systems
 * Public internet services, like low-latency
   push-capable services embedded in desktop
   applications or mobile apps
 * Non-networked multicore utilization in map-reduce
   type work using inproc sockets
 * Pub/sub systems, like network-wide event monitoring,
   newsreaders, etc.

Nitro Concepts
==============

Sockets
-------

The basic bind/connect relationship is the same as BSD
sockets:

 1. Every Nitro connection is between a bound socket
    and a connected socket.  A socket can only `bind` or
    `connect`, not both (this is enforced via the socket constructors).
 2. Bound sockets may be communicating with many connected sockets
 3. Connected sockets are only ever communicating with one bound socket.
 4. Connected sockets will attempt reconnection if the connection is
    interrupted; messages sent while disconnected are queued.

Frame
-----

 1. Every frame (except some internal frames) is created by the application
    programmer, using `nitro_frame_new_copy` and similar frame constructors.
 2. Frames given to one of the sending functions will transfer
    ownership to Nitro unless the NITRO_REUSE flag is passed by
    the application programmer.
 3. Frames received from `nitro_recv` are owned by the application programmer,
    and must either be destroyed or given back to a sending 
    function (for forwarding, etc, thus re-transfering ownership to nitro).
 4. Frames received from inproc sockets can transparently be sent to
    TCP sockets and vice versa.

Receiving
---------

 1. `nitro_recv` is the only way to retrieve incoming frames from a
    socket.
 2. `nitro_recv` will block until a socket is available unless a NOBLOCK
    flag is given; otherwise, it will return NITRO_ERR_EAGAIN

Sending
-------

There are a few different functions in the that can send
frames in slightly different ways ranging from simple sends 
to replies and proxy relays.

**nitro_send**

 1. `nitro_send` is a "naive" send that will deliver to any available
    peer socket.
 2. If the socket is a connected socket, the frame is sent to the
    peer socket if it is currently connected; otherwise it is queued
 3. If the socket is a bound socket, the frame is sent to:
    * On a tcp socket, the first peer socket that will accept the
      write() on the network (i.e, when there is room in the outgoing
      kernel buffer)
    * On an inproc socket, the next peer socket in round-robin order
 4. `nitro_send` will block if the outbound queue is full, unless
    a NOBLOCK flag is given; then it will return NITRO_ERR_EAGAIN

**nitro_reply**

 1. Commonly used on a bound socket with lots of peers, `nitro_reply`
    lets you send a frame *back* to a particular peer.  The peer is
    identified by a frame they originally sent and was dequeued via
    `nitro_recv`.
 2. If the destination socket is still a connected peer, 
    `nitro_reply` will attempt delivery to it; otherwise, an error is 
    returned and the frame is discarded.

**nitro_relay_fw**

 1. Commonly used in a proxy-type situation.  Given a frame that
    came from `nitro_recv`, and a new frame, forward the new frame
    to the destination socket while retaining the routing history
    associated with the old frame.
 2. The general delivery of the new frame follows the same rules
    as `nitro_send`.
 3. The "routing frame" and the "message frame" can be (and often are)
    the same frame.

**nitro_relay_bk**

 1. Commonly used in a proxy-type situation.  Given an old frame that
    came from `nitro_recv`, and a new frame, relay the new frame
    *back* to the original sender using the top of the routing
    stack in the old frame.
 2. The targeted delivery to the socket in the routing stack follows
    the same rules as nitro_reply.
 3. The "routing frame" and the "message frame" can be (and often are)
    the same frame.

Pub and Sub
-----------

**nitro_sub/nitro_unsub**

 1. Subscribe or unsubscribe to some bytestring prefix.  The subscriptions
    are registered with any peer connections.  (Note: even over the network,
    so published messages are sender-filtered).

**nitro_pub**

 1. Publish a frame to a given key.  All peers who are subscribing
    to a prefix of that key will receive the message.
 2. The return value is the number of peers who were sent the message.

Examples
========

*Note: these examples assume you have already used the README.md to
successfully install Nitro and learn how to build Nitro programs.
Please review the README.md if this is not so.*

Pipelines
---------

The simplest example (and fastest, due to nearly no latency) is a single
send/recv pipeline.

We'll build to separate programs, called `rec` and `send`.  One
is the pipeline sender, and the other the receiver.

Let's write the receiver first.  Every Nitro program begins with starting the 
Nitro runtime:

~~~~~{.c}
nitro_runtime_start();
~~~~~

This sets up some global locks, a libev event loop, etc, and starts
the main Nitro thread in the background.

Then, let's create a bound socket.  This will be the side doing the
receiving.

~~~~~{.c}
nitro_socket_t *sock = nitro_socket_bind("tcp://*:4444", NULL);
~~~~~

Now we have a socket listening on port 4444.  Let's make this socket
just print the messages it receives.

~~~~~{.c}
char buf[50] = {0};
while (1) {
    nitro_frame_t *f = nitro_recv(sock, 0);
    assert(nitro_frame_size(f) < 50);
    memcpy(buf, (char *)nitro_frame_data(f), nitro_frame_size(f));
    buf[nitro_frame_size(f)] = 0;
    printf("They said: %s\n", buf);
    nitro_frame_destroy(f);
}
~~~~~

(Due to the dangerous nature of C strings, we copy the given frame into a buffer and
make sure to set the terminating null byte before we print.)

So, that's about it.  Here's our whole program:

~~~~~{.c}
#include <nitro.h>

int main(int argc, char **argv) {
    nitro_runtime_start();
    nitro_socket_t *sock = nitro_socket_bind("tcp://*:4444", NULL);
    char buf[50] = {0};
    while (1) {
        nitro_frame_t *f = nitro_recv(sock, 0);
        assert(nitro_frame_size(f) < 50);
        memcpy(buf, (char *)nitro_frame_data(f), nitro_frame_size(f));
        buf[nitro_frame_size(f)] = 0;
        printf("They said: %s\n", buf);
        nitro_frame_destroy(f);
    }
    return 0;
}
~~~~~

The sender is very similar, so here's the whole program:

~~~~~{.c}
#include <nitro.h>

int main(int argc, char **argv) {
    nitro_runtime_start();
    nitro_socket_t *sock = nitro_socket_connect("tcp://127.0.0.1:4444", NULL);
    int i = 0;
    for (; i < 10; ++i) {
        nitro_frame_t *f = nitro_frame_new_copy("Pinky", 6);
        nitro_send(&f, sock, 0);
    }
    sleep(1);
    return 0;
}
~~~~~

The differences between files are that we're connecting to the service
instead of binding, then calling `nitro_send` on a fixed number of frames.
Notice we're not calling `nitro_frame_destroy`; as per the frame ownership
rules, `nitro_send` is taking ownership of that frame in a zero-copy way.

We also sleep one second at the end to let the TCP socket flush before the
program ends.

Here's what a compile/run looks like:

~~~~~
$ gcc -Wall -Werror -g -O2 `pkg-config --cflags nitro` -o rec rec.c `pkg-config --libs nitro`
$ gcc -Wall -Werror -g -O2 `pkg-config --cflags nitro` -o send send.c `pkg-config --libs nitro`
$ ./rec &
[1] 16608
$ ./send
They said: Pinky
They said: Pinky
They said: Pinky
They said: Pinky
They said: Pinky
They said: Pinky
They said: Pinky
They said: Pinky
They said: Pinky
They said: Pinky
$
~~~~~

Proxy Time
----------

*coming soon... see examples/ in project directory*

Publish This
------------

*coming soon... see examples/ in project directory*

Secure Sockets
--------------

*coming soon... see examples/ in project directory*

API
===

Runtime
-------

**nitro_runtime_start**

~~~~~~{.c}
int nitro_runtime_start();
~~~~~~

Starts up all the Nitro thread, sets up global locks, etc.

This function must be called and must return before calling any other
Nitro functions.

*Return Value*

0 on success, < 0 on error.

 * `NITRO_ERR_ALREADY_RUNNING` - If the nitro_runtime_start() has already been
   called

*Thread Safety*

Not thread safe.  The runtime must be started exactly once
(until `nitro_runtime_stop` is called).

**nitro_runtime_stop**

~~~~~~{.c}
int nitro_runtime_stop();
~~~~~~

Stops the Nitro thread, cleans up all memory, etc.

This function must not be called until all sockets are
`nitro_socket_close`ed and destroyed.

For TCP sockets (which have a linger value), this means it is only safe to
call `nitro_runtime_stop` after the longest linger value has elapsed.

*Return Value*

0 on success, < 0 on error.

 * `NITRO_ERR_NOT_RUNNING` - The Nitro runtime is not currently running
   (`nitro_runtime_start` has not been called).

*Thread Safety*

Not thread safe.

*Crash Note*

If the socket count is non-zero, assert() will fire SIGABORT.

*Usage Note*

This function is useful for tests, and to prove the memory-soundness of
nitro, but is probably rarely used in practice.

Errors
------

Nitro tracks detail error codes and has explanations for issues
that may arise during the execution of your program.

Nitro errors are thread-local, so it is safe to assume the error
number Nitro returns to your thread was triggered by you.

**nitro_error**

~~~~~{.c}
NITRO_ERROR nitro_error();
~~~~~

Returns the last error to occur on this thread (the "current" error).

*Return Value*

The current error.  NITRO_ERR_NONE means there is no error set.

*Thread Safety*

`nitro_error` is thread safe.

**nitro_errmsg**

~~~~~{.c}
char *nitro_errmsg(NITRO_ERROR error);
~~~~~

Produce a user-readable message in english describing the given error.

*Arguments*

 * `NITRO_ERROR error` - The error code in question (typically, returned from
   `nitro_error`.)

*Return Value*

A string containing the user-readable description of the given error.

*Thread Safety*

Reentrant and thread safe.

*Ownership*

The returned string is a static global.  You do not need to free it.

Logging
-------

Nitro includes several logging macros that print nicely-formatted messages
with timestamps to stderr.

**nitro_log_**

~~~~~{.c}
void nitro_log_info(char *location, char *format, ...)
void nitro_log_warn(char *location, char *format, ...)
void nitro_log_warning(char *location, char *format, ...)
void nitro_log_err(char *location, char *format, ...)
void nitro_log_error(char *location, char *format, ...)
~~~~~

Write a printf-style formatted string to stderr, prefixing the line
with the current time and location.  `err` and `warn` are
shortened versions of `error` and `warning` respectively.

*Arguments*

 * `location` - An identifer to represent the general domain or subsystem
   where the event being logged took place.  __FILE__ is a decent choice if
   you can't think of anything better.
 * `format` - `printf`-style format string
 * `...` - Format args, ala printf

*Thread Safety*

Reentrant and thread safe.

Socket Options
--------------

Socket options live in a structure that is built ahead of socket
creation and passed to the socket constructors.

**nitro_sockopt_new**

~~~~~{.c}
nitro_sockopt_t *nitro_sockopt_new();
~~~~~

*Return Value*

Socket options structure, initialized to default values.

*Thread Safety*

Thread safe.

*Ownership Notes*

When `nitro_socket_t` objects are given to a socket constructor,
the socket now owns that options object.  The application does
not need to destroy the options object.

**nitro_sockopt_set_want_eventfd**

~~~~~{.c}
void nitro_sockopt_set_want_eventfd(nitro_sockopt_t *opt, int want_eventfd);
~~~~~

Enable the event fd on this socket.

The event fd is a file descriptor whose readability indicates one or more
whole frames is ready to be read via `nitro_recv`.  This is useful for
integrating Nitro into other event loops.

The event fd is level triggered, meaning it will remain readable until
the socket recieve queue is empty.

The highest-performance way to consume this is to repeatedly
call `nitro_recv` in your readable-callback with `NITRO_NOBLOCK`
set until it returns NULL.

*Arguments*

 * `nitro_socket_t *opt` - The socket options structure to modify
 * `int want_eventfd` - 1 or 0 to indicate if the eventfd should or
   should not be updated when the recieve queue changes state

*Thread Safety*

Reentrant and thread safe.

*Default Value*

The default value is `0`.

**nitro_sockopt_set_close_linger**

~~~~~{.c}
void nitro_sockopt_set_close_linger(nitro_sockopt_t *opt,
    double close_linger);
~~~~~

Adjust the "linger" time on a TCP socket.

The linger times indicates how long a TCP socket should remain
connected with peers after `nitro_socket_close` is called before
destroying the socket object.  This is useful to give pending
messages in the send queue a chance to flush before cleaning up.

*Arguments*

 * `nitro_socket_t *opt` - The socket options structure to modify
 * `double close_linger` - Time, in seconds, to allow messages to
   flush.

*Thread Safety*

Reentrant and thread safe.

*Default Value*

The default value is `1.0` seconds.

**nitro_sockopt_set_reconnect_interval**

*Socket Type Limitations*

Only applicable to TCP sockets; inproc sockets will
assert if this value is set.

~~~~~{.c}
void nitro_sockopt_set_reconnect_interval(nitro_sockopt_t *opt,
    double reconnect_interval);
~~~~~

The reconnect interval is how often connected TCP sockets
(sockets created with `nitro_socket_connect`) will retry
to (re)connect with their remote peer when in a disconnected
state.

Connected sockets attempt to transparently keep the link alive
as much as possible in the face of network failures, etc.  This
function effectively sets the poll time when a connect attempt
has failed or an existing connection was dropped.

*Arguments*

 * `nitro_socket_t *opt` - The socket options structure to modify
 * `double reconnect_interval` - Time, in seconds, to wait before
   retrying connection.

*Thread Safety*

Reentrant and thread safe.

*Default Value*

The default value is `0.2` seconds.

**nitro_sockopt_set_max_message_size**

~~~~~{.c}
void nitro_sockopt_set_max_message_size(nitro_sockopt_t *opt,
    uint32_t max_message_size);
~~~~~

Set the maximum frame size allowable on this socket.

If a remote connection attempts to send a frame larger than
this value, the Nitro error handler will be invoked and the
connection will be dropped.

Similarly, if the local application attempts to send
a frame larger than this value, the library call will fail
and the message will not be queued.

The maximum possible value accepted by this function is
1GB.

*Arguments*

 * `nitro_socket_t *opt` - The socket options structure to modify
 * `uint32_t max_message_size` - Size, in bytes, of largest frame
   this socket will allow to be sent or received.

*Thread Safety*

Reentrant and thread safe.

*Default Value*

The default value is 16MB (1024 * 1024 * 16 bytes).

**nitro_sockopt_set_secure**

~~~~~{.c}
void nitro_sockopt_set_secure(nitro_sockopt_t *opt,
    int enabled);
~~~~~

Make this TCP socket a secure socket.

Frames will be encrypted using NaCl's authenticated,
public-key encryption (`crypto_box`).

How crypto works in nitro:

 1. Every socket created in Nitro has a socket identity.
 2. The socket identity is actually a `crypto_box` public
    key
 3. Every socket also has a corresponding `crypto_box`
    private key
 4. Secure TCP sockets exchange public keys after the
    connection is established.  This `HELLO` frame is
    the only frame sent unencrypted.
 5. Subsequent frames are all sent in a `SECURE` frame
    wrapper.  This payload is a nonce and a encrypted
    bytestring that contains the real frame (including
    frame envelope).  
 6. The encrypted bytestrings are generated like so:
    When socket A sends a frame F to
    socket B, A encrypts F using
    (B.public_key, A.private_key, nonce).  Socket
    B decrypts back to F using
    (A.public_key, B.private_key, nonce).

Secure sockets will refuse to exchange any frame types
after `HELLO` except `SECURE` frames.  Any peer socket
sending a non-`SECURE` frame will cause the error handler
to be invoked and the peer to be dropped.

*Arguments*

 * `nitro_socket_t *opt` - The socket options structure to modify
 * `int enabled` - 1 or 0, to enable or disable encryption.

*Security Note*

Without also requiring that a *specific* certificate be provided
by the peer (via `nitro_sockopt_set_required_remote_ident`), this
function *does not provide authentication alone* and is vulnerable
to man-in-the-middle attacks.

*Thread Safety*

Reentrant and thread safe.

*Default Value*

The default value is 0.

*Socket Type Limitations*

Only applicable to TCP sockets; inproc sockets will
assert if this value is set.

**nitro_sockopt_set_secure_identity**

~~~~~{.c}
void nitro_sockopt_set_secure_identity(nitro_sockopt_t *opt,
    uint8_t *ident, size_t ident_length,
    uint8_t *pkey, size_t pkey_length);
~~~~~

Explicitly set the crypto_box public and private keys for
this socket.

It is imperative to use this function (as well as
`nitro_sockopt_set_required_remote_ident` below) to prevent
man-in-the-middle attacks, DNS spoofing, etc.

The typical deployment would be that the bound socket sets
a specific known keypair, and then the clients would ship
the public key from this pair and require that this key
be the one sent exchanged in the `HELLO`.

You can create a valid keypair using `crypto_box_keypair`
from NaCl.  Keep your private keys safe on your production
machines!  Do not ship them to clients.

*Arguments*

 * `nitro_socket_t *opt` - The socket options structure to modify
 * `uint8_t *ident` - The identity (`crypto_box` public key) of the
    socket
 * `size_t ident_length` - The length of the identity (Nitro will assert
   if it is not equal to `crypto_box_PUBLICKEYBYTES`
 * `uint8_t *pkey` - The `crypto_box` private key.
 * `size_t pkey_length` - The length of the private key (Nitro will assert
   if it is not equal to `crypto_box_SECRETKEYBYTES`

*Thread Safety*

Reentrant and thread safe.

*Default Value*

By default, as long as `nitro_sockopt_set_secure` is enabled,
Nitro will generate a random keypair (using `crypto_box_keypair`) whose
lifetime ends with the socket.

*Socket Type Limitations*

Only applicable to TCP sockets; inproc sockets will
assert if this value is set.

**nitro_sockopt_set_required_remote_ident**

~~~~~{.c}
void nitro_sockopt_set_required_remote_ident(nitro_sockopt_t *opt,
    uint8_t *ident, size_t ident_length);
~~~~~

Require that this secure TCP socket only accept frames from
peers who provide a known identity (public key).

The public key provided here must correspond to one given to
`nitro_sockopt_set_secure_identity` on the remote peer.

*Arguments*

 * `nitro_socket_t *opt` - The socket options structure to modify
 * `uint8_t *ident` - The identity (`crypto_box` public key) of the
    socket
 * `size_t ident_length` - The length of the identity (Nitro will assert
   if it is not equal to `crypto_box_PUBLICKEYBYTES`

*Thread Safety*

Reentrant and thread safe.

*Default Value*

By default Nitro will accept encrypted packets
from any peer providing any public key.

*Socket Type Limitations*

Only applicable to TCP sockets; inproc sockets will
assert if this value is set.

**nitro_sockopt_set_error_handler**

~~~~~{.c}
typedef void (*nitro_error_handler)(int nitro_error, void *baton);
void nitro_sockopt_set_error_handler(nitro_sockopt_t *opt,
    nitro_error_handler handler, void *baton);
~~~~~

Set a handler for any errors that occur on the Nitro thread
during communication with TCP sockets.

This handler will be invoked from the Nitro thread, so it should
do its work quickly and return.  No TCP Nitro traffic will
take place while your custom handler is being executed.

*Arguments*

 * `nitro_socket_t *opt` - The socket options structure to modify
 * `nitro_error_handler handler` - An error handler callback
   that will take action when a background error occurs.
 * `void *baton` - Some pass-through object to be given
   to the error handler

*Thread Safety*

Reentrant and thread safe.

*Default Value*

Nitro's default error handler logs the errors to stdout.
Using `nitro_log_error` and `nitro_errmsg`.

*Possible Background Errors*

 * `NITRO_ERR_ENCRYPT` "(pipe) frame encryption failed".  This
   error can occur if a secure socket is unable to encrypt
   a frame using the remote public/local private keypair.
   It's possible the remote peer could have provided an invalid
   public key, or you have given an incorrect private key
   to `nitro_sockopt_set_secure_identity`.
 * `NITRO_ERR_DECRYPT` "(pipe) frame decryption failed".
   Frame decryption failed on SECURE frame data coming
   from the TCP socket.  This can occur because of
   bad keys, invalid NONCE, any number of reasons.
 * `NITRO_ERR_MAX_FRAME_EXCEEDED` "(pipe) remote tried to send a frame larger than the maximum allowable size".
   The remote peer attempted to send a frame larger
   than the specified maximum allowable size.
 * `NITRO_ERR_BAD_PROTOCOL_VERSION` "(pipe) remote sent a Nitro protocol version that is unsupported by this application".
   A frame header contained a protocol version field that
   was either invalid or not implemented by this version
   of nitro.
 * `NITRO_ERR_INVALID_CLEAR` "(pipe) remote sent a unencrypted message over a secure socket".
   After `HELLO`, a non-encrypted message was sent by
   remote peer to this secure socket.
 * `NITRO_ERR_DOUBLE_HANDSHAKE` "(pipe) remote sent two HELLO packets on same connection".
   The remote peer sent the `HELLO` frame twice.
 * `NITRO_ERR_INVALID_CERT` "(pipe) remote identity/public key does not match the one required by this socket".
   Key validation is enabled (using `nitro_sockopt_set_required_remote_ident`) and
   the key provided by the remote peer did not match.
 * `NITRO_ERR_NO_HANDSHAKE` "(pipe) remote sent a non-HELLO packet before HELLO".
   The remote peer sent some other frame before `HELLO`.
 * `NITRO_ERR_BAD_SUB` "(pipe) remote sent a SUB packet that is too short to be valid".
   For pub/sub work, a subscription list was relayed that
   was invalid.
 * `NITRO_ERR_BAD_HANDSHAKE` "(pipe) remote sent a HELLO packet that is too short to be valid".
   An invalid `HELLO` frame was sent.
 * `NITRO_ERR_BAD_SECURE` "(pipe) remote sent a secure envelope on an insecure connection".
   The remote peer sent a secure frame when the local socket
   has is not secure (`nitro_sockopt_set_secure` has not
   been enabled)

*Practical Consideration*

Though you should always be on your toes, many of these protocol errors
about invalid frames and invalid keys are in practice more likely to
be because you have something misconfigured (HTTP request to a nitro port,
for example) than because something malicious is going on.

**nitro_sockopt_disable_error_handler**

~~~~~{.c}
void nitro_sockopt_disable_error_handler(nitro_sockopt_t *opt);
~~~~~

Disable the default error handler (or any error handler,
really).

This means strange socket behavior will be
dropped silently--not advisable.

*Arguments*

 * `nitro_socket_t *opt` - The socket options structure to modify

*Thread Safety*

Reentrant and thread safe.

**nitro_sockopt_destroy**

~~~~~{.c}
void nitro_sockopt_destroy(nitro_sockopt_t *opt);
~~~~~

Destroy a socket option object.

Used very rarely, because `nitro_sockopt_t` objects
given to socket constructors are owned and destroyed
by those sockets... and there's not a lot of other
reasons to create a `nitro_sockopt_t` besides giving
it to a socket.

*Arguments*

 * `nitro_socket_t *opt` - The socket options structure to destroy

*Thread Safety*

Reentrant and thread safe.

Socket Lifetime
---------------

Creation and destruction of sockets.

*Socket Locations*

Socket locations given to `bind` and `connect` are
of this form:

~~~~~
protocol://location
~~~~~

Nitro supports two protocols, "tcp" and "inproc".

TCP locations must be given as "<IPv4>:<port>"
specifications.  Nitro does not currently do
DNS resolutions, so you'll need to resolve names
at a higher level before creating sockets.  Nitro
also does not currently support IPv6.

Inproc locations can be any arbitrary string,
but by convention it should be a reasonable
identifer like alphanumeric with dashes.

Here are some valid locations:

~~~~~
tcp://127.0.0.1:4444
tcp://10.1.1.1:443
inproc://foobar
inproc://router-database
~~~~~

There is one special form available for passing
to bound tcp sockets:

~~~~~
tcp://*:443
~~~~~

This is shorthand for "bind on all interfaces".

**nitro_socket_bind**

~~~{.c}
nitro_socket_t *nitro_socket_bind(char *location, nitro_sockopt_t *opt);
~~~

Create a new bound socket at `location` using options
`opt`.

*Arguments*

 * `char *location` - The socket location for binding.
 * `nitro_sockopt_t *opt` - The socket options, or
   NULL for default options.

*Return Value*

A new socket, or NULL on error.  `nitro_error()`
will be set.

Possible errors:

 * `NITRO_ERR_PARSE_BAD_TRANSPORT` "invalid transport type for socket".
   Socket protocol was not "tcp" or "inproc".
 * `NITRO_ERR_TCP_LOC_NOCOLON` "TCP socket location did not contain a colon".
 * `NITRO_ERR_TCP_LOC_BADPORT` "TCP socket location did not contain an integer port number".
 * `NITRO_ERR_TCP_LOC_BADIPV4` "TCP socket location was not a valid IPv4 address (a.b.c.d)".
 * `NITRO_ERR_BAD_INPROC_OPT` "inproc socket creation was given an unsupported socket option".
   An inproc socket was probably given an option documented with "Socket Type Restrictions"
   that require tcp-only.
 * `NITRO_ERR_INPROC_ALREADY_BOUND` "another inproc socket is already bound to that location".
 * `NITRO_ERR_ERRNO` A low-level socket operation failed; check errno.

*Thread Safety*

Reentrant and thread safe.

**nitro_socket_connect**

~~~~~{.c}
nitro_socket_t *nitro_socket_connect(char *location, nitro_sockopt_t *opt);
~~~~~

Create a new connected socket (or, in the case of TCP,
that *will* connect) at `location` using options
`opt`.

*Arguments*

 * `char *location` - The location of a bound socket.
 * `nitro_sockopt_t *opt` - The socket options, or
   NULL for default options.

*Return Value*

A new socket, or NULL on error.  `nitro_error()`
will be set.

Possible Errors:

 * `NITRO_ERR_PARSE_BAD_TRANSPORT` "invalid transport type for socket".
   Socket protocol was not "tcp" or "inproc".
 * `NITRO_ERR_TCP_LOC_NOCOLON` "TCP socket location did not contain a colon".
 * `NITRO_ERR_TCP_LOC_BADPORT` "TCP socket location did not contain an integer port number".
 * `NITRO_ERR_TCP_LOC_BADIPV4` "TCP socket location was not a valid IPv4 address (a.b.c.d)".
 * `NITRO_ERR_BAD_INPROC_OPT` "inproc socket creation was given an unsupported socket option".
   An inproc socket was probably given an option documented with "Socket Type Restrictions"
   that require tcp-only.
 * `NITRO_ERR_INPROC_NOT_BOUND` "cannot connect to inproc: not bound".
   No inproc socket is bound at that location.
 * `NITRO_ERR_ERRNO` A low-level socket operation failed; check errno.

*Connection Timing Notes*

TCP sockets connect asynchronously and continuously
as needed. Inproc sockets connect synchronously,
during the call to `nitro_socket_connect`.

The ramifications of this are that inproc sockets
require `nitro_socket_bind` to be called before any
corresponding `nitro_socket_connect` calls.


**nitro_socket_close**

~~~~~{.c}
void nitro_socket_close(nitro_socket_t *socket);
~~~~~

Close a socket and (eventually) destory all its
resources.

For a TCP socket, this means let outgoing traffic
flush for the linger time, then destroy.

For an inproc socket, the socket is closed
immediately.

*Arguments*

 * `nitro_socket_t *socket` - The socket to close

*Thread Safety*

This function is not thread safe in that each
socket can only be closed exactly once.

So if you've distributed socket frame activity
across a thread pool, for example, all those
threads must stop and before `nitro_socket_close`
is called.

*Destruction Semantics*

After calling `nitro_socket_close`, the application
must make no more calls involving that socket.  Doing
so could cause nondeterministic behavior and crashes.

Frame Management
----------------

Frames are the containers for messages in nitro.
Creating and destroying them efficiently and
safely is key to a healthy application.

**nitro_frame_new_copy**

~~~~~{.c}
nitro_frame_t *nitro_frame_new_copy(void *data, uint32_t size);
~~~~~

The easiest way to create a frame is to use
`nitro_frame_new_copy`.  This function copies
the buffer at pointer `data` for `size` bytes
onto the heap, and frees its private copy after it is sent.

Note that if frames are sent using the
`NITRO_REUSE` flags,
the copied buffer is reference counted, so you
can safely create a new frame with
`nitro_frame_new_copy` and send it many times
efficiently without invoking a copy each time.

*Arguments*

 * `void *data` - A pointer to the start of your message
 * `uint32_t size` - The length of your message

*Return Value*

A new frame, ready for sending.

*Thread Safety*

Reentrant and thread safe.

**nitro_frame_new**

~~~~~{.c}
typedef void (*nitro_free_function)(void *data, void *baton);
nitro_frame_t *nitro_frame_new(void *data, uint32_t size, nitro_free_function ff, void *baton);
~~~~~

The lower-level, zero-copy frame constructor.
`data` must stay valid until Nitro calls `ff`!

You provide the function to call when nitro
is done with your message data.

*Arguments*

 * `void *data` - A pointer to the start of your message
 * `uint32_t size` - The length of your message
 * `nitro_free_function ff` - A function Nitro will invoke
   when it is done using your buffer.
 * `void *baton` - Something to pass through as the second argument
   of your free function.

*Return Value*

A new frame, ready for sending.

*Thread Safety*

Reentrant and thread safe.

**nitro_frame_new_heap**

~~~~~{.c}
nitro_frame_t *nitro_frame_new_heap(void *data, uint32_t size);
~~~~~

A zero-copy macro on `nitro_frame_new` that tells nitro
you allocated `data` on the heap using `malloc` and nitro
should call `free` when it's finished.

*Arguments*

 * `void *data` - A pointer to the start of your message
 * `uint32_t size` - The length of your message

*Return Value*

A new frame, ready for sending.

*Thread Safety*

Reentrant and thread safe.

*Implementation Note*

This function is equal to:

~~~~~{.c}
void run_free(void *region, void *unused) {
    free(region);
}
#define nitro_frame_new_heap(data, size)\
    nitro_frame_new(data, size, run_free, NULL)
~~~~~

**nitro_frame_data**

~~~~~{.c}
void *nitro_frame_data(nitro_frame_t *fr);
~~~~~

Return the pointer to the buffer that contains
the message data.

Read-only. Do not manipulate the contents of this buffer!

After getting a new frame from `nitro_recv` this
function is almost always used to deserialize the
message body into something structural that can
be processed.

*Arguments*

 * `nitro_frame_t *fr` - The frame

*Return Value*

Pointer to message data.

*Thread Safety*

Reentrant and thread safe.

**nitro_frame_size**

~~~~~{.c}
uint32_t *nitro_frame_size(nitro_frame_t *fr);
~~~~~

Return the length of valid message data
at the pointer returned from `nitro_frame_data`.

Like `nitro_frame_data`, used most often after getting 
a new frame from `nitro_recv`.

*Arguments*

 * `nitro_frame_t *fr` - The frame

*Return Value*

Length of message data.

*Thread Safety*

Reentrant and thread safe.

**nitro_frame_destroy**

~~~~~{.c}
void nitro_frame_destroy(nitro_frame_t *fr);
~~~~~

Destroy the frame.

Technically, this may or may not invoke the
`nitro_free_function` associated with the
frame.  For performance reasons, Nitro frames
are actually reference counted.  So while
this function indicates the *caller* is done
with the frame, it does not necessarily mean
all associated memory will be released, yet.

Keep in mind that many of the functions to send frames
take ownership by default of the frames and
NULL out your frame pointer.  So unless you
have a frame you got via `nitro_recv`, or
you used the `NITRO_REUSE` flag, it is
not your responsibility to destroy the frame.

*Arguments*

 * `nitro_frame_t *fr` - The frame

*Thread Safety*

Reentrant and thread safe.

Receiving Frames
----------------

**nitro_recv**

~~~~~{.c}
nitro_frame_t *nitro_recv(nitro_socket_t *s, int flags);
~~~~~

As documented in the Concepts, this is the only
way to receive frames from a socket.

*Arguments*

 * `nitro_socket_t *s` - The socket to receive from.
 * `int flags` - Flags to modify receive behavior

*Flags*

 * `NITRO_NOBLOCK` - Do not block on this `nitro_recv`
   call; return immediately

*Return Value*

A new frame, or NULL if error.

Possible Errors:

 * `NITRO_ERR_EAGAIN` - No frames were waiting in the
   incoming socket buffer, and `NITRO_NOBLOCK` was
   passed to the `nitro_recv` call.

*Ownership*

You own all frames you receive.  You must either
call `nitro_frame_destroy`, or forward them on
to another socket via a send function.

*Thread Safety*

Thread safe, including using the same socket
in multiple threads.

Sending Frames
----------------

**nitro_send**

~~~~~{.c}
int nitro_send(nitro_frame_t **frp, nitro_socket_t *s, int flags);
~~~~~

Send a frame `*frp` to any connected peer on socket `s`.

See Concepts for details about behavior.

*Arguments*

 * `nitro_frame_t **frp` - A pointer to a frame
   pointer.  The frame will be queued and
   the pointer will be NULLified unless
   `NITRO_REUSE` is in flags.
 * `nitro_socket_t *s` - The socket to send to.
 * `int flags` - Flags to modify send behavior

*Flags*

 * `NITRO_NOBLOCK` - Do not block on this `nitro_send`
   call; return immediately
 * `NITRO_REUSE` - Copy/refcount the frame, and
   do not NULLify the pointer, so the application
   can reuse it.

*Return Value*

0 on success, < 0 on error.  `nitro_error` will be set.

Possible Errors:

 * `NITRO_ERR_EAGAIN` - The high water mark has
   been hit on the outgoing frame queue, so
   this operation would have blocked, but
   `NITRO_NOBLOCK` was in `flags`.
 * `NITRO_ERR_INPROC_NO_CONNECTIONS` - The
   send operation was attempted on a
   bound inproc socket without any
   current peers.

*Ownership*

Unless you pass `NITRO_REUSE` in flags,
this function takes ownership of the frame
and is responsible for destroying it.

*Thread Safety*

Thread safe, including using the same socket
in multiple threads.  Using the same frame in
multiple threads is possible but inadvisable.

**nitro_reply**

~~~~~{.c}
int nitro_reply(nitro_frame_t *snd, nitro_frame_t **frp,
    nitro_socket_t *s, int flags);
~~~~~

Send a frame `*frp` directly to the peer that sent frame `snd`.

This is RPC-style behavior; see Concepts for details.

*Arguments*

 * `nitro_frame_t *snd` - A frame sent by
   a specific peer that identifies the "return address"
   for `*frp`.
 * `nitro_frame_t **frp` - A pointer to a frame
   pointer.  The frame will be queued and
   the pointer will be NULLified unless
   `NITRO_REUSE` is in flags.
 * `nitro_socket_t *s` - The socket to send to.
 * `int flags` - Flags to modify send behavior

*Flags*

 * `NITRO_NOBLOCK` - Do not block on this `nitro_send`
   call; return immediately
 * `NITRO_REUSE` - Copy/refcount the frame, and
   do not NULLify the pointer, so the application
   can reuse it.

*Return Value*

0 on success, < 0 on error.  `nitro_error` will be set.

Possible Errors:

 * `NITRO_ERR_EAGAIN` - The high water mark has
   been hit on the outgoing frame queue, so
   this operation would have blocked, but
   `NITRO_NOBLOCK` was in `flags`.
 * `NITRO_ERR_NO_RECIPIENT` - The recipient identified
   by `snd` is no longer (or never was) in the connection table.

*Ownership*

Unless you pass `NITRO_REUSE` in flags,
this function takes ownership of the frame
and is responsible for destroying it.

*Thread Safety*

Thread safe, including using the same socket
in multiple threads.  Using the same frame in
multiple threads is possible but inadvisable.

**nitro_relay_fw**

~~~~~{.c}
int nitro_relay_fw(nitro_frame_t *snd, nitro_frame_t **frp,
    nitro_socket_t *s, int flags);
~~~~~

Forward a frame `*frp` to any peer on socket `s` while preserving
the routing information in received frame `snd`.

This is proxy-style behavior; see Concepts for details.

*Arguments*

 * `nitro_frame_t *snd` - A frame sent by
   a specific peer that contains a routing stack
   (a stack of "return addresses") that should be
   preserved in `*frp`.
 * `nitro_frame_t **frp` - A pointer to a frame
   pointer.  The frame will be queued and
   the pointer will be NULLified unless
   `NITRO_REUSE` is in flags.
 * `nitro_socket_t *s` - The socket to send to.
 * `int flags` - Flags to modify send behavior

*Flags*

 * `NITRO_NOBLOCK` - Do not block on this `nitro_send`
   call; return immediately
 * `NITRO_REUSE` - Copy/refcount the frame, and
   do not NULLify the pointer, so the application
   can reuse it.

*Return Value*

0 on success, < 0 on error.  `nitro_error` will be set.

Possible Errors:

 * `NITRO_ERR_EAGAIN` - The high water mark has
   been hit on the outgoing frame queue, so
   this operation would have blocked, but
   `NITRO_NOBLOCK` was in `flags`.
 * `NITRO_ERR_INPROC_NO_CONNECTIONS` - The
   send operation was attempted on a
   bound inproc socket without any
   current peers.

*Ownership*

Unless you pass `NITRO_REUSE` in flags,
this function takes ownership of the frame
and is responsible for destroying it.

*Thread Safety*

Thread safe, including using the same socket
in multiple threads.  Using the same frame in
multiple threads is possible but inadvisable.

**nitro_relay_bk**

~~~~~{.c}
int nitro_relay_bk(nitro_frame_t *snd, nitro_frame_t **frp,
    nitro_socket_t *s, int flags);
~~~~~

Pop the top address off the routing stack in `snd`
and forward the frame `*frp` to that specific
peer on socket `s`.

This is proxy-style behavior; see Concepts for details.

*Arguments*

 * `nitro_frame_t *snd` - A frame coming back
   from a _reply that contains a routing stack
   that includes the appropriate next hop.
 * `nitro_frame_t **frp` - A pointer to a frame
   pointer.  The frame will be queued and
   the pointer will be NULLified unless
   `NITRO_REUSE` is in flags.
 * `nitro_socket_t *s` - The socket to send to.
 * `int flags` - Flags to modify send behavior

*Flags*

 * `NITRO_NOBLOCK` - Do not block on this `nitro_send`
   call; return immediately
 * `NITRO_REUSE` - Copy/refcount the frame, and
   do not NULLify the pointer, so the application
   can reuse it.

*Return Value*

0 on success, < 0 on error.  `nitro_error` will be set.

Possible Errors:

 * `NITRO_ERR_EAGAIN` - The high water mark has
   been hit on the outgoing frame queue, so
   this operation would have blocked, but
   `NITRO_NOBLOCK` was in `flags`.
 * `NITRO_ERR_NO_RECIPIENT` - The recipient identified
   by the top of the routing stack in `snd` is no longer
   (or never was) in this socket's connection table.

*Ownership*

Unless you pass `NITRO_REUSE` in flags,
this function takes ownership of the frame
and is responsible for destroying it.

*Thread Safety*

Thread safe, including using the same socket
in multiple threads.  Using the same frame in
multiple threads is possible but inadvisable.

Publish/Subscribe
-----------------

**Stcp_socket_sub/Stcp_socket_unsub**

~~~~~{.c}
int Stcp_socket_sub(nitro_tcp_socket_t *s,
    uint8_t *k, size_t length);
int Stcp_socket_unsub(nitro_tcp_socket_t *s,
    uint8_t *k, size_t length);
~~~~~

Subscribe or unsubscribe to frames
broadcast on channels that begin with prefix `k`
of length `length`.  `length` must be <255.

See Concepts for details.

*Arguments*

 * `nitro_socket_t *s` - The socket with the subscribtion
 * `uint8_t *k` - The channel prefix
 * `size_t length` - The length of the prefix

*Return Value*

0 on success, < 0 on error.  `nitro_error` will be set.

Possible Errors:

 * `NITRO_ERR_SUB_ALREADY` - `nitro_sub` called with a
   prefix the socket is already subscribed to.
 * `NITRO_ERR_SUB_MISSING` - `nitro_unsub` called with a
   prefix the socket is not subscribed to.

*Ownership*

Sub makes a copy of the key, so it does not alter
ownership state of the calling function on
the buffer in `k`.

*Thread Safety*

Reentrant and thread safe.

**nitro_pub**

~~~~~{.c}
int nitro_pub(nitro_frame_t **frp, const uint8_t *k,
    size_t length, nitro_socket_t *s, int flags);
~~~~~

Public a frame `*frp` to all connected peers
who have a known subscription to a prefix
of channel `k`.

See concepts for details.

*Arguments*

 * `nitro_frame_t **frp` - A pointer to a frame
   pointer.  The frame will be queued and
   the pointer will be NULLified unless
   `NITRO_REUSE` is in flags.
 * `const uint8_t *k` - The channel key
 * `size_t length` - The length of the channel key
 * `nitro_socket_t *s` - The socket to broadcast on
 * `int flags` - Flags to modify send behavior

*Flags*

 * `NITRO_REUSE` - Copy/refcount the frame, and
   do not NULLify the pointer, so the application
   can reuse it.

*Return Value*

The number of recipients for whom
the message was queued.

*Ownership*

Unless you pass `NITRO_REUSE` in flags,
this function takes ownership of the frame
and is responsible for destroying it.

*Thread Safety*

Thread safe, including using the same socket
in multiple threads.  Using the same frame in
multiple threads is possible but inadvisable.

*Special Non-Blocking Behavior*

Due to the nature of publishing, there is
no `NITRO_NOBLOCK` flag, because `nitro_pub`
is always nonblocking.

If a peer subscribed to the channel has a
frame queue that is at its high
water mark, the delivery will simply be
skipped.  Ergo, it is possible to drop
subscribed messages if socket does not
keep up with the message stream.

FAQ
===

**Q: I don't want to write my service in C. Where's a binding for
my language?**

Unfortunately, Nitro doesn't have many language bindings yet.  If you write
one, please drop us a line and we'll add it to the home page.

Python, Haskell, and Objective-C/iOS bindings are being developed by
members of the Nitro project; we'll probably work on Java/Android
soon after that.

**Q: What about other platforms?**

We're working on ports to iOS and Android, and while
we have no other ports planned, porting to other POSIX
systems like *BSD should be pretty straightforward.

**Q: What about Windows?**

Oy.  That's a bigger effort and one that's frankly not a priority for us.
If someone out there wants to put some effort into the port, that'd be great.

**Q: There are a few reference to "high water mark".  What's that about?  Is it like
ZeroMQ's high water mark?**

Yes, but the implementation is not quite done and the tests aren't written,
so its use is not recommended yet.

**Q: No DNS resolution is inconvenient.**

That's not a question, but yes, yes it is.  Unfortunately, standard library
support for asynchronous DNS resolution is still nonexistant.  We might
add support for doing resolutions in the top-level functions (like bind()
and connect()) on your thread, but that doesn't solve the problem of
names changing for long-running services.

**Q: How is this different than ZeroMQ?**

Nitro is in many ways a reaction to ZeroMQ (ZMQ), and is heavily
inspired by it (especially czmq).  The team at bu.mp (that wrote
Nitro) uses ZeroMQ extensively, including shipping it on tens of
millions of smartphones, and there's a lot to love about ZMQ.  
So much, in fact, that the deficiencies of ZMQ we encountered 
motivated Nitro--because the basic communication model that ZMQ
pioneered is mostly wonderful to use and develop against.

However ZeroMQ was designed for relatively
low-concurrency, very high throughput (especially multicast PGM)
communication on private, trusted, low-latency networks... not for large
scale public Internet services with high connection counts, fickle clients
and wobbly links.

These are some of the design decisions Nitro made that differ from ZeroMQ:

 * Nitro provides more transparency about the socket/queue state (client
   X is connected, queue count is Y) for monitoring reasons and because
   clients quite often never come back in public networks, so
   state needs to be cleared, etc.
 * Nitro does not commit messages to a particular socket at send() time,
   but does send() on a general queue and lets peers "work steal"
   stripes of frames as soon as they have room on their TCP buffer.
   This makes for a lot more transparency about the "true" high-water mark
   for a socket, it constrains the total number of messages that may be lost due to a
   client disconnect, and it can minimize mean latency of receipt of
   any general message (vs. naive round robin).
 * Nitro clears private queues for dead peer sockets instead of leaving
   them around indefintely in case they return.  This fixes one of
   the biggest issues with doing high-concurrency work in ZeroMQ:
   an unavoidable memory leak in ROUTER sockets when there is pending
   data for clients who will never return.
 * ZeroMQ (esp 2.X) had some more or less hard coded peer limits
   (1024), which is far less than a good C daemon on epoll can handle;
   nitro has no such restrictions.
 * ZeroMQ does not have any crypto story, so we had to roll our
   own awkardly using NaCl.  With Nitro, we built NaCl in, including
   a key exchange step, so you don't need to ship keys with every
   frame.
 * ZeroMQ's heritage of private networks has bit us and others with
   things that are assert()s instead of closes-and-logs.  On the
   public internet, sometimes people with your socket with a random
   HTTP request.  It is also not clear how much attention ZeroMQ
   has paid to invalid data in the form of attacks on inbound TCP
   streams, integer overflows, etc.  Nitro tries to be paranoid
   and it shuts down bad peers instead of crashing.
 * ZeroMQ ROUTER sockets also have some O(n) algorithms, where n is
   the number of connected peers on a socket; nitro is all O(1).
   This doesn't matter much when you have 5 or 10 or 50 big server
   systems pushing loads to each other on a private network, but
   it sucks when you have 50,000 mostly-idle clients on high-latency
   Internet links.
 * In practice we found the "typed socket" paradigm (REQ/REP/PUSH)
   more of a hindrance than a help.  We often ended up with hybrid
   schemes, like "REQ/maybe REP", or "REQ/multi REP".  Also, if you
   want REQ/REP with multiple clients where you do some processing to
   produce the REP result, you'll need to chain together ROUTER/DEALER.
   REQ/REP stacks and make sure you carefully track the address frames.
   Nitro lets you create any topology you want, and the routing is
   relatively abstracted from the application developer--you don't need
   to worry how deep or shallow you are, for example.
 * We found having the ZMQ's routing information in special MORE frames that have implict
   rules that differ on the basis of socket types (DEALER will generate it,
   REQ will not) cumbersome.
 * ZMQ Socket options have documented rules about when they take effect and
   when not, but these rules are not enforced by the API so they can bite
   you.  Nitro separates things that must be decided at construction time
   from those you can modify on the fly (_sub and _unsub, etc).
 * Pub/sub based on message body was limiting for us in practice.  Oftentimes
   we wanted a separation of the "channel name" and the message body.
 * ZMQ sockets are not thread safe.  So the way to make a multicore exploiting
   RPC service is to chain tcp/ROUTER frontends to an array of inproc
   backends, each running in a separate pthread.  This is a layer of
   complexity nitro removes by just having sockets be thread afe.
 * ZMQ_FD is edge triggered.  It's much harder to integrate an edge-triggered
   interface into other event loops.  Though it has a theoretical performance
   benefit, Nitro uses a level-triggered activity fd to make integration
   easier for 3rd party binding developers.
 * Nitro is written in C.  We prefer C, and we don't like
   linking against libstdc++ :-) .

As always, though, there are pros and cons:

 * On very small (<40 byte) TCP messages and inproc messages,
   ZeroMQ is faster (30-40%) than Nitro;
   Nitro's use of mutex/cond on socket queues probably costs it there.  
   Nitro is somewhat faster, though, on frames > 50-70 bytes).
 * Nitro has no equivalent of PGM support, nor will it ever. It doesn't
   fit the project's goals. Nitro's target users don't usually control
   the switching hardware to a degree to use PGM.
   So if you're on a network where very high performance multicast
   is key, ZMQ is probably a better fit.
 * Nitro does not have multi-host connect.  If that topology is critical to you,
   ZeroMQ can help (or HAproxy, but this is not as clean or as fast.)
 * Nitro is very young, and does not have *nearly* the language support ZeroMQ
   has.  Chances are if you want to use Nitro in your language of choice, you're
   going to have to make it happen.  Unless your language is C, Python, or Haskell
 * ZeroMQ is ported to work on Windows and lots of other places.
   Nitro has not yet been ported to anything but Linux and Mac OS X.

**Q: With the domain gonitro.io, I expected something written for
(or in) golang.  Does this have anything to do with go?**

No.  Go is a cool language, and we might have a go port soon if we can
talk @xb95 into writing it :-), but nitro proper is written in C and
has nothing directly to do with golang.  Hey, good domain names are hard
to come by.

Contact/Credits
===============

nitro was written by @jamwt, with help from @magicseth and @dowski.  We all
work for @bumptech.

Come talk to us on Freenode/#gonitro

Send our bugs and pull requests to GitHub: https://github.com/bumptech/nitro

If there are typos or inaccuracies in this document, let us know that as
well!
