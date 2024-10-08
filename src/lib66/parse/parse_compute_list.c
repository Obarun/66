/*
 * parse_compute_list.c
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

#include <string.h>
#include <stdint.h>
#include <unistd.h> // getuid

#include <oblibs/string.h>
#include <oblibs/sastr.h>
#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/module.h>

/**
 * @opts -> 1 : build list removing commented optional deps
 * */
int parse_compute_list(resolve_wrapper_t_ref wres, stack *store, uint32_t *res, uint8_t opts)
{
    log_flow() ;

    if (!store->len)
        return resolve_add_string(wres, "") ;

    int r, found = 0 ;
    size_t len = store->len, pos = 0 ;
    size_t nelement = stack_count_element(store) ;
    stralloc tmp = STRALLOC_ZERO ;
    char const *exclude[2] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1 } ;
    char f[len + nelement + 2] ;

    memset(f, 0, (len + nelement + 2) * sizeof(char)) ;

    FOREACH_STK(store, pos) {

        if (store->s[pos] == '#')
            continue ;

        if (opts) {

            tmp.len = 0 ;

            r = service_frontend_path(&tmp, store->s + pos, getuid(), 0, exclude, 2) ;
            if (r == -1)
                log_dieu(LOG_EXIT_SYS, "get frontend service file of: ", store->s + pos) ;

            if (!r)
                continue ;

            found++ ;

        }

        auto_strings(f + strlen(f), store->s + pos, " ") ;

        (*res)++ ;

        if (found)
            break ;
    }

    f[strlen(f) - 1] = 0 ;

    stralloc_free(&tmp) ;

    return resolve_add_string(wres, f) ;
}

