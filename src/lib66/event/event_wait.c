/*
 * event_wait.c
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
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#include <oblibs/log.h>
#include <oblibs/sse.h>

#include <66/event.h>

struct wait_slot_s
{
    unsigned char done ; // this source has reached OK or FAIL: ignore its stream
    event_wait_t *w ;
    event_state_t s ; // per-source state interpreter
    event_aggregator_t ag ; // per-source frame reassembler
} ;
typedef struct wait_slot_s wait_slot_t ;

static void event_wait_timeout_cb(sse_watcher_t *w, void *data, int revents)
{
    log_flow() ;

    (void)data ; (void)revents ;

    w->p->running = false ;
}

static void event_wait_isok(event_frame_t const *f, void *data)
{
    log_flow() ;

    wait_slot_t *slot = data ;
    event_wait_t *w = slot->w ;

    if (slot->done)
        return ;

    int verdict = event_state_update(&slot->s, f) ;

    if (verdict == EVENT_STATE_OK) {
        slot->done = 1 ;
        if (++w->triggered == w->n)
            w->epoll.running = false ;
    } else if (verdict == EVENT_STATE_FAIL) {
        /* permanent failure (a terminal down while waiting up, or the supervisor
         * exiting): a wait_and can never complete, so end the wait now. w->failed
         * lets event_wait_run report failure distinctly from a timeout; the caller
         * reconciles against the real status. */
        slot->done = 1 ;
        w->failed = 1 ;
        w->epoll.running = false ;
    }
}

static void event_wait_handler(event_reader_t *r, char const *buf, size_t len, void *data)
{
    log_flow() ;

    (void)r ;

    wait_slot_t *slot = data ;

    if (slot->done)
        return ;

    event_aggregate(&slot->ag, buf, len, &event_wait_isok, slot) ;
}

int event_wait_init(event_wait_t *w, char const *const *eventdirs, size_t n, event_t wanted)
{
    log_flow() ;

    memset(w, 0, sizeof(*w)) ;
    w->n = n ;
    w->wanted = wanted ;

    if (!sse_new(&w->epoll, n ? (uint32_t)n : 1))
        log_warnusys_return(LOG_EXIT_ZERO, "create event loop") ;

    if (!n)
        return 1 ;

    w->fifos = malloc(n * sizeof(event_fifo_t)) ;
    if (!w->fifos) {
        sse_free(&w->epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "allocate event fifo sources") ;
    }

    wait_slot_t *slots = calloc(n, sizeof(wait_slot_t)) ;
    if (!slots) {
        free(w->fifos) ; w->fifos = NULL ;
        sse_free(&w->epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "allocate event wait slots") ;
    }
    w->slots = slots ;

    for (size_t i = 0 ; i < n ; i++) {

        slots[i].w = w ;
        slots[i].done = 0 ;
        event_state_init(&slots[i].s, wanted, 0, 0) ;

        if (!event_subscribe(&w->fifos[i], &w->epoll, eventdirs[i], &event_wait_handler, &slots[i], 0)) {

            for (size_t j = 0 ; j < i ; j++)
                event_unsubscribe(&w->fifos[j]) ;

            free(w->slots) ; w->slots = NULL ;
            free(w->fifos) ; w->fifos = NULL ;
            sse_free(&w->epoll) ;
            log_warnusys_return(LOG_EXIT_ZERO, "subscribe to event fifo: ", eventdirs[i]) ;
        }
    }

    return 1 ;
}

int event_wait_run(event_wait_t *w, int timeout_ms)
{
    log_flow() ;

    if (w->triggered == w->n)
        return 1 ;

    if (timeout_ms > 0 && sse_start_timer(&w->epoll, &w->timer, event_wait_timeout_cb, w, timeout_ms, 0, 0))
        w->timer_active = 1 ;

    int r = sse_poll(&w->epoll, SSE_TIMEOUT_INFINITE) ;

    if (w->timer_active) {
        sse_free_timer(&w->timer) ;
        w->timer_active = 0 ;
    }

    if (!r)
        log_warnusys_return(LOG_EXIT_LESSONE, "run event loop") ;

    return w->triggered == w->n ? 1 : 0 ;
}

void event_wait_free(event_wait_t *w)
{
    log_flow() ;

    if (w->fifos) {

        for (size_t i = 0 ; i < w->n ; i++)
            event_unsubscribe(&w->fifos[i]) ;

        free(w->fifos) ;
        w->fifos = NULL ;
    }

    free(w->slots) ;
    w->slots = NULL ;

    sse_free(&w->epoll) ;
    w->n = 0 ;
    w->triggered = 0 ;
}
