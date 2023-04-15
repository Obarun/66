/*
 * write_service.c
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



#include <string.h>
#include <stdint.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/enum.h>
#include <66/write.h>
#include <66/constants.h>
#include <66/sanitize.h>

/** @Return 0 on fail
 * @Return 1 on success
 * @Return 2 if the service is ignored
 *
 * @workdir -> /var/lib/66/system/<tree>/servicedirs/
 * */
void write_services(resolve_service_t *res, char const *workdir)
{
    log_flow() ;

    char *name = res->sa.s + res->name ;
    uint32_t type = res->type ;
    char logname = get_rstrlen_until(name, SS_LOG_SUFFIX) ;
    if (logname > 0)
        type = 4 ;

    /**
     *
     *
     * please pass through a temporary or backup
     *
     * just need to switch the /run/66/state/0/<service> symlink
     * with atomic_symlink
     *
     *
     *
     * */

    log_trace("write service ", name) ;

    switch(type) {

        case TYPE_MODULE:
        case TYPE_BUNDLE:
            break ;

        case TYPE_CLASSIC:

            write_classic(res, workdir) ;
            break ;

        case TYPE_ONESHOT:

            write_oneshot(res, workdir) ;
            break ;

        case 4:

            write_logger(res, workdir) ;
            break ;

        default:
            log_die(LOG_EXIT_SYS, "unkown type: ", get_key_by_enum(ENUM_TYPE, type)) ;
    }

}




