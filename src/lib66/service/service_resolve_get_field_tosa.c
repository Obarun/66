/*
 * service_resolve_get_field_tosa.c
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

#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include <66/resolve.h>
#include <66/service.h>

int service_resolve_get_field_tosa(stralloc *sa, resolve_service_t *res, resolve_service_enum_t field)
{
    log_flow() ;

    char fmt[UINT32_FMT] ;
    char const *str = 0 ;

    switch(field) {

        case SERVICE_ENUM_NAME:
            str = res->sa.s + res->name ;
            break ;

        case SERVICE_ENUM_DESCRIPTION:
            str = res->sa.s + res->description ;
            break ;

        case SERVICE_ENUM_VERSION:
            str = res->sa.s + res->version ;
            break ;

        case SERVICE_ENUM_LOGGER:
            str = res->sa.s + res->logger ;
            break ;

        case SERVICE_ENUM_LOGREAL:
            str = res->sa.s + res->logreal ;
            break ;

        case SERVICE_ENUM_LOGASSOC:
            str = res->sa.s + res->logassoc ;
            break ;

        case SERVICE_ENUM_DSTLOG:
            str = res->sa.s + res->dstlog ;
            break ;

        case SERVICE_ENUM_DEPENDS:
            str = res->sa.s + res->depends ;
            break ;

        case SERVICE_ENUM_REQUIREDBY:
            str = res->sa.s + res->requiredby ;
            break ;

        case SERVICE_ENUM_OPTSDEPS:
            str = res->sa.s + res->optsdeps ;
            break ;

        case SERVICE_ENUM_EXTDEPS:
            str = res->sa.s + res->extdeps ;
            break ;

        case SERVICE_ENUM_CONTENTS:
            str = res->sa.s + res->contents ;
            break ;

        case SERVICE_ENUM_SRC:
            str = res->sa.s + res->src ;
            break ;

        case SERVICE_ENUM_SRCONF:
            str = res->sa.s + res->srconf ;
            break ;

        case SERVICE_ENUM_LIVE:
            str = res->sa.s + res->live ;
            break ;

        case SERVICE_ENUM_RUNAT:
            str = res->sa.s + res->runat ;
            break ;

        case SERVICE_ENUM_TREE:
            str = res->sa.s + res->tree ;
            break ;

        case SERVICE_ENUM_TREENAME:
            str = res->sa.s + res->treename ;
            break ;

        case SERVICE_ENUM_STATE:
            str = res->sa.s + res->state ;
            break ;

        case SERVICE_ENUM_EXEC_RUN:
            str = res->sa.s + res->exec_run ;
            break ;

        case SERVICE_ENUM_EXEC_LOG_RUN:
            str = res->sa.s + res->exec_log_run ;
            break ;

        case SERVICE_ENUM_REAL_EXEC_RUN:
            str = res->sa.s + res->real_exec_run ;
            break ;

        case SERVICE_ENUM_REAL_EXEC_LOG_RUN:
            str = res->sa.s + res->real_exec_log_run ;
            break ;

        case SERVICE_ENUM_EXEC_FINISH:
            str = res->sa.s + res->exec_finish ;
            break ;

        case SERVICE_ENUM_REAL_EXEC_FINISH:
            str = res->sa.s + res->real_exec_finish ;
            break ;

        case SERVICE_ENUM_TYPE:
            fmt[uint32_fmt(fmt,res->type)] = 0 ;
            str = fmt ;
            break ;

        case SERVICE_ENUM_NDEPENDS:
            fmt[uint32_fmt(fmt,res->ndepends)] = 0 ;
            str = fmt ;
            break ;

        case SERVICE_ENUM_NREQUIREDBY:
            fmt[uint32_fmt(fmt,res->nrequiredby)] = 0 ;
            str = fmt ;
            break ;

        case SERVICE_ENUM_NOPTSDEPS:
            fmt[uint32_fmt(fmt,res->noptsdeps)] = 0 ;
            str = fmt ;
            break ;

        case SERVICE_ENUM_NEXTDEPS:
            fmt[uint32_fmt(fmt,res->nextdeps)] = 0 ;
            str = fmt ;
            break ;

        case SERVICE_ENUM_NCONTENTS:
            fmt[uint32_fmt(fmt,res->ncontents)] = 0 ;
            str = fmt ;
            break ;

        case SERVICE_ENUM_DOWN:
            fmt[uint32_fmt(fmt,res->down)] = 0 ;
            str = fmt ;
            break ;

        case SERVICE_ENUM_DISEN:
            fmt[uint32_fmt(fmt,res->disen)] = 0 ;
            str = fmt ;
            break ;

        default:
            return 0 ;
    }

    if (!auto_stra(sa,str))
        return 0 ;

    return 1 ;
}
