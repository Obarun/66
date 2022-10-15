/*
 * write_classic.c
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

#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/service.h>
#include <66/write.h>

/* dst e.g. /var/lib/66/system/<tree>/servicedirs/svc/<name> */

void write_classic(resolve_service_t *res, char const *dst, uint8_t force)
{
    log_flow() ;

    /**notification,timeout, ... */
    write_common(res, dst) ;

    /** run file */
    write_execute_scripts("run", res->sa.s + res->execute.run.run, dst) ;

    /** finish file */
    if (res->execute.finish.run_user)
        write_execute_scripts("finish", res->sa.s + res->execute.finish.run, dst) ;

    /** run.user file */
    write_execute_scripts( "run.user", res->sa.s + res->execute.run.run_user, dst) ;

    /** finish.user file */
    if (res->execute.finish.run_user)
        write_execute_scripts("finish.user", res->sa.s + res->execute.finish.run_user, dst) ;

    /** logger */
    if (res->logger.name) {

        char destination[strlen(dst) + 1] ;

        if (!ob_dirname(destination, dst))
            log_dieu(LOG_EXIT_SYS, "get dirname of: ", dst) ;

        write_logger(res, destination, force) ;
    }

}
