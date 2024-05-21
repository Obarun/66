#!/bin/sh

html='66-echo 66-hpr 66-nuke 66-shutdown 66-shutdownd 66-umountall 66 66-boot 66-configure 66-deeper 66-disable 66-enable execl-envfile 66-free 66-frontend 66-halt index 66-instantiated-service 66-module-creation 66-module-usage 66-parse 66-poweroff 66-reboot 66-reconfigure 66-reload 66-remove 66-resolve 66-restart 66-rosetta 66-scandir 66-service-configuration-file 66-signal 66-start 66-state 66-status 66-stop 66-tree 66-upgrade 66-version 66-wall'

version=${1}

if [ ! -d doc/${version}/html ]; then
    mkdir -p -m 0755 doc/${version}/html || exit 1
fi

for i in ${html};do
     lowdown -s doc/${i}.md -o doc/${version}/html/${i}.html || exit 1
done

exit 0
