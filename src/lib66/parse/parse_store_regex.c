/*
 * parse_store_regex.c
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

#include <oblibs/log.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/parser.h>
#include <66/resolve.h>
#include <66/enum.h>

int parse_store_regex(resolve_service_t *res, char *store, int idsec, int idkey)
{
    log_flow() ;

    if (res->type == TYPE_CLASSIC || res->type == TYPE_ONESHOT)
        return 1 ;

    stralloc sa = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch(idkey) {

        case KEY_REGEX_CONFIGURE:

            if (!parse_clean_quotes(store))
                parse_error_return(0, 8, idsec, idkey) ;

            res->regex.configure = resolve_add_string(wres, store) ;

            break ;

        case KEY_REGEX_DIRECTORIES:

            if (!parse_clean_list(&sa, store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (sa.len)
                res->regex.directories = parse_compute_list(wres, &sa, &res->regex.ndirectories, 0) ;

            break ;

        case KEY_REGEX_FILES:

            if (!parse_clean_list(&sa, store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (sa.len)
                res->regex.files = parse_compute_list(wres, &sa, &res->regex.nfiles, 0) ;

            break ;

        case KEY_REGEX_INFILES:

            if (!parse_clean_list(&sa, store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (sa.len)
                res->regex.infiles = parse_compute_list(wres, &sa, &res->regex.ninfiles, 0) ;

            break ;

        default:
            log_warn_return(LOG_EXIT_ZERO, "unknown key: ", get_key_by_key_all(idsec, idkey)) ;
    }

    stralloc_free(&sa) ;
    free(wres) ;
    return 1 ;
}
