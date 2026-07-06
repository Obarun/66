/*
 * service_resolve_modify_field.c
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

#include <stdint.h>
#include <stdlib.h>//free

#include <oblibs/log.h>
#include <oblibs/types.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_service.h>

static void modify_config(resolve_service_t *res, char const *data, uint32_t field)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch (field) {

        case E_RESOLVE_SERVICE_CONFIG_RVERSION:
            res->rversion = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_NAME:
            res->name = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_DESCRIPTION:
            res->description = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_VERSION:
            res->version = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_TYPE:
            res->type = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_EARLIER:
            res->earlier = resolve_add_uint32(data) ; ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_COPYFROM:
            res->copyfrom = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_INTREE:
            res->intree = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_OWNERSTR:
            res->ownerstr = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_OWNER:
            res->owner = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_TREENAME:
            res->treename = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_USER:
            res->user = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_INNS:
            res->inns = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_ENABLED:
            res->enabled = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_ISLOG:
            res->islog = resolve_add_uint32(data) ;
            break ;

        default:
            break;
    }

    free(wres) ;
}

static void modify_path(resolve_service_t *res, char const *data, uint32_t field)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch (field) {

        case E_RESOLVE_SERVICE_PATH_HOME:
        res->path.home = resolve_add_string(wres, data) ;
        break ;

        case E_RESOLVE_SERVICE_PATH_FRONTEND:
            res->path.frontend = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_PATH_SERVICEDIR:
            res->path.servicedir = resolve_add_string(wres, data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;
}

static void modify_live(resolve_service_t *res, char const *data, uint32_t field)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch (field) {

        case E_RESOLVE_SERVICE_LIVE_LIVEDIR:
            res->live.livedir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_STATUS:
            res->live.status = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_SERVICEDIR:
            res->live.servicedir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_SCANDIR:
            res->live.scandir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_STATEDIR:
            res->live.statedir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_EVENTDIR:
            res->live.eventdir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_NOTIFDIR:
            res->live.notifdir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_SUPERVISEDIR:
            res->live.supervisedir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_FDHOLDERDIR:
            res->live.fdholderdir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_ONESHOTDDIR:
            res->live.oneshotddir = resolve_add_string(wres, data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;
}

void service_resolve_modify_field(resolve_service_t *res, resolve_service_enum_table_t table, char const *data)
{
    log_flow() ;

    switch(table.category) {

        case E_RESOLVE_SERVICE_CATEGORY_CONFIG:
            modify_config(res, data, table.id) ;
            break ;

        case E_RESOLVE_SERVICE_CATEGORY_PATH:
            modify_path(res, data, table.id) ;
            break ;

        case E_RESOLVE_SERVICE_CATEGORY_LIVE:
            modify_live(res, data, table.id) ;
            break ;

        default:
            break ;
    }

    service_resolve_sanitize(res) ;
}
