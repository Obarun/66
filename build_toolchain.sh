#!/bin/sh

tag=1

if [ "$1" == "commit" ]; then
    tag=0
fi

skalibs_tag="v2.14.0.1"
execline_tag="v2.9.4.0"
s6_tag="v2.12.0.2"
oblibs_tag="0.2.0.1"

check_tag(){

    if ((tag)); then
        git checkout tags/"${1}"
    fi
}

rm_package(){

    pacman -Rdd ${1}

}

## skalibs
build_skalibs() {

    rm_package "skalibs"
    git clone https://github.com/skarnet/skalibs
    cd skalibs
    check_tag "${skalibs_tag}"
    ./configure \
        --prefix=/usr \
        --with-default-path=/usr/bin \
        --disable-shared

    make install || return 1
    cd ..
}

## execline
build_execline() {

    rm_package "execline"
    git clone https://github.com/skarnet/execline
    cd execline
    check_tag "${execline_tag}"
    ./configure \
        --prefix=/usr \
        --libexecdir=/usr/libexec \
        --bindir=/usr/bin \
        --sbindir=/usr/bin \
        --shebangdir=/usr/bin \
        --disable-shared

    make install || return 1
    cd ..
}

## s6
build_s6() {

    rm_package "s6"
    git clone https://github.com/skarnet/s6
    cd s6
    check_tag "${s6_tag}"
    ./configure \
        --prefix=/usr \
        --bindir=/usr/bin \
        --sbindir=/usr/bin \
        --disable-shared

    make install || return 1
    cd ..
}

## oblibs
build_oblibs() {

    rm_package "oblibs"
    git clone https://git.obarun.org/obarun/oblibs
    cd oblibs
    check_tag "${oblibs_tag}"
    ./configure \
        --prefix=/usr \
        --with-lib=/usr/lib/skalibs \
        --with-lib=/usr/lib/execline \
        --disable-shared

    make install || return 1
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
