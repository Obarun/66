/*
 * service_resolve_modify_logger_field.c
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

void service_resolve_modify_logger_field(resolve_service_addon_logger_t *lg, resolve_service_enum_table_t table, char const *data)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_LOGGER, lg) ;

    switch (table.id) {

        case E_RESOLVE_SERVICE_LOGGER_LOGBACKUP:
            lg->backup = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGMAXSIZE:
            lg->maxsize = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGTIMESTAMP:
            lg->timestamp = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGRUN:
            lg->execute.run.run = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGRUN_USER:
            lg->execute.run.run_user = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGRUN_BUILD:
            lg->execute.run.build = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGRUN_RUNAS:
            lg->execute.run.runas = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGTIMEOUTSTART:
            lg->execute.timeout.start = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGTIMEOUTSTOP:
            lg->execute.timeout.stop = resolve_add_uint32(data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;

    service_resolve_sanitize_addon_logger(lg) ;
}
