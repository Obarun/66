/*
 * service_info_walk.c
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
 * */

#include <stdint.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/info.h>
#include <66/graph.h>
#include <66/config.h>

static int info_add_name(strbuf *sa, const char *name, uint32_t *count, const char *treename, depth_t *depth)
{
    if (treename) {

        char atree[SS_MAX_TREENAME + 1] ;

        if (!service_get_treename(atree, name))
            return 0 ;

        if (strcmp(treename, atree) && depth->level <= 1)
            return 1 ;
    }

    if (!sbl_add(sa, name))
        return (errno = ENOMEM, 0) ;

    (*count)++ ;

    return 1 ;
}

static int info_add_sort(strbuf *sa, service_graph_t *g, uint32_t *count, const char *treename, depth_t *depth)
{
    uint32_t pos = 0 ;
    FOREACH_GRAPH_SORT(service_graph_t, g, pos) {

        uint32_t index = g->g.sort[pos] ;
        char *name = g->g.sindex[index]->name ;

        if (!info_add_name(sa, name, count, treename, depth))
            return 0 ;
    }

    return 1 ;
}

int service_info_walk(service_graph_t *g, char const *name, char const *treename, uint8_t requiredby, uint8_t reverse, depth_t *depth, int padding, info_graph_style *style, ssexec_t *info)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    uint32_t of = requiredby ? GRAPH_WANT_REQUIREDBY : GRAPH_WANT_DEPENDS ;
    uint32_t flag = GRAPH_COLLECT_PARSE|of, pos = 0, count = 0, idx = 0 ;
    vertex_t *v = NULL ;

    if ((unsigned int) depth->level > INFO_MAXDEPTH)
        return 1 ;

    if (name) {

        service_graph_t gt = GRAPH_SERVICE_ZERO ;

        if (!graph_new(&gt, g->g.len))
            return (errno = ENOMEM, 0) ;

        HASH_FIND_STR(g->g.vertexes, name, v) ;
        if (v == NULL)
            log_dieu(LOG_EXIT_SYS, "get information of service: ", name, " -- please make a bug report") ;

        uint32_t nvertex = !requiredby ? v->ndepends : v->nrequiredby, pos = 0 ;
        vertex_t *vl[nvertex] ;

        // Service inside module can have an empty field.
        if (nvertex) {

            graph_get_edge(&g->g, v, vl, requiredby) ;

            for (; pos < nvertex ; pos++) {
                if (!sbl_add(&sa, vl[pos]->name))
                    return (errno = ENOMEM, 0) ;
            }

        } else {

            if (!sbl_add(&sa, v->name))
                return (errno = ENOMEM, 0) ;
        }

        if (!service_graph_build_list(&gt, sa.s, sa.len, info, flag) && errno == EINVAL)
            return 0 ;

        sa.len = 0 ;
        if (!info_add_sort(&sa, &gt, &count, treename, depth))
            return 0 ;

        service_graph_destroy(&gt) ;

    } else {

        if (!info_add_sort(&sa, g, &count, treename, depth))
            return 0 ;
    }

    if (!sa.len)
        return 1 ;

    if (reverse)
        if (!sbl_reverse(&sa))
            return 0 ;

    pos = 0 ;
    FOREACH_SBL(&sa, pos) {

        v = NULL ;
        idx++ ;
        int last = idx < count ? 0 : 1 ;

        if (!info_graph_display(sa.s + pos, &info_graph_display_service, depth, last, padding, style))
            return 0 ;

        HASH_FIND_STR(g->g.vertexes, sa.s + pos, v) ;
        if (v == NULL)
            log_dieu(LOG_EXIT_SYS, "get information of service: ", sa.s + pos, " -- please make a bug report") ;

        uint32_t nv = !requiredby ? v->ndepends : v->nrequiredby ;

        if (nv) {

            depth_t d = {
                depth,
                NULL,
                depth->level + 1
            } ;
            depth->next = &d;

            if (last) {

                if (depth->prev) {

                    depth->prev->next = &d;
                    d.prev = depth->prev;
                    depth = &d;
                }
                else
                    d.prev = NULL;
            }

            if (!service_info_walk(g, sa.s + pos, treename, requiredby, reverse, &d, padding, style, info))
                return 0 ;

            depth->next = NULL ;
        }
    }


    return 1 ;
}
