#!/bin/sh

man1='66 66-configure 66-disable 66-enable 66-free execl-envfile 66-halt 66-parse 66-poweroff 66-reboot 66-reconfigure 66-reload 66-remove 66-resolve 66-restart 66-rosetta 66-scandir 66-signal 66-start 66-state 66-status 66-stop 66-tree 66-upgrade 66-version 66-wall 66-snapshot'

man8='66-boot 66-hpr 66-shutdown 66-shutdownd 66-umountall'

man5='66-deeper 66-frontend 66-instantiated-service 66-module-creation 66-module-usage 66-service-configuration-file 66-standard-io-redirection'

for i in 1 5 8;do
    if [ ! -d doc/man/man${i} ]; then
       mkdir -p -m 0755 doc/man/man"${i}" || exit 1
    fi
done

for i in ${man1}; do
    lowdown -s -Tman doc/"${i}".md -o doc/man/man1/"${i}".1 || exit 1
    var=$( sed -n -e '/^.TH/p' < doc/man/man1/"${i}".1)
    var=$(printf '%s' "$var" | tr '7' '1')
    sed -i "s!^.TH.*!${var}!" doc/man/man1/"${i}".1 || exit 1
    sed -i '4,8d' doc/man/man1/"${i}".1 || exit 1
done

for i in ${man5}; do
    lowdown -s -Tman doc/"${i}".md -o doc/man/man5/"${i}".5 || exit 1
    var=$( sed -n -e '/^.TH/p' < doc/man/man5/"${i}".5)
    var=$(printf '%s' "$var" | tr '7' '5')
    sed -i "s!^.TH.*!${var}!" doc/man/man5/"${i}".5 || exit 1
    sed -i '4,8d' doc/man/man5/"${i}".5 || exit 1
done

for i in ${man8}; do
    lowdown -s -Tman doc/"${i}".md -o doc/man/man8/"${i}".8 || exit 1
    var=$( sed -n -e '/^.TH/p' < doc/man/man8/"${i}".8)
    var=$(printf '%s' "$var" | tr '7' '8')
    sed -i "s!^.TH.*!${var}!" doc/man/man8/"${i}".8 || exit 1
    sed -i '4,8d' doc/man/man8/"${i}".8 || exit 1
done

exit 0
