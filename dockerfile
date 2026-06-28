FROM obarun/base

LABEL maintainer="Eric Vidal <eric@obarun.org>"

RUN pacman -Sy --noconfirm git base-devel meson ninja execline

RUN git clone https://git.obarun.org/obarun/oblibs.git

WORKDIR /oblibs

RUN meson setup builddir --prefix=/usr && meson compile -C builddir && meson install -C builddir

WORKDIR /

RUN git clone -b dev https://git.obarun.org/obarun/66.git

WORKDIR /66

RUN meson setup builddir --prefix=/usr -D 66-log-user=66log -D 66-log-timestamp=iso && meson compile -C builddir && meson install -C builddir

WORKDIR /

RUN git clone https://git.obarun.org/obarun/66-tools.git

WORKDIR /66-tools

RUN meson setup builddir --prefix=/usr && meson compile -C builddir && meson install -C builddir
