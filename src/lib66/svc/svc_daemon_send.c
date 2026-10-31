/*
 * svc_daemon_send.c
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/constants.h>
#include <66/event.h>
#include <66/svc.h>

static int is_down(event_t wanted)
{
    return wanted == EVENT_DOWN || wanted == EVENT_DOWN_READY || wanted == EVENT_SUPERVISE_DOWN ;
}

static int in_state(event_t wanted, int have, unsigned char up, unsigned char ready)
{
    if (wanted == EVENT_SUPERVISE_DOWN)
        return !have ; // no status: supervisor (and service) already gone
    if (wanted == EVENT_SUPERVISE_UP)
        return have ; // status present: supervisor is up
    if (!have)
        return 0 ; // no status: cannot confirm a service state -> must act

    event_state_t s ;
    event_state_init(&s, wanted, up, ready) ;
    return event_state_satisfied(&s) ;
}

int svc_daemon_send(char const *dir, char const *control, uint8_t who, event_t wanted, int timeout_ms)
{
    log_flow() ;

    int r, e ;
    unsigned char up = 0, ready = 0 ;
    int have = svc_status_state(dir, &up, &ready) ;

    // already in the wanted state: nothing to wait for.
    if (in_state(wanted, have, up, ready)) {
        // an up target is still (idempotently) asserted; a down target is not.
        if (!is_down(wanted) && !svc_control_send(dir, control, strlen(control), who))
            return -1 ;
        return 1 ;
    }

    char eventdir[strlen(dir) + SS_EVENTDIR_LEN + 1] ;
    auto_strings(eventdir, dir, SS_EVENTDIR) ;
    char const *dirs[1] = { eventdir } ;

    event_wait_t w ;
    if (!event_wait_init(&w, dirs, 1, wanted))
        log_warnusys_return(LOG_EXIT_LESSONE, "subscribe to event dir: ", eventdir) ;

    if (!svc_control_send(dir, control, strlen(control), who)) {
        e = errno ;
        event_wait_free(&w) ;
        errno = e ;
        return -1 ;
    }

    r = event_wait_run(&w, timeout_ms) ;
    event_wait_free(&w) ;

    if (r == 1)
        return 1 ; // confirmed by the event

    // timeout or permanent failure: reconcile against the real status.
    up = ready = 0 ;
    have = svc_status_state(dir, &up, &ready) ;
    if (in_state(wanted, have, up, ready))
        return 1 ;

    if (is_down(wanted))
        log_warn("daemon did not confirm down within timeout: ", dir) ;

    return 0 ;
}
