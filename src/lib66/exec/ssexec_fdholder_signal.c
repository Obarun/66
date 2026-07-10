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

    /* map the lifecycle action onto a control sequence and the state to confirm.
     * start/stop use the uppercase U/D so the wanted state persists (they edit
     * the down file: deldown / adddown); restart sends SIGTERM and the
     * supervisor brings the service back up (down then up). */
    char const *control ;
    event_t wanted ;
    if (!strcmp(fdh_signame, "start")) {
        control = "U" ; wanted = EVENT_UP_READY ;
    } else if (!strcmp(fdh_signame, "stop")) {
        control = "D" ; wanted = EVENT_DOWN_READY ;
    } else {
        control = "t" ; wanted = EVENT_RESTART_READY ;
    }

    char dir[info->scandir.len + 1 + SS_FDHOLDER_LEN + 1] ;
    auto_strings(dir, info->scandir.s, "/" SS_FDHOLDER) ;

    log_trace(fdh_signame, " fdholder service: ", dir) ;

    svc_send_daemon(dir, control, STATUS_WHO_USER, wanted, (int)fdh_sig_timeout) ;
    return 0 ;
}
