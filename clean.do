rm -f test/*.o src/*.o
rm -f *.a

TEST_PROGRAMS="`find test -name '*.c' | sed 's/\.c//g'`"
rm -f $TEST_PROGRAMS
