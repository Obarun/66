/*
 * service_resolve_get_event_field.c
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

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/strbuf.h>

#include <66/service.h>
#include <66/enum_service.h>

int service_resolve_get_event_field(strbuf *sa, resolve_service_addon_event_t *ev, resolve_service_enum_table_t table)
{
    log_flow() ;

    char fmt[U32_FMT] ;
    char const *str = 0 ;

    switch (table.id) {

        case E_RESOLVE_SERVICE_EVENT_FROM:
            str = ev->sa.s + ev->from ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_ON:
            str = ev->sa.s + ev->on ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_EMIT:
            str = ev->sa.s + ev->emit ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_WATCH:
            str = ev->sa.s + ev->watch ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_EXPRESSION:
            str = ev->sa.s + ev->expression ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_TIMEZONE:
            str = ev->sa.s + ev->timezone ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_TYPE:
            fmt[u32_fmt(fmt, ev->type)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_NFROM:
            fmt[u32_fmt(fmt, ev->nfrom)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_FROMFIELD:
            fmt[u32_fmt(fmt, ev->fromfield)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_NON:
            fmt[u32_fmt(fmt, ev->non)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_COMBINE:
            fmt[u32_fmt(fmt, ev->combine)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_DO:
            fmt[u32_fmt(fmt, ev->docmd)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EVENT_INTERVAL:
            fmt[u32_fmt(fmt, ev->interval)] = 0 ;
            str = fmt ;
            break ;

        default:
            return 0 ;
    }

    return auto_strbuf(sa, str) ;
}
