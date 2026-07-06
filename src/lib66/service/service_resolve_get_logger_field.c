/*
 * service_resolve_get_logger_field.c
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

int service_resolve_get_logger_field(strbuf *sa, resolve_service_addon_logger_t *lg, resolve_service_enum_table_t table)
{
    log_flow() ;

    char fmt[U32_FMT] ;
    char const *str = 0 ;

    switch (table.id) {

        case E_RESOLVE_SERVICE_LOGGER_LOGBACKUP:
            fmt[u32_fmt(fmt, lg->backup)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGMAXSIZE:
            fmt[u32_fmt(fmt, lg->maxsize)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGTIMESTAMP:
            fmt[u32_fmt(fmt, lg->timestamp)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGRUN:
            str = lg->sa.s + lg->execute.run.run ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGRUN_USER:
            str = lg->sa.s + lg->execute.run.run_user ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGRUN_BUILD:
            str = lg->sa.s + lg->execute.run.build ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGRUN_RUNAS:
            str = lg->sa.s + lg->execute.run.runas ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGTIMEOUTSTART:
            fmt[u32_fmt(fmt, lg->execute.timeout.start)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGTIMEOUTSTOP:
            fmt[u32_fmt(fmt, lg->execute.timeout.stop)] = 0 ;
            str = fmt ;
            break ;

        default:
            return 0 ;
    }

    return auto_strbuf(sa, str) ;
}
