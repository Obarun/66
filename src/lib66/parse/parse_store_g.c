/*
 * parse_store_g.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <66/enum_parser.h>

int parse_store_g(resolve_service_t *res, stack *store, resolve_enum_table_t table)
{
    log_flow() ;

    log_trace("storing key: ", enum_to_key(table.u.parser.list, table.u.parser.id)) ;

    switch(table.u.parser.sid) {

        case E_PARSER_SECTION_MAIN:

            if (!parse_store_main(res, store, table))
                log_warnu_return(LOG_EXIT_ZERO, "store value of section: ", enum_str_parser_section[E_PARSER_SECTION_MAIN]);

            break ;

        case E_PARSER_SECTION_START:

            if (!parse_store_start_stop(res, store, table))
                log_warnu_return(LOG_EXIT_ZERO, "store value of section: ", enum_str_parser_section[E_PARSER_SECTION_START]);

            break ;

        case E_PARSER_SECTION_STOP:

            if (!parse_store_start_stop(res, store, table))
                log_warnu_return(LOG_EXIT_ZERO, "store value of section: ", enum_str_parser_section[E_PARSER_SECTION_STOP]);

            break ;

        case E_PARSER_SECTION_LOGGER:

            if (!parse_store_logger(res, store, table))
                log_warnu_return(LOG_EXIT_ZERO, "store value of section: ", enum_str_parser_section[E_PARSER_SECTION_LOGGER]);
            break ;

        case E_PARSER_SECTION_ENVIRONMENT:

            if (!parse_store_environ(res, store, table))
                log_warnu_return(LOG_EXIT_ZERO, "store value of section: ", enum_str_parser_section[E_PARSER_SECTION_ENVIRONMENT]);

            break ;

        case E_PARSER_SECTION_REGEX:

            if (!parse_store_regex(res, store, table))
                log_warnu_return(LOG_EXIT_ZERO, "store value of section: ", enum_str_parser_section[E_PARSER_SECTION_REGEX]);

            break ;

        case E_PARSER_SECTION_EXECUTE:

            if (!parse_store_execute(res, store, table))
                log_warnu_return(LOG_EXIT_ZERO, "store value of section: ", enum_str_parser_section[E_PARSER_SECTION_EXECUTE]);

            break ;
    }

    return 1 ;
}
