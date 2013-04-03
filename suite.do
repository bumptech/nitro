export LIBOBJ="`find $HERE/src -name '*.c' | sed 's/\.c/\.o/g'`"
redo-ifchange libnitro.a
redo examples/basic
redo examples/sender
redo examples/targeted
redo examples/proxied
redo examples/pubsub
