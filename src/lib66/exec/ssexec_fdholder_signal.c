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

#include <66/ssexec.h>
#include <66/svc.h>
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

    /* map the lifecycle action onto a control byte ; restart sends SIGTERM and
     * the supervisor brings the service back up (it stays wanted-up) */
    char byte ;
    if (!strcmp(fdh_signame, "start"))
        byte = 'u' ;
    else if (!strcmp(fdh_signame, "stop"))
        byte = 'd' ;
    else
        byte = 't' ;

    char dir[info->scandir.len + sizeof("/" SS_FDHOLDER) + 1] ;
    auto_strings(dir, info->scandir.s, "/" SS_FDHOLDER) ;

    log_trace(fdh_signame, " fdholder service: ", dir) ;

    return svc_control_send(dir, &byte, 1) ? 0 : LOG_EXIT_SYS ;
}
