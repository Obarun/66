/*
 * service_resolve_modify_field.c
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
#include <stdlib.h>//free

#include <oblibs/log.h>

#include <66/resolve.h>
#include <66/service.h>

int service_resolve_modify_field(resolve_service_t *res, resolve_service_enum_t field, char const *data)
{
    log_flow() ;

    uint32_t ifield ;
    int e = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch(field) {

        case SERVICE_ENUM_NAME:
            res->name = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_DESCRIPTION:
            res->description = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_VERSION:
            res->version = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_LOGGER:
            res->logger = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_LOGREAL:
            res->logreal = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_LOGASSOC:
            res->logassoc = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_DSTLOG:
            res->dstlog = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_DEPENDS:
            res->depends = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_REQUIREDBY:
            res->requiredby = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_OPTSDEPS:
            res->optsdeps = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_EXTDEPS:
            res->extdeps = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_CONTENTS:
            res->contents = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_SRC:
            res->src = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_SRCONF:
            res->srconf = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_LIVE:
            res->live = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_RUNAT:
            res->runat = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_TREE:
            res->tree = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_TREENAME:
            res->treename = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_STATE:
            res->state = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_EXEC_RUN:
            res->exec_run = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_EXEC_LOG_RUN:
            res->exec_log_run = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_REAL_EXEC_RUN:
            res->real_exec_run = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_REAL_EXEC_LOG_RUN:
            res->real_exec_log_run = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_EXEC_FINISH:
            res->exec_finish = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_REAL_EXEC_FINISH:
            res->real_exec_finish = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_TYPE:
            if (!uint0_scan(data, &ifield)) goto err ;
            res->type = ifield ;
            break ;

        case SERVICE_ENUM_NDEPENDS:
            if (!uint0_scan(data, &ifield)) goto err ;
            res->ndepends = ifield ;
            break ;

        case SERVICE_ENUM_NREQUIREDBY:
            if (!uint0_scan(data, &ifield)) goto err ;
            res->nrequiredby = ifield ;
            break ;

        case SERVICE_ENUM_NOPTSDEPS:
            if (!uint0_scan(data, &ifield)) goto err ;
            res->noptsdeps = ifield ;
            break ;

        case SERVICE_ENUM_NEXTDEPS:
            if (!uint0_scan(data, &ifield)) goto err ;
            res->nextdeps = ifield ;
            break ;

        case SERVICE_ENUM_NCONTENTS:
            if (!uint0_scan(data, &ifield)) goto err ;
            res->ncontents = ifield ;
            break ;

        case SERVICE_ENUM_DOWN:
            if (!uint0_scan(data, &ifield)) goto err ;
            res->down = ifield ;
            break ;

        case SERVICE_ENUM_DISEN:
            if (!uint0_scan(data, &ifield)) goto err ;
            res->disen = ifield ;
            break ;

        default:
            break ;
    }

    e = 1 ;

    err:
        free(wres) ;
        return e ;

}
