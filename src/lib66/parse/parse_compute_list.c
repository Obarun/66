/*
 * parse_compute_list.c
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

#include <string.h>
#include <stdint.h>
#include <unistd.h> // getuid

#include <oblibs/string.h>
#include <oblibs/sastr.h>
#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/parser.h>
#include <66/resolve.h>
#include <66/service.h>

/**
 * @opts -> 1 : build list of optional deps
 * */
int parse_compute_list(resolve_wrapper_t_ref wres, stralloc *sa, uint32_t *res, uint8_t opts)
{
    int r, found = 0 ;
    size_t len = sa->len, pos = 0 ;
    char t[len + 1] ;
    char f[len + 1] ;

    memset(f, 0, len) ;
    memset(t, 0, len) ;

    sastr_to_char(t, sa) ;

    for (; pos < len ; pos += strlen(t + pos) + 1) {

        if (t[pos] == '#')
            continue ;

        if (opts) {

            sa->len = 0 ;

            r = service_frontend_path(sa, t + pos, getuid(), 0) ;
            if (r == -1)
                log_dieu(LOG_EXIT_SYS, "get frontend service file of: ", t + pos) ;

            if (!r)
                continue ;

            found++ ;

        }

        auto_strings(f + strlen(f), t + pos, " ") ;

        (*res)++ ;

        if (found)
            break ;
    }

    f[strlen(f) - 1] = 0 ;

    return resolve_add_string(wres, f) ;

}

