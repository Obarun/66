/*
 * service_graph_resolve.c
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
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/hash.h>
#include <oblibs/lexer.h>

#include <66/graph.h>
#include <66/service.h>
#include <66/state.h>
#include <66/enum_parser.h>

static int graph_action(service_graph_t *g, resolve_service_t *res, uint32_t flag) ;
static int graph_action_logger(service_graph_t *g, resolve_service_t *res, uint32_t flag) ;
static int graph_build_module(service_graph_t *g, resolve_service_t *res, uint32_t flag) ;

static bool issupervised(resolve_service_t *res)
{
    log_flow() ;

    char *name = res->sa.s + res->name ;
    ss_state_t ste = STATE_ZERO ;

    if (!state_check(res) || !state_read(&ste, res)) {
        log_warnu("get state of service: ", name) ;
        return (errno = EINVAL, false) ;
    }

    if (ste.issupervised == STATE_FLAGS_FALSE) {
        log_warn("requested flags issupervised where service ", name, " is not -- ignoring it") ;
        return false ;
    }

    return true ;
}

static int issupervised_list(service_graph_t *g, stack *stk)
{
    log_flow() ;

    size_t pos = 0 ;
    _alloc_stk_(c, stk->len) ;

    FOREACH_STK(stk, pos) {

        char *name = stk->s + pos ;
        struct resolve_hash_s *h = hash_search(&g->hres, name) ;
        if (h == NULL || !issupervised(&h->res)) {
            if (errno == EINVAL)
                return 0 ;

            continue ;
        }

        if (!stack_add_g(&c, name))
            return 0 ;
    }

    if (!stack_close(&c))
        return 0 ;

    if (!stack_copy_stack(stk,&c))
        return 0 ;

    return 1 ;
}

static bool isdone(service_graph_t *g, const char *name)
{
    vertex_t *v = NULL ;
    HASH_FIND_STR(g->g.vertexes, name, v) ;
    if (v)
        return true ;

    return false ;
}

static int graph_add_depends(service_graph_t *g, const char *vertex, stack *edge, uint32_t flag, bool requiredby)
{
    log_flow() ;

    _alloc_stk_(stk, edge->len) ;

    if (!stack_copy_stack(&stk, edge))
        return 0 ;

    if (FLAGS_ISSET(flag, GRAPH_WANT_EARLIER)) {

        struct resolve_hash_s *h = hash_search(&g->hres, vertex) ;
        if (!h)
            log_warnu_return(LOG_EXIT_ZERO, "get information of service: ", vertex, " -- please make a bug report.") ;

        if (!h->res.earlier)
            return 1 ;
    }

    if (FLAGS_ISSET(flag, GRAPH_WANT_SUPERVISED))
        if (!issupervised_list(g, &stk))
            return 0 ;

    log_trace("add ", !requiredby ? "depends" : "requiredby", " of service: ", vertex) ;
    if (!graph_add_nedge(&g->g, vertex, &stk, requiredby, true))
        log_warnu_return(LOG_EXIT_ZERO, "add ", !requiredby ? "depends" : "requiredby", " of service: ", vertex) ;

    return 1 ;
}

static int graph_action_add(service_graph_t *g, resolve_service_t *res, uint32_t flag)
{
    log_flow() ;

    char *name = res->sa.s + res->name ;

    if (isdone(g, name) || (flag == /* flag only content */ GRAPH_WANT_LOGGER && !res->islog))
        return 1 ;

    if (FLAGS_ISSET(flag, GRAPH_WANT_EARLIER)) {

        if (res->earlier) {
            log_trace("add earlier service: ", name, " to the graph") ;
            if (!graph_add(&g->g, name)) {
                log_warnusys("add service: ", name) ;
                return (errno = EINVAL, 0) ;
            }
        }

        return 1 ;

    } else if (FLAGS_ISSET(flag, GRAPH_SKIP_EARLIER)) {

        if (res->earlier)
            return -1 ;
    }

    if (FLAGS_ISSET(flag, GRAPH_WANT_SUPERVISED)) {

        bool b = issupervised(res) ;
        if (!b) {
            if (errno == EINVAL)
                return 0 ;

            return -1 ;
        }
    }

    log_trace("add service: ", name, " to the graph") ;
    if (!graph_add(&g->g, name)) {
        log_warnusys("add service: ", name) ;
        return (errno = EINVAL, 0) ;
    }

    return 1 ;
}

