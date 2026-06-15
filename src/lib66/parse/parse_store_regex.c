/*
 * parse_store_regex.c
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

#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum_parser.h>

int parse_store_regex(resolve_service_t *res, strbuf *store, resolve_enum_table_t table)
{
    log_flow() ;

    if (res->type != E_PARSER_TYPE_MODULE)
        return 1 ;

    _cleanup_wres_ resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    uint32_t kid = table.u.parser.id ;

    switch(kid) {

        case E_PARSER_SECTION_REGEX_CONFIGURE:

            res->regex.configure = resolve_add_string(wres, store->s) ;

            break ;

        case E_PARSER_SECTION_REGEX_DIRECTORIES:

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len)
                res->regex.directories = parse_compute_list(wres, store, &res->regex.ndirectories, 0) ;

            break ;

        case E_PARSER_SECTION_REGEX_FILES:

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len)
                res->regex.files = parse_compute_list(wres, store, &res->regex.nfiles, 0) ;

            break ;

        case E_PARSER_SECTION_REGEX_INFILES:

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len)
                res->regex.infiles = parse_compute_list(wres, store, &res->regex.ninfiles, 0) ;

            break ;

        default:
            /** never happen*/
            log_warn_return(LOG_EXIT_ZERO, "unknown id key in section regex -- please make a bug report") ;
    }

    free(wres) ;
    return 1 ;
}
