/*
 * tree_resolve_field_tosa.c
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

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/types.h>//UINT_FMT

#include <66/tree.h>
#include <66/resolve.h>

int tree_resolve_field_tosa(stralloc *sa, resolve_tree_t *tres, resolve_tree_enum_t field)
{
    log_flow() ;

    char fmt[UINT32_FMT] ;
    char const *str = 0 ;

    switch(field) {

        case TREE_ENUM_NAME:
            str = tres->sa.s + tres->name ;
            break ;

        case TREE_ENUM_DEPENDS:
            str = tres->sa.s + tres->depends ;
            break ;

        case TREE_ENUM_REQUIREDBY:
            str = tres->sa.s + tres->requiredby ;
            break ;

        case TREE_ENUM_ALLOW:
            str = tres->sa.s + tres->allow ;
            break ;

        case TREE_ENUM_GROUPS:
            str = tres->sa.s + tres->groups ;
            break ;

        case TREE_ENUM_CONTENTS:
            str = tres->sa.s + tres->contents ;
            break ;

        case TREE_ENUM_NDEPENDS:
            fmt[uint32_fmt(fmt,tres->ndepends)] = 0 ;
            str = fmt ;
            break ;

        case TREE_ENUM_NREQUIREDBY:
            fmt[uint32_fmt(fmt,tres->nrequiredby)] = 0 ;
            str = fmt ;
            break ;

        case TREE_ENUM_NALLOW:
            fmt[uint32_fmt(fmt,tres->nallow)] = 0 ;
            str = fmt ;
            break ;

        case TREE_ENUM_NGROUPS:
            fmt[uint32_fmt(fmt,tres->ngroups)] = 0 ;
            str = fmt ;
            break ;

        case TREE_ENUM_NCONTENTS:
            fmt[uint32_fmt(fmt,tres->ncontents)] = 0 ;
            str = fmt ;
            break ;

        case TREE_ENUM_INIT:
            fmt[uint32_fmt(fmt,tres->init)] = 0 ;
            str = fmt ;
            break ;

        case TREE_ENUM_DISEN:
            fmt[uint32_fmt(fmt,tres->disen)] = 0 ;
            str = fmt ;
            break ;

        default:
            return 0 ;
    }

    if (!auto_stra(sa,str))
        return 0 ;

    return 1 ;
}
