#!/bin/sh

man1='66-all 66-dbctl 66-disable 66-echo 66-enable 66-env 66-init 66-inresolve 66-inservice 66-instate 66-intree 66-parser 66-scanctl 66-scandir 66-start 66-stop 66-svctl 66-tree 66-update'

man8='66-boot 66-hpr 66-shutdown 66-shutdownd 66-umountall'

man5='frontend'

for i in 1 5 8;do
    if [ ! -d man/man${i} ]; then
       mkdir -p -m 0755 man/man${i}
    fi
done

for i in ${man1}; do
    lowdown -s -Tman ${i}.md -o man/man1/${i}.1 
    var=$(head -n1 < man/man1/${i}.1)
    var=${var/" 7 "/" 1 "}
    var="${var} \"\" \"General Commands Manual\""
    sed -i "s!^.TH.*!${var}!" man/man1/${i}.1
    sed -i '2,5d' man/man1/${i}.1
done

for i in ${man5}; do
    lowdown -s -Tman ${i}.md -o man/man5/${i}.1 
    var=$(head -n1 < man/man5/${i}.1)
    var=${var/" 7 "/" 5 "}
    var="${var} \"\" \"General Commands Manual\""
    sed -i "s!^.TH.*!${var}!" man/man5/${i}.1
    sed -i '2,5d' man/man5/${i}.1
done

for i in ${man8}; do
    lowdown -s -Tman ${i}.md -o man/man8/${i}.1 
    var=$(head -n1 < man/man8/${i}.1)
    var=${var/" 7 "/" 8 "}
    var="${var} \"\" \"General Commands Manual\""
    sed -i "s!^.TH.*!${var}!" man/man8/${i}.1
    sed -i '2,5d' man/man8/${i}.1
done

