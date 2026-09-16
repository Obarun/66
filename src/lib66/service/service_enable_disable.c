/*
 * service_enable_disable.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/service.h>
#include <66/constants.h>
#include <66/graph.h>
#include <66/resolve.h>
#include <66/tree.h>
#include <66/enum_parser.h>
#include <66/ssexec.h>
#include <66/symlink.h>

static bool isdone(hash_t *hres, const char *name)
{
    struct resolve_hash_s *t ;
    t = resolve_hash_search(hres, name) ;
    if (t == NULL)
        return false ;

    return t->visit == 1 ? true : false ;
}

static void mark_isdone(hash_t *hres, const char *name)
{
    struct resolve_hash_s *t ;
    t = resolve_hash_search(hres, name) ;
    t->visit = 1 ;
}

static void write_enabled(resolve_service_t *res, bool action, char const *base)
{
    log_flow() ;

    resolve_service_t fresh = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &fresh) ;
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_MAIN_ZERO ;

    table.u.service.id = E_RESOLVE_SERVICE_CONFIG_ENABLED ;
    res->enabled = action ? 1 : 0 ;

    if (!resolve_modify_field(wres, base, res->sa.s + res->name, table, action ? "1" : "0"))
        log_dieu(LOG_EXIT_SYS, "write resolve file of: ", res->sa.s + res->name) ;

    resolve_free(wres) ;
}

/** @action == false disable
 * @action == true enable */
void service_enable_disable(service_graph_t *g, struct resolve_hash_s *hash, bool action, ssexec_t *info, strbuf *argv)
{
    log_flow() ;

    if (!isdone(&g->hres, hash->name)) {

        resolve_service_t_ref res = &hash->res ;
        char const *treename = 0 ;
        bool same = sbl_search(argv, hash->name) >= 0 ? true : false ;
        bool ns = hash->res.inns ? true : false ;

        if (hash->dependencies.nprovide && action)
            if (!symlink_provide_update(info->base.s, res, SYMLINK_PROVIDE_ENABLE))
                log_dieu(LOG_EXIT_SYS, "make provide symlink") ;

        if (hash->dependencies.nconflict && action) {

            _alloc_sbl_(stk, strlen(hash->dependencies.sa.s + hash->dependencies.conflict)) ;
            size_t pos = 0 ;
            char sv[SS_MAX_SERVICE_NAME + 1] ;

            if (!sbl_clean_string(&stk, hash->dependencies.sa.s + hash->dependencies.conflict))
                log_dieu(LOG_EXIT_SYS, "clean string") ;

            FOREACH_SBL(&stk, pos) {

                if (!service_resolve_provide(sv, stk.s + pos, info->base.s))
                    log_dieusys(LOG_EXIT_SYS, "resolve provide alias: ", stk.s + pos) ;

                int r = service_isenabled(info->base.s, sv) ;
                if (r < 0)
                    log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", sv) ;

                if (r)
                    log_die(LOG_EXIT_SYS,"conflicting service for '", hash->res.sa.s + hash->res.name, "' -- please disable the '", sv, "' service first.") ;
            }
        }

        if (info->opt_tree && ((hash->res.inns && ns) || same))
            treename = info->treename.s ;
        else
            treename = res->sa.s + (res->intree ? res->intree : res->treename) ;

        /** resolve file may already exist. Be sure to add it to the contents field of the tree.*/
        if (action) {

            if (info->opt_tree && ((hash->res.inns && ns) || same))
                service_switch_tree(res, treename, info) ;
            else
                tree_service_add(treename, res->sa.s + res->name, info) ;
        }

        write_enabled(res, action, res->sa.s + res->path.home) ;

        /** the logger must be disabled to avoid to start it
         * with the 66 tree start <tree> command */
        if (res->logger && !action && res->type == E_PARSER_TYPE_CLASSIC && !res->inns) {

            char logname[strlen(res->sa.s + res->name) + SS_LOG_SUFFIX_LEN + 1] ;
            auto_strings(logname, res->sa.s + res->name, SS_LOG_SUFFIX) ;
            char *name = logname ;

            struct resolve_hash_s *h = resolve_hash_search(&g->hres, name) ;
            if (h == NULL)
                log_die(LOG_EXIT_USER, "service: ", name, " not available -- did you parse it?") ;

            if (!isdone(&g->hres, name)) {

                write_enabled(&h->res, action, h->res.sa.s + h->res.path.home) ;

                log_info("Disabled successfully: ", name) ;

                mark_isdone(&g->hres, name) ;
            }
        }

        if (res->type == E_PARSER_TYPE_MODULE) {

            if (hash->dependencies.ncontents) {

                service_graph_t graph = GRAPH_SERVICE_ZERO ;
                uint32_t nservice = 0, flag = GRAPH_WANT_DEPENDS|GRAPH_WANT_REQUIREDBY ;
                vertex_t *v, *tmp ;
                struct resolve_hash_s *h = NULL ;
                _alloc_sbl_(stk, strlen(hash->dependencies.sa.s + hash->dependencies.contents) + 1) ;

                if (!sbl_clean_string(&stk, hash->dependencies.sa.s + hash->dependencies.contents))
                    log_dieu(LOG_EXIT_SYS, "clean string") ;

                if (!service_graph_new(&graph, hash->dependencies.ncontents))
                    log_dieusys(LOG_EXIT_SYS, "allocate the graph") ;

                /** build the graph of the ns */
                nservice = service_graph_build_list(&graph, stk.s, stk.len, info, flag) ;

                if (!nservice)
                    log_dieu(LOG_EXIT_USER, "build the graph of the module: ", res->sa.s + res->name," -- please make a bug report") ;

                resolve_hash_reset_visit(&graph.hres) ;

                HASH_FOREACH(&graph.g.vertexes, v, tmp) {

                    char *name = v->name ;

                    h = resolve_hash_search(&graph.hres, name) ;
                    if (h == NULL)
                        log_die(LOG_EXIT_USER, "service: ", name, " not available -- did you parse it?") ;

                    if (!isdone(&g->hres, name)) {

                        if (action) {

                            if (info->opt_tree && (hash->res.inns || sbl_search(argv, hash->name) >= 0))
                                service_switch_tree(&h->res, treename, info) ;
                            else
                                tree_service_add(treename, h->res.sa.s + h->res.name, info) ;
                        }

                        write_enabled(&h->res, action, h->res.sa.s + h->res.path.home) ;

                        mark_isdone(&g->hres, h->res.sa.s + h->res.name) ;

                        log_info(!action ? "Disabled" : "Enabled"," successfully: ", h->res.sa.s + h->res.name) ;
                    }
                }
                service_graph_destroy(&graph) ;
            }
        }

        mark_isdone(&g->hres, hash->name) ;

        log_info(!action ? "Disabled" : "Enabled"," successfully: ", res->sa.s + res->name) ;
    }
}
