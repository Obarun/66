/*
 * hpr_wall.c
 *
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 *
 * This file is a strict copy of hpr_wall.c file
 * coming from skarnet software at https://skarnet.org/software/s6-linux-init.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */

#include <string.h>
#include <utmpx.h>

#include <oblibs/log.h>

#include <skalibs/posixishard.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/djbunix.h>

#include <66/hpr.h>

#ifndef UT_LINESIZE
#define UT_LINESIZE 32
#endif

void hpr_wall (char const *s)
{
    log_flow() ;

    size_t n = strlen(s) ;
    char tty[10 + UT_LINESIZE] = "/dev/" ;
    char msg[n+1] ;
    memcpy(msg, s, n) ;
    msg[n++] = '\n' ;
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
        allwrite(fd, msg, n) ;
        fd_close(fd) ;
    }
    endutxent() ;
}
