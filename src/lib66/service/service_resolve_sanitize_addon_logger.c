/*
 * service_resolve_sanitize_addon_logger.c
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

#include <string.h>
#include <stdlib.h> // free

#include <oblibs/log.h>

#include <66/resolve.h>
#include <66/service.h>

void service_resolve_sanitize_addon_logger(resolve_service_addon_logger_t *lg)
{
    log_flow() ;

    char stk[lg->sa.len + 1] ;

    memcpy(stk, lg->sa.s, lg->sa.len) ;
    stk[lg->sa.len] = 0 ;

    lg->sa.len = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_LOGGER, lg) ;

    resolve_init(wres) ;

    // string fields only; backup/maxsize/timestamp and the timeouts are integers
    lg->rversion = lg->rversion ? resolve_add_string(wres, stk + lg->rversion) : 0 ;
    lg->execute.run.run = lg->execute.run.run ? resolve_add_string(wres, stk + lg->execute.run.run) : 0 ;
    lg->execute.run.run_user = lg->execute.run.run_user ? resolve_add_string(wres, stk + lg->execute.run.run_user) : 0 ;
    lg->execute.run.build = lg->execute.run.build ? resolve_add_string(wres, stk + lg->execute.run.build) : 0 ;
    lg->execute.run.runas = lg->execute.run.runas ? resolve_add_string(wres, stk + lg->execute.run.runas) : 0 ;

    free(wres) ;
}
