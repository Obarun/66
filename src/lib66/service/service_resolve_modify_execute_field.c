/*
 * service_resolve_modify_execute_field.c
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

void service_resolve_modify_execute_field(resolve_service_addon_execute_t *ex, resolve_service_enum_table_t table, char const *data)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_EXECUTE, ex) ;

    switch (table.id) {

        case E_RESOLVE_SERVICE_EXECUTE_RUN:
            ex->run.run = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_RUN_USER:
            ex->run.run_user = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_RUN_BUILD:
            ex->run.build = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_RUN_RUNAS:
            ex->run.runas = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_FINISH:
            ex->finish.run = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_FINISH_USER:
            ex->finish.run_user = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_FINISH_BUILD:
            ex->finish.build = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_FINISH_RUNAS:
            ex->finish.runas = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_TIMEOUTSTART:
            ex->timeout.start = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_TIMEOUTSTOP:
            ex->timeout.stop = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_DOWN:
            ex->down = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_DOWNSIGNAL:
            ex->downsignal = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_BLOCK_PRIVILEGES:
            ex->blockprivileges = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_UMASK:
            ex->umask = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_WANT_UMASK:
            ex->want_umask = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_NICE:
            ex->nice = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_WANT_NICE:
            ex->want_nice = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_CHDIR:
            ex->chdir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_CAPS_BOUND:
            ex->capsbound = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_CAPS_AMBIENT:
            ex->capsambient = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_NOTIFY:
            ex->notify = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_MAXDEATH:
            ex->maxdeath = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_MAXDEATHTIME:
            ex->maxdeathtime = resolve_add_uint32(data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;

    service_resolve_sanitize_addon_execute(ex) ;
}
