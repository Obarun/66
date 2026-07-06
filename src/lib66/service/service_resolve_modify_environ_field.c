/*
 * service_resolve_modify_environ_field.c
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

void service_resolve_modify_environ_field(resolve_service_addon_environ_t *e, resolve_service_enum_table_t table, char const *data)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_ENVIRON, e) ;

    switch (table.id) {

        case E_RESOLVE_SERVICE_ENVIRON_ENV:
            e->env = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_ENVIRON_ENVDIR:
            e->envdir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_ENVIRON_ENV_OVERWRITE:
            e->env_overwrite = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_ENVIRON_IMPORTFILE:
            e->importfile = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_ENVIRON_NIMPORTFILE:
            e->nimportfile = resolve_add_uint32(data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;

    service_resolve_sanitize_addon_environ(e) ;
}
