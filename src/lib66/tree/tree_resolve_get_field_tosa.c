/*
 * tree_resolve_get_field_tosa.c
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

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/strbuf.h>

#include <66/tree.h>
#include <66/enum_tree.h>

int tree_resolve_get_field_tosa(strbuf *sa, resolve_tree_t *tres, resolve_tree_enum_table_t table)
{
    log_flow() ;

    char fmt[U32_FMT] ;
    char const *str = 0 ;

    switch(table.id) {

        case E_RESOLVE_TREE_NAME:
            str = tres->sa.s + tres->name ;
            break ;

        case E_RESOLVE_TREE_ENABLED:
            fmt[u32_fmt(fmt,tres->enabled)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_TREE_DEPENDS:
            str = tres->sa.s + tres->depends ;
            break ;

        case E_RESOLVE_TREE_REQUIREDBY:
            str = tres->sa.s + tres->requiredby ;
            break ;

        case E_RESOLVE_TREE_ALLOW:
            str = tres->sa.s + tres->allow ;
            break ;

        case E_RESOLVE_TREE_GROUPS:
            str = tres->sa.s + tres->groups ;
            break ;

        case E_RESOLVE_TREE_CONTENTS:
            str = tres->sa.s + tres->contents ;
            break ;

        case E_RESOLVE_TREE_NDEPENDS:
            fmt[u32_fmt(fmt,tres->ndepends)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_TREE_NREQUIREDBY:
            fmt[u32_fmt(fmt,tres->nrequiredby)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_TREE_NALLOW:
            fmt[u32_fmt(fmt,tres->nallow)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_TREE_NGROUPS:
            fmt[u32_fmt(fmt,tres->ngroups)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_TREE_NCONTENTS:
            fmt[u32_fmt(fmt,tres->ncontents)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_TREE_INIT:
            fmt[u32_fmt(fmt,tres->init)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_TREE_SUPERVISED:
            fmt[u32_fmt(fmt,tres->supervised)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_TREE_RVERSION:
            fmt[u32_fmt(fmt,tres->rversion)] = 0 ;
            str = fmt ;
            break ;

        default:
            return 0 ;
    }

    if (!auto_strbuf(sa,str))
        return 0 ;

    return 1 ;
}
