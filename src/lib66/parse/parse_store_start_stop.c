/*
 * parse_store_start_stop.c
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

#include <skalibs/stralloc.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum.h>

int parse_store_start_stop(resolve_service_t *res, char *store, int idsec, int idkey)
{
    log_flow() ;

    if (res->type == TYPE_BUNDLE || res->type == TYPE_MODULE)
        return 1 ;

    int e = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch(idkey) {

        case KEY_STARTSTOP_BUILD:

            if (!parse_clean_line(store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (idsec == SECTION_START)
                res->execute.run.build = resolve_add_string(wres, store) ;
            else if (idsec == SECTION_STOP)
                res->execute.finish.build = resolve_add_string(wres, store) ;
            else if (idsec == SECTION_LOG)
                res->logger.execute.run.build = resolve_add_string(wres, store) ;

            break ;

        case KEY_STARTSTOP_RUNAS:

            if (!parse_clean_line(store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (!parse_clean_runas(store, idsec, idkey))
                goto err ;

            if (idsec == SECTION_START)
                res->execute.run.runas = resolve_add_string(wres, store) ;
            else if (idsec == SECTION_STOP)
                res->execute.finish.runas = resolve_add_string(wres, store) ;
            else if (idsec == SECTION_LOG)
                res->logger.execute.run.runas = resolve_add_string(wres, store) ;

            break ;

        case KEY_STARTSTOP_SHEBANG:

            log_1_warn("deprecated key @shebang -- define your complete shebang directly inside your @execute key field") ;

            if (!parse_clean_quotes(store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (store[0] != '/')
                parse_error_return(0, 4, idsec, idkey) ;

            if (idsec == SECTION_START)
                res->execute.run.shebang = resolve_add_string(wres, store) ;
            else if (idsec == SECTION_STOP)
                res->execute.finish.shebang = resolve_add_string(wres, store) ;
            else if (idsec == SECTION_LOG)
                res->logger.execute.run.shebang = resolve_add_string(wres, store) ;

            break ;

        case KEY_STARTSTOP_EXEC:

            if (idsec == SECTION_START)
                res->execute.run.run_user = resolve_add_string(wres, store) ;
            else if (idsec == SECTION_STOP)
                res->execute.finish.run_user = resolve_add_string(wres, store) ;
            else if (idsec == SECTION_LOG)
                res->logger.execute.run.run_user = resolve_add_string(wres, store) ;

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
