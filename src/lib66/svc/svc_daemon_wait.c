/*
 * svc_daemon_wait.c
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

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/svc.h>
#include <66/event.h>
#include <66/constants.h>

int svc_daemon_wait(char const *dir, int timeout_ms)
{
    log_flow() ;

    int r = 0 ;
    unsigned char up = 0, ready = 0 ;
    event_wait_t w ;

    // a scandir that does not run has no daemon to come
    size_t slen = str_rsearch(dir, '/') ;
    char scandir[slen + 1] ;
    memcpy(scandir, dir, slen) ;
    scandir[slen] = 0 ;

    r = svc_scandir_ok(scandir) ;
    if (r <= 0)
        return r ;

    char eventdir[strlen(dir) + SS_EVENTDIR_LEN + 1] ;
    auto_strings(eventdir, dir, SS_EVENTDIR) ;
    char const *dirs[1] = { eventdir } ;

    // the supervisor makes the fifodir on start: make it first, subscribe, then read the state
    if (!event_fifo_make(eventdir, getgid()) || !event_wait_init(&w, dirs, 1, EVENT_UP_READY))
        return -1 ;

    if (svc_supervise_ok(dir) > 0 && svc_status_state(dir, &up, &ready) > 0 && ready) {
        r = 1 ;
    } else {
        r = event_wait_run(&w, timeout_ms) ;
    }

    int e = errno ;
    event_wait_free(&w) ;
    errno = e ;

    return r ;
}
