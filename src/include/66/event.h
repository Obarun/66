/*
 * event.h
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
 * Reads service transitions straight off the per-service event fifodir
 * (supervise/event), the broadcast fifodir mechanism s6 has always used: every
 * subscriber drops its own fifo in the directory and the producer (s6-supervise)
 * fans each transition byte out to all of them. This replaces the
 * ftrigr/s6-ftrigrd client layer - and the skalibs it dragged in - with a plain
 * libc + oblibs SSE reader. We read the native s6 transition bytes as-is; no
 * separate daemon, no regex, no IPC protocol.
 */

#ifndef SS_EVENT_H
#define SS_EVENT_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <oblibs/sse.h>
#include <oblibs/clock.h>

#include <66/config.h>

/*
 * Native s6-supervise transition bytes, emitted one byte at a time on a
 * service's supervise/event fifodir. The canonical service state s6 derives from
 * these is the (up, ready) pair.
 */
#define EVENT_S6_UP             'u'  // process up, not ready          (up=1 ready=0)
#define EVENT_S6_READY          'U'  // process up and ready           (up=1 ready=1)
#define EVENT_S6_DOWN           'd'  // finishing / down               (up=0 ready=0)
#define EVENT_S6_DOWN_READY     'D'  // fully down, ready to start      (up=0 ready=1)
#define EVENT_S6_NORESTART      'O'  // will not be restarted
#define EVENT_S6_SUPERVISE_UP   's'  // s6-supervise started
#define EVENT_S6_SUPERVISE_DOWN 'x'  // s6-supervise exiting

/*
 * Subscriber fifo naming: "ftrig1:" + TAI64N stamp + ":" + random suffix.
 * s6-supervise's fanout filters directory entries on the "ftrig1:" prefix and an
 * exact name length, so this layout must match it byte for byte.
 */
#define EVENT_FIFO_PREFIX     "ftrig1:"
#define EVENT_FIFO_PREFIXLEN  (sizeof(EVENT_FIFO_PREFIX) - 1)        /* 7 */
#define EVENT_FIFO_RANDLEN    6
#define EVENT_FIFO_NAMELEN    (EVENT_FIFO_PREFIXLEN + CLOCK_TAI64N_LEN + 1 + EVENT_FIFO_RANDLEN)  /* 39 */

typedef struct event_wait_s event_wait_t ;

/*
 * One subscriber fifo in a service event fifodir, read straight from the SSE
 * loop. Its address must stay stable between subscribe and unsubscribe: the loop
 * hashes &watcher and hands r back to the callback.
 */
typedef struct event_reader_s event_reader_t ;
struct event_reader_s
{
    sse_watcher_t watcher ;      // io watcher; owns the read-end fd (closed by sse_free_io)
    int wfd ;                    // write end held open so reads never see EOF
    char fifopath[SS_MAX_PATH] ; // our fifo, unlinked on unsubscribe (empty = none)
    char wanted ;                // s6 transition byte we are waiting for
    uint8_t got ;                // set once wanted has been seen
    event_wait_t *owner ;        // wait set this reader belongs to (for accounting)
} ;

/*
 * A wait_and over a set of service event fifodirs: blocks until every reader has
 * seen its wanted byte, or a deadline fires.
 */
struct event_wait_s
{
    sse_epoll_t epoll ;
    event_reader_t *readers ;    // n readers held at a stable address
    sse_watcher_t timer ;
    size_t n ;
    size_t triggered ;           // readers that have seen their byte
    int timer_active ;
} ;

/** Create a service event fifodir (replaces ftrigw_fifodir_make). mkdir 0700,
 * then if gid != -1 chown to it and chmod 03730 (sgid + group-write so a
 * same-group s6-supervise can fan out into it), else chmod 01733. An existing
 * directory owned by the caller is left as-is. Returns 1 on success, 0 on error
 * (errno set).
 */
extern int event_fifodir_make(char const *path, gid_t gid) ;

/** Remove orphan subscriber fifos from a fifodir (replaces ftrigw_clean): every
 * ftrig1 entry that no longer has a reader (opening it for writing yields ENXIO)
 * is unlinked. Returns 1 on success, 0 on error (errno set).
 */
extern int event_fifodir_clean(char const *path) ;

/** Drop our own fifo into eventdir (s6 ftrig1 naming so s6-supervise's fanout
 * writes to it) and attach its read end to ep. The fifo is created under a hidden
 * "." name and renamed into place only once both ends are open, so the producer
 * never sees it without a reader. Reports a match on wanted to owner. Returns 1
 * on success, 0 on failure (errno set). r MUST keep a stable address until
 * event_reader_unsubscribe.
 */
extern int event_reader_subscribe(event_reader_t *r, sse_epoll_t *ep, char const *eventdir, char wanted, event_wait_t *owner, int priority) ;

// Detach from the loop, close both ends, unlink our fifo. Not callable from the handler.
extern void event_reader_unsubscribe(event_reader_t *r) ;

/** Subscribe to every eventdir, each waiting for the byte wanted. The caller must
 * trigger the events (e.g. reload the scandir) AFTER this returns and BEFORE
 * event_wait_run, so the subscriptions are in place before the producer starts.
 * Returns 1 on success, 0 on failure (errno set).
 */
extern int event_wait_init(event_wait_t *w, char const *const *eventdirs, size_t n, char wanted) ;

/** Block until every subscribed reader has seen its byte (returns 1), the
 * deadline elapses (returns 0), or the loop errors (returns -1, errno set).
 * timeout_ms is the whole-wait deadline in milliseconds.
 */
extern int event_wait_run(event_wait_t *w, int timeout_ms) ;

// Unsubscribe every reader and release the loop.
extern void event_wait_free(event_wait_t *w) ;

#endif
