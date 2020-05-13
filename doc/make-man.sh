#!/bin/sh

man1='66-all 66-dbctl 66-disable 66-echo 66-enable 66-env 66-init 66-inresolve 66-inservice 66-instate 66-intree 66-parser 66-scanctl 66-scandir 66-start 66-stop 66-svctl 66-tree 66-update'

man8='66-boot 66-hpr 66-shutdown 66-shutdownd 66-umountall'

man5='frontend'

for i in 1 5 8;do
    if [ ! -d doc/man/man${i} ]; then
       mkdir -p -m 0755 doc/man/man"${i}" || exit 1
    fi
done

for i in ${man1}; do
    lowdown -s -Tman doc/"${i}".md -o doc/man/man1/"${i}".1 || exit 1 
    var=$(head -n1 < doc/man/man1/"${i}".1)
    var=$(printf '%s' "$var" | tr '7' '1')
    var="${var} \"\" \"General Commands Manual\""
    sed -i "s!^.TH.*!${var}!" doc/man/man1/"${i}".1 || exit 1
    sed -i '2,5d' doc/man/man1/"${i}".1 || exit 1
done

for i in ${man5}; do
    lowdown -s -Tman doc/"${i}".md -o doc/man/man5/"${i}".5 || exit 1 
    var=$(head -n1 < doc/man/man5/"${i}".5)
    var=$(printf '%s' "$var" | tr '7' '5')
    var="${var} \"\" \"File Formats Manual\""
    sed -i "s!^.TH.*!${var}!" doc/man/man5/"${i}".5 || exit 1
    sed -i '2,5d' doc/man/man5/"${i}".5 || exit 1
done

for i in ${man8}; do
    lowdown -s -Tman doc/"${i}".md -o doc/man/man8/"${i}".8 || exit 1 
    var=$(head -n1 < doc/man/man8/"${i}".8)
    var=$(printf '%s' "$var" | tr '7' '8')
    var="${var} \"\" \"System Administration\""
    sed -i "s!^.TH.*!${var}!" doc/man/man8/"${i}".8 || exit 1
    sed -i '2,5d' doc/man/man8/"${i}".8 || exit 1
done

exit 0
