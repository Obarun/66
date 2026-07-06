/*
 * service_resolve_get_environ_field.c
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

int service_resolve_get_environ_field(strbuf *sa, resolve_service_addon_environ_t *e, resolve_service_enum_table_t table)
{
    log_flow() ;

    char fmt[U32_FMT] ;
    char const *str = 0 ;

    switch (table.id) {

        case E_RESOLVE_SERVICE_ENVIRON_ENV:
            str = e->sa.s + e->env ;
            break ;

        case E_RESOLVE_SERVICE_ENVIRON_ENVDIR:
            str = e->sa.s + e->envdir ;
            break ;

        case E_RESOLVE_SERVICE_ENVIRON_ENV_OVERWRITE:
            fmt[u32_fmt(fmt, e->env_overwrite)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_ENVIRON_IMPORTFILE:
            str = e->sa.s + e->importfile ;
            break ;

        case E_RESOLVE_SERVICE_ENVIRON_NIMPORTFILE:
            fmt[u32_fmt(fmt, e->nimportfile)] = 0 ;
            str = fmt ;
            break ;

        default:
            return 0 ;
    }

    return auto_strbuf(sa, str) ;
}
