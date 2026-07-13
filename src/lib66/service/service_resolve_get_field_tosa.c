/*
 * service_resolve_get_table_tosa.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <oblibs/types.h>
#include <oblibs/strbuf.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_service.h>

static int get_config(strbuf *sa, resolve_service_t *res, resolve_service_enum_config_t table)
{
    log_flow() ;

    char fmt[U32_FMT] ;
    char const *str = 0 ;
    int e = 0 ;

    switch(table) {

        case E_RESOLVE_SERVICE_CONFIG_RVERSION:
            fmt[u32_fmt(fmt,res->rversion)] = 0 ;
            str = fmt ;
        break ;

        case E_RESOLVE_SERVICE_CONFIG_NAME:
            str = res->sa.s + res->name ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_DESCRIPTION:
            str = res->sa.s + res->description ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_VERSION:
            str = res->sa.s + res->version ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_TYPE:
            fmt[u32_fmt(fmt,res->type)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_EARLIER:
            fmt[u32_fmt(fmt,res->earlier)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_COPYFROM:
            str = res->sa.s + res->copyfrom ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_INTREE:
            str = res->sa.s + res->intree ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_OWNERSTR:
            str = res->sa.s + res->ownerstr ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_OWNER:
            fmt[u32_fmt(fmt,res->owner)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_TREENAME:
            str = res->sa.s + res->treename ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_USER:
            str = res->sa.s + res->user ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_INNS:
            str = res->sa.s + res->inns ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_ENABLED:
            fmt[u32_fmt(fmt,res->enabled)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_ISLOG:
            fmt[u32_fmt(fmt,res->islog)] = 0 ;
            str = fmt ;
            break ;

        default:
            return e ;
    }

    if (!auto_strbuf(sa,str))
        return e ;

    e = 1 ;
    return e ;
}

static int get_path(strbuf *sa, resolve_service_t *res, resolve_service_enum_path_t table)
{
    log_flow() ;

    char const *str = 0 ;
    int e = 0 ;

    switch(table) {

        case E_RESOLVE_SERVICE_PATH_HOME:
            str = res->sa.s + res->path.home ;
            break ;

        case E_RESOLVE_SERVICE_PATH_FRONTEND:
            str = res->sa.s + res->path.frontend ;
            break ;

        case E_RESOLVE_SERVICE_PATH_SERVICEDIR:
            str = res->sa.s + res->path.servicedir ;
            break ;

        default:
            return e ;
    }

    if (!auto_strbuf(sa,str))
        return e ;

    e = 1 ;
    return e ;
}

static int get_live(strbuf *sa, resolve_service_t *res, resolve_service_enum_live_t table)
{
    log_flow() ;

    char const *str = 0 ;
    int e = 0 ;

    switch(table) {

        case E_RESOLVE_SERVICE_LIVE_LIVEDIR:
            str = res->sa.s + res->live.livedir ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_STATUS:
            str = res->sa.s + res->live.status ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_SERVICEDIR:
            str = res->sa.s + res->live.servicedir ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_SCANDIR:
            str = res->sa.s + res->live.scandir ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_STATEDIR:
            str = res->sa.s + res->live.statedir ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_EVENTDIR:
            str = res->sa.s + res->live.eventdir ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_SUPERVISEDIR:
            str = res->sa.s + res->live.supervisedir ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_FDHOLDERDIR:
            str = res->sa.s + res->live.fdholderdir ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_ONESHOTDDIR:
            str = res->sa.s + res->live.oneshotddir ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_EVENTDDIR:
            str = res->sa.s + res->live.eventddir ;
            break ;

        default:
            return e ;
    }

    if (!auto_strbuf(sa,str))
        return e ;

    e = 1 ;
    return e ;
}

int service_resolve_get_field_tosa(strbuf *sa, resolve_service_t *res, resolve_service_enum_table_t table)
{
    log_flow() ;

    switch(table.category) {

        case E_RESOLVE_SERVICE_CATEGORY_CONFIG :
            return get_config(sa, res, table.id) ;

        case E_RESOLVE_SERVICE_CATEGORY_PATH:
            return get_path(sa, res, table.id) ;

        case E_RESOLVE_SERVICE_CATEGORY_LIVE:
            return get_live(sa, res, table.id) ;

        default:
            return 0 ;
    }

    return 1 ;
}
