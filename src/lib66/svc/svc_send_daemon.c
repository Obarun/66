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

void svc_send_daemon(char const *dir, char const *signal)
{
    log_flow() ;

    int down = strchr(signal, 'x') != NULL ;
    event_t wanted = down ? EVENT_SUPERVISE_DOWN : EVENT_READY ;

    unsigned char up = 0, ready = 0 ;
    int have = svc_status_state(dir, &up, &ready) ;

    // already in the target state: nothing to wait for.
    if (down) {

        if (!have || !up)
            return ; // no supervisor, or already down

    } else {

        if (have && up && ready) {
            // already up and ready: still assert wanted-up (idempotent), no wait
            if (!svc_control_send(dir, signal, strlen(signal)))
                log_dieu(LOG_EXIT_SYS, "send control signal ", signal, " to daemon: ", dir) ;
            return ;
        }
    }

    // subscribe to the event dir BEFORE sending, so the transition is not missed.
    char eventdir[strlen(dir) + sizeof("/event")] ;
    auto_strings(eventdir, dir, "/event") ;
    char const *dirs[1] = { eventdir } ;

    event_wait_t w ;
    if (!event_wait_init(&w, dirs, 1, wanted))
        log_dieusys(LOG_EXIT_SYS, "subscribe to event dir: ", eventdir) ;

    if (!svc_control_send(dir, signal, strlen(signal))) {
        event_wait_free(&w) ;
        log_dieu(LOG_EXIT_SYS, "send control signal ", signal, " to daemon: ", dir) ;
    }

    int r = event_wait_run(&w, 3000) ;
    event_wait_free(&w) ;

    if (r == 1)
        return ; // confirmed by the event

    // timeout or permanent failure: reconcile against the real status.
    up = ready = 0 ;
    have = svc_status_state(dir, &up, &ready) ;
    if (down) {

        if (!have || !up)
            return ; // it did go down after all

        log_warn("daemon did not confirm down within timeout: ", dir) ;
    } else {

        if (have && up && ready)
            return ;

        log_dieu(LOG_EXIT_SYS, "daemon did not reach ready: ", dir) ;
    }
}
