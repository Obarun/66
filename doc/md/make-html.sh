#!/bin/sh

html='66-all 66-dbctl 66-disable 66-echo 66-enable 66-env 66-init 66-inresolve 66-inservice 66-instate 66-intree 66-parser 66-scanctl 66-scandir 66-start 66-stop 66-svctl 66-tree 66-update 66-boot 66-hpr 66-shutdown 66-shutdownd 66-umountall frontend index'

if [ ! -d html ]; then
    mkdir -p -m 0755 html
fi

for i in ${html};do
     lowdown -s ${i}.md -o html/${i}.html 
done

