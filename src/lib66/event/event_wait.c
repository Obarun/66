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
 *
 * wait_and over a set of service event fifodirs, replacing ftrigr's
 * subscribe/wait_and. The caller subscribes (event_wait_init), triggers the
 * events (reloads the scandir), then blocks on event_wait_run.
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#include <oblibs/log.h>
#include <oblibs/sse.h>

#include <66/event.h>

static void event_wait_timeout_cb(sse_watcher_t *w, void *data, int revents)
{
    log_flow() ;

    (void)data ; (void)revents ;

    /* deadline hit before every reader saw its byte */
    w->p->running = false ;
}

int event_wait_init(event_wait_t *w, char const *const *eventdirs, size_t n, char wanted)
{
    log_flow() ;

    memset(w, 0, sizeof(*w)) ;
    w->n = n ;

    if (!sse_new(&w->epoll, n ? (uint32_t)n : 1))
        log_warnusys_return(LOG_EXIT_ZERO, "create event loop") ;

    if (!n)
        return 1 ;

    w->readers = malloc(n * sizeof(event_reader_t)) ;
    if (!w->readers) {
        sse_free(&w->epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "allocate event readers") ;
    }

    for (size_t i = 0 ; i < n ; i++) {

        if (!event_reader_subscribe(&w->readers[i], &w->epoll, eventdirs[i], wanted, w, 0)) {

            for (size_t j = 0 ; j < i ; j++)
                event_reader_unsubscribe(&w->readers[j]) ;

            free(w->readers) ; w->readers = NULL ;
            sse_free(&w->epoll) ;
            log_warnusys_return(LOG_EXIT_ZERO, "subscribe to event fifo: ", eventdirs[i]) ;
        }
    }

    return 1 ;
}

int event_wait_run(event_wait_t *w, int timeout_ms)
{
    log_flow() ;

    if (!w->n)
        return 1 ;   /* nothing to wait for */

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

    if (w->readers) {

        for (size_t i = 0 ; i < w->n ; i++)
            event_reader_unsubscribe(&w->readers[i]) ;

        free(w->readers) ;
        w->readers = NULL ;
    }

    sse_free(&w->epoll) ;
    w->n = 0 ;
    w->triggered = 0 ;
}
