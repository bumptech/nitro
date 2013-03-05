TESTS=`ls *.c | sed 's/\.c//g'`
redo $TESTS

for x in $TESTS; do $VALGRIND ./$x; done
