/*
 * tree_resolve_read_cdb.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdlib.h>//free
#include <errno.h>

#include <oblibs/log.h>

#include <skalibs/cdb.h>

#include <66/resolve.h>
#include <66/tree.h>

int tree_resolve_read_cdb(cdb *c, resolve_tree_t *tres)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, tres) ;

    if (resolve_get_sa(&tres->sa,c) <= 0) {
        free(wres) ;
        return (errno = EINVAL, 0)  ;
    }

    if (!tres->sa.len) {
        free(wres) ;
        return (errno = EINVAL, 0) ;
    }

    /* configuration */
    if (!resolve_get_key(c, "rversion", &tres->rversion)) {
        free(wres) ;
        return (errno = EINVAL, 0)  ;
    }

    if (!resolve_get_key(c, "name", &tres->name) ||
        !resolve_get_key(c, "enabled", &tres->enabled) ||
        !resolve_get_key(c, "depends", &tres->depends) ||
        !resolve_get_key(c, "requiredby", &tres->requiredby) ||
        !resolve_get_key(c, "allow", &tres->allow) ||
        !resolve_get_key(c, "groups", &tres->groups) ||
        !resolve_get_key(c, "contents", &tres->contents) ||
        !resolve_get_key(c, "ndepends", &tres->ndepends) ||
        !resolve_get_key(c, "nrequiredby", &tres->nrequiredby) ||
        !resolve_get_key(c, "nallow", &tres->nallow) ||
        !resolve_get_key(c, "ngroups", &tres->ngroups) ||
        !resolve_get_key(c, "ncontents", &tres->ncontents) ||
        !resolve_get_key(c, "init", &tres->init) ||
        !resolve_get_key(c, "supervised", &tres->supervised)) {
            free(wres) ;
            return (errno = EINVAL, 0)  ;
    }

    free(wres) ;

    return 1 ;
}
