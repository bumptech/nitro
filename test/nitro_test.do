export TESTS="tests.o"
redo-ifchange $TESTS wvtest/wvtestmain.o wvtest/wvtest.o
gcc -L.. $TESTS wvtest/wvtestmain.o wvtest/wvtest.o -lnitro -luv -lpthread -o $3 $EXTRA_LDFLAGS
