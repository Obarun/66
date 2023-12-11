#!/bin/sh

html='66-echo 66-hpr 66-nuke 66-shutdown 66-shutdownd 66-umountall 66 boot configure deeper disable enable execl-envfile frontend halt index init instantiated-service module-creation module-usage parse poweroff reboot reconfigure reload remove resolve restart rosetta scandir service-configuration-file signal start state status stop tree upgrade'

version=${1}

if [ ! -d doc/${version}/html ]; then
    mkdir -p -m 0755 doc/${version}/html || exit 1
fi

for i in ${html};do
     lowdown -s doc/${i}.md -o doc/${version}/html/${i}.html || exit 1
done

exit 0
