export ALLC="`find ./src -maxdepth 1 -name '*.[ch]' | grep -v 'sha1.c'`"
astyle --style=java -s4 -w -Y -f -p -H -xd -c -j --align-pointer=name --align-reference=name -n $ALLC

for i in $ALLC; do
    grep -HnP "^$" $i | python2 -u util/check_newlines.py  1>&2;
done
