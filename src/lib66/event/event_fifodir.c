/*
 * event_fifodir.c
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
 * Creating and cleaning a service event fifodir, in plain libc. Replaces s6's
 * ftrigw_fifodir_make / ftrigw_clean (which dragged in skalibs). The directory
 * is owned by the producer (s6-supervise) at runtime; 66 only pre-creates it and
 * sweeps stale subscriber fifos.
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/fd.h>

#include <66/event.h>

int event_fifodir_make(char const *path, gid_t gid)
{
    log_flow() ;

    mode_t m = umask(0) ;

    if (mkdir(path, 0700) < 0) {

        struct stat st ;
        umask(m) ;

        if (errno != EEXIST)
            log_warnusys_return(LOG_EXIT_ZERO, "create directory: ", path) ;

        if (stat(path, &st) < 0)
            log_warnusys_return(LOG_EXIT_ZERO, "stat directory: ", path) ;

        if (st.st_uid != getuid()) {
            errno = EACCES ;
            log_warnusys_return(LOG_EXIT_ZERO, "directory not owned by us: ", path) ;
        }

        if (!S_ISDIR(st.st_mode)) {
            errno = ENOTDIR ;
            log_warnusys_return(LOG_EXIT_ZERO, "not a directory: ", path) ;
        }

        /* already there and ours: leave its permissions untouched */
        return 1 ;
    }
    umask(m) ;

    if (gid != (gid_t)-1 && chown(path, (uid_t)-1, gid) < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "chown directory: ", path) ;

    if (chmod(path, gid != (gid_t)-1 ? 03730 : 01733) < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "chmod directory: ", path) ;

    return 1 ;
}

int event_fifodir_clean(char const *path)
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
        if (!d)
            break ;

        if (strncmp(d->d_name, EVENT_FIFO_PREFIX, EVENT_FIFO_PREFIXLEN))
            continue ;
        if (strlen(d->d_name) != EVENT_FIFO_NAMELEN)
            continue ;

        memcpy(tmp + pathlen + 1, d->d_name, EVENT_FIFO_NAMELEN + 1) ;

        /* a subscriber fifo with no reader left rejects an O_WRONLY|O_NONBLOCK
         * open with ENXIO; that is the orphan we sweep. */
        int fd = open(tmp, O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
        if (fd >= 0)
            close_fd(fd) ;
        else if (errno == ENXIO && unlink(tmp) < 0)
            e = errno ;
    }

    if (errno)
        e = errno ;

    closedir(dir) ;

    if (e) {
        errno = e ;
        log_warnusys_return(LOG_EXIT_ZERO, "clean directory: ", path) ;
    }

    return 1 ;
}
