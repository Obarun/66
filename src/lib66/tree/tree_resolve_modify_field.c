/*
 * tree_resolve_modify_field.c
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

#include <stdint.h>
#include <stdlib.h>//free

#include <oblibs/log.h>

#include <skalibs/types.h>

#include <66/resolve.h>
#include <66/tree.h>

resolve_field_table_t resolve_tree_field_table[] = {

    [E_RESOLVE_TREE_RVERSION] = { .field = "rversion" },

    [E_RESOLVE_TREE_NAME] = { .field = "name" },
    [E_RESOLVE_TREE_NAME] = { .field = "enabled" },
    [E_RESOLVE_TREE_DEPENDS] = { .field = "depends" },
    [E_RESOLVE_TREE_REQUIREDBY] = { .field = "requiredby" },
    [E_RESOLVE_TREE_ALLOW] = { .field = "allow" },
    [E_RESOLVE_TREE_GROUPS] = { .field = "groups" },
    [E_RESOLVE_TREE_CONTENTS] = { .field = "contents" },
    [E_RESOLVE_TREE_NDEPENDS] = { .field = "ndepends" },
    [E_RESOLVE_TREE_NREQUIREDBY] = { .field = "nrequiredby" },
    [E_RESOLVE_TREE_NALLOW] = { .field = "nallow" },
    [E_RESOLVE_TREE_NGROUPS] = { .field = "ngroups" },
    [E_RESOLVE_TREE_NCONTENTS] = { .field = "ncontents" },
    [E_RESOLVE_TREE_INIT] = { .field = "init" },
    [E_RESOLVE_TREE_SUPERVISED] = { .field = "supervised" },
    [E_RESOLVE_TREE_ENDOFKEY] = { .field = 0 }
} ;

static uint32_t resolve_add_uint(char const *data)
{
    uint32_t u ;

    if (!data)
        data = "0" ;
    if (!uint0_scan(data, &u))
        return 0 ;
    return u ;
}

void tree_resolve_modify_field(resolve_tree_t *tres, uint8_t field, char const *data)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, tres) ;

    switch(field) {

        case E_RESOLVE_TREE_RVERSION:
            tres->rversion = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_TREE_NAME:
            tres->name = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_TREE_ENABLED:

            tres->enabled = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_TREE_DEPENDS:
            tres->depends = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_TREE_REQUIREDBY:
            tres->requiredby = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_TREE_ALLOW:
            tres->allow = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_TREE_GROUPS:
            tres->groups = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_TREE_CONTENTS:
            tres->contents = resolve_add_string(wres,data) ;
            break ;

        case E_RESOLVE_TREE_NDEPENDS:

            tres->ndepends = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_TREE_NREQUIREDBY:
            tres->nrequiredby = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_TREE_NALLOW:
            tres->nallow = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_TREE_NGROUPS:
            tres->ngroups = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_TREE_NCONTENTS:
            tres->ncontents = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_TREE_INIT:
            tres->init = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_TREE_SUPERVISED:
            tres->supervised = resolve_add_uint(data) ;
            break ;

        default:
            break ;
    }

    tree_resolve_sanitize(tres) ;
    free(wres) ;
}
