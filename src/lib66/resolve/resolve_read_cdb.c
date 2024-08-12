/*
 * resolve_read_cdb.c
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

#include <unistd.h>

#include <oblibs/log.h>

#include <skalibs/cdb.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>

int resolve_read_cdb(resolve_wrapper_t *wres, char const *path, const char *name)
{
    log_flow() ;

    int fd, e = 0 ;
    cdb c = CDB_ZERO ;

    e = resolve_open_cdb(&fd, &c, path, name) ;
    if (e <= 0)
        return e ;

    e = 0 ;

    if (wres->type == DATA_SERVICE) {

        if (!service_resolve_read_cdb(&c, ((resolve_service_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_TREE){

        if (!tree_resolve_read_cdb(&c, ((resolve_tree_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_TREE_MASTER) {

        if (!tree_resolve_master_read_cdb(&c, ((resolve_tree_master_t *)wres->obj)))
            goto err ;
    }

    e = 1 ;

    err:
        close(fd) ;
        cdb_free(&c) ;
        return e ;
}
