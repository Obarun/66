/*
 * resolve_write_cdb.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/djbunix.h>
#include <skalibs/cdbmake.h>
#include <skalibs/posixplz.h>//unlink

#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>

int resolve_write_cdb(resolve_wrapper_t *wres, char const *file)
{
    log_flow() ;

    int fd ;
    size_t filelen = strlen(file);
    cdbmaker c = CDBMAKER_ZERO ;
    char tfile[filelen + 9] ;

    auto_strings(tfile, file, ":", "XXXXXX") ;

    fd = mkstemp(tfile) ;
    if (fd < 0 || ndelay_off(fd)) {
        log_warnusys("mkstemp: ", tfile) ;
        goto err_fd ;
    }

    if (!cdbmake_start(&c, fd)) {
        log_warnusys("cdbmake_start") ;
        goto err ;
    }

    if (wres->type == DATA_SERVICE) {

        if (!service_resolve_write_cdb(&c, ((resolve_service_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_TREE) {

        if (!tree_resolve_write_cdb(&c, ((resolve_tree_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_TREE_MASTER) {

        if (!tree_resolve_master_write_cdb(&c, ((resolve_tree_master_t *)wres->obj)))
            goto err ;

    }

    if (!cdbmake_finish(&c) || fsync(fd) < 0) {
        log_warnusys("write to: ", tfile) ;
        goto err ;
    }

    close(fd) ;

    if (rename(tfile, file) < 0) {
        log_warnusys("rename ", tfile, " to ", file) ;
        goto err_fd ;
    }

    return 1 ;

    err:
        close(fd) ;
    err_fd:
        unlink_void(tfile) ;
        return 0 ;
}
