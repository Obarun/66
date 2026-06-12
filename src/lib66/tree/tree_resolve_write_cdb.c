/*
 * tree_resolve_write_cdb.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/cdb.h>

#include <66/tree.h>
#include <66/resolve.h>
#include <66/config.h>

static void add_version(resolve_tree_t *tres)
{
    log_flow() ;
    log_trace("resolve file version for: ", tres->sa.s + tres->name, " set to: ", SS_VERSION) ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, tres) ;
    tres->rversion = resolve_add_string(wres, SS_VERSION) ;
    free(wres) ;
}

int tree_resolve_write_cdb(ocdbmaker *c, resolve_tree_t *tres)
{
    log_flow() ;

    add_version(tres) ;

    if (!ocdb_make_add(c, "sa", 2, tres->sa.s, tres->sa.len))
        return 0 ;

    /* name */
    if (!resolve_add_cdb_uint(c, "rversion", tres->rversion) ||
        !resolve_add_cdb_uint(c, "name", tres->name) ||
        !resolve_add_cdb_uint(c, "enabled", tres->enabled) ||
        !resolve_add_cdb_uint(c, "depends", tres->depends) ||
        !resolve_add_cdb_uint(c, "requiredby", tres->requiredby) ||
        !resolve_add_cdb_uint(c, "allow", tres->allow) ||
        !resolve_add_cdb_uint(c, "groups", tres->groups) ||
        !resolve_add_cdb_uint(c, "contents", tres->contents) ||
        !resolve_add_cdb_uint(c, "ndepends", tres->ndepends) ||
        !resolve_add_cdb_uint(c, "nrequiredby", tres->nrequiredby) ||
        !resolve_add_cdb_uint(c, "nallow", tres->nallow) ||
        !resolve_add_cdb_uint(c, "ngroups", tres->ngroups) ||
        !resolve_add_cdb_uint(c, "ncontents", tres->ncontents) ||
        !resolve_add_cdb_uint(c, "init", tres->init) ||
        !resolve_add_cdb_uint(c, "supervised", tres->supervised))
            return 0 ;

    return 1 ;
}
