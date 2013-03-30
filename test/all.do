TESTS=`ls *.c | sed 's/\.c//g'`
redo $TESTS

for i in $TESTS; do
    $VALGRIND ./$i;
done
