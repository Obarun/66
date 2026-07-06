/*
 * service_resolve_modify_regex_field.c
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

void service_resolve_modify_regex_field(resolve_service_addon_regex_t *rx, resolve_service_enum_table_t table, char const *data)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_REGEX, rx) ;

    switch (table.id) {

        case E_RESOLVE_SERVICE_REGEX_CONFIGURE:
            rx->configure = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_DIRECTORIES:
            rx->directories = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_FILES:
            rx->files = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_INFILES:
            rx->infiles = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_NDIRECTORIES:
            rx->ndirectories = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_NFILES:
            rx->nfiles = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_NINFILES:
            rx->ninfiles = resolve_add_uint32(data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;

    service_resolve_sanitize_addon_regex(rx) ;
}
