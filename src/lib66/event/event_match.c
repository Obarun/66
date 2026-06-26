/*
 * event_match.c
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
 *
 * The CONSUMER of the event pump for 66 service transitions: it gives the raw
 * bytes meaning. A small state machine that tracks the (up, ready) pair from the
 * 66 transition bytes and decides when a wanted service state has been reached
 * (or has permanently failed). Replaces the duplicated logic that lived inline
 * in event_wait (exact byte) and in svc_launch (rich wait): one interpreter,
 * shared by every waiter.
 */

#include <stddef.h>

#include <oblibs/log.h>

#include <66/event.h>

static void wanted_to_upready(event_t wanted, unsigned char *wantup, unsigned char *wantready)
{
    switch (wanted) {
        case EVENT_UP :            *wantup = 1 ; *wantready = 0 ; break ;
        case EVENT_READY :         *wantup = 1 ; *wantready = 1 ; break ;
        case EVENT_DOWN :          *wantup = 0 ; *wantready = 0 ; break ;
        case EVENT_DOWN_READY :    *wantup = 0 ; *wantready = 1 ; break ;
        case EVENT_RESTART :       *wantup = 2 ; *wantready = 0 ; break ;
        case EVENT_RESTART_READY : *wantup = 2 ; *wantready = 1 ; break ;
        default :                  *wantup = 0 ; *wantready = 0 ; break ;
    }
}

static int satisfied(unsigned char up, unsigned char ready, unsigned char wantup, unsigned char wantready)
{
    int u = wantup ? up : !up ;
    int r = wantready ? ready : 1 ;
    return u && r ;
}

static int eval(event_match_t *m, unsigned char wantup, unsigned char wantready)
{
    if (wantup == 2) {
        if (!m->restart_done)
            return 0 ;
        return satisfied(m->up, m->ready, 1, wantready) ;
    }
    return satisfied(m->up, m->ready, wantup, wantready) ;
}

void event_match_init(event_match_t *m, event_t wanted, unsigned char up, unsigned char ready)
{
    log_flow() ;

    m->wanted = wanted ;
    m->up = up ? 1 : 0 ;
    m->ready = ready ? 1 : 0 ;
    m->restart_done = 0 ;
}

int event_match_feed(event_match_t *m, char const *buf, size_t len)
{
    log_flow() ;

    if (m->wanted == EVENT_SUPERVISE_UP || m->wanted == EVENT_SUPERVISE_DOWN) {

        for (size_t i = 0 ; i < len ; i++) {

            if (event_from_byte(buf[i]) == m->wanted)
                return EVENT_MATCH_OK ;
        }

        return EVENT_MATCH_PENDING ;
    }

    unsigned char wantup, wantready ;
    wanted_to_upready(m->wanted, &wantup, &wantready) ;

    if (eval(m, wantup, wantready))
        return EVENT_MATCH_OK ;

    for (size_t i = 0 ; i < len ; i++) {

        switch (event_from_byte(buf[i])) {

            case EVENT_SUPERVISE_DOWN : // 'x' supervisor exiting: permanent failure
                return EVENT_MATCH_FAIL ;

            case EVENT_NORESTART : { // 'O' will not be restarted
                unsigned char phase_up = wantup == 2 ? !m->restart_done : wantup ;
                if (phase_up)
                    return EVENT_MATCH_FAIL ;
                continue ;
            }

            case EVENT_DOWN :       m->up = 0 ; m->ready = 0 ; break ;
            case EVENT_DOWN_READY : m->up = 0 ; m->ready = 1 ; break ;
            case EVENT_UP :         m->up = 1 ; m->ready = 0 ; break ;
            case EVENT_READY :      m->up = 1 ; m->ready = 1 ; break ;

            default : continue ; // supervise-up or unknown: not a service transition
        }

        if (wantup == 2 && !m->restart_done) {
            // restart phase 1: wait until fully down
            if (satisfied(m->up, m->ready, 0, 0))
                m->restart_done = 1 ;
            else
                continue ;
        }

        if (eval(m, wantup, wantready))
            return EVENT_MATCH_OK ;
    }

    return EVENT_MATCH_PENDING ;
}
