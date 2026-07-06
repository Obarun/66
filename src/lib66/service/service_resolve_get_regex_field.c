/*
 * service_resolve_get_regex_field.c
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

int service_resolve_get_regex_field(strbuf *sa, resolve_service_addon_regex_t *rx, resolve_service_enum_table_t table)
{
    log_flow() ;

    char fmt[U32_FMT] ;
    char const *str = 0 ;

    switch (table.id) {

        case E_RESOLVE_SERVICE_REGEX_CONFIGURE:
            str = rx->sa.s + rx->configure ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_DIRECTORIES:
            str = rx->sa.s + rx->directories ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_FILES:
            str = rx->sa.s + rx->files ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_INFILES:
            str = rx->sa.s + rx->infiles ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_NDIRECTORIES:
            fmt[u32_fmt(fmt, rx->ndirectories)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_NFILES:
            fmt[u32_fmt(fmt, rx->nfiles)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_NINFILES:
            fmt[u32_fmt(fmt, rx->ninfiles)] = 0 ;
            str = fmt ;
            break ;

        default:
            return 0 ;
    }

    return auto_strbuf(sa, str) ;
}
