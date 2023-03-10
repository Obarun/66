/*
 * parse_store_environ.c
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

#include <oblibs/string.h>
#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/parser.h>
#include <66/resolve.h>
#include <66/enum.h>
#include <66/utils.h>
#include <66/environ.h>

int parse_store_environ(resolve_service_t *res, char *store, int idsec, int idkey)
{
    log_flow() ;

    if (res->type == TYPE_BUNDLE)
        return 1 ;

    int e = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch(idkey) {

        case KEY_ENVIRON_ENVAL:

            if (!auto_stra(&sa, store))
                goto err ;

            if (!env_clean_with_comment(&sa))
                log_warnu_return(LOG_EXIT_ZERO,"clean environment value") ;

            res->environ.env = resolve_add_string(wres, sa.s) ;

            sa.len = 0 ;
            if (!env_resolve_conf(&sa, res->sa.s + res->name, MYUID))
                goto err ;

            res->environ.envdir = resolve_add_string(wres, sa.s) ;

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
