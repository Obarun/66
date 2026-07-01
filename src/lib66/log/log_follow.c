/*
 * log_follow.c
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

#include "66/constants.h"
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <regex.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/sse.h>
#include <oblibs/linux.h>
#include <oblibs/stream.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>
#include <oblibs/io.h>

#include <66/log.h>

typedef struct follow_s follow_t ;
struct follow_s {
    char const *name ;    // tag label
    char const *path ; // the "current" file to read (logdir/current, or a plain file)
    regex_t *re ;         // line filter, or 0
    strbuf pending ;      // partial trailing line kept between reads
    int fd ;              // fd of the current file being read, -1 if not open yet
    uint8_t is_logdir ;
    uint8_t withname ;
} ;

static void emit_one(follow_t *f, char *line, size_t llen)
{
    struct timespec ts ;
    size_t msgoff ;
    int type = log_line_key(line, llen, &ts, &msgoff) ;

    if (f->re) {
        line[llen] = 0 ;
        int nomatch = regexec(f->re, line, 0, 0, 0) ;
        line[llen] = '\n' ;
        if (nomatch)
            return ;
    }

    log_emit(line, llen, msgoff, (uint8_t)type, &ts, f->name, f->withname) ;
}

static void emit_pending(follow_t *f)
{
    strbuf *sb = &f->pending ;
    size_t start = 0 ;

    for (;;) {

        char *nl = memchr(sb->s + start, '\n', sb->len - start) ;
        if (!nl)
            break ;

        emit_one(f, sb->s + start, (size_t)(nl - (sb->s + start))) ;

        start = (size_t)(nl - sb->s) + 1 ;
    }

    if (start) {

        if (start < sb->len)
            memmove(sb->s, sb->s + start, sb->len - start) ;

        sb->len -= start ;
    }
}

static int drain(follow_t *f)
{
    if (f->fd < 0)
        return 1 ;

    char buf[4096] ;

    for (;;) {
        ssize_t r = io_read(f->fd, buf, sizeof(buf)) ;
        if (r < 0)
            return 0 ;
        if (!r)
            break ;

        if (!strbuf_catb(&f->pending, buf, (size_t)r))
            return 0 ;

        emit_pending(f) ;
    }

    return 1 ;
}

static int reopen_current(follow_t *f)
{
    if (f->fd >= 0) {
        close(f->fd) ;
        f->fd = -1 ;
    }

    f->pending.len = 0 ;

    ssize_t fd = io_open(f->path, O_RDONLY | O_CLOEXEC) ;
    if (fd < 0)
        return 0 ;

    f->fd = (int)fd ;
    return 1 ;
}

static void signal_cb(sse_watcher_t *w, void *data, int revents)
{
    (void)data ;
    (void)revents ;
    w->p->running = false ;
}

static void inotify_cb(sse_watcher_t *w, void *cbdata, int revents)
{
    follow_t *f = cbdata ;

    if (revents & SSE_ERROR) {
        log_warnu("inotify watcher") ;
        w->p->running = false ;
        return ;
    }

    if (!(revents & SSE_READ))
        return ;

    uint8_t rotated = 0, modified = 0 ;

    FOREACH_INOTIFY() {

        if (f->is_logdir) {

            if (event->len && !strcmp(event->name, SS_CURRENT)) {

                if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
                    rotated = 1 ;
                } else if (event->mask & IN_MODIFY) {
                    modified = 1 ;
                }
            }

        } else if (event->mask & IN_MODIFY)
            modified = 1 ;
    }

    if (modified && !rotated && !drain(f))
        goto err ;

    if (rotated) {
        // finish the archived current, then follow the new one from its start
        if (!drain(f) || !reopen_current(f) || !drain(f))
            goto err ;
    }

    if (!ostream_flush(ostream_1))
        goto err ;

    return ;

 err:
    log_warnusys("follow log: ", f->name) ;
    w->p->running = false ;
}

int log_follow(char const *name, char const *path, uint8_t is_logdir, uint8_t withname, regex_t *re)
{
    follow_t f = { .name = name,
                   .path = path,
                   .re = re,
                   .pending = STRBUF_ZERO,
                   .fd = -1,
                   .is_logdir = is_logdir,
                   .withname = withname } ;

    char buf[strlen(path) + 1 + SS_CURRENT_LEN + 1] ;
    if (is_logdir) {
        auto_strings(buf, path, "/" , SS_CURRENT) ;
        f.path = buf ;
    }

    ssize_t fd = io_open(f.path, O_RDONLY | O_CLOEXEC) ;
    if (fd < 0) {

        if (!(is_logdir && errno == ENOENT))
            log_warnusys_return(LOG_EXIT_ZERO, "open: ", f.path) ;

        // a logdir whose 'current' does not exist yet: it appears on rotation

    } else {

        f.fd = (int)fd ;
        if (lseek(f.fd, 0, SEEK_END) == (off_t)-1) {
            close(f.fd) ;
            log_warnusys_return(LOG_EXIT_ZERO, "seek: ", f.path) ;
        }
    }

    sse_epoll_t loop = SSE_EPOLL_ZERO ;
    sse_watcher_t wsig = SSE_WATCHER_ZERO, wino = SSE_WATCHER_ZERO ;

    if (!sse_new(&loop, SSE_MAX_EVENTS)) {
        if (f.fd >= 0)
            close(f.fd) ;
        log_warnusys_return(LOG_EXIT_ZERO, "create event loop") ;
    }

    if (!sse_start_signal(&loop, &wsig, signal_cb, 0, 10)
     || !sse_ignore_signal(&wsig, SIGPIPE)
     || !sse_attach_signal(&wsig, SIGTERM)
     || !sse_attach_signal(&wsig, SIGINT))
        goto looperr ;

    if (!sse_start_inotify(&loop, &wino, inotify_cb, &f, 1))
        goto looperr ;

    // a logdir: watch the directory (new lines AND rotation of 'current');
    // a plain file: watch the file itself for new lines
    if (!sse_attach_inotify(&wino, path, is_logdir ? (IN_MODIFY | IN_CREATE | IN_MOVED_TO) : IN_MODIFY))
        goto looperr ;

    int r = sse_poll(&loop, SSE_TIMEOUT_INFINITE) ;

    ostream_flush(ostream_1) ;

    if (f.fd >= 0)
        close(f.fd) ;

    strbuf_free(&f.pending) ;
    sse_free(&loop) ;

    return r ;

 looperr:
    if (f.fd >= 0)
        close(f.fd) ;
    strbuf_free(&f.pending) ;
    sse_free(&loop) ;
    log_warnusys_return(LOG_EXIT_ZERO, "set up follow watchers") ;
}
