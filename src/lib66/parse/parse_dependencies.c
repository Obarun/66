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
#include <string.h>
#include <sys/types.h> // ssize_t

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/utils.h>
#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>
#include <66/event_rule.h>

static int event_from_is_supervised(char const *type)
{
    if (!type)
        return 0 ;
    int src = event_src_from_string(type) ;
    return src == EVENT_SOURCE_SERVICE || src == EVENT_SOURCE_SIGNAL ;
}

int parse_dependencies(parse_store_t *st, resolve_service_addon_dependencies_t *dep, char const *service)
{
    log_flow() ;

    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_SERVICE_DEPENDENCIES, dep) ;
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_MAIN_ZERO ;

    for (uint32_t kid = 0 ; kid < E_PARSER_SECTION_MAIN_ENDOFKEY ; kid++) {

        if (!parse_store_present(st, E_PARSER_SECTION_MAIN, kid))
            continue ;

        table.u.parser.id = kid ;
        size_t len = 0 ;
        char const *v = parse_store_get(st, E_PARSER_SECTION_MAIN, kid, &len) ;

        uint32_t *field = 0, *nfield = 0 ;
        uint8_t opts = 0, alias = 0 ;

        switch (kid) {

            case E_PARSER_SECTION_MAIN_DEPENDS:    field = &dep->depends ;    nfield = &dep->ndepends ;    break ;
            case E_PARSER_SECTION_MAIN_REQUIREDBY: field = &dep->requiredby ; nfield = &dep->nrequiredby ; break ;
            case E_PARSER_SECTION_MAIN_OPTSDEPS:   field = &dep->optsdeps ;   nfield = &dep->noptsdeps ;   opts = 1 ; break ;
            case E_PARSER_SECTION_MAIN_PROVIDE:    field = &dep->provide ;    nfield = &dep->nprovide ;    alias = 1 ; break ;
            case E_PARSER_SECTION_MAIN_CONFLICT:   field = &dep->conflict ;   nfield = &dep->nconflict ;   break ;

            default: // core/execute/io keys of [Main] -- not ours
                continue ;
        }

        _alloc_sbl_(stk, len + 1) ;

        int r = strbuf_copyb(&stk, v, len) ? parse_list(&stk) : -1 ;
        if (r <= 0) {
            if (r < 0)
                return -1 ;
            parse_error_return(0, 8, table) ;
        }

        {
            size_t pos = 0 ;

            FOREACH_SBL(&stk, pos) {

                if (stk.s[pos] == '#')
                    continue ;

                if (get_rstrlen_until(stk.s + pos, SS_LOG_SUFFIX) >= 0) {
                    log_warn_return(LOG_EXIT_ZERO, "service: ", service, " names: ", stk.s + pos, " -- the suffix " SS_LOG_SUFFIX " is reserved to the loggers") ;
                }

                if (strlen(stk.s + pos) > SS_MAX_SERVICE_NAME) {
                    log_warn_return(LOG_EXIT_ZERO, "service: ", service, " names: ", stk.s + pos, " -- the name is too long") ;
                }
            }
        }

        if (alias) {

            size_t len = stk.len ;

            (void)sbl_remove(&stk, service) ;

            if (stk.len != len)
                log_warn("service: ", service, " provides its own name -- ignoring it") ;
        }

        if (stk.len) {

            int off = parse_compute_list(&wres, &stk, nfield, opts) ;
            if (off < 0) {
                return -1 ;
            }

            *field = (uint32_t)off ;
        }
    }

    // fold the optional dependencies into the depends list
    if (dep->noptsdeps) {

        ssize_t off ;

        if (dep->ndepends) {
            size_t len = strlen(dep->sa.s + dep->depends) ;
            char t[len + strlen(dep->sa.s + dep->optsdeps) + 2] ;
            auto_strings(t, dep->sa.s + dep->depends, " ", dep->sa.s + dep->optsdeps) ;
            off = resolve_add_string(&wres, t) ;

        } else {

            off = dep->optsdeps ;
        }

        if (off < 0) {
            return -1 ;
        }

        dep->depends = (uint32_t)off ;
        dep->ndepends += dep->noptsdeps ;
    }

    /** a tick reactor (inotify/timer/schedule) depends on its From source so the
     * graph arms the passive watch before the reactor wants it: fold From into
     * depends. The From of a service or signal reactor is an edge
     * SERVICE_GRAPH_EDGE_FROM of the graph, read from the event addon, never a
     * depend, so a Do=start reaction never pulls (and revives) its source. A
     * user reactor is sourceless (no From) and never reaches this fold. */
    if (parse_store_present(st, E_PARSER_SECTION_EVENT, E_PARSER_SECTION_EVENT_EVENTTYPE) &&
        parse_store_present(st, E_PARSER_SECTION_EVENT, E_PARSER_SECTION_EVENT_FROM) &&
        !event_from_is_supervised(parse_store_get(st, E_PARSER_SECTION_EVENT, E_PARSER_SECTION_EVENT_EVENTTYPE, 0))) {

        size_t flen = 0 ;
        char const *fv = parse_store_get(st, E_PARSER_SECTION_EVENT, E_PARSER_SECTION_EVENT_FROM, &flen) ;

        _alloc_sbl_(strb, flen + 1) ;

        int r = strbuf_copyb(&strb, fv, flen) ? parse_list(&strb) : -1 ;
        if (r <= 0) {
            if (r < 0)
                return -1 ;
            table = (resolve_enum_table_t)E_TABLE_PARSER_SECTION_EVENT_ZERO ;
            table.u.parser.id = E_PARSER_SECTION_EVENT_FROM ;
            parse_error_return(0, 8, table) ;
        }

        if (strb.len) {

            uint32_t nfrom = 0 ;
            ssize_t off ;
            int from = parse_compute_list(&wres, &strb, &nfrom, 0) ;

            if (from < 0) {
                return -1 ;
            }

            if (dep->ndepends) {
                char t[strlen(dep->sa.s + dep->depends) + strlen(dep->sa.s + from) + 2] ;
                auto_strings(t, dep->sa.s + dep->depends, " ", dep->sa.s + from) ;
                off = resolve_add_string(&wres, t) ;
            } else {
                off = from ;
            }

            if (off < 0) {
                return -1 ;
            }

            dep->depends = (uint32_t)off ;
            dep->ndepends += nfrom ;
        }
    }

    return 1 ;
}
