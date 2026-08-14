/*
 * service_resolve_modify_event_field.c
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

void service_resolve_modify_event_field(resolve_service_addon_event_t *ev, resolve_service_enum_table_t table, char const *data)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_EVENT, ev) ;

    switch (table.id) {

        case E_RESOLVE_SERVICE_EVENT_FROM:
            ev->from = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_ON:
            ev->on = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_EMIT:
            ev->emit = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_WATCH:
            ev->watch = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_EXPRESSION:
            ev->expression = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_TIMEZONE:
            ev->timezone = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_TYPE:
            ev->type = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_NFROM:
            ev->nfrom = resolve_add_uint32(data) ;
            break ;


        case E_RESOLVE_SERVICE_EVENT_NON:
            ev->non = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_COMBINE:
            ev->combine = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_DO:
            ev->docmd = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_INTERVAL:
            ev->interval = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_PROPAGATE:
            ev->propagate = resolve_add_uint32(data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;

    service_resolve_sanitize_addon_event(ev) ;
}
