FROM obarun/base

LABEL maintainer="Eric Vidal <eric@obarun.org>"

RUN pacman -Sy --noconfirm git base-devel skalibs execline s6

RUN git clone https://git.obarun.org/obarun/oblibs.git

WORKDIR /oblibs

RUN ./configure --prefix=/usr

RUN make install

WORKDIR /

RUN git clone -b dev https://git.obarun.org/obarun/66.git

WORKDIR /66

RUN ./configure --prefix=/usr --with-s6-log-user=s6log --with-s6-log-timestamp=iso

RUN make install

WORKDIR /

RUN git clone https://git.obarun.org/obarun/66-tools.git

WORKDIR /66-tools

RUN ./configure --prefix=/usr

RUN make install
