/*
 * parse_store_start_stop.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <oblibs/strbuf.h>
#include <oblibs/string.h>
#include <oblibs/types.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>

int parse_store_start_stop(resolve_service_t *res, strbuf *store, resolve_enum_table_t table)
{
    log_flow() ;

    if (res->type == E_PARSER_TYPE_MODULE)
        return 1 ;

    int e = 0 ;
    _cleanup_wres_ resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    uint32_t kid = table.u.parser.id ;
    uint32_t sid = table.u.parser.sid ;

    switch(kid) {

        case E_PARSER_SECTION_STARTSTOP_BUILD:

            log_warn("key Build is deprecated and ignored -- declare a shebang (#!) at the start of the Execute field instead") ;
            break ;

        case E_PARSER_SECTION_STARTSTOP_RUNAS:
            {
                char tmp[store->len + 1] ;
                auto_strings(tmp, store->s) ;
                if (!parse_clean_runas(tmp, table))
                    goto err ;

                if (sid == E_PARSER_SECTION_START)
                    res->execute.run.runas = resolve_add_string(wres, tmp) ;
                else if (sid == E_PARSER_SECTION_STOP)
                    res->execute.finish.runas = resolve_add_string(wres, tmp) ;
            }
            break ;

        case E_PARSER_SECTION_STARTSTOP_EXEC:

            if (sid == E_PARSER_SECTION_START)
                res->execute.run.run_user = resolve_add_string(wres, store->s) ;
            else if (sid == E_PARSER_SECTION_STOP)
                res->execute.finish.run_user = resolve_add_string(wres, store->s) ;
            break ;

        case E_PARSER_SECTION_STARTSTOP_TIMESTART:

            if (sid != E_PARSER_SECTION_START)
                log_warn_return(LOG_EXIT_ZERO, "key TimeoutStart is only valid at section [Start]") ;

            if (!u32_scan_strict(store->s, &res->execute.timeout.start))
                parse_error_return(0, 3, table) ;

            break ;

        case E_PARSER_SECTION_STARTSTOP_TIMESTOP:

            if (sid != E_PARSER_SECTION_STOP)
                log_warn_return(LOG_EXIT_ZERO, "key TimeoutStop is only valid at section [Stop]") ;

            if (!u32_scan_strict(store->s, &res->execute.timeout.stop))
                parse_error_return(0, 3, table) ;

            break ;

        default:
            /** never happen*/
            log_warn_return(LOG_EXIT_ZERO, "unknown id key in section ", *table.u.parser.list[sid].name, "  -- please make a bug report") ;
    }

    e = 1 ;

    err :
        return e ;
}
