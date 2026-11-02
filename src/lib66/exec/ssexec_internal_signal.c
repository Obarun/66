/*
 * ssexec_internal_signal.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <string.h>
#include <limits.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/ssexec.h>
#include <66/svc.h>
#include <66/constants.h>

static char const *internal_signame = "start" ;

void internal_signal_set_name(char const *name)
{
    internal_signame = name ;
}

int ssexec_internal_signal(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    if (argc != 1)
        log_die(LOG_EXIT_USER, "expected one internal service: " SS_FDHOLDER ", " SS_ONESHOTD " or " SS_EVENTD) ;

    char const *name = argv[0] ;
    int timeout = info->timeout > INT_MAX ? INT_MAX : (int)info->timeout ;

    if (strcmp(name, SS_FDHOLDER) && strcmp(name, SS_ONESHOTD) && strcmp(name, SS_EVENTD))
        log_die(LOG_EXIT_USER, "unknown internal service: ", name) ;

    /* map the lifecycle action onto a control sequence and the state to confirm.
     * start/stop use the lowercase u/d: no down file is written, so a stop
     * lasts until the scandir restarts, which always brings its internal
     * services back up; restart sends the stop signal of the service and the
     * supervisor brings it back up (down then up). */
    char const *control ;
    event_t wanted ;
    if (!strcmp(internal_signame, "start")) {
        control = "u" ; wanted = EVENT_UP_READY ;
    } else if (!strcmp(internal_signame, "stop")) {
        control = "d" ; wanted = EVENT_DOWN_READY ;
    } else {
        control = "r" ; wanted = EVENT_RESTART_READY ;
    }

    char dir[info->scandir.len + 1 + strlen(name) + 1] ;
    auto_strings(dir, info->scandir.s, "/", name) ;

    log_trace(internal_signame, " internal service: ", dir) ;

    int r = svc_daemon_send(dir, control, STATUS_WHO_USER, wanted, timeout) ;
    if (r < 0)
        log_dieu(LOG_EXIT_SYS, "send control ", control, " to internal service: ", dir) ;

    if (!r && wanted != EVENT_DOWN_READY)
        log_dieu(LOG_EXIT_SYS, "internal service did not reach the wanted state: ", dir) ;

    return 0 ;
}
