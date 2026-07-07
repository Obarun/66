/*
 * parse_regex.c
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

#include <stdint.h>
#include <stdlib.h> // free

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>

int parse_regex(struct resolve_hash_s *c, parse_build_ctx_t *ctx)
{
    log_flow() ;

    parse_store_t *st = ctx->st ;
    resolve_service_t *res = &c->res ;
    resolve_service_addon_regex_t *rx = &c->regex ;

    res->has_regex = 0 ;

    // the [Regex] section is only meaningful for module services
    if (res->type != E_PARSER_TYPE_MODULE)
        return 1 ;

    // lazily initialised on the first present key, so a module without any [Regex]
    // key leaves sa untouched (no addon, no leak)
    resolve_wrapper_t_ref wres = 0 ;
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_REGEX_ZERO ;

    for (uint32_t kid = 0 ; kid < E_PARSER_SECTION_REGEX_ENDOFKEY ; kid++) {

        if (!parse_store_present(st, E_PARSER_SECTION_REGEX, kid))
            continue ;

        if (!wres) {
            wres = resolve_set_struct(DATA_SERVICE_REGEX, rx) ;
            resolve_init(wres) ; // offset 0 = "" convention
        }

        table.u.parser.id = kid ;
        size_t len = 0 ;
        char const *v = parse_store_get(st, E_PARSER_SECTION_REGEX, kid, &len) ;

        switch (kid) {

            case E_PARSER_SECTION_REGEX_CONFIGURE:
                rx->configure = resolve_add_string(wres, v) ;
                break ;

            case E_PARSER_SECTION_REGEX_DIRECTORIES:
            case E_PARSER_SECTION_REGEX_FILES:
            case E_PARSER_SECTION_REGEX_INFILES:
                {
                    uint32_t *field = kid == E_PARSER_SECTION_REGEX_DIRECTORIES ? &rx->directories :
                                      kid == E_PARSER_SECTION_REGEX_FILES       ? &rx->files : &rx->infiles ;
                    uint32_t *nfield = kid == E_PARSER_SECTION_REGEX_DIRECTORIES ? &rx->ndirectories :
                                       kid == E_PARSER_SECTION_REGEX_FILES       ? &rx->nfiles : &rx->ninfiles ;

                    _alloc_sbl_(stk, len + 1) ;
                    if (!strbuf_copyb(&stk, v, len))
                        log_die_nomem("stack") ;
                    if (!parse_list(&stk)) { free(wres) ; parse_error_return(0, 8, table) ; }
                    if (stk.len)
                        *field = parse_compute_list(wres, &stk, nfield, 0) ;
                }
                break ;

            default:
                break ;
        }
    }

    if (wres) {
        free(wres) ;
        res->has_regex = 1 ;
    }

    return 1 ;
}
