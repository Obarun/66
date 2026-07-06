/*
 * service_resolve_get_dependencies_field.c
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

int service_resolve_get_dependencies_field(strbuf *sa, resolve_service_addon_dependencies_t *dep, resolve_service_enum_table_t table)
{
    log_flow() ;

    char fmt[U32_FMT] ;
    char const *str = 0 ;

    switch (table.id) {

        case E_RESOLVE_SERVICE_DEPS_DEPENDS:
            str = dep->sa.s + dep->depends ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_REQUIREDBY:
            str = dep->sa.s + dep->requiredby ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_OPTSDEPS:
            str = dep->sa.s + dep->optsdeps ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_CONTENTS:
            str = dep->sa.s + dep->contents ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_PROVIDE:
            str = dep->sa.s + dep->provide ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_CONFLICT:
            str = dep->sa.s + dep->conflict ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NDEPENDS:
            fmt[u32_fmt(fmt, dep->ndepends)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NREQUIREDBY:
            fmt[u32_fmt(fmt, dep->nrequiredby)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NOPTSDEPS:
            fmt[u32_fmt(fmt, dep->noptsdeps)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NCONTENTS:
            fmt[u32_fmt(fmt, dep->ncontents)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NPROVIDE:
            fmt[u32_fmt(fmt, dep->nprovide)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NCONFLICT:
            fmt[u32_fmt(fmt, dep->nconflict)] = 0 ;
            str = fmt ;
            break ;

        default:
            return 0 ;
    }

    return auto_strbuf(sa, str) ;
}
