/*
 * parse_store_logger.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/strbuf.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>

int parse_store_logger(resolve_service_t *res, strbuf *store, resolve_enum_table_t table)
{
    log_flow() ;

    if (res->type == E_PARSER_TYPE_MODULE)
        return 1 ;

    int r = 0, e = 0 ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    uint32_t kid = table.u.parser.id ;
    resolve_enum_table_t ttable ;
    ttable.category = table.category ; ttable.u.parser.category = table.u.parser.category ;
    ttable.u.parser.list = table.u.parser.list ; ttable.u.parser.sid = table.u.parser.sid ;


    switch(kid) {

        case E_PARSER_SECTION_LOGGER_BUILD:

            ttable.u.parser.id = E_PARSER_SECTION_STARTSTOP_BUILD ;

            if (!parse_store_start_stop(res, store, ttable))
                goto err ;

            break ;

        case E_PARSER_SECTION_LOGGER_RUNAS:

            ttable.u.parser.id = E_PARSER_SECTION_STARTSTOP_RUNAS ;

            if (!parse_store_start_stop(res, store, ttable))
                goto err ;

            break ;

        case E_PARSER_SECTION_LOGGER_EXEC:

            ttable.u.parser.id = E_PARSER_SECTION_STARTSTOP_EXEC ;

            if (!parse_store_start_stop(res, store, ttable))
                goto err ;

            break ;

        case E_PARSER_SECTION_LOGGER_TIMESTART:

            if (!u32_scan_strict(store->s, &res->logger.execute.timeout.start))
                parse_error_return(0, 3, table) ;

            break ;

        case E_PARSER_SECTION_LOGGER_TIMESTOP:

            if (!u32_scan_strict(store->s, &res->logger.execute.timeout.stop))
                parse_error_return(0, 3, table) ;

            break ;

        case E_PARSER_SECTION_LOGGER_DESTINATION:

            if (store->s[0] != '/')
                parse_error_return(0, 4, table) ;

            log_1_warn("Destination field is deprecated -- convert it automatically to StdOut=s6log:", store->s) ;

            res->io.fdout.destination = resolve_add_string(wres, store->s) ;

           break ;

        case E_PARSER_SECTION_LOGGER_BACKUP:

            if (!u32_scan_strict(store->s, &res->logger.backup))
                parse_error_return(0, 3, table) ;

            break ;

        case E_PARSER_SECTION_LOGGER_MAXSIZE:

            if (!u32_scan_strict(store->s, &res->logger.maxsize))
                parse_error_return(0, 3, table) ;

            if (res->logger.maxsize < 4096 || res->logger.maxsize > 268435455)
                parse_error_return(0, 0, table) ;

            break ;

        case E_PARSER_SECTION_LOGGER_TIMESTAMP:

            r = key_to_enum(enum_list_parser_time, store->s) ;
            if (r == -1)
                parse_error_return(0, 0, table) ;

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