static int graph_action_depends(service_graph_t *g, resolve_service_t *res, uint32_t flag)
{
    log_flow() ;

    size_t pos = 0 ;
    struct resolve_hash_s *h = NULL ;

    if (FLAGS_ISSET(flag, GRAPH_WANT_DEPENDS) && res->dependencies.ndepends) {

        _alloc_stk_(stk, strlen(res->sa.s + res->dependencies.depends) + 1) ;

        if (!stack_string_clean(&stk, res->sa.s + res->dependencies.depends))
            log_warnusys_return(LOG_EXIT_ZERO, "clean string") ;

        if (!graph_add_depends(g, res->sa.s + res->name, &stk, flag, false)) {
            if (errno == EINVAL)
                return 0 ;

            return 1 ;
        }

        // do it recursively
        FOREACH_STK(&stk, pos) {

            h = hash_search(&g->hres, stk.s + pos) ;
            if (h == NULL)
                log_warnusys_return(LOG_EXIT_ZERO,"get information of service: ", stk.s + pos) ;

            if (FLAGS_ISSET(flag, GRAPH_WANT_LOGGER))
                if (!graph_action_logger(g, &h->res, flag))
                    return 0 ;

            if (!graph_action(g, &h->res, flag))
                return 0 ;
        }
    }

    return 1 ;
}

static int graph_action_requiredby(service_graph_t *g, resolve_service_t *res, uint32_t flag)
{
    log_flow() ;

    size_t pos = 0 ;
    struct resolve_hash_s *h = NULL ;

    if (FLAGS_ISSET(flag, GRAPH_WANT_REQUIREDBY) && res->dependencies.nrequiredby) {

        _alloc_stk_(stk, strlen(res->sa.s + res->dependencies.requiredby) + 1) ;

        if (!stack_string_clean(&stk, res->sa.s + res->dependencies.requiredby))
            log_warnusys_return(LOG_EXIT_ZERO, "clean string") ;

        if (!graph_add_depends(g, res->sa.s + res->name, &stk, flag, true)) {
            if (errno == EINVAL)
                return 0 ;

            return 1 ;
        }

        // do it recursively
        FOREACH_STK(&stk, pos) {

            h = hash_search(&g->hres, stk.s + pos) ;
            if (h == NULL)
                log_warnusys_return(LOG_EXIT_ZERO,"get information of service: ", stk.s + pos) ;

            if (FLAGS_ISSET(flag, GRAPH_WANT_LOGGER))
                if (!graph_action_logger(g, &h->res, flag))
                    return 0 ;

            if (!graph_action(g, &h->res, flag))
                return 0 ;
        }
    }

    return 1 ;
}

static int graph_action_logger(service_graph_t *g, resolve_service_t *res, uint32_t flag)
{
    log_flow() ;

    if (res->type == E_PARSER_TYPE_CLASSIC && res->logger.want && !FLAGS_ISSET(flag, GRAPH_WANT_EARLIER)) {

        if (isdone(g, res->sa.s + res->logger.name) || res->earlier)
            return 1 ;

        if (FLAGS_ISSET(flag, GRAPH_WANT_SUPERVISED)) {
            bool b = issupervised(res) ;
            if (!b) {
                if (errno == EINVAL)
                    return 0 ;

                return 1 ;
            }
        }

        struct resolve_hash_s *h = hash_search(&g->hres, res->sa.s + res->logger.name) ;
        if (h == NULL)
            return (errno = EINVAL, 0) ;

        log_trace("add logger: ", res->sa.s + res->logger.name, " of service: ", res->sa.s + res->name, " to the graph") ;
        if (!graph_action(g, &h->res, flag))
             return (errno = EINVAL, 0) ;
    }

    return 1 ;
}

static int graph_build_module(service_graph_t *g, resolve_service_t *res, uint32_t flag)
{
    log_flow() ;

    size_t pos = 0 ;
    struct resolve_hash_s *h = NULL ;
    if (res->type == E_PARSER_TYPE_MODULE && res->dependencies.ncontents) {

        _alloc_stk_(stk, strlen(res->sa.s + res->dependencies.contents)) ;

        if (!stack_string_clean(&stk, res->sa.s + res->dependencies.contents))
            log_warnusys_return(LOG_EXIT_ZERO, "clean string") ;

        FOREACH_STK(&stk, pos) {

            h = hash_search(&g->hres, stk.s + pos) ;
            if (h == NULL)
                log_warnu_return(LOG_EXIT_ZERO, "find service of module: ", stk.s + pos, " -- please make a bug report.") ;

            if (!graph_action(g, &h->res, flag))
                return 0 ;

        }
    }
    return 1 ;
}

