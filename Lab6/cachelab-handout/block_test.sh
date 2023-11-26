#!/bin/bash
rm log.txt
touch log.txt
for ((R=1; R<=64; R=R*2))
do
    for((C=1; C<=64; C=C*2))
    do
        echo "Testing ($R, $C) Block combination."
        echo -n "$R $C | " >> log.txt
        ./test-trans -M 61 -N 67 -R $R -C $C | sed -n "/misses:*[0-9]/p" >> log.txt
    done
done