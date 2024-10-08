/*
 * tree_resolve_master_get_field_tosa.c
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

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>//UINT_FMT

#include <66/tree.h>

int tree_resolve_master_get_field_tosa(stralloc *sa, resolve_tree_master_t *mres, resolve_tree_master_enum_t field)
{
    log_flow() ;

    char fmt[UINT32_FMT] ;
    char const *str = 0 ;

    switch(field) {

        case E_RESOLVE_TREE_MASTER_NAME:
            str = mres->sa.s + mres->name ;
            break ;

        case E_RESOLVE_TREE_MASTER_ALLOW:
            str = mres->sa.s + mres->allow ;
            break ;

        case E_RESOLVE_TREE_MASTER_CURRENT:
            str = mres->sa.s + mres->current ;
            break ;

        case E_RESOLVE_TREE_MASTER_CONTENTS:
            str = mres->sa.s + mres->contents ;
            break ;

        case E_RESOLVE_TREE_MASTER_NALLOW:
            fmt[uint32_fmt(fmt,mres->nallow)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_TREE_MASTER_NCONTENTS:
            fmt[uint32_fmt(fmt,mres->ncontents)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_TREE_MASTER_RVERSION:
            fmt[uint32_fmt(fmt,mres->rversion)] = 0 ;
            str = fmt ;
            break ;

        default:
            return 0 ;
    }

    if (!auto_stra(sa,str))
        return 0 ;

    return 1 ;
}
