/*
 * event_state.c
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

#include <stddef.h>

#include <oblibs/log.h>

#include <66/event.h>
#include <66/status.h>

static const struct { unsigned char up, ready ; } state_upready[] = {
    [STATUS_STATE_DOWN]       = { 0, 1 },
    [STATUS_STATE_STARTING]   = { 1, 0 },
    [STATUS_STATE_UP]         = { 1, 1 },
    [STATUS_STATE_STOPPING]   = { 1, 0 },
    [STATUS_STATE_FINISHING]  = { 0, 0 },
    [STATUS_STATE_RESTARTING] = { 0, 0 },
    [STATUS_STATE_DONE]       = { 0, 1 },
    [STATUS_STATE_FAILED]     = { 0, 1 }
} ;

/** adding a status_state_e forces a row here, or this fails to compile
_Static_assert(sizeof(state_upready) / sizeof(state_upready[0]) == STATUS_STATE_ENDOFKEY,
               "state_upready must have exactly one row per status_state_e") ;
*/

static void init_wanted(event_t wanted, unsigned char *wantup, unsigned char *wantready)
{
    switch (wanted) {
        case EVENT_UP :            *wantup = 1 ; *wantready = 0 ; break ;
        case EVENT_UP_READY :         *wantup = 1 ; *wantready = 1 ; break ;
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

static int isok(event_state_t *m, unsigned char wantup, unsigned char wantready)
{
    if (wantup == 2) {
        if (!m->restart_done)
            return 0 ;
        return satisfied(m->up, m->ready, 1, wantready) ;
    }
    return satisfied(m->up, m->ready, wantup, wantready) ;
}

static void state_set(uint8_t state, unsigned char *up, unsigned char *ready)
{
    if (state >= STATUS_STATE_ENDOFKEY) {
        *up = 0 ;
        *ready = 0 ;
        return ;
    } // corrupt frame

    *up = state_upready[state].up ;
    *ready = state_upready[state].ready ;
}

void event_state_init(event_state_t *m, event_t wanted, unsigned char up, unsigned char ready)
{
    log_flow() ;

    m->wanted = wanted ;
    m->up = up ? 1 : 0 ;
    m->ready = ready ? 1 : 0 ;
    m->restart_done = 0 ;
}

int event_state_update(event_state_t *m, event_frame_t const *f)
{
    log_flow() ;

    // a supervise-up/down wait matches the corresponding LIFECYCLE frame directly
    if (m->wanted == EVENT_SUPERVISE_UP || m->wanted == EVENT_SUPERVISE_DOWN) {

        if (f->kind != EVENT_KIND_LIFECYCLE)
            return EVENT_STATE_PENDING ;

        unsigned char want_up = m->wanted == EVENT_SUPERVISE_UP ;
        unsigned char phase_up = f->phase == EVENT_LIFECYCLE_UP ;
        return phase_up == want_up ? EVENT_STATE_OK : EVENT_STATE_PENDING ;
    }

    unsigned char wantup, wantready ;
    init_wanted(m->wanted, &wantup, &wantready) ;

    // already satisfied
    if (isok(m, wantup, wantready))
        return EVENT_STATE_OK ;

    // the supervisor exiting can never satisfy a service wait
    if (f->kind == EVENT_KIND_LIFECYCLE && f->phase == EVENT_LIFECYCLE_DOWN)
        return EVENT_STATE_FAIL ;

    // only a state transition moves (up, ready)
    if (f->kind != EVENT_KIND_TRANSITION)
        return EVENT_STATE_PENDING ;

    state_set(f->state, &m->up, &m->ready) ;

    if (wantup == 2 && !m->restart_done) {
        // restart phase 1: wait until fully down before watching for up
        if (satisfied(m->up, m->ready, 0, 0))
            m->restart_done = 1 ;
        else
            return EVENT_STATE_PENDING ;
    }

    if (isok(m, wantup, wantready))
        return EVENT_STATE_OK ;

    /* a terminal down (crash budget exhausted, or ./finish exit 125) can never
     * complete a wait that still needs the service up (a plain up wait, or either
     * phase of a restart); a down wait was already satisfied above. */
    if ((f->flags & EVENT_FLAG_TERMINAL) && wantup != 0)
        return EVENT_STATE_FAIL ;

    return EVENT_STATE_PENDING ;
}

int event_state_satisfied(event_state_t const *m)
{
    log_flow() ;

    if (m->wanted == EVENT_SUPERVISE_UP || m->wanted == EVENT_SUPERVISE_DOWN)
        return 0 ;

    unsigned char wantup, wantready ;
    init_wanted(m->wanted, &wantup, &wantready) ;

    event_state_t tmp = *m ; // isok reads restart_done; keep the caller's matcher intact
    return isok(&tmp, wantup, wantready) ? 1 : 0 ;
}
