/*
 * service_enable_disable.c
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

#include <stdint.h>
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/graph.h>
#include <oblibs/stack.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/service.h>
#include <66/graph.h>
#include <66/state.h>
#include <66/enum.h>
#include <66/ssexec.h>

static void service_enable_disable_deps(graph_t *g, struct resolve_hash_s *hash, struct resolve_hash_s **hres, uint8_t action, uint8_t propagate, ssexec_t *info)
{
    log_flow() ;

    size_t pos = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_service_t_ref res = &hash->res ;

    if (graph_matrix_get_edge_g_sa(&sa, g, res->sa.s + res->name, action ? 0 : 1, 0) < 0)
        log_dieu(LOG_EXIT_SYS, "get ", action ? "dependencies" : "required by" ," of: ", res->sa.s + res->name) ;

    if (sa.len) {

        FOREACH_SASTR(&sa, pos) {

            char *name = sa.s + pos ;

            struct resolve_hash_s *h = hash_search(hres, name) ;
            if (h == NULL)
                log_die(LOG_EXIT_USER, "service: ", name, " not available -- did you parse it?") ;

            if (!h->visit) {
                service_enable_disable(g, h, hres, action, propagate, info) ;
                h->visit = 1 ;
            }
        }
    }

    stralloc_free(&sa) ;
}

/** @action -> 0 disable
 * @action -> 1 enable */
void service_enable_disable(graph_t *g, struct resolve_hash_s *hash, struct resolve_hash_s **hres, uint8_t action, uint8_t propagate, ssexec_t *info)
{
    log_flow() ;

    if (!hash->visit) {

        resolve_service_t_ref res = &hash->res ;
        resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
        char const *treename = 0 ;
        if (info->opt_tree)
            treename = info->treename.s ;
        else
            treename = res->sa.s + (res->intree ? res->intree : res->treename) ;

        /** resolve file may already exist. Be sure to add it to the contents field of the tree.*/
        if (action) {

            if (info->opt_tree)
                service_switch_tree(res, res->sa.s + res->path.home, treename, info) ;
            else
                tree_service_add(treename, res->sa.s + res->name, info) ;
        }

        res->enabled = action ;

        if (!resolve_write_g(wres, res->sa.s + res->path.home, res->sa.s + res->name))
            log_dieu(LOG_EXIT_SYS, "write  resolve file of: ", res->sa.s + res->name) ;

        if (propagate)
            service_enable_disable_deps(g, hash, hres, action, propagate, info) ;

        free(wres) ;

        /** the logger must be disabled to avoid to start it
         * with the 66 tree start <tree> command */
        if (res->logger.want && !action && res->type == TYPE_CLASSIC && !res->inns) {

            char *name = res->sa.s + res->logger.name ;

            struct resolve_hash_s *h = hash_search(hres, name) ;
            if (h == NULL)
                log_die(LOG_EXIT_USER, "service: ", name, " not available -- did you parse it?") ;

            if (!h->visit) {

                wres = resolve_set_struct(DATA_SERVICE,  &h->res) ;

                h->res.enabled = action ;

                if (!resolve_write_g(wres, h->res.sa.s + h->res.path.home, h->res.sa.s + h->res.name))
                    log_dieu(LOG_EXIT_SYS, "write  resolve file of: ", h->res.sa.s + h->res.name) ;

                log_info("Disabled successfully: ", name) ;

                h->visit = 1 ;

                resolve_free(wres) ;
            }
        }

        if (res->type == TYPE_MODULE) {

            if (res->dependencies.ncontents) {

                size_t pos = 0 ;
                _init_stack_(stk, strlen(res->sa.s + res->dependencies.contents)) ;

                if (!stack_clean_string_g(&stk, res->sa.s + res->dependencies.contents))
                    log_dieu(LOG_EXIT_SYS, "clean string") ;

                FOREACH_STK(&stk, pos) {

                    char *name = stk.s + pos ;

                    struct resolve_hash_s *h = hash_search(hres, name) ;
                    if (h == NULL)
                        log_die(LOG_EXIT_USER, "service: ", name, " not available -- did you parse it?") ;

                    if (!h->visit) {

                        wres = resolve_set_struct(DATA_SERVICE,  &h->res) ;

                        if (action) {

                            if (info->opt_tree)
                                service_switch_tree(&h->res, h->res.sa.s + h->res.path.home, treename, info) ;
                            else
                                tree_service_add(treename, h->res.sa.s + h->res.name, info) ;
                        }

                        h->res.enabled = action ;

                        if (!resolve_write_g(wres, h->res.sa.s + h->res.path.home, h->res.sa.s + h->res.name))
                            log_dieu(LOG_EXIT_SYS, "write  resolve file of: ", h->res.sa.s + h->res.name) ;

                        service_enable_disable_deps(g, h, hres, action, propagate, info) ;

                        h->visit = 1 ;

                        log_info(!action ? "Disabled" : "Enabled"," successfully: ", h->res.sa.s + h->res.name) ;

                        resolve_free(wres) ;
                    }
                }
            }
        }

        hash->visit = 1 ;

        log_info(!action ? "Disabled" : "Enabled"," successfully: ", hash->res.sa.s + hash->res.name) ;
    }
}
