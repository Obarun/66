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

        case SERVICE_ENUM_MASTER_NAME:
            str = mres->sa.s + mres->name ;
            break ;

        case SERVICE_ENUM_MASTER_CLASSIC:
            str = mres->sa.s + mres->classic ;
            break ;

        case SERVICE_ENUM_MASTER_LONGRUN:
            str = mres->sa.s + mres->longrun ;
            break ;

        case SERVICE_ENUM_MASTER_BUNDLE:
            str = mres->sa.s + mres->bundle ;
            break ;

        case SERVICE_ENUM_MASTER_ONESHOT:
            str = mres->sa.s + mres->oneshot ;
            break ;

        case SERVICE_ENUM_MASTER_MODULE:
            str = mres->sa.s + mres->module ;
            break ;

        case SERVICE_ENUM_MASTER_NCLASSIC:
            fmt[uint32_fmt(fmt,mres->nclassic)] = 0 ;
            str = fmt ;
            break ;

        case SERVICE_ENUM_MASTER_NBUNDLE:
            fmt[uint32_fmt(fmt,mres->nbundle)] = 0 ;
            str = fmt ;
            break ;

        case SERVICE_ENUM_MASTER_NLONGRUN:
            fmt[uint32_fmt(fmt,mres->nlongrun)] = 0 ;
            str = fmt ;
            break ;

        case SERVICE_ENUM_MASTER_NONESHOT:
            fmt[uint32_fmt(fmt,mres->noneshot)] = 0 ;
            str = fmt ;
            break ;

        case SERVICE_ENUM_MASTER_NMODULE:
            fmt[uint32_fmt(fmt,mres->nmodule)] = 0 ;
            str = fmt ;
            break ;

        default:
            return 0 ;
    }

    if (!auto_stra(sa,str))
        return 0 ;

    return 1 ;
}
