/*
 * svc_send_wait.c
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

#include <oblibs/log.h>
#include <oblibs/environ.h>

#include <skalibs/djbunix.h>
#include <skalibs/cspawn.h>

#include <66/svc.h>
#include <66/resolve.h>
#include <66/ssexec.h>

int svc_send_wait(char const *const *list, unsigned int nservice, char **sig, unsigned int siglen, ssexec_t *info)
{
    log_flow() ;

    pid_t pid ;
    int wstat ;

    int nargc = 5 + nservice + siglen + info->opt_color + (info->opt_timeout ? 2 : 0) ;
    char const *newargv[nargc] ;
    unsigned int m = 0 ;
    char verbo[UINT_FMT] ;
    char fmt[UINT32_FMT] ;

    verbo[uint_fmt(verbo, VERBOSITY)] = 0 ;

    if (info->opt_timeout)
        fmt[uint32_fmt(fmt,info->timeout)] = 0 ;

    newargv[m++] = "66" ;

    if (info->opt_color)
        newargv[m++] = "-z" ;

    if (info->timeout) {
        newargv[m++] = "-T" ;
        newargv[m++] = fmt ;
    }

    newargv[m++] = "-v" ;
    newargv[m++] = verbo ;
    newargv[m++] = "signal" ;

    for (; *sig ; sig++)
        newargv[m++] = *sig ;

    for (; *list ; list++)
        newargv[m++] = *list ;

    newargv[m++] = 0 ;

    pid = child_spawn0(newargv[0], newargv, (char const *const *) environ) ;

    if (waitpid_nointr(pid, &wstat, 0) < 0)
        log_warnusys_return(LOG_EXIT_SYS, "wait for svctl") ;

    if (wstat)
        log_warnu_return(WIFSIGNALED(wstat) ? WTERMSIG(wstat) : WEXITSTATUS(wstat), "handle services selection") ;

    return 0 ;
}
