/*
 * event_fifo.c
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
 * The FIFO SOURCE: drops our own subscriber fifo into a service event fifodir (s6
 * ftrig1 naming so s6-supervise's fanout writes to it) and pumps it through an
 * event_reader. We create the fifo under a hidden "." name, open both ends, then
 * rename it into place — so the producer never sees a visible fifo without a
 * reader. We hold the write end open so reads never see EOF. Only the fd
 * acquisition lives here; the read loop is the transport-agnostic pump.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
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

int event_fifo_subscribe(event_fifo_t *f, sse_epoll_t *ep, char const *eventdir, event_handler_t *handler, void *data, int priority)
{
    log_flow() ;

    memset(f, 0, sizeof(*f)) ;
    f->wfd = -1 ;

    // hidden name carries one extra leading '.' over the visible one
    if (strlen(eventdir) + 2 + EVENT_FIFO_NAMELEN + 1 > sizeof(f->fifopath)) {
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
        auto_strings(f->fifopath, eventdir, "/", EVENT_FIFO_PREFIX, stamp, ":", rnd) ;

        if (!mkfifo(hidden, 0622))
            break ;

        if (errno != EEXIST) {
            f->fifopath[0] = 0 ;
            log_warnusys_return(LOG_EXIT_ZERO, "create event fifo: ", hidden) ;
        }
    }

    /* read end first (nonblocking), then a write end so the fifo always has a
     * writer and reads return EAGAIN instead of EOF when idle. Once attached, the
     * read end is owned by the pump (event_reader_detach closes it). */
    int rfd = io_open(hidden, O_RDONLY | O_NONBLOCK | O_CLOEXEC) ;
    if (rfd < 0) {
        file_tryunlink(hidden) ;
        f->fifopath[0] = 0 ;
        log_warnusys_return(LOG_EXIT_ZERO, "open event fifo read end: ", hidden) ;
    }

    /* force the mode regardless of the umask so a same-group producer
     * (s6-supervise) can always open the fifo for writing */
    if (fchmod(rfd, 0622) < 0) {
        close_fd(rfd) ;
        file_tryunlink(hidden) ;
        f->fifopath[0] = 0 ;
        log_warnusys_return(LOG_EXIT_ZERO, "set event fifo mode: ", hidden) ;
    }

    f->wfd = io_open(hidden, O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
    if (f->wfd < 0) {
        close_fd(rfd) ;
        file_tryunlink(hidden) ;
        f->fifopath[0] = 0 ;
        log_warnusys_return(LOG_EXIT_ZERO, "open event fifo write end: ", hidden) ;
    }

    /* both ends are open: publish the fifo for the producer's fanout */
    if (rename(hidden, f->fifopath) < 0) {
        close_fd(rfd) ;
        close_fd(f->wfd) ; f->wfd = -1 ;
        file_tryunlink(hidden) ;
        f->fifopath[0] = 0 ;
        log_warnusys_return(LOG_EXIT_ZERO, "publish event fifo: ", f->fifopath) ;
    }

    // hand the read end to the pump; it owns it from here (closed by detach)
    if (!event_reader_attach(&f->reader, ep, rfd, handler, data, priority)) {
        close_fd(rfd) ;
        close_fd(f->wfd) ; f->wfd = -1 ;
        file_tryunlink(f->fifopath) ;
        f->fifopath[0] = 0 ;
        log_warnusys_return(LOG_EXIT_ZERO, "attach event fifo reader") ;
    }

    return 1 ;
}

void event_fifo_unsubscribe(event_fifo_t *f)
{
    log_flow() ;

    if (!f)
        return ;

    // unlink first avoiding reaching ENXIO
    if (f->fifopath[0]) {
        file_tryunlink(f->fifopath) ;
        f->fifopath[0] = 0 ;
    }

    event_reader_detach(&f->reader) ;

    if (f->wfd >= 0) {
        close_fd(f->wfd) ;
        f->wfd = -1 ;
    }
}
