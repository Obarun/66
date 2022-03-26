/*
 * service.c
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

#include <66/resolve.h>
#include <66/service.h>

int service_resolve_field_to_sa(stralloc *sa, resolve_service_t *res, resolve_service_enum_t field)
{
    log_flow() ;

    uint32_t ifield ;

    switch(field) {

        case SERVICE_ENUM_NAME:
            ifield = res->name ;
            break ;

        case SERVICE_ENUM_DESCRIPTION:
            ifield = res->description ;
            break ;

        case SERVICE_ENUM_VERSION:
            ifield = res->version ;
            break ;

        case SERVICE_ENUM_LOGGER:
            ifield = res->logger ;
            break ;

        case SERVICE_ENUM_LOGREAL:
            ifield = res->logreal ;
            break ;

        case SERVICE_ENUM_LOGASSOC:
            ifield = res->logassoc ;
            break ;

        case SERVICE_ENUM_DSTLOG:
            ifield = res->dstlog ;
            break ;

        case SERVICE_ENUM_DEPENDS:
            ifield = res->depends ;
            break ;

        case SERVICE_ENUM_REQUIREDBY:
            ifield = res->requiredby ;
            break ;

        case SERVICE_ENUM_OPTSDEPS:
            ifield = res->optsdeps ;
            break ;

        case SERVICE_ENUM_EXTDEPS:
            ifield = res->extdeps ;
            break ;

        case SERVICE_ENUM_CONTENTS:
            ifield = res->contents ;
            break ;

        case SERVICE_ENUM_SRC:
            ifield = res->src ;
            break ;

        case SERVICE_ENUM_SRCONF:
            ifield = res->srconf ;
            break ;

        case SERVICE_ENUM_LIVE:
            ifield = res->live ;
            break ;

        case SERVICE_ENUM_RUNAT:
            ifield = res->runat ;
            break ;

        case SERVICE_ENUM_TREE:
            ifield = res->tree ;
            break ;

        case SERVICE_ENUM_TREENAME:
            ifield = res->treename ;
            break ;

        case SERVICE_ENUM_STATE:
            ifield = res->state ;
            break ;

        case SERVICE_ENUM_EXEC_RUN:
            ifield = res->exec_run ;
            break ;

        case SERVICE_ENUM_EXEC_LOG_RUN:
            ifield = res->exec_log_run ;
            break ;

        case SERVICE_ENUM_REAL_EXEC_RUN:
            ifield = res->real_exec_run ;
            break ;

        case SERVICE_ENUM_REAL_EXEC_LOG_RUN:
            ifield = res->real_exec_log_run ;
            break ;

        case SERVICE_ENUM_EXEC_FINISH:
            ifield = res->exec_finish ;
            break ;

        case SERVICE_ENUM_REAL_EXEC_FINISH:
            ifield = res->real_exec_finish ;
            break ;

        case SERVICE_ENUM_TYPE:
            ifield = res->type ;
            break ;

        case SERVICE_ENUM_NDEPENDS:
            ifield = res->ndepends ;
            break ;

        case SERVICE_ENUM_NREQUIREDBY:
            ifield = res->nrequiredby ;
            break ;

        case SERVICE_ENUM_NOPTSDEPS:
            ifield = res->noptsdeps ;
            break ;

        case SERVICE_ENUM_NEXTDEPS:
            ifield = res->nextdeps ;
            break ;

        case SERVICE_ENUM_NCONTENTS:
            ifield = res->ncontents ;
            break ;

        case SERVICE_ENUM_DOWN:
            ifield = res->down ;
            break ;

        case SERVICE_ENUM_DISEN:
            ifield = res->disen ;
            break ;

        default:
            return 0 ;
    }

    if (!auto_stra(sa,res->sa.s + ifield))
        return 0 ;

    return 1 ;
}
