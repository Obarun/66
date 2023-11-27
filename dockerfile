FROM obarun/base

LABEL maintainer="Eric Vidal <eric@obarun.org>"

RUN pacman -Sy --noconfirm git base-devel skalibs execline s6

RUN git clone https://git.obarun.org/obarun/oblibs.git

WORKDIR /oblibs

RUN ./configure --prefix=/usr --with-lib=/usr/include/skalibs --with-lib=/usr/include/execline --disable-shared

RUN make install

WORKDIR /

RUN git clone -b dev https://git.obarun.org/obarun/66.git

WORKDIR /66

RUN ./configure --prefix=/usr --bindir=/usr/bin --shebangdir=/usr/bin --with-system-service=/usr/lib/66/service --with-system-script=/usr/lib/66/script --with-system-seed=/usr/lib/66/seed --with-s6-log-user=root --with-s6-log-timestamp=iso --with-lib=/usr/lib/skalibs --with-lib=/usr/lib/execline --with-lib=/usr/lib/s6 --with-lib=/usr/lib/oblibs --disable-shared

RUN make install

WORKDIR /

RUN git clone https://git.obarun.org/obarun/66-tools.git

WORKDIR /66-tools

RUN ./configure --bindir=/usr/bin --with-ns-rule=/usr/lib/66/script/ns --with-lib=/usr/lib/skalibs/ --with-lib=/usr/lib/oblibs --with-lib=/usr/lib/execline --disable-shared

RUN make install
