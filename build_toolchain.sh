#!/bin/sh

tag=1

if [ "$1" == "commit" ]; then
    tag=0
fi

skalibs_tag="v2.14.5.1"
execline_tag="v2.9.8.1"
s6_tag="v2.14.0.1"
oblibs_tag="0.3.4.0"

check_tag(){

    if ((tag)); then
        git checkout tags/"${1}"
    fi
}

## skalibs
build_skalibs() {

    git clone https://github.com/skarnet/skalibs
    cd skalibs
    check_tag "${skalibs_tag}"
    ./configure \
        --prefix=/usr

    make install || return 1
    cd ..
}

## execline
build_execline() {

    git clone https://github.com/skarnet/execline
    cd execline
    check_tag "${execline_tag}"
    ./configure \
        --prefix=/usr \
        --enable-shared \
        --disable-allstatic

    make install || return 1
    cd ..
}

## s6
build_s6() {

    git clone https://github.com/skarnet/s6
    cd s6
    check_tag "${s6_tag}"
    ./configure \
        --prefix=/usr \
        --libexecdir=/usr/libexec \
        --enable-shared \
        --disable-allstatic

    make install || return 1
    cd ..
}

## oblibs
build_oblibs() {

    git clone https://git.obarun.org/obarun/oblibs
    cd oblibs

    check_tag "${oblibs_tag}"

    meson setup builddir -D prefix=/usr || return 1
    meson compile -C builddir || return 1
    meson install -C builddir || return 1

    cd ..
}

_run() {

    if ! ${1} ; then
        printf "%s" "unable to build ${1#*_}"
        exit 1
    fi
}

## do it
_run build_skalibs
_run build_execline
_run build_s6
_run build_oblibs
