/*
 * service_resolve_modify_dependencies_field.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdint.h>
#include <stdlib.h> // free

#include <oblibs/log.h>
#include <oblibs/types.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_service.h>

void service_resolve_modify_dependencies_field(resolve_service_addon_dependencies_t *dep, resolve_service_enum_table_t table, char const *data)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, dep) ;

    switch (table.id) {

        case E_RESOLVE_SERVICE_DEPS_DEPENDS:
            dep->depends = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_REQUIREDBY:
            dep->requiredby = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_OPTSDEPS:
            dep->optsdeps = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_CONTENTS:
            dep->contents = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_PROVIDE:
            dep->provide = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_CONFLICT:
            dep->conflict = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NDEPENDS:
            dep->ndepends = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NREQUIREDBY:
            dep->nrequiredby = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NOPTSDEPS:
            dep->noptsdeps = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NCONTENTS:
            dep->ncontents = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NPROVIDE:
            dep->nprovide = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NCONFLICT:
            dep->nconflict = resolve_add_uint32(data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;

    service_resolve_sanitize_addon_dependencies(dep) ;
}
