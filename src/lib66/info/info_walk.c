/*
 * info_walk.c
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

    int idx = 0, count = -1 ;
    size_t pos = 0, len ;
    _alloc_sa_(sa) ;

    if ((unsigned int) depth->level > INFO_MAXDEPTH)
        return 1 ;

    if (!name) {
        if (!graph_matrix_sort_tosa(&sa, g))
            return 0 ;

        count = sastr_nelement(&sa) ;

    } else {

        count = graph_matrix_get_edge_g_sorted_sa(&sa, g, name, requiredby, 0) ;
    }

    if (count == -1)
        return 0 ;

    len = sa.len ;
    char vertex[len + 1] ;
    memset(vertex, 0, sizeof(char) * len) ;

    if (!sa.len)
        return 1 ;

    if (reverse)
        if (!sastr_reverse(&sa))
            return 0 ;

    sastr_to_char(vertex, &sa) ;

    if (treename) {

        sa.len = 0 ;
        for (; pos < len ; pos += strlen(vertex + pos) + 1) {

            char atree[SS_MAX_TREENAME + 1] ;
            char *name = vertex + pos ;

            if (!service_get_treename(atree, name, 0))
                return 0 ;

            if (!strcmp(treename, atree)) {
                if (!sastr_add_string(&sa, name))
                    return 0 ;
            } else if (depth->level > 1) {
                // Displays dependencies even if it's not the same tree
                if (!sastr_add_string(&sa, name))
                    return 0 ;
            }
        }
        count = sastr_nelement(&sa) ;
        memset(vertex, 0, sizeof(char) * len) ;
        sastr_to_char(vertex, &sa) ;

        len = sa.len ;
    }

    for (pos = 0 ; pos < len ; pos += strlen(vertex + pos) + 1, idx++ ) {

        sa.len = 0 ;
        int last =  idx + 1 < count ? 0 : 1 ;
        char *name = vertex + pos ;

        if (!info_graph_display(name, func, depth, last, padding, style))
            return 0 ;

        if (graph_matrix_get_edge_g_sorted_sa(&sa, g, name, requiredby, 0) == -1)
            return 0 ;

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
                return 0 ;
            depth->next = NULL ;
        }
    }

    return 1 ;
}
