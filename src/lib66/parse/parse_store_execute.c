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
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum_parser.h>
#include <66/caps.h>

static int limit_compute(stack *store, uint64_t *u, resolve_enum_table_t table)
{
    if (store->s[0] == 'u') {
        (*u) = (uint64_t)(RLIM_INFINITY) ;
        return 1 ;
    }

    if (!uint640_scan(store->s, u))
        parse_error_return(0, 3, table) ;

    return 1 ;
} ;

int parse_store_execute(resolve_service_t *res, stack *store, resolve_enum_table_t table)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    uint32_t kid = table.u.parser.id ;

    switch(kid) {

        case E_PARSER_SECTION_EXECUTE_LIMITAS:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (!limit_compute(store, &res->limit.limitas, table))
                return 0 ;

            break ;

        case E_PARSER_SECTION_EXECUTE_LIMITCORE:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (!limit_compute(store, &res->limit.limitcore, table))
                return 0 ;

            break ;

        case E_PARSER_SECTION_EXECUTE_LIMITCPU:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (!limit_compute(store, &res->limit.limitcpu, table))
                return 0 ;

            break ;

        case E_PARSER_SECTION_EXECUTE_LIMITDATA:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (!limit_compute(store, &res->limit.limitdata, table))
                return 0 ;

            break ;

        case E_PARSER_SECTION_EXECUTE_LIMITFSIZE:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (!limit_compute(store, &res->limit.limitfsize, table))
                return 0 ;

            break ;

        case E_PARSER_SECTION_EXECUTE_LIMITLOCKS:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (!limit_compute(store, &res->limit.limitlocks, table))
                return 0 ;

            break ;

        case E_PARSER_SECTION_EXECUTE_LIMITMEMLOCK:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (!limit_compute(store, &res->limit.limitmemlock, table))
                return 0 ;

            break ;

        case E_PARSER_SECTION_EXECUTE_LIMITMSGQUEUE:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (!limit_compute(store, &res->limit.limitmsgqueue, table))
                return 0 ;

            break ;

        case E_PARSER_SECTION_EXECUTE_LIMITNICE:

            {
                parse_error_type(res->type, enum_list_parser_section_execute, kid) ;

                if (store->s[0] == 'u') {
                    res->limit.limitnice = RLIM_INFINITY;
                    break;
                }

                int64_t n = 0 ;
                if (!int64_scan_base_max(store->s, &n, 10, INT64_MAX))
                    parse_error_return(0, 3, table) ;

                if (n < -20 || n > 19)
                    parse_error_return(0, 0, table) ;

                if (!n) {
                    res->limit.limitnice = 1 ;
                } else {
                    res->limit.limitnice = (uint64_t)(20 - n) ;
                }
            }
            break ;

        case E_PARSER_SECTION_EXECUTE_LIMITNOFILE:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (!limit_compute(store, &res->limit.limitnofile, table))
                return 0 ;

            break ;

        case E_PARSER_SECTION_EXECUTE_LIMITNPROC:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (!limit_compute(store, &res->limit.limitnproc, table))
                return 0 ;

            break ;

        case E_PARSER_SECTION_EXECUTE_LIMITRTPRIO:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (!limit_compute(store, &res->limit.limitrtprio, table))
                return 0 ;

            if (res->limit.limitrtprio > 100)
                res->limit.limitrtprio = 100 ;

            break ;

        case E_PARSER_SECTION_EXECUTE_LIMITRTTIME:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (!limit_compute(store, &res->limit.limitrttime, table))
                return 0 ;

            break ;

        case E_PARSER_SECTION_EXECUTE_LIMITSIGPENDING:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (!limit_compute(store, &res->limit.limitsigpending, table))
                return 0 ;

            break ;

        case E_PARSER_SECTION_EXECUTE_LIMITSTACK:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (!limit_compute(store, &res->limit.limitstack, table))
                return 0 ;

            break ;

        case E_PARSER_SECTION_EXECUTE_BLOCK_PRIVILEGES:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (store->s[0] == 'T' || store->s[0] == 't' || store->s[0] == '1')
                res->execute.blockprivileges = 1 ;

            break ;

        case E_PARSER_SECTION_EXECUTE_UMASK:

            uint32_t mode ;
            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
            if (!uint32_oscan(store->s, &mode))
                parse_error_return(0, 3, table) ;

            if (mode > 0777)
                parse_error_return(0, 0, table) ;

            res->execute.umask = mode ;
            res->execute.want_umask = 1 ;

        case E_PARSER_SECTION_EXECUTE_NICE:
            {
                parse_error_type(res->type, enum_list_parser_section_execute, kid) ;

                int64_t n = 0 ;
                if (!int64_scan_base_max(store->s, &n, 10, INT64_MAX))
                    parse_error_return(0, 3, table) ;

                if (n < -20 || n > 19)
                    parse_error_return(0, 0, table) ;

                if (!n) {
                    res->execute.nice = 1 ;
                } else {
                    res->execute.nice = (uint32_t)(20 - n) ;
                }

                res->execute.want_nice = 1 ;
            }
            break ;

        case E_PARSER_SECTION_EXECUTE_CHDIR:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;

            if (store->s[0] != '/')
                parse_error_return(0, 4, table) ;

            res->execute.chdir = resolve_add_string(wres, store->s) ;

            break ;

        case E_PARSER_SECTION_EXECUTE_CAPS_BOUND:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len && !res->owner) {
                _alloc_stk_(stk, 2048) ; // ~ (70 CAPS * 30)
                parse_store_caps(&stk, store, &res->execute.ncapsbound) ;
                if (stk.len)
                    res->execute.capsbound = resolve_add_string(wres, stk.s) ;
            }
            break ;

        case E_PARSER_SECTION_EXECUTE_CAPS_AMBIENT:

            parse_error_type(res->type, enum_list_parser_section_execute, kid) ;

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len) {
                _alloc_stk_(stk, 2048) ; // ~ (70 CAPS * 30)
                parse_store_caps(&stk, store, &res->execute.ncapsambient) ;
                if (stk.len)
                    res->execute.capsambient = resolve_add_string(wres, stk.s) ;
            }
            break ;

        default:
            /** never happen*/
            log_warn_return(LOG_EXIT_ZERO, "unknown id key in section regex -- please make a bug report") ;
    }

    free(wres) ;
    return 1 ;
}
