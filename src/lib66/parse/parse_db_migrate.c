/*
 * parse_db_migrate.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/log.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/ssexec.h>

void parse_db_migrate(resolve_service_t *res, ssexec_t *info)
{
    log_flow() ;

    int r ;
    resolve_service_t ores = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref owres = resolve_set_struct(DATA_SERVICE, &ores) ;

    /** Try to open the old resolve file.
     * Do not crash if it does not find it. User
     * can use -f options even if it's the first parse
     * process of the module.*/
    r = resolve_read_g(owres, info->base.s, res->sa.s + res->name) ;
    if (r < 0) {

        log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", res->sa.s + res->name) ;

    } else if (r) {

        /* depends */
        service_db_migrate(&ores, res, info->base.s, 0) ;

        /* requiredby */
        service_db_migrate(&ores, res, info->base.s, 1) ;
    }
    resolve_free(owres) ;
}