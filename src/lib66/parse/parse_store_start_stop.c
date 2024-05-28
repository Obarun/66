/*
 * parse_store_start_stop.c
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
#include <oblibs/string.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum.h>

int parse_store_start_stop(resolve_service_t *res, stack *store, const int sid, const int kid)
{
    log_flow() ;

    if (res->type == TYPE_MODULE)
        return 1 ;

    int e = 0 ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch(kid) {

        case KEY_STARTSTOP_BUILD:

            if (sid == SECTION_START)
                res->execute.run.build = resolve_add_string(wres, store->s) ;
            else if (sid == SECTION_STOP)
                res->execute.finish.build = resolve_add_string(wres, store->s) ;
            else if (sid == SECTION_LOG)
                res->logger.execute.run.build = resolve_add_string(wres, store->s) ;
            break ;

        case KEY_STARTSTOP_RUNAS:
            {
                char tmp[store->len + 1] ;
                auto_strings(tmp, store->s) ;
                if (!parse_clean_runas(tmp, sid, kid))
                    goto err ;

                if (sid == SECTION_START)
                    res->execute.run.runas = resolve_add_string(wres, tmp) ;
                else if (sid == SECTION_STOP)
                    res->execute.finish.runas = resolve_add_string(wres, tmp) ;
                else if (sid == SECTION_LOG)
                    res->logger.execute.run.runas = resolve_add_string(wres, tmp) ;
            }
            break ;

        case KEY_STARTSTOP_SHEBANG:

            log_1_warn("deprecated key @shebang -- define your complete shebang directly inside your @execute key field") ;

            if (store->s[0] != '/')
                parse_error_return(0, 4, sid, list_section_startstop, kid) ;

            if (sid == SECTION_START)
                res->execute.run.shebang = resolve_add_string(wres, store->s) ;
            else if (sid == SECTION_STOP)
                res->execute.finish.shebang = resolve_add_string(wres, store->s) ;
            else if (sid == SECTION_LOG)
                res->logger.execute.run.shebang = resolve_add_string(wres, store->s) ;
            break ;

        case KEY_STARTSTOP_EXEC:

            if (sid == SECTION_START)
                res->execute.run.run_user = resolve_add_string(wres, store->s) ;
            else if (sid == SECTION_STOP)
                res->execute.finish.run_user = resolve_add_string(wres, store->s) ;
            else if (sid == SECTION_LOG)
                res->logger.execute.run.run_user = resolve_add_string(wres, store->s) ;
            break ;

        default:
            /** never happen*/
            log_warn_return(LOG_EXIT_ZERO, "unknown id key in section ", *list_section_startstop[sid].name, "  -- please make a bug report") ;
    }

    e = 1 ;

    err :
        free(wres) ;
        return e ;
}
