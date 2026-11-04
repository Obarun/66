/*
 * svc_unsupervise_one.c
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

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/constants.h>
#include <66/event.h>
#include <66/fdholder.h>
#include <66/service.h>
#include <66/state.h>
#include <66/sanitize.h>
#include <66/svc.h>

int svc_unsupervise_one(resolve_service_t *res, fdholder_client_t *a, bool *started, uint8_t who, uint64_t timeout)
{
    log_flow() ;

    int r, e ;
    char const *name = res->sa.s + res->name ;
    char const *dir = res->sa.s + res->live.servicedir ;
    int ms = !timeout ? SS_READY_TIMEOUT : timeout > INT_MAX ? INT_MAX : (int)timeout ;
    char eventdir[strlen(dir) + SS_EVENTDIR_LEN + 1] ;
    char const *dirs[1] = { eventdir } ;
    event_wait_t w ;
    ss_state_t sta = STATE_ZERO ;

    auto_strings(eventdir, dir, SS_EVENTDIR) ;

    if (!*started) {

        if (!sanitize_fdholder_start(a, res->sa.s + res->live.fdholderdir)) {
            log_warnusys("start fdholder: ", res->sa.s + res->live.fdholderdir) ;
            return -1 ;
        }

        *started = true ;
    }

    if (res->has_event && !svcd_notify(res->sa.s + res->live.eventddir, 'd', who, name))
        log_warnusys("disarm event reactor: ", name) ;

    if (state_read(&sta, res) <= 0) {
        log_warnusys("read state file of: ", name) ;
        return -1 ;
    }

    if (!sanitize_fdholder(res, a, &sta, STATE_FLAGS_FALSE, 0))
        log_warnusys("sanitize fdholder") ;

    r = svc_supervise_ok(dir) ;
    if (r > 0) {

        if (!event_wait_init(&w, dirs, 1, EVENT_SUPERVISE_DOWN))
            log_warnusys_return(LOG_EXIT_LESSONE, "subscribe to: ", eventdir) ;

        r = svc_supervise_ok(dir) ;
        if (r <= 0) {
            e = errno ;
            event_wait_free(&w) ;
            errno = e ;
        }
    }

    if (r < 0)
        log_warnusys_return(LOG_EXIT_LESSONE, "probe the supervisor of: ", name) ;

    state_set_flag(&sta, STATE_FLAGS_TOUNSUPERVISE, STATE_FLAGS_TRUE) ;

    if (!sanitize_scandir(res, &sta))
        log_warnusys("sanitize scandir") ;

    if (r) {

        r = event_wait_run(&w, ms) ;
        e = errno ;
        event_wait_free(&w) ;
        errno = e ;

        if (r < 0)
            log_warnusys_return(LOG_EXIT_LESSONE, "wait for the supervisor of: ", name) ;

        if (!r && svc_supervise_ok(dir) > 0) {
            log_warn("supervisor of: ", name, " did not exit") ;
            return (errno = ETIMEDOUT, -1) ;
        }
    }

    state_set_flag(&sta, STATE_FLAGS_TOUNSUPERVISE, STATE_FLAGS_TRUE) ;

    if (!sanitize_livestate(res, &sta))
        log_warnusys("sanitize livestate") ;

    log_info("Unsupervised successfully: ", name) ;

    return 1 ;
}
