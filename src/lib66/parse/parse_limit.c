/*
 * parse_limit.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdint.h>
#include <sys/resource.h> // RLIM_INFINITY

#include <oblibs/log.h>
#include <oblibs/types.h>

#include <66/parse.h>
#include <66/service.h>
#include <66/enum_parser.h>

int parse_limit(parse_store_t *st, resolve_service_addon_limit_t *l, uint8_t *has_limit)
{
    log_flow() ;

    uint64_t *field[E_PARSER_SECTION_EXECUTE_ENDOFKEY] = {
        [E_PARSER_SECTION_EXECUTE_LIMITAS]         = &l->limitas,
        [E_PARSER_SECTION_EXECUTE_LIMITCORE]       = &l->limitcore,
        [E_PARSER_SECTION_EXECUTE_LIMITCPU]        = &l->limitcpu,
        [E_PARSER_SECTION_EXECUTE_LIMITDATA]       = &l->limitdata,
        [E_PARSER_SECTION_EXECUTE_LIMITFSIZE]      = &l->limitfsize,
        [E_PARSER_SECTION_EXECUTE_LIMITLOCKS]      = &l->limitlocks,
        [E_PARSER_SECTION_EXECUTE_LIMITMEMLOCK]    = &l->limitmemlock,
        [E_PARSER_SECTION_EXECUTE_LIMITMSGQUEUE]   = &l->limitmsgqueue,
        [E_PARSER_SECTION_EXECUTE_LIMITNICE]       = &l->limitnice,
        [E_PARSER_SECTION_EXECUTE_LIMITNOFILE]     = &l->limitnofile,
        [E_PARSER_SECTION_EXECUTE_LIMITNPROC]      = &l->limitnproc,
        [E_PARSER_SECTION_EXECUTE_LIMITRTPRIO]     = &l->limitrtprio,
        [E_PARSER_SECTION_EXECUTE_LIMITRTTIME]     = &l->limitrttime,
        [E_PARSER_SECTION_EXECUTE_LIMITSIGPENDING] = &l->limitsigpending,
        [E_PARSER_SECTION_EXECUTE_LIMITSTACK]      = &l->limitstack,
    } ;

    *has_limit = 0 ;

    for (uint32_t kid = 0 ; kid < E_PARSER_SECTION_EXECUTE_ENDOFKEY ; kid++) {

        if (!st->present[E_PARSER_SECTION_EXECUTE][kid] || !field[kid])
            continue ; // absent, or a non-limit [Execute] key

        *has_limit = 1 ;

        char const *v = parse_store_get(st, E_PARSER_SECTION_EXECUTE, kid, 0) ;

        resolve_enum_table_t t = E_TABLE_PARSER_SECTION_EXECUTE_ZERO ;
        t.u.parser.id = kid ;

        // Nice: signed value -20..19 stored as rlimit 20-n (n==0 -> 1); 'u' -> infinity
        if (kid == E_PARSER_SECTION_EXECUTE_LIMITNICE) {

            if (v[0] == 'u') {
                l->limitnice = (uint64_t)RLIM_INFINITY ;
                continue ;
            }

            int64_t n = 0 ;
            if (!i64_scan(v, &n))
                parse_error_return(0, 3, t) ;
            if (n < -20 || n > 19)
                parse_error_return(0, 0, t) ;

            l->limitnice = n ? (uint64_t)(20 - n) : 1 ;
            continue ;
        }

        // common rlimit: 'u' -> infinity, else a strict u64
        uint64_t val = 0 ;
        if (v[0] == 'u')
            val = (uint64_t)RLIM_INFINITY ;
        else if (!u64_scan_strict(v, &val))
            parse_error_return(0, 3, t) ;

        // RTPRIO is clamped to 100
        if (kid == E_PARSER_SECTION_EXECUTE_LIMITRTPRIO && val > 100)
            val = 100 ;

        *field[kid] = val ;
    }

    return 1 ;
}
