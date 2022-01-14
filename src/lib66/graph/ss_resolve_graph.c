/*
 * ss_resolve_graph.c
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
 */

#include <66/resolve.h>

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/log.h>
#include <oblibs/sastr.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/service.h>

void ss_resolve_graph_ndeps_free(ss_resolve_graph_ndeps_t *graph)
{
    log_flow() ;

    genalloc_free(uint32_t,&graph->ndeps) ;
}

void ss_resolve_graph_free(ss_resolve_graph_t *graph)
{
    log_flow() ;

    resolve_deep_free(DATA_SERVICE, &graph->name) ;
    genalloc_deepfree(ss_resolve_graph_ndeps_t,&graph->cp,ss_resolve_graph_ndeps_free) ;
    genalloc_free(resolve_service_t,&graph->sorted) ;
}

int ss_resolve_dfs(ss_resolve_graph_t *graph, unsigned int idx, visit *c,unsigned int *ename,unsigned int *edeps)
{
    log_flow() ;

    int cycle = 0 ;
    unsigned int i, data ;
    unsigned int len = genalloc_len(uint32_t,&genalloc_s(ss_resolve_graph_ndeps_t,&graph->cp)[idx].ndeps) ;
    if (c[idx] == SS_GRAY) return 1 ;
    if (c[idx] == SS_WHITE)
    {
        c[idx] = SS_GRAY ;
        for (i = 0 ; i < len ; i++)
        {
            data = genalloc_s(uint32_t,&genalloc_s(ss_resolve_graph_ndeps_t,&graph->cp)[idx].ndeps)[i] ;
            cycle = (cycle || ss_resolve_dfs(graph,data,c,ename,edeps)) ;
            if (cycle)
            {
                if (!*ename)
                {
                    (*ename) = idx ;
                    (*edeps) =  i ;
                }
                goto end ;
            }
        }
        c[idx] = SS_BLACK ;
        if (!genalloc_insertb(resolve_service_t, &graph->sorted, 0, &genalloc_s(resolve_service_t,&graph->name)[idx],1))
            log_warnusys_return(LOG_EXIT_SYS,"genalloc") ;
    }
    end:
    return cycle ;
}

int ss_resolve_graph_sort(ss_resolve_graph_t *graph)
{
    log_flow() ;

    unsigned int len = genalloc_len(ss_resolve_graph_ndeps_t,&graph->cp) ;
    visit c[len] ;
    unsigned int i, ename = 0, edeps = 0 ;
    for (i = 0 ; i < len; i++) c[i] = SS_WHITE ;
    if (!len) return 0 ;

    for (i = 0 ; i < len ; i++)
    {
        if ((c[i] == SS_WHITE) && ss_resolve_dfs(graph,genalloc_s(ss_resolve_graph_ndeps_t,&graph->cp)[i].idx,c,&ename,&edeps))
        {
            int data = genalloc_s(uint32_t,&genalloc_s(ss_resolve_graph_ndeps_t,&graph->cp)[ename].ndeps)[edeps] ;
            char *name = genalloc_s(resolve_service_t,&graph->name)[ename].sa.s + genalloc_s(resolve_service_t,&graph->name)[ename].name ;
            char *deps = genalloc_s(resolve_service_t,&graph->name)[data].sa.s + genalloc_s(resolve_service_t,&graph->name)[data].name ;
            log_warn_return(LOG_EXIT_LESSONE,"resolution of : ",name,": encountered a cycle involving service: ",deps) ;
        }
    }

    return 1 ;
}

