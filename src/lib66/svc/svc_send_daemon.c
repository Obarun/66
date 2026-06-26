/*
 * svc_send_daemon.c
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
#include <oblibs/string.h>

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

    event_match_t m ;
    event_match_init(&m, wanted, up, ready) ;
    return event_match_feed(&m, 0, 0) == EVENT_MATCH_OK ;
}

void svc_send_daemon(char const *dir, char const *control, event_t wanted, int timeout_ms)
{
    log_flow() ;

    unsigned char up = 0, ready = 0 ;
    int have = svc_status_state(dir, &up, &ready) ;

    // already in the wanted state: nothing to wait for.
    if (in_state(wanted, have, up, ready)) {
        // an up target is still (idempotently) asserted; a down target is not.
        if (!is_down(wanted) && !svc_control_send(dir, control, strlen(control)))
            log_dieu(LOG_EXIT_SYS, "send control ", control, " to daemon: ", dir) ;
        return ;
    }

    char eventdir[strlen(dir) + sizeof("/event")] ;
    auto_strings(eventdir, dir, "/event") ;
    char const *dirs[1] = { eventdir } ;

    event_wait_t w ;
    if (!event_wait_init(&w, dirs, 1, wanted))
        log_dieusys(LOG_EXIT_SYS, "subscribe to event dir: ", eventdir) ;

    if (!svc_control_send(dir, control, strlen(control))) {
        event_wait_free(&w) ;
        log_dieu(LOG_EXIT_SYS, "send control ", control, " to daemon: ", dir) ;
    }

    int r = event_wait_run(&w, timeout_ms) ;
    event_wait_free(&w) ;

    if (r == 1)
        return ; // confirmed by the event

    // timeout or permanent failure: reconcile against the real status.
    up = ready = 0 ;
    have = svc_status_state(dir, &up, &ready) ;
    if (in_state(wanted, have, up, ready))
        return ;

    if (is_down(wanted))
        log_warn("daemon did not confirm down within timeout: ", dir) ;
    else
        log_dieu(LOG_EXIT_SYS, "daemon did not reach the wanted state: ", dir) ;
}
