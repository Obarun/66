/*
 * parse_dependencies.c
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
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>

int parse_dependencies(parse_store_t *st, resolve_service_addon_dependencies_t *dep)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, dep) ;
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_MAIN_ZERO ;

    for (uint32_t kid = 0 ; kid < E_PARSER_SECTION_MAIN_ENDOFKEY ; kid++) {

        if (!parse_store_present(st, E_PARSER_SECTION_MAIN, kid))
            continue ;

        table.u.parser.id = kid ;
        size_t len = 0 ;
        char const *v = parse_store_get(st, E_PARSER_SECTION_MAIN, kid, &len) ;

        uint32_t *field = 0, *nfield = 0 ;
        uint8_t opts = 0 ;

        switch (kid) {

            case E_PARSER_SECTION_MAIN_DEPENDS:    field = &dep->depends ;    nfield = &dep->ndepends ;    break ;
            case E_PARSER_SECTION_MAIN_REQUIREDBY: field = &dep->requiredby ; nfield = &dep->nrequiredby ; break ;
            case E_PARSER_SECTION_MAIN_OPTSDEPS:   field = &dep->optsdeps ;   nfield = &dep->noptsdeps ;   opts = 1 ; break ;
            case E_PARSER_SECTION_MAIN_CONTENTS:   field = &dep->contents ;   nfield = &dep->ncontents ;   break ;
            case E_PARSER_SECTION_MAIN_PROVIDE:    field = &dep->provide ;    nfield = &dep->nprovide ;    break ;
            case E_PARSER_SECTION_MAIN_CONFLICT:   field = &dep->conflict ;   nfield = &dep->nconflict ;   break ;

            default: // core/execute/io keys of [Main] -- not ours
                continue ;
        }

        _alloc_sbl_(stk, len + 1) ;
        if (!strbuf_copyb(&stk, v, len))
            log_die_nomem("strbuf") ;

        if (!parse_list(&stk)) { free(wres) ; parse_error_return(0, 8, table) ; }

        if (stk.len)
            *field = parse_compute_list(wres, &stk, nfield, opts) ;
    }

    /* fold the optional dependencies into the depends list */
    if (dep->noptsdeps) {

        if (dep->ndepends) {
            size_t len = strlen(dep->sa.s + dep->depends) ;
            char t[len + strlen(dep->sa.s + dep->optsdeps) + 2] ;
            auto_strings(t, dep->sa.s + dep->depends, " ", dep->sa.s + dep->optsdeps) ;
            dep->depends = resolve_add_string(wres, t) ;

        } else {

            dep->depends = resolve_add_string(wres, dep->sa.s + dep->optsdeps) ;
        }
        dep->ndepends += dep->noptsdeps ;
    }

    /* a reactor depends on its From sources so the graph starts them before it
     * arms: a service/signal source is supervised before eventd subscribes to its
     * fifodir, a tick source (inotify/timer/schedule) is armed before its reactor
     * wants it. A user reactor is sourceless (no From) and never reaches this fold. */
    if (parse_store_present(st, E_PARSER_SECTION_EVENT, E_PARSER_SECTION_EVENT_EVENTTYPE) &&
        parse_store_present(st, E_PARSER_SECTION_EVENT, E_PARSER_SECTION_EVENT_FROM)) {

        size_t flen = 0 ;
        char const *fv = parse_store_get(st, E_PARSER_SECTION_EVENT, E_PARSER_SECTION_EVENT_FROM, &flen) ;

        _alloc_sbl_(strb, flen + 1) ;
        if (!strbuf_copyb(&strb, fv, flen))
            log_die_nomem("strbuf") ;

        if (!parse_list(&strb)) {
            free(wres) ;
            parse_error_return(0, 8, table) ;
        }

        if (strb.len) {

            uint32_t nfrom = 0 ;
            uint32_t from = parse_compute_list(wres, &strb, &nfrom, 0) ;

            if (dep->ndepends) {
                char t[strlen(dep->sa.s + dep->depends) + strlen(dep->sa.s + from) + 2] ;
                auto_strings(t, dep->sa.s + dep->depends, " ", dep->sa.s + from) ;
                dep->depends = resolve_add_string(wres, t) ;
            } else {
                char t[strlen(dep->sa.s + from) + 1] ;
                auto_strings(t, dep->sa.s + from) ;
                dep->depends = resolve_add_string(wres, t) ;
            }
            dep->ndepends += nfrom ;
        }
    }

    free(wres) ;

    return 1 ;
}
