/*
 * parse_store_g.c
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

#include <oblibs/log.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum.h>

int parse_store_g(resolve_service_t *res, char *store, int idsec, int idkey)
{
    log_flow() ;

    log_trace("storing key: ", get_key_by_key_all(idsec, idkey)) ;

    switch(idsec) {


        case SECTION_MAIN:

            if (!parse_store_main(res, store, idsec, idkey))
                log_warnu_return(LOG_EXIT_ZERO, "store value of section: ", enum_str_section[SECTION_MAIN]);

            break ;

        case SECTION_START:
        case SECTION_STOP:

            if (!parse_store_start_stop(res, store, idsec, idkey))
                log_warnu_return(LOG_EXIT_ZERO, "store value of section: ", enum_str_section[SECTION_START]);

            break ;

        case SECTION_LOG:

            if (!parse_store_logger(res, store, idsec, idkey))
                log_warnu_return(LOG_EXIT_ZERO, "store value of section: ", enum_str_section[SECTION_LOG]);


            break ;

        case SECTION_ENV:

            if (!parse_store_environ(res, store, idsec, idkey))
                log_warnu_return(LOG_EXIT_ZERO, "store value of section: ", enum_str_section[SECTION_ENV]);

            break ;

        case SECTION_REGEX:

            if (!parse_store_regex(res, store, idsec, idkey))
                log_warnu_return(LOG_EXIT_ZERO, "store value of section: ", enum_str_section[SECTION_REGEX]);

            break ;
    }

    return 1 ;
}
