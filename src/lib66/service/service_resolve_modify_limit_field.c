/*
 * service_resolve_modify_limit_field.c
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
#include <oblibs/types.h>

#include <66/service.h>
#include <66/enum_service.h>

void service_resolve_modify_limit_field(resolve_service_addon_limit_t *l, resolve_service_enum_table_t table, char const *data)
{
    log_flow() ;

    uint64_t v = resolve_add_uint64(data) ;
    switch (table.id) {

        case E_RESOLVE_SERVICE_LIMIT_AS:        l->limitas = v ; break ;
        case E_RESOLVE_SERVICE_LIMIT_CORE:      l->limitcore = v ; break ;
        case E_RESOLVE_SERVICE_LIMIT_CPU:       l->limitcpu = v ; break ;
        case E_RESOLVE_SERVICE_LIMIT_DATA:      l->limitdata = v ; break ;
        case E_RESOLVE_SERVICE_LIMIT_FSIZE:     l->limitfsize = v ; break ;
        case E_RESOLVE_SERVICE_LIMIT_LOCKS:     l->limitlocks = v ; break ;
        case E_RESOLVE_SERVICE_LIMIT_MEMLOCK:   l->limitmemlock = v ; break ;
        case E_RESOLVE_SERVICE_LIMIT_MSGQUEUE:  l->limitmsgqueue = v ; break ;
        case E_RESOLVE_SERVICE_LIMIT_NICE:      l->limitnice = v ; break ;
        case E_RESOLVE_SERVICE_LIMIT_NOFILE:    l->limitnofile = v ; break ;
        case E_RESOLVE_SERVICE_LIMIT_NPROC:     l->limitnproc = v ; break ;
        case E_RESOLVE_SERVICE_LIMIT_RTPRIO:    l->limitrtprio = v ; break ;
        case E_RESOLVE_SERVICE_LIMIT_RTTIME:    l->limitrttime = v ; break ;
        case E_RESOLVE_SERVICE_LIMIT_SIGPENDING:l->limitsigpending = v ; break ;
        case E_RESOLVE_SERVICE_LIMIT_STACK:     l->limitstack = v ; break ;

        default:
            break ;
    }

    service_resolve_sanitize_addon_limit(l) ;
}
