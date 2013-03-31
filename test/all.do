TESTS=`ls *.c | sed 's/\.c//g'`
redo $TESTS

for i in $TESTS; do
    if [ -z "$TEST" ];
    then
        $VALGRIND ./$i
    else
        match=$( (echo $i | grep $TEST) || echo -n "")
        if [ -n "$match" ];
        then
            $VALGRIND ./$i
        else
            echo "Skipping $i" 1>&2
        fi
    fi
done
