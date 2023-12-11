#!/bin/sh

man1='66 configure disable enable execl-envfile halt init parse poweroff reboot reconfigure reload remove resolve restart rosetta scandir signal start state status stop tree upgrade version'

man8='boot 66-hpr 66-shutdown 66-shutdownd 66-umountall'

man5='deeper frontend instantiated-service module-creation module-usage service-configuration-file'

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
