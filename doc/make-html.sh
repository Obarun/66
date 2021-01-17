#!/bin/sh

html='66-all 66-dbctl 66-disable 66-echo 66-enable 66-env 66-init 66-inresolve 66-inservice 66-instate 66-intree 66-parser 66-scanctl 66-scandir 66-start 66-stop 66-svctl 66-tree 66-update 66-boot 66-hpr 66-shutdown 66-shutdownd 66-umountall 66-update 66-nuke frontend instantiated-service module-service index execl-envfile service-configuration-file upgrade'

version=${1}

if [ ! -d doc/${version}/html ]; then
    mkdir -p -m 0755 doc/${version}/html || exit 1
fi

for i in ${html};do
     lowdown -s doc/${i}.md -o doc/${version}/html/${i}.html || exit 1
done

exit 0
