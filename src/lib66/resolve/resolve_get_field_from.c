/*
 * resolve_get_field_from.c
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
#include <oblibs/strbuf.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>

int resolve_get_field_from(strbuf *sa, resolve_wrapper_t_ref wres, resolve_enum_table_t table)
{
    log_flow() ;

    if (wres->type == DATA_SERVICE) {

        resolve_service_t_ref res = (resolve_service_t *)wres->obj  ;

        return service_resolve_get_field_tosa(sa, res, table.u.service) ;

    } else if (wres->type == DATA_SERVICE_LIMIT) {

        resolve_service_addon_limit_t *l = (resolve_service_addon_limit_t *)wres->obj ;

        return service_resolve_get_limit_field(sa, l, table.u.service) ;

    } else if (wres->type == DATA_SERVICE_ENVIRON) {

        resolve_service_addon_environ_t *e = (resolve_service_addon_environ_t *)wres->obj ;

        return service_resolve_get_environ_field(sa, e, table.u.service) ;

    } else if (wres->type == DATA_SERVICE_IO) {

        resolve_service_addon_io_t *io = (resolve_service_addon_io_t *)wres->obj ;

        return service_resolve_get_io_field(sa, io, table.u.service) ;

    } else if (wres->type == DATA_SERVICE_LOGGER) {

        resolve_service_addon_logger_t *lg = (resolve_service_addon_logger_t *)wres->obj ;

        return service_resolve_get_logger_field(sa, lg, table.u.service) ;

    } else if (wres->type == DATA_SERVICE_EXECUTE) {

        resolve_service_addon_execute_t *ex = (resolve_service_addon_execute_t *)wres->obj ;

        return service_resolve_get_execute_field(sa, ex, table.u.service) ;

    } else if (wres->type == DATA_SERVICE_DEPENDENCIES) {

        resolve_service_addon_dependencies_t *dep = (resolve_service_addon_dependencies_t *)wres->obj ;

        return service_resolve_get_dependencies_field(sa, dep, table.u.service) ;

    } else if (wres->type == DATA_SERVICE_REGEX) {

        resolve_service_addon_regex_t *rx = (resolve_service_addon_regex_t *)wres->obj ;

        return service_resolve_get_regex_field(sa, rx, table.u.service) ;

    } else if (wres->type == DATA_TREE) {

        resolve_tree_t_ref res = (resolve_tree_t *)wres->obj  ;

        return tree_resolve_get_field_tosa(sa, res, table.u.tree) ;

    } else if (wres->type == DATA_TREE_MASTER) {

        resolve_tree_master_t_ref res = (resolve_tree_master_t *)wres->obj  ;

        return tree_resolve_master_get_field_tosa(sa, res, table.u.tree) ;

    } else return 0 ;
}
