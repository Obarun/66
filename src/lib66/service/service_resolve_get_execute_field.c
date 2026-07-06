/*
 * service_resolve_get_execute_field.c
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

int service_resolve_get_execute_field(strbuf *sa, resolve_service_addon_execute_t *ex, resolve_service_enum_table_t table)
{
    log_flow() ;

    char fmt[U32_FMT] ;
    char const *str = 0 ;

    switch (table.id) {

        case E_RESOLVE_SERVICE_EXECUTE_RUN:
            str = ex->sa.s + ex->run.run ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_RUN_USER:
            str = ex->sa.s + ex->run.run_user ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_RUN_BUILD:
            str = ex->sa.s + ex->run.build ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_RUN_RUNAS:
            str = ex->sa.s + ex->run.runas ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_FINISH:
            str = ex->sa.s + ex->finish.run ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_FINISH_USER:
            str = ex->sa.s + ex->finish.run_user ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_FINISH_BUILD:
            str = ex->sa.s + ex->finish.build ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_FINISH_RUNAS:
            str = ex->sa.s + ex->finish.runas ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_TIMEOUTSTART:
            fmt[u32_fmt(fmt, ex->timeout.start)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_TIMEOUTSTOP:
            fmt[u32_fmt(fmt, ex->timeout.stop)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_DOWN:
            fmt[u32_fmt(fmt, ex->down)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_DOWNSIGNAL:
            fmt[u32_fmt(fmt, ex->downsignal)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_BLOCK_PRIVILEGES:
            fmt[u32_fmt(fmt, ex->blockprivileges)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_UMASK:
            fmt[u32_fmt(fmt, ex->umask)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_WANT_UMASK:
            fmt[u32_fmt(fmt, ex->want_umask)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_NICE:
            fmt[u32_fmt(fmt, ex->nice)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_WANT_NICE:
            fmt[u32_fmt(fmt, ex->want_nice)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_CHDIR:
            str = ex->sa.s + ex->chdir ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_CAPS_BOUND:
            str = ex->sa.s + ex->capsbound ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_CAPS_AMBIENT:
            str = ex->sa.s + ex->capsambient ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_NOTIFY:
            fmt[u32_fmt(fmt, ex->notify)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_MAXDEATH:
            fmt[u32_fmt(fmt, ex->maxdeath)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_MAXDEATHTIME:
            fmt[u32_fmt(fmt, ex->maxdeathtime)] = 0 ;
            str = fmt ;
            break ;

        default:
            return 0 ;
    }

    return auto_strbuf(sa, str) ;
}
