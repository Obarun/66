/*
 * service_resolve_get_limit_field.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
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
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/strbuf.h>

#include <66/service.h>
#include <66/enum_service.h>

int service_resolve_get_limit_field(strbuf *sa, resolve_service_addon_limit_t *l, resolve_service_enum_table_t table)
{
    log_flow() ;

    uint64_t v = 0 ;
    switch (table.id) {

        case E_RESOLVE_SERVICE_LIMIT_AS:        v = l->limitas ; break ;
        case E_RESOLVE_SERVICE_LIMIT_CORE:      v = l->limitcore ; break ;
        case E_RESOLVE_SERVICE_LIMIT_CPU:       v = l->limitcpu ; break ;
        case E_RESOLVE_SERVICE_LIMIT_DATA:      v = l->limitdata ; break ;
        case E_RESOLVE_SERVICE_LIMIT_FSIZE:     v = l->limitfsize ; break ;
        case E_RESOLVE_SERVICE_LIMIT_LOCKS:     v = l->limitlocks ; break ;
        case E_RESOLVE_SERVICE_LIMIT_MEMLOCK:   v = l->limitmemlock ; break ;
        case E_RESOLVE_SERVICE_LIMIT_MSGQUEUE:  v = l->limitmsgqueue ; break ;
        case E_RESOLVE_SERVICE_LIMIT_NICE:      v = l->limitnice ; break ;
        case E_RESOLVE_SERVICE_LIMIT_NOFILE:    v = l->limitnofile ; break ;
        case E_RESOLVE_SERVICE_LIMIT_NPROC:     v = l->limitnproc ; break ;
        case E_RESOLVE_SERVICE_LIMIT_RTPRIO:    v = l->limitrtprio ; break ;
        case E_RESOLVE_SERVICE_LIMIT_RTTIME:    v = l->limitrttime ; break ;
        case E_RESOLVE_SERVICE_LIMIT_SIGPENDING:v = l->limitsigpending ; break ;
        case E_RESOLVE_SERVICE_LIMIT_STACK:     v = l->limitstack ; break ;

        default:
            return 0 ;
    }

    char fmt[U64_FMT] ;
    fmt[u64_fmt(fmt, v)] = 0 ;

    return auto_strbuf(sa, fmt) ;
}
