/*
 * sanitize_resolve.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copiextern sanitize_resolveed, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/hash.h>

#include <66/graph.h>
#include <66/ssexec.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>
#include <66/constants.h>

static int sanitize_service(ssexec_t *info)
{
    log_flow() ;

    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t flag = GRAPH_WANT_DEPENDS|GRAPH_WANT_REQUIREDBY, nservice = 0 ;
    struct resolve_hash_s *c, *tmp ;
    vertex_t *v = NULL ;
    resolve_wrapper_t_ref wres = 0 ;

    log_trace("sanitize services resolve files") ;

    if (!service_graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_warnusys_return(LOG_EXIT_ZERO, "allocate the service graph") ;

    /** build the graph of the entire system */
    nservice = service_graph_build_system(&graph, info, flag) ;

    if (!nservice && errno == EINVAL)
        log_warnusys_return(LOG_EXIT_ZERO, "build system graph -- please make a bug report") ;

    HASH_FOREACH(&graph.hres, c, tmp) {

        wres = resolve_set_struct(DATA_SERVICE, &c->res) ;
        char name[strlen(c->res.sa.s + c->res.name) + 1] ;
        auto_strings(name, c->res.sa.s + c->res.name) ;

        v = hash_find(&graph.g.vertexes, name, strlen(name)) ;
        if (v == NULL)
            log_warnusys_return(LOG_EXIT_ZERO, "get information of service: ", name, " -- please make a bug report") ;

        service_resolve_sanitize(&c->res) ;

        if (!resolve_write(wres, info->base.s, name))
            log_warnusys_return(LOG_EXIT_ZERO, "write resolve file of service: ", name) ;
    }

    service_graph_destroy(&graph) ;
    free(wres) ;

    return 1 ;
}

static int sanitize_tree(ssexec_t *info)
{
    log_flow() ;

    tree_graph_t graph = GRAPH_TREE_ZERO ;
    uint32_t flag = GRAPH_WANT_DEPENDS|GRAPH_WANT_REQUIREDBY, ntree = 0 ;
    struct resolve_hash_tree_s *c, *tmp ;
    vertex_t *v = NULL ;
    resolve_wrapper_t_ref wres = 0 ;

    log_trace("sanitize trees resolve files") ;

    if (!tree_graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_warnusys_return(LOG_EXIT_ZERO, "allocate the tree graph") ;

    /** build the graph of the entire system */
    ntree = tree_graph_build_system(&graph, info, flag) ;

    if (!ntree && errno == EINVAL)
        log_warnusys_return(LOG_EXIT_ZERO, "build system graph -- please make a bug report") ;

    HASH_FOREACH(&graph.hres, c, tmp) {

        wres = resolve_set_struct(DATA_TREE, &c->tres) ;
        char name[strlen(c->tres.sa.s + c->tres.name) + 1] ;
        auto_strings(name, c->tres.sa.s + c->tres.name) ;

        v = hash_find(&graph.g.vertexes, name, strlen(name)) ;
        if (v == NULL)
            log_warnusys_return(LOG_EXIT_ZERO, "get information of tree: ", name, " -- please make a bug report") ;

        tree_resolve_sanitize(&c->tres) ;

        if (!resolve_write(wres, info->base.s, name))
            log_warnusys_return(LOG_EXIT_ZERO, "write resolve file of tree: ", name) ;
    }

    tree_graph_destroy(&graph) ;
    free(wres) ;

    return 1 ;
}

static int sanitize_tree_master(ssexec_t *info)
{
    log_flow() ;

    log_trace("sanitize Master resolve") ;

    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;

    if (resolve_read(wres, info->base.s, SS_MASTER + 1) <= 0)
        log_warnusys_return(LOG_EXIT_ZERO, "read Master resolve file") ;

    tree_resolve_master_sanitize(&mres) ;

    if (!resolve_write(wres, info->base.s, SS_MASTER + 1))
        log_warnusys_return(LOG_EXIT_ZERO, "write Master resolve file") ;

    resolve_free(wres) ;

    return 1 ;
}

int sanitize_resolve(ssexec_t *info, uint8_t type)
{
    log_flow() ;

    if (type == DATA_SERVICE)
        return sanitize_service(info) ;

    if (type == DATA_TREE)
        return sanitize_tree(info) ;

    if (type == DATA_TREE_MASTER)
        return sanitize_tree_master(info) ;

    return 1 ;
}