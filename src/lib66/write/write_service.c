/*
 * write_service.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <66/parse.h>

/** @Return 0 on fail
 * @Return 1 on success
 * @Return 2 if the service is ignored
 * */
void write_services(resolve_service_t *res, char const *workdir, uint8_t force)
{
    log_flow() ;

    char *name = res->sa.s + res->name ;
    uint32_t type = res->type ;
    ssize_t logname = get_rstrlen_until(name, SS_LOG_SUFFIX) ;
    if (logname > 0)
        type = 10 ;

    log_trace("write service: ", name) ;

    switch(type) {

        case TYPE_MODULE:
            break ;

        case TYPE_CLASSIC:
        case TYPE_ONESHOT:
            write_common(res, workdir, force) ;
            break ;

        case 10:

            write_logger(res, workdir, force) ;
            break ;

        default:
            parse_cleanup(res, workdir, force) ;
            log_die(LOG_EXIT_SYS, "unkown type: ", get_key_by_enum(list_type, type)) ;
    }

}




