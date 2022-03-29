/*
 * tree_resolve_master_get_field_tosa.c
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

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>//UINT_FMT

#include <66/tree.h>
#include <stdio.h>
int tree_resolve_master_get_field_tosa(stralloc *sa, resolve_tree_master_t *mres, resolve_tree_master_enum_t field)
{
    log_flow() ;

    char fmt[UINT32_FMT] ;
    char const *str = 0 ;

    switch(field) {

        case TREE_ENUM_MASTER_NAME:
            str = mres->sa.s + mres->name ;
            break ;

        case TREE_ENUM_MASTER_ALLOW:
            str = mres->sa.s + mres->allow ;
            break ;

        case TREE_ENUM_MASTER_ENABLED:
            str = mres->sa.s + mres->enabled ;
            break ;

        case TREE_ENUM_MASTER_CURRENT:
            str = mres->sa.s + mres->current ;
            break ;

        case TREE_ENUM_MASTER_CONTENTS:
            str = mres->sa.s + mres->contents ;
            break ;

        case TREE_ENUM_MASTER_NENABLED:
            fmt[uint32_fmt(fmt,mres->nenabled)] = 0 ;
            str = fmt ;
            break ;

        case TREE_ENUM_MASTER_NCONTENTS:
            fmt[uint32_fmt(fmt,mres->ncontents)] = 0 ;
            str = fmt ;
            break ;

        default:
            return 0 ;
    }

    if (!auto_stra(sa,str))
        return 0 ;

    return 1 ;
}
