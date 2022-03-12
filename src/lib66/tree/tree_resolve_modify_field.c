/*
 * tree_resolve_modify_field.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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

    [TREE_ENUM_NAME] = { .field = "name" },
    [TREE_ENUM_DEPENDS] = { .field = "depends" },
    [TREE_ENUM_REQUIREDBY] = { .field = "requiredby" },
    [TREE_ENUM_ALLOW] = { .field = "allow" },
    [TREE_ENUM_GROUPS] = { .field = "groups" },
    [TREE_ENUM_CONTENTS] = { .field = "contents" },
    [TREE_ENUM_NDEPENDS] = { .field = "ndepends" },
    [TREE_ENUM_NREQUIREDBY] = { .field = "nrequiredby" },
    [TREE_ENUM_NALLOW] = { .field = "nallow" },
    [TREE_ENUM_NGROUPS] = { .field = "ngroups" },
    [TREE_ENUM_NCONTENTS] = { .field = "ncontents" },
    [TREE_ENUM_INIT] = { .field = "init" },
    [TREE_ENUM_DISEN] = { .field = "disen" },
    [TREE_ENUM_ENDOFKEY] = { .field = 0 }
} ;

int tree_resolve_modify_field(resolve_tree_t *tres, uint8_t field, char const *data)
{
    log_flow() ;

    uint32_t ifield ;
    int e = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, tres) ;

    switch(field) {

        case TREE_ENUM_NAME:
            tres->name = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_DEPENDS:
            tres->depends = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_REQUIREDBY:
            tres->requiredby = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_ALLOW:
            tres->allow = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_GROUPS:
            tres->groups = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_CONTENTS:
            tres->contents = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_NDEPENDS:
            if (!uint0_scan(data, &ifield)) goto err ;
            tres->ndepends = ifield ;
            break ;

        case TREE_ENUM_NREQUIREDBY:
            if (!uint0_scan(data, &ifield)) goto err ;
            tres->nrequiredby = ifield ;
            break ;

        case TREE_ENUM_NALLOW:
            if (!uint0_scan(data, &ifield)) goto err ;
            tres->nallow = ifield ;
            break ;

        case TREE_ENUM_NGROUPS:
            if (!uint0_scan(data, &ifield)) goto err ;
            tres->ngroups = ifield ;
            break ;

        case TREE_ENUM_NCONTENTS:
            if (!uint0_scan(data, &ifield)) goto err ;
            tres->ncontents = ifield ;
            break ;

        case TREE_ENUM_INIT:
            if (!uint0_scan(data, &ifield)) goto err ;
            tres->init = ifield ;
            break ;

        case TREE_ENUM_DISEN:
            if (!uint0_scan(data, &ifield)) goto err ;
            tres->disen = ifield ;
            break ;

        default:
            break ;
    }

    e = 1 ;

    err:
        free(wres) ;
        return e ;

}
