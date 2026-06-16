/*
 * graph_dag.c
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

/* Build small service dependency graphs through the lib66 wrappers and check
 * the topological sort: an acyclic graph sorts every vertex, a cyclic one is
 * rejected. */

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

#include <oblibs/graph.h>

#include <66/graph.h>

#define ASSERT(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg) ; assert(0) ; } \
} while (0)

static void test_acyclic(void)
{
    printf("Running test_acyclic\n") ;

    service_graph_t g = GRAPH_SERVICE_ZERO ;
    ASSERT(service_graph_new(&g, 10), "service_graph_new") ;

    ASSERT(graph_add(&g.g, "a"), "add a") ;
    ASSERT(graph_add(&g.g, "b"), "add b") ;
    ASSERT(graph_add(&g.g, "c"), "add c") ;

    /* a depends on b and c; b depends on c */
    ASSERT(graph_add_deps(&g.g, "a", "b c", false), "deps a") ;
    ASSERT(graph_add_deps(&g.g, "b", "c", false), "deps b") ;

    ASSERT(graph_sort(&g.g, false), "sort acyclic graph") ;
    ASSERT(g.g.nsort == 3, "every vertex sorted") ;

    service_graph_destroy(&g) ;
    printf(" PASS\n") ;
}

static void test_cycle(void)
{
    printf("Running test_cycle\n") ;

    service_graph_t g = GRAPH_SERVICE_ZERO ;
    ASSERT(service_graph_new(&g, 10), "service_graph_new") ;

    ASSERT(graph_add(&g.g, "x"), "add x") ;
    ASSERT(graph_add(&g.g, "y"), "add y") ;

    /* x depends on y and y depends on x: a cycle */
    ASSERT(graph_add_deps(&g.g, "x", "y", false), "deps x") ;
    ASSERT(graph_add_deps(&g.g, "y", "x", false), "deps y") ;

    ASSERT(!graph_sort(&g.g, false), "cyclic graph must be rejected") ;

    service_graph_destroy(&g) ;
    printf(" PASS\n") ;
}

int main(void)
{
    printf("Starting graph DAG tests...\n") ;
    test_acyclic() ;
    test_cycle() ;
    printf("All tests passed!\n") ;
    return 0 ;
}
