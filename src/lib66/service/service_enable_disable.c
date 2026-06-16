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
#include <66/graph.h>
#include <66/resolve.h>
#include <66/tree.h>
#include <66/enum_parser.h>
#include <66/ssexec.h>
#include <66/symlink.h>

static bool isdone(struct resolve_hash_s *hres, const char *name)
{
    struct resolve_hash_s *t ;
    t = hash_search(&hres, name) ;
    if (t == NULL)
        return false ;

    return t->visit == 1 ? true : false ;
}

static void mark_isdone(struct resolve_hash_s *hres, const char *name)
{
    struct resolve_hash_s *t ;
    t = hash_search(&hres, name) ;
    t->visit = 1 ;
}

/** @action == false disable
 * @action == true enable */
void service_enable_disable(service_graph_t *g, struct resolve_hash_s *hash, bool action, ssexec_t *info, strbuf *argv)
{
    log_flow() ;

    if (!isdone(g->hres, hash->name)) {

        resolve_service_t_ref res = &hash->res ;
        resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
        char const *treename = 0 ;
        bool same = sbl_search(argv, hash->name) >= 0 ? true : false ;
        bool ns = hash->res.inns ? true : false ;

        if (hash->res.dependencies.nprovide)
            if (!symlink_provide(info->base.s, res, action))
                log_dieu(LOG_EXIT_SYS, "make provide symlink") ;

        if (hash->res.dependencies.nconflict && action) {

            _alloc_sbl_(stk, strlen(hash->res.sa.s + hash->res.dependencies.conflict)) ;
            resolve_service_t c = RESOLVE_SERVICE_ZERO ;
            resolve_wrapper_t_ref w = resolve_set_struct(DATA_SERVICE, &c) ;
            size_t pos = 0 ;

            if (!sbl_clean_string(&stk, hash->res.sa.s + hash->res.dependencies.conflict))
                log_dieu(LOG_EXIT_SYS, "clean string") ;

            FOREACH_SBL(&stk, pos) {

                if (resolve_read_g(w, info->base.s, stk.s + pos) > 0 && c.enabled)
                    log_die(LOG_EXIT_SYS,"conflicting service for '", hash->res.sa.s + hash->res.name, "' -- please disable the '", c.sa.s + c.name, "' service first.") ;
            }

            resolve_free(w) ;
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

        res->enabled = action ? 1 : 0 ;

        if (!resolve_write_g(wres, res->sa.s + res->path.home, res->sa.s + res->name))
            log_dieu(LOG_EXIT_SYS, "write  resolve file of: ", res->sa.s + res->name) ;

        free(wres) ;

        /** the logger must be disabled to avoid to start it
         * with the 66 tree start <tree> command */
        if (res->logger.want && !action && res->type == E_PARSER_TYPE_CLASSIC && !res->inns) {

            char *name = res->sa.s + res->logger.name ;

            struct resolve_hash_s *h = hash_search(&g->hres, name) ;
            if (h == NULL)
                log_die(LOG_EXIT_USER, "service: ", name, " not available -- did you parse it?") ;

            if (!isdone(g->hres, name)) {

                wres = resolve_set_struct(DATA_SERVICE,  &h->res) ;

                h->res.enabled = action ? 1 : 0 ;

                if (!resolve_write_g(wres, h->res.sa.s + h->res.path.home, h->res.sa.s + h->res.name))
                    log_dieu(LOG_EXIT_SYS, "write  resolve file of: ", h->res.sa.s + h->res.name) ;

                log_info("Disabled successfully: ", name) ;

                mark_isdone(g->hres, name) ;

                free(wres) ;
            }
        }

        if (res->type == E_PARSER_TYPE_MODULE) {

            if (res->dependencies.ncontents) {

                service_graph_t graph = GRAPH_SERVICE_ZERO ;
                uint32_t nservice = 0, flag = GRAPH_WANT_DEPENDS|GRAPH_WANT_REQUIREDBY ;
                vertex_t *v, *tmp ;
                struct resolve_hash_s *h = NULL ;
                _alloc_sbl_(stk, strlen(res->sa.s + res->dependencies.contents) + 1) ;

                if (!sbl_clean_string(&stk, res->sa.s + res->dependencies.contents))
                    log_dieu(LOG_EXIT_SYS, "clean string") ;

                if (!graph_new(&graph, res->dependencies.ncontents))
                    log_dieusys(LOG_EXIT_SYS, "allocate the graph") ;

                /** build the graph of the ns */
                nservice = service_graph_build_list(&graph, stk.s, stk.len, info, flag) ;

                if (!nservice)
                    log_dieu(LOG_EXIT_USER, "build the graph of the module: ", res->sa.s + res->name," -- please make a bug report") ;

                hash_reset_visit(graph.hres) ;

                HASH_ITER(hh, graph.g.vertexes, v, tmp) {

                    char *name = v->name ;

                    h = hash_search(&graph.hres, name) ;
                    if (h == NULL)
                        log_die(LOG_EXIT_USER, "service: ", name, " not available -- did you parse it?") ;

                    if (!isdone(g->hres, name)) {

                        wres = resolve_set_struct(DATA_SERVICE,  &h->res) ;

                        if (action) {

                            if (info->opt_tree && (hash->res.inns || sbl_search(argv, hash->name) >= 0))
                                service_switch_tree(&h->res, treename, info) ;
                            else
                                tree_service_add(treename, h->res.sa.s + h->res.name, info) ;
                        }

                        h->res.enabled = action ? 1 : 0 ;

                        if (!resolve_write_g(wres, h->res.sa.s + h->res.path.home, h->res.sa.s + h->res.name))
                            log_dieu(LOG_EXIT_SYS, "write  resolve file of: ", h->res.sa.s + h->res.name) ;

                        mark_isdone(g->hres, h->res.sa.s + h->res.name) ;

                        log_info(!action ? "Disabled" : "Enabled"," successfully: ", h->res.sa.s + h->res.name) ;

                        free(wres) ;
                    }
                }
                service_graph_destroy(&graph) ;
            }
        }

        mark_isdone(g->hres, hash->name) ;

        log_info(!action ? "Disabled" : "Enabled"," successfully: ", res->sa.s + res->name) ;
    }
}
