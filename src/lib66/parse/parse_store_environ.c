/*
 * parse_store_environ.c
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

#include <oblibs/string.h>
#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum.h>
#include <66/utils.h>
#include <66/environ.h>

int parse_store_environ(resolve_service_t *res, stack *store, const int sid, const int kid)
{
    log_flow() ;

    int e = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch(kid) {

        case KEY_ENVIRON_ENVAL:

            res->environ.env = resolve_add_string(wres, store->s) ;

            if (!env_resolve_conf(&sa, res))
                goto err ;

            res->environ.envdir = resolve_add_string(wres, sa.s) ;

            break ;

        default:
            /** never happen*/
            log_warn_return(LOG_EXIT_ZERO, "unknown id key in section environment -- please make a bug report") ;
    }

    e = 1 ;

    err :
        stralloc_free(&sa) ;
        free(wres) ;
        return e ;
}
