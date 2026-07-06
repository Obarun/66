/*
 * resolve_modify_field_by.c
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

#include <oblibs/log.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>
#include <66/enum_service.h>
#include <66/enum_tree.h>

int resolve_modify_field_by(resolve_wrapper_t_ref wres, resolve_enum_table_t table, char const *by)
{
    log_flow() ;

    key_description_t const *list = enum_get_list(table) ;

    if (wres->type == DATA_SERVICE) {

        resolve_service_t_ref res = (resolve_service_t *)wres->obj  ;

        log_trace("store field ", list->name[table.u.service.id], " of service ", res->sa.s + res->name, " with value: ", by) ;

        service_resolve_modify_field(res, table.u.service, by) ;

        return 1 ;

    } else if (wres->type == DATA_SERVICE_LIMIT) {

        resolve_service_addon_limit_t *l = (resolve_service_addon_limit_t *)wres->obj ;

        log_trace("store limit field ", list->name[table.u.service.id], " with value: ", by) ;

        service_resolve_modify_limit_field(l, table.u.service, by) ;

        return 1 ;

    } else if (wres->type == DATA_SERVICE_ENVIRON) {

        resolve_service_addon_environ_t *e = (resolve_service_addon_environ_t *)wres->obj ;

        log_trace("store environ field ", list->name[table.u.service.id], " with value: ", by) ;

        service_resolve_modify_environ_field(e, table.u.service, by) ;

        return 1 ;

    } else if (wres->type == DATA_SERVICE_IO) {

        resolve_service_addon_io_t *io = (resolve_service_addon_io_t *)wres->obj ;

        log_trace("store io field ", list->name[table.u.service.id], " with value: ", by) ;

        service_resolve_modify_io_field(io, table.u.service, by) ;

        return 1 ;

    } else if (wres->type == DATA_SERVICE_LOGGER) {

        resolve_service_addon_logger_t *lg = (resolve_service_addon_logger_t *)wres->obj ;

        log_trace("store logger field ", list->name[table.u.service.id], " with value: ", by) ;

        service_resolve_modify_logger_field(lg, table.u.service, by) ;

        return 1 ;

    } else if (wres->type == DATA_TREE) {

        resolve_tree_t_ref res = (resolve_tree_t *)wres->obj  ;

        log_trace("store field ", list->name[table.u.tree.id], " of tree ", res->sa.s + res->name, " with value: ", by) ;

        tree_resolve_modify_field(res, table.u.tree.id, by) ;

        return 1 ;

    } else if (wres->type == DATA_TREE_MASTER) {

        resolve_tree_master_t_ref res = (resolve_tree_master_t *)wres->obj  ;

        log_trace("store field ", list->name[table.u.tree.id], " of resolve Master file of trees with value: ", by) ;

        tree_resolve_master_modify_field(res, table.u.tree.id, by) ;

        return 1 ;

    } else return 0 ;
}