static int graph_action(service_graph_t *g, resolve_service_t *res, uint32_t flag)
{
    log_flow() ;

    int r ;
    char *name = res->sa.s + res->name ;
    struct resolve_hash_s *h = hash_search(&g->hres, name) ;

    if (h->visit)
        return 1 ;

    h->visit = 1 ;

    log_trace("compute service: ", name, " for the graph") ;
    r = graph_action_add(g, res, flag) ;
    if (!r)
        log_warnu_return(LOG_EXIT_ZERO, "include service: ", name, " in graph selection") ;

    if (r < 0)
        // GRAPH_WANT_SUPERVISED|GRAPH_SKIP_EARLIER requested
        return 1 ;

    if (FLAGS_ISSET(flag, GRAPH_WANT_DEPENDS)) {
        log_trace("compute dependencies of service: ", name) ;
        if (!graph_action_depends(g, res, flag))
            log_warnu_return(LOG_EXIT_ZERO, "include dependencies of service: ", name, " in graph selection") ;
    }

    if (FLAGS_ISSET(flag, GRAPH_WANT_REQUIREDBY)) {
        log_trace("compute requiredby dependencies of service: ", name) ;
        if (!graph_action_requiredby(g, res, flag))
            log_warnu_return(LOG_EXIT_ZERO, "include requiredby of service: ", name, " in graph selection") ;
    }

    if (FLAGS_ISSET(flag, GRAPH_WANT_LOGGER)) {
        log_trace("compute logger of service: ", name) ;
        if (!graph_action_logger(g, res, flag))
            log_warnu_return(LOG_EXIT_ZERO, "include logger of service: ", name, " in graph selection") ;
    }

    if (!graph_build_module(g, &h->res, flag))
        return 0 ;

    return 1 ;
}

static int sanitize_module_service(service_graph_t *g, uint32_t flag)
{
    log_flow() ;

    if (!FLAGS_ISSET(flag, GRAPH_SKIP_MODULECONTENTS))
        return 1 ;

    vertex_t *c, *tmp, *ns ;
    struct resolve_hash_s *h = NULL ;
    HASH_ITER(hh, g->g.vertexes, c, tmp) {
        char *name = c->name ;
        h = hash_search(&g->hres, name) ;
        if (h == NULL)
            log_warnusys_return(LOG_EXIT_ZERO, "get information of service: ", name) ;

        if (h->res.inns) {
            HASH_FIND_STR(g->g.vertexes, h->res.sa.s + h->res.inns, ns) ;
            if (ns != NULL) {
                if (!graph_remove_vertex(&g->g, name, false, false))
                    return 0 ;
            }
        }
    }

    return 1 ;
}

int service_graph_resolve(service_graph_t *g, const char *name, uint32_t flag)
{
    log_flow() ;

    bool reverse = false ;
    struct resolve_hash_s *h = hash_search(&g->hres, name) ;
    if (h == NULL)
        log_warnusys_return(LOG_EXIT_ZERO, "get information of service: ", name) ;

    if (!graph_action(g, &h->res, flag))
        return 0 ;

    if (!sanitize_module_service(g, flag))
        log_warnu_return(LOG_EXIT_ZERO, "sanitize module service") ;

    // Flag can contain both. In this case, fall back to the depends behavior.
    if (FLAGS_ISSET(flag, GRAPH_WANT_REQUIREDBY) && !FLAGS_ISSET(flag, GRAPH_WANT_DEPENDS))
        reverse = true ;

    if (!graph_sort(&g->g, reverse))
        log_warnu_return(LOG_EXIT_ZERO, "sort the graph") ;

    return 1 ;
}

int service_graph_nresolve(service_graph_t *g, const char *list, size_t len, uint32_t flag)
{
    log_flow() ;

    bool reverse = false ;
    size_t pos = 0 ;
    struct resolve_hash_s *h = NULL ;

    for (; pos < len ; pos += strlen(list + pos) + 1) {

        h = hash_search(&g->hres, list + pos) ;
        if (h == NULL)
            log_warnusys_return(LOG_EXIT_ZERO, "get information of service: ", list + pos) ;

        if (!graph_action(g, &h->res, flag))
            return 0 ;
    }

    if (!sanitize_module_service(g, flag))
        log_warnu_return(LOG_EXIT_ZERO, "sanitize module service") ;

    // Flag can contain both. In this case, fall back to the depends behavior.
    if (FLAGS_ISSET(flag, GRAPH_WANT_REQUIREDBY) && !FLAGS_ISSET(flag, GRAPH_WANT_DEPENDS))
        reverse = true ;

    if (!graph_sort(&g->g, reverse))
        log_warnu_return(LOG_EXIT_ZERO, "sort the graph") ;

    return 1 ;
}
