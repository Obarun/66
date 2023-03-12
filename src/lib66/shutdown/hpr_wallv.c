/*
 * hpr_wallv.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 *
 * This file is a strict copy of hpr_wallv.c file
 * coming from skarnet software at https://skarnet.org/software/s6-linux-init.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */

#include <string.h>
#include <sys/uio.h>
#include <utmpx.h>

#include <oblibs/log.h>

#include <skalibs/allreadwrite.h>
#include <skalibs/djbunix.h>
#include <skalibs/posixishard.h>

#include <66/hpr.h>

#ifndef UT_LINESIZE
#define UT_LINESIZE 32
#endif

void hpr_wallv (struct iovec const *v, unsigned int n)
{
    char tty[10 + UT_LINESIZE] = "/dev/" ;
    setutxent() ;
    for (;;)
    {
        size_t linelen ;
        int fd ;
        struct utmpx *utx = getutxent() ;
        if (!utx) break ;
        if (utx->ut_type != USER_PROCESS) continue ;
        linelen = strnlen(utx->ut_line, UT_LINESIZE) ;
        memcpy(tty + 5, utx->ut_line, linelen) ;
        tty[5 + linelen] = 0 ;
        fd = open_append(tty) ;
        if (fd == -1) continue ;
        allwritev(fd, v, n) ;
        fd_close(fd) ;
    }
    endutxent() ;
}
