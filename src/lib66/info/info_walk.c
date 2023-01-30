/*
 * info_walk.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 * */

#include <stdint.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/info.h>
#include <66/graph.h>

int info_walk(graph_t *g, char const *name, char const *treename, info_graph_func *func, uint8_t requiredby, uint8_t reverse, depth_t *depth, int padding, info_graph_style *style)
{
    log_flow() ;

    int e = 0, idx = 0, count ;
    size_t pos = 0, len ;

    if ((unsigned int) depth->level > MAXDEPTH)
        return 1 ;

    stralloc sa = STRALLOC_ZERO ;

    if (!name) {
        if (!graph_matrix_sort_tosa(&sa, g)) {
            stralloc_free(&sa) ;
            return e ;
        }
        count = sastr_nelement(&sa) ;

    } else {

        count = graph_matrix_get_edge_g_sorted_sa(&sa, g, name, requiredby, 0) ;

        if (count == -1) {
            stralloc_free(&sa) ;
            return e ;
        }
    }

    len = sa.len ;
    char vertex[len + 1] ;

    if (!sa.len)
        goto freed ;

    if (reverse)
        if (!sastr_reverse(&sa))
            goto err ;

    sastr_to_char(vertex, &sa) ;

    for (; pos < len ; pos += strlen(vertex + pos) + 1, idx++ ) {

        sa.len = 0 ;
        int last =  idx + 1 < count  ? 0 : 1 ;
        char *name = vertex + pos ;

        if (depth->level == 1 && treename) {
            char atree[SS_MAX_TREENAME + 1] ;

            service_is_g(atree, name, 0) ;

            if (strcmp(treename, atree))
                continue ;
        }

        if (!info_graph_display(name, func, depth, last, padding, style))
            goto err ;

        if (graph_matrix_get_edge_g_sorted_sa(&sa, g, name, requiredby, 0) == -1)
            goto err ;

        if (sa.len) {

            depth_t d = {
                depth,
                NULL,
                depth->level + 1
            } ;
            depth->next = &d;

            if(last) {

                if(depth->prev) {

                    depth->prev->next = &d;
                    d.prev = depth->prev;
                    depth = &d;
                }
                else
                    d.prev = NULL;
            }
            if (!info_walk(g, name, treename, func, requiredby, reverse, &d, padding, style))
                goto err ;
            depth->next = NULL ;
        }
    }

    freed:
        e = 1 ;

    err:
        stralloc_free(&sa) ;

    return e ;

}
