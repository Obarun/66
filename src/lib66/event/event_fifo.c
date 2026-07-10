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
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/io.h>
#include <oblibs/fd.h>

#include <66/event.h>

int event_fifo_make(char const *path, gid_t gid)
{
    log_flow() ;

    mode_t m = umask(0) ;

    if (mkdir(path, 0700) < 0) {

        struct stat st ;
        umask(m) ;

        if (errno != EEXIST)
            log_warnusys_return(LOG_EXIT_ZERO, "create directory: ", path) ;

        // lstat, not stat: a symlink planted at path must be rejected, not followed
        if (lstat(path, &st) < 0)
            log_warnusys_return(LOG_EXIT_ZERO, "stat directory: ", path) ;

        if (st.st_uid != getuid()) {
            errno = EACCES ;
            log_warnusys_return(LOG_EXIT_ZERO, "directory not owned by us: ", path) ;
        }

        if (!S_ISDIR(st.st_mode)) {
            errno = ENOTDIR ;
            log_warnusys_return(LOG_EXIT_ZERO, "not a directory: ", path) ;
        }

    } else umask(m) ;

    if (gid != (gid_t)-1 && chown(path, (uid_t)-1, gid) < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "chown directory: ", path) ;

    if (chmod(path, gid != (gid_t)-1 ? 03730 : 01733) < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "chmod directory: ", path) ;

    return 1 ;
}

int event_fifo_clean(char const *path)
{
    log_flow() ;

    size_t pathlen = strlen(path) ;
    DIR *dir = opendir(path) ;
    if (!dir)
        log_warnusys_return(LOG_EXIT_ZERO, "open directory: ", path) ;

    int e = 0 ;
    char tmp[pathlen + 1 + EVENT_FIFO_NAMELEN + 1] ;
    memcpy(tmp, path, pathlen) ;
    tmp[pathlen] = '/' ;

    for (;;) {

        errno = 0 ;
        struct dirent *d = readdir(dir) ;
        if (!d) {
            if (errno && !e) e = errno ; // readdir failed (errno==0 means clean end)
            break ;
        }

        if (strncmp(d->d_name, EVENT_FIFO_PREFIX, EVENT_FIFO_PREFIXLEN))
            continue ;
        if (strlen(d->d_name) != EVENT_FIFO_NAMELEN)
            continue ;

        memcpy(tmp + pathlen + 1, d->d_name, EVENT_FIFO_NAMELEN + 1) ;

        /* an orphan fifo (no reader) rejects O_WRONLY|O_NONBLOCK with ENXIO: sweep
         * it. A live reader (open succeeds) or any other open error is left alone;
         * the sweep only fails on an unlink that itself fails. */
        int fd = io_open(tmp, O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
        if (fd >= 0)
            close_fd(fd) ;
        else if (errno == ENXIO && unlink(tmp) < 0 && !e)
            e = errno ; // capture the unlink errno right where it happens
    }

    closedir(dir) ;

    if (e) {
        errno = e ;
        log_warnusys_return(LOG_EXIT_ZERO, "clean directory: ", path) ;
    }

    return 1 ;
}

int event_fifo_notify(char const *path, char const *s, size_t len)
{
    log_flow() ;

    size_t pathlen = strlen(path) ;
    DIR *dir = opendir(path) ;
    if (!dir)
        log_warnusys_return(LOG_EXIT_ZERO, "open directory: ", path) ;

    int e = 0 ; // first error, deferred past closedir so the DIR is never leaked
    char tmp[pathlen + 1 + EVENT_FIFO_NAMELEN + 1] ;
    memcpy(tmp, path, pathlen) ;
    tmp[pathlen] = '/' ;

    for (;;) {

        errno = 0 ;
        struct dirent *d = readdir(dir) ;
        if (!d) {
            if (errno && !e) e = errno ;   // readdir failed (errno==0 means clean end)
            break ;
        }

        if (strncmp(d->d_name, EVENT_FIFO_PREFIX, EVENT_FIFO_PREFIXLEN))
            continue ;
        if (strlen(d->d_name) != EVENT_FIFO_NAMELEN)
            continue ;

        memcpy(tmp + pathlen + 1, d->d_name, EVENT_FIFO_NAMELEN + 1) ;

        /** Fan the message out: open the subscriber fifo non-blocking and write
         * @s into it. A fifo with no reader (ENXIO) or whose reader has gone
         * (EPIPE) is unlinked; a full fifo (EAGAIN) or any short write is dropped.
         * The producer NEVER blocks: a bad subscriber must not jam the supervisor.
         * Only a failing unlink (and opendir/readdir above) is a real error. */
        int fd = io_open(tmp, O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
        if (fd < 0) {
            if (errno == ENXIO && unlink(tmp) < 0 && !e)
                e = errno ;

            continue ;
        }

        ssize_t r = io_write(fd, (char *)s, len) ;
        if ((r < 0 || (size_t)r < len) && errno == EPIPE && unlink(tmp) < 0 && !e)
            e = errno ;

        close_fd(fd) ;
    }

    closedir(dir) ;

    if (e) {
        errno = e ;
        log_warnusys_return(LOG_EXIT_ZERO, "notify directory: ", path) ;
    }

    return 1 ;
}
