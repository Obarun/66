/*
 * parse_store_logger.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
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

int parse_store_logger(resolve_service_t *res, char *store, int idsec, int idkey)
{
    log_flow() ;

    if (res->type == TYPE_MODULE)
        return 1 ;

    int r = 0, e = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch(idkey) {

        case KEY_LOGGER_BUILD:

            if (!parse_store_start_stop(res, store, idsec, KEY_STARTSTOP_BUILD))
                goto err ;

            break ;

        case KEY_LOGGER_RUNAS:

            if (!parse_store_start_stop(res, store, idsec, KEY_STARTSTOP_RUNAS))
                goto err ;

            break ;

        case KEY_LOGGER_SHEBANG:

             log_1_warn("deprecated key @shebang -- define your complete shebang directly inside your @execute key field") ;

            if (!parse_store_start_stop(res, store, idsec, KEY_STARTSTOP_SHEBANG))
                goto err ;

            break ;

        case KEY_LOGGER_EXEC:

            if (!parse_store_start_stop(res, store, idsec, KEY_STARTSTOP_EXEC))
                goto err ;

            break ;

        case KEY_LOGGER_T_KILL:

            if (!uint320_scan(store, &res->logger.execute.timeout.kill))
                parse_error_return(0, 3, idsec, idkey) ;

            break ;

        case KEY_LOGGER_T_FINISH:

            if (!uint320_scan(store, &res->logger.execute.timeout.finish))
                parse_error_return(0, 3, idsec, idkey) ;

            break ;

        case KEY_LOGGER_DESTINATION:

            if (!parse_clean_line(store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (store[0] != '/')
                parse_error_return(0, 4, idsec, idkey) ;

            res->logger.destination = resolve_add_string(wres, store) ;

           break ;

        case KEY_LOGGER_BACKUP:

            if (!uint320_scan(store, &res->logger.backup))
                parse_error_return(0, 3, idsec, idkey) ;

            break ;

        case KEY_LOGGER_MAXSIZE:

            if (!uint320_scan(store, &res->logger.maxsize))
                parse_error_return(0, 3, idsec, idkey) ;

            if (res->logger.maxsize < 4096 || res->logger.maxsize > 268435455)
                parse_error_return(0, 0, idsec, idkey) ;

            break ;

        case KEY_LOGGER_TIMESTP:

            r = get_enum_by_key(store) ;
            if (r == -1)
                parse_error_return(0, 0, idsec, idkey) ;

            res->logger.timestamp = (uint32_t)r ;

            break ;

        default:
            log_warn_return(LOG_EXIT_ZERO, "unknown key: ", get_key_by_key_all(idsec, idkey)) ;
    }

    e = 1 ;

    err :
        stralloc_free(&sa) ;
        free(wres) ;
        return e ;
}
