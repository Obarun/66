/*
 * parse_store_logger.c
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

#include <stdlib.h> //free

#include <oblibs/log.h>

#include <skalibs/types.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum.h>

int parse_store_logger(resolve_service_t *res, stack *store, int sid, int kid)
{
    log_flow() ;

    if (res->type == TYPE_MODULE)
        return 1 ;

    int r = 0, e = 0 ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch(kid) {

        case KEY_LOGGER_BUILD:

            if (!parse_store_start_stop(res, store, sid, KEY_STARTSTOP_BUILD))
                goto err ;

            break ;

        case KEY_LOGGER_RUNAS:

            if (!parse_store_start_stop(res, store, sid, KEY_STARTSTOP_RUNAS))
                goto err ;

            break ;

        case KEY_LOGGER_EXEC:

            if (!parse_store_start_stop(res, store, sid, KEY_STARTSTOP_EXEC))
                goto err ;

            break ;

        case KEY_LOGGER_T_START:

            if (!uint320_scan(store->s, &res->logger.execute.timeout.start))
                parse_error_return(0, 3, sid, list_section_logger, kid) ;

            break ;

        case KEY_LOGGER_T_STOP:

            if (!uint320_scan(store->s, &res->logger.execute.timeout.stop))
                parse_error_return(0, 3, sid, list_section_logger, kid) ;

            break ;

        case KEY_LOGGER_DESTINATION:

            if (store->s[0] != '/')
                parse_error_return(0, 4, sid, list_section_logger, kid) ;

            log_1_warn("Destination field is deprecated -- convert it automatically to StdOut=s6log:", store->s) ;

            res->io.fdout.destination = resolve_add_string(wres, store->s) ;

           break ;

        case KEY_LOGGER_BACKUP:

            if (!uint320_scan(store->s, &res->logger.backup))
                parse_error_return(0, 3, sid, list_section_logger, kid) ;

            break ;

        case KEY_LOGGER_MAXSIZE:

            if (!uint320_scan(store->s, &res->logger.maxsize))
                parse_error_return(0, 3, sid, list_section_logger, kid) ;

            if (res->logger.maxsize < 4096 || res->logger.maxsize > 268435455)
                parse_error_return(0, 0, sid, list_section_logger, kid) ;

            break ;

        case KEY_LOGGER_TIMESTP:

            r = get_enum_by_key(list_timestamp, store->s) ;
            if (r == -1)
                parse_error_return(0, 0, sid, list_section_logger, kid) ;

            res->logger.timestamp = (uint32_t)r ;

            break ;

        default:
            /** never happen*/
            log_warn_return(LOG_EXIT_ZERO, "unknown id key in section logger -- please make a bug report") ;
    }

    e = 1 ;

    err :
        free(wres) ;
        return e ;
}
