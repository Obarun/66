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
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>
#include <66/constants.h>
#include <66/config.h>

static int get_shebang(strbuf *stk, char const *line)
{
    size_t len = strlen(line) ;
    uint32_t i = 0 ;

    while (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '\n')
        i++ ;

    if (i >= len || line[i] != '#' || line[i + 1] != '!')
        return 0 ;

    if (!sbl_addb(stk, line + i, len - i))
        log_warnsys_return(LOG_EXIT_LESSONE, "stack add") ;

    return 1 ;
}

int parse_logger(struct resolve_hash_s *c, parse_build_ctx_t *ctx)
{
    log_flow() ;

    parse_store_t *st = ctx->st ;
    resolve_service_t *res = &c->res ;
    resolve_service_addon_logger_t *lg = &c->logger ;

    if (res->type == E_PARSER_TYPE_MODULE) {
        res->logger = 0 ;
        return 1 ;
    }

    res->logger = 1 ;

    if (parse_store_present(st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_OPTIONS)) {

        resolve_enum_table_t t = E_TABLE_PARSER_SECTION_MAIN_ZERO ;
        t.u.parser.id = E_PARSER_SECTION_MAIN_OPTIONS ;
        size_t len = 0 ;
        char const *v = parse_store_get(st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_OPTIONS, &len) ;

        _alloc_sbl_(stk, len + 1) ;

        if (!strbuf_copyb(&stk, v, len))
            log_die_nomem("stack") ;

        if (!parse_list(&stk))
            parse_error_return(0, 8, t) ;

        size_t pos = 0 ;
        FOREACH_SBL(&stk, pos) {

            uint8_t reverse = stk.s[pos] == '!' ? 1 : 0 ;
            int r = key_to_enum(enum_list_parser_opts, stk.s + pos + reverse) ;
            if (r == -1)
                parse_error_return(0, 0, t) ;

            if (reverse && r == E_PARSER_OPTS_LOGGER)
                res->logger = 0 ;
        }
    }

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

    if (lg->execute.run.run_user) {

        size_t len = strlen(lg->sa.s + lg->execute.run.run_user) ;
        _alloc_sbl_(stk, len) ;

        int r = get_shebang(&stk, lg->sa.s + lg->execute.run.run_user) ;
        if (r < 0)
            return 0 ;
        if (r) {
            lg->execute.run.run_user = resolve_add_string(wres, stk.s) ;
            lg->execute.run.build = resolve_add_string(wres, "custom") ;
        }
    }

    return 1 ;
}
