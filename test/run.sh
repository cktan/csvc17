#!/bin/bash

set -e

DIR=$(dirname ${BASH_SOURCE[0]})
DIR=$(realpath ${DIR})

mkdir -p $DIR/out

# input file name {number}.csv uses quote as escape
for i in {1..100}; do
	IN=$DIR/in/$i.csv
        GOOD=$DIR/good/$i.py
        OUT=$DIR/out/$i.py

        [ -f $IN ] || continue

        echo test ${i}

	./csv2py -n '' -d '|' $IN > $OUT ||
                { echo '--- csvnorm FAILED ---'; exit 1; }
        python3 $DIR/pydiff.py $OUT $GOOD ||
                { echo '--- pydiff FAILED ---'; exit 1; }
done


# input file name {number}x.csv uses backslash as escape
for i in {1..100}x; do
	IN=$DIR/in/$i.csv
        GOOD=$DIR/good/$i.py
        OUT=$DIR/out/$i.py

        [ -f $IN ] || continue

        echo test ${i}

	./csv2py -n '' -e '\' -d '|' $IN > $OUT ||
                { echo '--- csvnorm FAILED ---'; exit 1; }
        python3 $DIR/pydiff.py $OUT $GOOD ||
                { echo '--- pydiff FAILED ---'; exit 1; }
done
