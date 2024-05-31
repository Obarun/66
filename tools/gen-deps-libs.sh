#!/bin/sh -e

for dir in src/lib66/* ; do

    rm $dir/deps-lib/deps
    for file in $(ls -1 $dir | grep -- \\.c$); do
        echo $file | sed s/\\.c$/.o/ >> $dir/deps-lib/deps
    done
    echo "-ls6
-loblibs
-lexecline
-lskarnet
" >> $dir/deps-lib/deps
done
