/*
 * event_reader.c
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
 * The PUMP: owns one readable fd attached to the oblibs SSE loop, reads it as the
 * bytes arrive, and hands each raw chunk to a handler. It is the stable core of
 * the event engine: transport-agnostic (the fd may come from a fifo, a socket, an
 * fdholder hand-off) and format-agnostic (it never interprets the bytes). A source
 * closing surfaces as a len==0 handler call. The fifo-specific plumbing lives in
 * the source (event_fifo.c), never here.
 */

#include <errno.h>
#include <stddef.h>

#include <oblibs/log.h>
#include <oblibs/sse.h>
#include <oblibs/io.h>

#include <66/event.h>

static void event_reader_cb(sse_watcher_t *w, void *cbdata, int revents)
{
    log_flow() ;

    event_reader_t *r = cbdata ;

    // transport-level close/error: a socket peer hangs up, an fdholder fd dies.
    // On the fifo source this cannot fire (it holds the write end), but the pump
    // stays transport-neutral: report EOF (len 0) and let the consumer react.
    if (revents & (SSE_ERROR | SSE_HUP)) {
        r->handler(r, 0, 0, r->data) ;
        return ;
    }

    for (;;) {

        char buf[256] ;
        ssize_t n = io_read_result(io_read(w->fd, buf, sizeof(buf))) ;
        if (n < 0) {
            // EPIPE is EOF: the source closed (impossible while a write end is
            // held, i.e. on a fifo). Report it as a close; any other errno is a
            // real read failure, logged, the drain stops but the source stays.
            if (errno == EPIPE)
                r->handler(r, 0, 0, r->data) ;
            else
                log_warnusys("read event source") ;
            break ;
        }
        if (!n)
            break ; // would block: nothing more to read for now

        // hand the raw bytes to the consumer; interpretation (byte vs frame) is
        // its job, not ours
        r->handler(r, buf, (size_t)n, r->data) ;
    }
}

int event_reader_attach(event_reader_t *r, sse_epoll_t *ep, int fd, event_handler_t *handler, void *data, int priority)
{
    log_flow() ;

    r->handler = handler ;
    r->data = data ;

    if (!sse_start_io(ep, &r->watcher, event_reader_cb, r, fd, SSE_READ, priority))
        log_warnusys_return(LOG_EXIT_ZERO, "attach event reader watcher") ;

    return 1 ;
}

/*
 * Must not be called from within the handler (it frees the very watcher being
 * dispatched). A handler that wants to stop reading should clear the epoll's
 * running flag instead.
 */
void event_reader_detach(event_reader_t *r)
{
    log_flow() ;

    if (!r)
        return ;

    sse_free_io(&r->watcher) ;   // removes from the loop and closes the fd

    r->handler = NULL ;
    r->data = NULL ;
}
