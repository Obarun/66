/*
 * parse_logger.c
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

#include <string.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/string.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>
#include <66/constants.h>
#include <66/config.h>

int parse_logger(parse_store_t *st, resolve_service_t *res, resolve_service_addon_logger_t *lg, uint8_t *has_logger)
{
    log_flow() ;

    if (res->type == E_PARSER_TYPE_MODULE) {
        *has_logger = 0 ;
        return 1 ;
    }

    *has_logger = 1 ;

    _cleanup_wres_ resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_LOGGER, lg) ;
    resolve_init(wres) ;

    for (uint32_t kid = 0 ; kid < E_PARSER_SECTION_LOGGER_ENDOFKEY ; kid++) {

        if (!parse_store_present(st, E_PARSER_SECTION_LOGGER, kid))
            continue ;

        char const *v = parse_store_get(st, E_PARSER_SECTION_LOGGER, kid, 0) ;

        resolve_enum_table_t t = E_TABLE_PARSER_SECTION_LOGGER_ZERO ;
        t.u.parser.id = kid ;

        switch (kid) {

            case E_PARSER_SECTION_LOGGER_BUILD :
                log_warn("key Build is deprecated and ignored -- declare a shebang (#!) at the start of the Execute field instead") ;
                break ;

            case E_PARSER_SECTION_LOGGER_RUNAS :
                {
                    char tmp[strlen(v) + 1] ;
                    auto_strings(tmp, v) ;
                    if (!parse_clean_runas(tmp, t))
                        return 0 ;
                    lg->execute.run.runas = resolve_add_string(wres, tmp) ;
                }
                break ;

            case E_PARSER_SECTION_LOGGER_EXEC :
                lg->execute.run.run_user = resolve_add_string(wres, v) ;
                break ;

            case E_PARSER_SECTION_LOGGER_TIMESTART :
                if (!u32_scan_strict(v, &lg->execute.timeout.start))
                    parse_error_return(0, 3, t) ;
                break ;

            case E_PARSER_SECTION_LOGGER_TIMESTOP :
                if (!u32_scan_strict(v, &lg->execute.timeout.stop))
                    parse_error_return(0, 3, t) ;
                break ;

            case E_PARSER_SECTION_LOGGER_BACKUP :
                if (!u32_scan_strict(v, &lg->backup))
                    parse_error_return(0, 3, t) ;
                break ;

            case E_PARSER_SECTION_LOGGER_MAXSIZE :
                if (!u32_scan_strict(v, &lg->maxsize))
                    parse_error_return(0, 3, t) ;
                if (lg->maxsize < 4096 || lg->maxsize > 268435455)
                    parse_error_return(0, 0, t) ;
                break ;

            case E_PARSER_SECTION_LOGGER_TIMESTAMP :
                {
                    int r = key_to_enum(enum_list_parser_time, v) ;
                    if (r == -1)
                        parse_error_return(0, 0, t) ;
                    lg->timestamp = (uint32_t)r ;
                }
                break ;

            default :
                break ;
        }
    }

    // default runner when the user did not set Runas
    if (!lg->execute.run.runas)
        lg->execute.run.runas = resolve_add_string(wres, SS_LOGGER_RUNNER) ;

    return 1 ;
}
