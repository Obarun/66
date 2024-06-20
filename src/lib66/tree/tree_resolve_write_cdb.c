/*
 * tree_resolve_write_cdb.c
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

#include <oblibs/log.h>

#include <skalibs/cdbmake.h>

#include <66/tree.h>
#include <66/resolve.h>

int tree_resolve_write_cdb(cdbmaker *c, resolve_tree_t *tres)
{
    log_flow() ;

    if (!cdbmake_add(c, "sa", 2, tres->sa.s, tres->sa.len))
        return 0 ;

    /* name */
    if (!resolve_add_cdb_uint(c, "name", tres->name) ||
        !resolve_add_cdb_uint(c, "depends", tres->depends) ||
        !resolve_add_cdb_uint(c, "requiredby", tres->requiredby) ||
        !resolve_add_cdb_uint(c, "allow", tres->allow) ||
        !resolve_add_cdb_uint(c, "groups", tres->groups) ||
        !resolve_add_cdb_uint(c, "contents", tres->contents) ||
        !resolve_add_cdb_uint(c, "enabled", tres->enabled) ||
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
