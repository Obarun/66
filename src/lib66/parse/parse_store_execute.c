/*
 * parse_store_execute.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h> //free
#include <sys/resource.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/types.h>
#include <oblibs/strbuf.h>


#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum_parser.h>
#include <66/caps.h>

int parse_store_execute(resolve_service_t *res, resolve_service_addon_execute_t *ex, strbuf *store, resolve_enum_table_t table)
{
    log_flow() ;

    _cleanup_wres_ resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_EXECUTE, ex) ;
    uint32_t kid = table.u.parser.id ;

    switch(kid) {

        case E_PARSER_SECTION_EXECUTE_LIMITAS:
        case E_PARSER_SECTION_EXECUTE_LIMITCORE:
        case E_PARSER_SECTION_EXECUTE_LIMITCPU:
        case E_PARSER_SECTION_EXECUTE_LIMITDATA:
        case E_PARSER_SECTION_EXECUTE_LIMITFSIZE:
        case E_PARSER_SECTION_EXECUTE_LIMITLOCKS:
        case E_PARSER_SECTION_EXECUTE_LIMITMEMLOCK:
        case E_PARSER_SECTION_EXECUTE_LIMITMSGQUEUE:
        case E_PARSER_SECTION_EXECUTE_LIMITNICE:
        case E_PARSER_SECTION_EXECUTE_LIMITNOFILE:
        case E_PARSER_SECTION_EXECUTE_LIMITNPROC:
        case E_PARSER_SECTION_EXECUTE_LIMITRTPRIO:
        case E_PARSER_SECTION_EXECUTE_LIMITRTTIME:
        case E_PARSER_SECTION_EXECUTE_LIMITSIGPENDING:
        case E_PARSER_SECTION_EXECUTE_LIMITSTACK:
            /* the `limit` addon is resolved separately by parse_limit() */
            break ;
        case E_PARSER_SECTION_EXECUTE_BLOCK_PRIVILEGES:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (store->s[0] == 'T' || store->s[0] == 't' || store->s[0] == '1')
                ex->blockprivileges = 1 ;

            break ;

        case E_PARSER_SECTION_EXECUTE_UMASK:

            {
                uint32_t mode ;
                parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
                /** UMask is octal notation (e.g. 022): scan base 8, not base 10. */
                if (!u32_scan_strict_base(store->s, &mode, 8))
                    parse_error_return(0, 3, table) ;

                if (mode > 0777)
                    parse_error_return(0, 0, table) ;

                ex->umask = mode ;
                ex->want_umask = 1 ;
            }
            break ;

        case E_PARSER_SECTION_EXECUTE_NICE:
            {
                parse_error_type(res->type, enum_list_parser_section_execute, kid) ;

                int64_t n = 0 ;
                if (!i64_scan_base_max(store->s, &n, 10, INT64_MAX))
                    parse_error_return(0, 3, table) ;

                if (n < -20 || n > 19)
                    parse_error_return(0, 0, table) ;

                ex->nice = (uint32_t)(20 - n) ;
                ex->want_nice = 1 ;
            }

            break ;

        case E_PARSER_SECTION_EXECUTE_CHDIR:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;

            if (store->s[0] != '/')
                parse_error_return(0, 4, table) ;

            ex->chdir = resolve_add_string(wres, store->s) ;

            break ;

        case E_PARSER_SECTION_EXECUTE_CAPS_BOUND:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len && !res->owner) {
                _alloc_sbl_(stk, 2048) ; // ~ (70 CAPS * 30)
                parse_store_caps(&stk, store, &ex->ncapsbound) ;
                if (stk.len)
                    ex->capsbound = resolve_add_string(wres, stk.s) ;
            }

            break ;

        case E_PARSER_SECTION_EXECUTE_CAPS_AMBIENT:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len) {
                _alloc_sbl_(stk, 2048) ; // ~ (70 CAPS * 30)
                parse_store_caps(&stk, store, &ex->ncapsambient) ;
                if (stk.len)
                    ex->capsambient = resolve_add_string(wres, stk.s) ;
            }

            break ;

        default:
            /** never happen*/
            log_warn_return(LOG_EXIT_ZERO, "unknown id key in section regex -- please make a bug report") ;
    }

    return 1 ;
}
