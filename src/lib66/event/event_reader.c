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
 * Reads a service's event fifodir straight from the oblibs SSE loop, without the
 * ftrigr/s6-ftrigrd client layer. We drop our own fifo in the directory (s6
 * ftrig1 naming so s6-supervise's fanout writes to it), hold both ends open so
 * reads never see EOF, and scan the incoming bytes for the transition we want.
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/sse.h>
#include <oblibs/clock.h>
#include <oblibs/files.h>
#include <oblibs/fd.h>
#include <oblibs/io.h>
#include <oblibs/string.h>

#include <66/event.h>

static void event_reader_cb(sse_watcher_t *w, void *cbdata, int revents)
{
    log_flow() ;

    event_reader_t *r = cbdata ;

    if (revents & (SSE_ERROR | SSE_HUP)) {
        /* we hold the write end ourselves, so EOF/error is not expected */
        log_warnusys("event fifo watcher") ;
        return ;
    }

    if (r->got)
        return ;

    for (;;) {

        char buf[256] ;
        ssize_t n = io_read_result(io_read(w->fd, buf, sizeof(buf))) ;
        if (n < 0) {
            /* EPIPE means EOF, which cannot happen while we hold the write end */
            if (errno != EPIPE)
                log_warnusys("read event fifo") ;
            break ;
        }
        if (!n)
            break ;   /* would block: nothing more to read for now */

        for (ssize_t i = 0 ; i < n ; i++) {
            if (buf[i] == r->wanted) {
                r->got = 1 ;
                if (++r->owner->triggered == r->owner->n)
                    w->p->running = false ;
                return ;
            }
        }
    }
}

int event_reader_subscribe(event_reader_t *r, sse_epoll_t *ep, char const *eventdir, char wanted, event_wait_t *owner, int priority)
{
    log_flow() ;

    memset(r, 0, sizeof(*r)) ;
    r->wfd = -1 ;
    r->wanted = wanted ;
    r->owner = owner ;

    /* hidden name carries one extra leading '.' over the visible one */
    if (strlen(eventdir) + 2 + EVENT_FIFO_NAMELEN + 1 > sizeof(r->fifopath)) {
        errno = ENAMETOOLONG ;
        log_warnu_return(LOG_EXIT_ZERO, "fit the event fifo path for: ", eventdir) ;
    }

    struct timespec ts ;
    if (!clock_now(&ts))
        log_warnusys_return(LOG_EXIT_ZERO, "read wall clock") ;

    char stamp[CLOCK_TAI64N_LEN + 1] ;
    clock_tai64n_fmt(stamp, &ts) ;

    char hidden[SS_MAX_PATH] ;

    /* create the fifo under a hidden name (leading '.') so s6-supervise's fanout,
     * which filters on the visible "ftrig1:" prefix, never opens it before the
     * read end exists; publish it with a rename once both ends are open. Retry
     * with a fresh random suffix on a name clash; file_tmpname returns 0 if
     * getrandom fails, so a broken RNG breaks the loop instead of spinning. */
    for (;;) {

        char rnd[EVENT_FIFO_RANDLEN + 1] ;
        if (!file_tmpname(rnd, EVENT_FIFO_RANDLEN))
            log_warnusys_return(LOG_EXIT_ZERO, "generate event fifo name") ;
        rnd[EVENT_FIFO_RANDLEN] = 0 ;

        auto_strings(hidden, eventdir, "/.", EVENT_FIFO_PREFIX, stamp, ":", rnd) ;
        auto_strings(r->fifopath, eventdir, "/", EVENT_FIFO_PREFIX, stamp, ":", rnd) ;

        if (!mkfifo(hidden, 0622))
            break ;

        if (errno != EEXIST) {
            r->fifopath[0] = 0 ;
            log_warnusys_return(LOG_EXIT_ZERO, "create event fifo: ", hidden) ;
        }
    }

    /* read end first (nonblocking), then a write end so the fifo always has a
     * writer and reads return EAGAIN instead of EOF when idle. Once handed to the
     * watcher, the read end is owned by it (sse_free_io closes it). */
    int rfd = io_open(hidden, O_RDONLY | O_NONBLOCK | O_CLOEXEC) ;
    if (rfd < 0) {
        file_tryunlink(hidden) ;
        r->fifopath[0] = 0 ;
        log_warnusys_return(LOG_EXIT_ZERO, "open event fifo read end: ", hidden) ;
    }

    /* force the mode regardless of the umask so a same-group producer
     * (s6-supervise) can always open the fifo for writing */
    if (fchmod(rfd, 0622) < 0) {
        close_fd(rfd) ;
        file_tryunlink(hidden) ;
        r->fifopath[0] = 0 ;
        log_warnusys_return(LOG_EXIT_ZERO, "set event fifo mode: ", hidden) ;
    }

    r->wfd = io_open(hidden, O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
    if (r->wfd < 0) {
        close_fd(rfd) ;
        file_tryunlink(hidden) ;
        r->fifopath[0] = 0 ;
        log_warnusys_return(LOG_EXIT_ZERO, "open event fifo write end: ", hidden) ;
    }

    /* both ends are open: publish the fifo for the producer's fanout */
    if (rename(hidden, r->fifopath) < 0) {
        close_fd(rfd) ;
        close_fd(r->wfd) ; r->wfd = -1 ;
        file_tryunlink(hidden) ;
        r->fifopath[0] = 0 ;
        log_warnusys_return(LOG_EXIT_ZERO, "publish event fifo: ", r->fifopath) ;
    }

    if (!sse_start_io(ep, &r->watcher, event_reader_cb, r, rfd, SSE_READ, priority)) {
        close_fd(rfd) ;
        close_fd(r->wfd) ; r->wfd = -1 ;
        file_tryunlink(r->fifopath) ;
        r->fifopath[0] = 0 ;
        log_warnusys_return(LOG_EXIT_ZERO, "attach event fifo watcher") ;
    }

    return 1 ;
}

/*
 * Must not be called from within the event handler (it frees the very watcher
 * being dispatched). A handler that wants to stop reading should signal its
 * owner instead - e.g. set w->p->running = false.
 */
void event_reader_unsubscribe(event_reader_t *r)
{
    log_flow() ;

    if (!r)
        return ;

    sse_free_io(&r->watcher) ;   /* removes from the loop and closes the read end */

    if (r->wfd >= 0) {
        close_fd(r->wfd) ;
        r->wfd = -1 ;
    }

    if (r->fifopath[0]) {
        file_tryunlink(r->fifopath) ;
        r->fifopath[0] = 0 ;
    }

    r->got = 0 ;
    r->owner = NULL ;
}
