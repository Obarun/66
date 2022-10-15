/*
 * service_resolve_master_get_field_tosa.c
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

#include <66/service.h>

int service_resolve_master_get_field_tosa(stralloc *sa, resolve_service_master_t *mres, resolve_service_master_enum_t field)
{
    log_flow() ;

    char fmt[UINT32_FMT] ;
    char const *str = 0 ;

    switch(field) {

        case E_RESOLVE_SERVICE_MASTER_NAME:
            str = mres->sa.s + mres->name ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_CLASSIC:
            str = mres->sa.s + mres->classic ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_BUNDLE:
            str = mres->sa.s + mres->bundle ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_ONESHOT:
            str = mres->sa.s + mres->oneshot ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_MODULE:
            str = mres->sa.s + mres->module ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_ENABLED:
            str = mres->sa.s + mres->enabled ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_DISABLED:
            str = mres->sa.s + mres->disabled ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_CONTENTS:
            str = mres->sa.s + mres->contents ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_NCLASSIC:
            fmt[uint32_fmt(fmt,mres->nclassic)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_NBUNDLE:
            fmt[uint32_fmt(fmt,mres->nbundle)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_NONESHOT:
            fmt[uint32_fmt(fmt,mres->noneshot)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_NMODULE:
            fmt[uint32_fmt(fmt,mres->nmodule)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_NENABLED:
            fmt[uint32_fmt(fmt,mres->nenabled)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_NDISABLED:
            fmt[uint32_fmt(fmt,mres->ndisabled)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_NCONTENTS:
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