int ss_resolve_graph_publish(ss_resolve_graph_t *graph,unsigned int reverse)
{
    log_flow() ;

    int r, ret = 0 ;
    size_t a = 0 , b = 0 ;
    stralloc sa = STRALLOC_ZERO ;

    for (; a < genalloc_len(resolve_service_t,&graph->name) ; a++)
    {
        ss_resolve_graph_ndeps_t rescp = RESOLVE_GRAPH_NDEPS_ZERO ;
        rescp.idx = a ;

        if (genalloc_s(resolve_service_t,&graph->name)[a].ndepends)
        {
            sa.len = 0 ;
            if (!sastr_clean_string(&sa, genalloc_s(resolve_service_t,&graph->name)[a].sa.s +  genalloc_s(resolve_service_t,&graph->name)[a].depends)) goto err ;
            for (b = 0 ; b < sa.len ; b += strlen(sa.s + b) + 1)
            {
                char *deps = sa.s + b ;
                r = resolve_search(&graph->name,deps, DATA_SERVICE) ;
                if (r >= 0)
                {
                    if (!genalloc_append(uint32_t,&rescp.ndeps,&r)) goto err ;
                }else continue ;
            }
        }

        if (!genalloc_append(ss_resolve_graph_ndeps_t,&graph->cp,&rescp)) goto err ;
    }

    if (ss_resolve_graph_sort(graph) < 0) { ret = -1 ; goto err ; }
    if (!reverse) genalloc_reverse(resolve_service_t,&graph->sorted) ;

    stralloc_free(&sa) ;
    return 1 ;
    err:
        stralloc_free(&sa) ;
        return ret ;
}

int ss_resolve_graph_build(ss_resolve_graph_t *graph,resolve_service_t *res,char const *src,unsigned int reverse)
{
    log_flow() ;

    int r, e = 0 ;
    char *string = res->sa.s ;
    char *name = string + res->name ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    r = resolve_search(&graph->name, name, DATA_SERVICE) ;
    if (r < 0)
    {
        if (!obstr_equal(name,SS_MASTER+1))
        {
            if(!resolve_append(&graph->name,wres)) {
                log_warnu("append: ", name) ;
                goto err ;
            }
        }
        if (!reverse)
        {
            if (!service_resolve_add_deps(&graph->name,res,src)) {
                log_warnu("add dependencies of: ", name) ;
                goto err ;
            }
        }
        else
        {
            if (!service_resolve_add_rdeps(&graph->name,res,src)) {
                log_warnu("add reverse dependencies of: ", name) ;
                goto err ;
            }
        }
    }

    e = 1 ;
    err:
        free(wres) ;
        return e ;
}
/** what = 0 -> only classic
 * what = 1 -> only atomic
 * what = 2 -> both
 * @Return 0 on fail*/
int ss_resolve_graph_src(ss_resolve_graph_t *graph, char const *dir, unsigned int reverse, unsigned int what)
{
    log_flow() ;

    stralloc sa = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    size_t dirlen = strlen(dir), pos = 0 ;
    char const *exclude[2] = { 0 } ;

    char solve[dirlen + SS_DB_LEN + SS_SRC_LEN + 1] ;
    memcpy(solve,dir,dirlen) ;

    if (!what || what == 2)
    {
        memcpy(solve + dirlen, SS_SVC, SS_SVC_LEN) ;
        solve[dirlen + SS_SVC_LEN] = 0 ;
        if (!sastr_dir_get(&sa,solve,exclude,S_IFDIR)) goto err ;
    }
    if (what)
    {
        memcpy(solve + dirlen, SS_DB, SS_DB_LEN) ;
        memcpy(solve + dirlen + SS_DB_LEN, SS_SRC, SS_SRC_LEN) ;
        solve[dirlen + SS_DB_LEN + SS_SRC_LEN] = 0 ;
        exclude[0] = SS_MASTER + 1 ;
        exclude[1] = 0 ;
        if (!sastr_dir_get(&sa,solve,exclude,S_IFDIR)) goto err ;
    }
    for (;pos < sa.len; pos += strlen(sa.s + pos) + 1)
    {
        char *name = sa.s + pos ;
        if (!resolve_check(dir,name)) goto err ;
        if (!resolve_read(wres,dir,name)) goto err ;
        if (!ss_resolve_graph_build(graph,&res,dir,reverse))
        {
            log_warnu("resolve dependencies of service: ",name) ;
            goto err ;
         }
    }

    stralloc_free(&sa) ;
    resolve_free(wres) ;
    return 1 ;
    err:
        stralloc_free(&sa) ;
        resolve_free(wres) ;
        return 0 ;
}
