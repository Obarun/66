/*
 * ssexec_fdholder_signal.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/environ.h>
#include <oblibs/spawn.h>
#include <oblibs/process.h>

#include <66/ssexec.h>
#include <66/constants.h>

/* set by the start/stop/restart trampolines in ssexec_fdholder_wrapper.c */
static char const *fdh_signame = "start" ;
static uint32_t fdh_sig_timeout = 3000 ;

void fdholder_signal_set_name(char const *name)
{
    fdh_signame = name ;
}

int on_fdholder_signal(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {
        case 'T' :
            if (!u32_scan_strict(arg, &fdh_sig_timeout))
                log_dieu(LOG_EXIT_USER, "parse timeout: ", arg) ;
            break ;
    }

    return 0 ;
}

int ssexec_fdholder_signal(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    (void)argc ;
    (void)argv ;

    ssexec_t *info = data ;

    /* map the lifecycle action onto an s6-svc flag ; restart sends SIGTERM and
     * the supervisor brings the service back up (it stays wanted-up) */
    char const *flag ;
    if (!strcmp(fdh_signame, "start"))
        flag = "-u" ;
    else if (!strcmp(fdh_signame, "stop"))
        flag = "-d" ;
    else
        flag = "-t" ;

    char dir[info->scandir.len + sizeof("/" SS_FDHOLDER) + 1] ;
    auto_strings(dir, info->scandir.s, "/" SS_FDHOLDER) ;

    char tfmt[U32_FMT] ;
    tfmt[u32_fmt(tfmt, fdh_sig_timeout)] = 0 ;

    char const *newargv[7] ;
    unsigned int m = 0 ;
    newargv[m++] = "s6-svc" ;
    newargv[m++] = flag ;
    newargv[m++] = "-T" ;
    newargv[m++] = tfmt ;
    newargv[m++] = "--" ;
    newargv[m++] = dir ;
    newargv[m] = 0 ;

    log_trace(fdh_signame, " fdholder service: ", dir) ;

    pid_t pid = spawn_path(newargv[0], newargv, (char const *const *)environ) ;
    int wstat ;
    if (process_wait(pid, &wstat) < 0)
        log_dieusys(LOG_EXIT_SYS, "wait for s6-svc") ;

    return wstat ? LOG_EXIT_SYS : 0 ;
}
