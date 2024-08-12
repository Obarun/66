/*
 * resolve_open_cdb.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/cdb.h>
#include <skalibs/djbunix.h>

int resolve_open_cdb(int *fd, cdb *c, const char *path, const char *name)
{
    log_flow() ;

    int err = errno ;
    char file[strlen(path) + strlen(name) + 1] ;

    errno = 0 ;

    auto_strings(file, path, name) ;

    (*fd) = open_readb(file) ;
    if ((*fd) < 0)
        log_warnusys_return(errno == ENOENT ? 0 : -1, "open: ",file) ;

    errno = err ;

    if (!cdb_init_fromfd(c, (*fd))) {
        log_warnusys("cdb_init: ", file) ;
        close((*fd)) ;
        cdb_free(c) ;
        return -1 ;
    }

    return 1 ;
}