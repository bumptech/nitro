find src -name '*.o' | xargs rm -f
find test -name '*.o' | xargs rm -f
find examples -name '*.o' | xargs rm -f
rm -f *.a
