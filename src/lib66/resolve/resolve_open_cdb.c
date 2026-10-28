/*
 * resolve_open_cdb.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <oblibs/fd.h>
#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/cdb.h>
#include <oblibs/io.h>

int resolve_open_cdb(int *fd, ocdb *c, const char *path, const char *name)
{
    log_flow() ;

    int err = errno ;
    char file[strlen(path) + strlen(name) + 1] ;

    errno = 0 ;

    auto_strings(file, path, name) ;

    (*fd) = io_open(file, O_RDONLY | O_CLOEXEC) ;
    if ((*fd) < 0) {

        if (errno == ENOENT)
            return 0 ;

        int e = errno ;
        log_warnusys("open: ", file) ;
        errno = e ;
        return -1 ;
    }

    errno = err ;

    if (!ocdb_init_fromfd(c, (*fd))) {
        /** mmap refuses a length of 0 with EINVAL, its only EINVAL for a
         * read-only shared map of a whole file: an empty file, which does not
         * read, not a fault of the system */
        int r = errno == EINVAL ? 0 : -1 ;
        log_warnusys("cdb_init: ", file) ;
        close_fd((*fd)) ;
        ocdb_free(c) ;
        return r ;
    }

    return 1 ;
}