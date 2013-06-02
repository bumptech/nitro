TESTS=`ls *.c | sed 's/\.c//g'`
redo $TESTS

RUNTESTS=`ls *.test`

for i in $RUNTESTS; do
    if [ -z "$TEST" ];
    then
        echo " ~~~~ $i ~~~~" 1>&2
        $VALGRIND ./$i
    else
        match=$( (echo $i | grep $TEST) || echo -n "")
        if [ -n "$match" ];
        then
            echo " ~~~~ $i ~~~~" 1>&2
            $VALGRIND ./$i
        else
            echo "(!skipping $i)" 1>&2
        fi
    fi
done
