/*
 * svc_send_fdholder.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/types.h>
#include <skalibs/djbunix.h>
#include <skalibs/cspawn.h>

#include <66/svc.h>

void svc_send_fdholder(char const *socket, char const *signal)
{
    log_flow() ;

    char tfmt[UINT32_FMT] ;
    tfmt[uint_fmt(tfmt, 3000)] = 0 ;
    pid_t pid ;
    int wstat ;

    char const *newargv[8] ;
    unsigned int m = 0 ;
    char sig[1 + strlen(signal) + 1] ;
    auto_strings(sig, "-", signal) ;

    newargv[m++] = "s6-svc" ;
    newargv[m++] = sig ;
    newargv[m++] = "-T" ;
    newargv[m++] = tfmt ;
    newargv[m++] = "--" ;
    newargv[m++] = socket ;
    newargv[m++] = 0 ;

    log_trace("sending -", signal, " signal to: ", socket) ;

    pid = child_spawn0(newargv[0], newargv, (char const *const *) environ) ;

    if (waitpid_nointr(pid, &wstat, 0) < 0)
        log_dieusys(LOG_EXIT_SYS, "waiting reload of the fdholder daemon") ;

    if (wstat)
        log_dieu(LOG_EXIT_SYS, "reload fdholder service; ", socket) ;
}
