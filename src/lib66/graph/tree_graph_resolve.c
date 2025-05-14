/*
 * tree_graph_resolve.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
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
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include <oblibs/graph.h>
#include <oblibs/hash.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/lexer.h>
#include <oblibs/stack.h>

#include <66/resolve.h>
#include <66/tree.h>
#include <66/graph.h>

static int graph_action(tree_graph_t *g, resolve_tree_t *tres, uint32_t flag) ;

static bool isdone(tree_graph_t *g, const char *name)
{
    vertex_t *v = NULL ;
    HASH_FIND_STR(g->g.vertexes, name, v) ;
    if (v)
        return true ;

    return false ;
}

static int graph_action_add(tree_graph_t *g, resolve_tree_t *tres, uint32_t flag)
{
    log_flow() ;

    char *treename = tres->sa.s + tres->name ;
    if (isdone(g, treename))
        return 1 ;

    if (FLAGS_ISSET(flag, GRAPH_WANT_ENABLED)) {

        if (!tres->enabled)
            return -1 ;
    }

    log_trace("add tree: ", treename, " to the graph") ;
    if (!graph_add(&g->g, treename)) {
        log_warnusys("add tree: ", treename) ;
        return (errno = EINVAL, 0) ;
    }

    return 1 ;
}

static int graph_action_depends(tree_graph_t *g, resolve_tree_t *tres, uint32_t flag)
{
    log_flow() ;

    char *treename = tres->sa.s + tres->name ;
    size_t pos = 0 ;
    struct resolve_hash_tree_s *h = NULL ;

    if (FLAGS_ISSET(flag, GRAPH_WANT_DEPENDS) && tres->ndepends) {

        _alloc_stk_(stk, strlen(tres->sa.s + tres->depends) + 1) ;

        if (!stack_string_clean(&stk, tres->sa.s + tres->depends))
            log_warnusys_return(LOG_EXIT_ZERO, "clean string") ;

        if (!graph_add_nedge(&g->g, treename, &stk, /*requiredby*/false, true))
            log_warnu_return(LOG_EXIT_ZERO, "add depends of tree: ", treename) ;

        // do it recursively
        FOREACH_STK(&stk, pos) {

            h = hash_search_tree(&g->hres, stk.s + pos) ;
            if (h == NULL)
                log_warnu_return(LOG_EXIT_ZERO, "find dependencies of tree: ", stk.s + pos, " -- please make a bug report.") ;

            if (isdone(g, stk.s + pos))
                continue ;

            if (!graph_action(g, &h->tres, flag))
                return 0 ;
        }
    }

    return 1 ;
}

static int graph_action_requiredby(tree_graph_t *g, resolve_tree_t *tres, uint32_t flag)
{
    log_flow() ;

    char *treename = tres->sa.s + tres->name ;
    size_t pos = 0 ;
    struct resolve_hash_tree_s *h = NULL ;

    if (FLAGS_ISSET(flag, GRAPH_WANT_REQUIREDBY) && tres->nrequiredby) {

        _alloc_stk_(stk, strlen(tres->sa.s + tres->requiredby) + 1) ;

        if (!stack_string_clean(&stk, tres->sa.s + tres->requiredby))
            log_warnusys_return(LOG_EXIT_ZERO, "clean string") ;

        if (!graph_add_nedge(&g->g, treename, &stk, /*requiredby*/true, true))
            log_warnu_return(LOG_EXIT_ZERO, "add requiredby dependencies of tree: ", treename) ;

        // do it recursively
        FOREACH_STK(&stk, pos) {

            h = hash_search_tree(&g->hres, stk.s + pos) ;
            if (h == NULL)
                log_warnu_return(LOG_EXIT_ZERO, "find requiredby dependencies of tree: ", stk.s + pos, " -- please make a bug report.") ;

            if (isdone(g, stk.s + pos))
                continue ;

            if (!graph_action(g, &h->tres, flag))
                return 0 ;
        }
    }

    return 1 ;
}

static int graph_action(tree_graph_t *g, resolve_tree_t *tres, uint32_t flag)
{
    log_flow() ;

    int r ;
    char *treename = tres->sa.s + tres->name ;
    struct resolve_hash_tree_s *h = hash_search_tree(&g->hres, treename) ;

    if (h->visit)
        return 1 ;

    h->visit = 1 ;

    log_trace("compute tree: ", treename, " for the graph") ;
    r = graph_action_add(g, tres, flag) ;
    if (!r)
        return 0 ;

    if (r < 0)
        // GRAPH_WANT_ENABLED requested
        return 1 ;

    if (FLAGS_ISSET(flag, GRAPH_WANT_DEPENDS)) {
        log_trace("compute dependencies of tree: ", treename) ;
        if (!graph_action_depends(g, tres, flag))
            log_warnu_return(LOG_EXIT_ZERO, "include dependencies of tree: ", treename, " in graph selection") ;
    }

    if (FLAGS_ISSET(flag, GRAPH_WANT_REQUIREDBY)) {
        log_trace("compute requiredby dependencies of tree: ", treename) ;
        if (!graph_action_requiredby(g, tres, flag))
            log_warnu_return(LOG_EXIT_ZERO, "include requiredby of tree: ", treename, " in graph selection") ;
    }

    return 1 ;
}

int tree_graph_resolve(tree_graph_t *g, const char *treename, uint32_t flag)
{
    log_flow() ;

    bool reverse = false ;
    struct resolve_hash_tree_s *h = hash_search_tree(&g->hres, treename) ;
    if (h == NULL)
        log_warnu_return(LOG_EXIT_ZERO, "find tree: ", treename, " -- please make a bug report") ;

    if (!graph_action(g, &h->tres, flag))
        return 0 ;

    // Flag can contain both. In this case, fall back to the depends behavior.
    if (FLAGS_ISSET(flag, GRAPH_WANT_REQUIREDBY) && !FLAGS_ISSET(flag, GRAPH_WANT_DEPENDS))
        reverse = true ;

    if (!graph_sort(&g->g, reverse))
        log_warnu_return(LOG_EXIT_ZERO, "sort the graph") ;

    return 1 ;
}

int tree_graph_nresolve(tree_graph_t *g, const char *list, size_t len, uint32_t flag)
{
    log_flow() ;

    bool reverse = false ;
    size_t pos = 0 ;
    struct resolve_hash_tree_s *h = NULL ;

    for (; pos < len ; pos += strlen(list + pos) + 1) {

        h = hash_search_tree(&g->hres, list + pos) ;
        if (h == NULL)
            log_warnusys_return(LOG_EXIT_ZERO, "find id of tree: ", list + pos) ;

        if (!graph_action(g, &h->tres, flag))
            return 0 ;
    }

    // Flag can contain both. In this case, fall back to the depends behavior.
    if (FLAGS_ISSET(flag, GRAPH_WANT_REQUIREDBY) && !FLAGS_ISSET(flag, GRAPH_WANT_DEPENDS))
        reverse = true ;

    if (!graph_sort(&g->g, reverse))
        log_warnu_return(LOG_EXIT_ZERO, "sort the graph") ;

    return 1 ;
}


