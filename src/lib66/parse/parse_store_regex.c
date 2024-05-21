/*
 * parse_store_regex.c
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
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum.h>

int parse_store_regex(resolve_service_t *res, stack *store, int idsec, int idkey)
{
    log_flow() ;

    if (res->type != TYPE_MODULE)
        return 1 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch(idkey) {

        case KEY_REGEX_CONFIGURE:

            res->regex.configure = resolve_add_string(wres, store->s) ;

            break ;

        case KEY_REGEX_DIRECTORIES:

            if (!parse_list(store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (store->len)
                res->regex.directories = parse_compute_list(wres, store, &res->regex.ndirectories, 0) ;

            break ;

        case KEY_REGEX_FILES:

            if (!parse_list(store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (store->len)
                res->regex.files = parse_compute_list(wres, store, &res->regex.nfiles, 0) ;

            break ;

        case KEY_REGEX_INFILES:

            if (!parse_list(store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (store->len)
                res->regex.infiles = parse_compute_list(wres, store, &res->regex.ninfiles, 0) ;

            break ;

        default:
            log_warn_return(LOG_EXIT_ZERO, "unknown key: ", get_key_by_key_all(idsec, idkey)) ;
    }

    free(wres) ;
    return 1 ;
}
