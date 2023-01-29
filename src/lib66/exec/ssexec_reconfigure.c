/*
 * ssexec_reconfigure.c
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

#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/graph.h>

#include <skalibs/sgetopt.h>

#include <66/ssexec.h>
#include <66/config.h>
#include <66/graph.h>
#include <66/state.h>
#include <66/svc.h>
#include <66/sanitize.h>
#include <66/service.h>

#include <stdio.h>

int ssexec_reconfigure(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r, e = 0 ;
    uint32_t flag = 0 ;
    uint8_t siglen = 0 ;
    graph_t graph = GRAPH_ZERO ;

    unsigned int areslen = 0, list[SS_MAX_SERVICE], visit[SS_MAX_SERVICE], nservice = 0, n = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE] ;
    char atree[SS_MAX_TREENAME + 1] ;

    FLAGS_SET(flag, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_TOINIT|STATE_FLAGS_WANTUP) ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = subgetopt_r(argc, argv, OPTS_SUBSTART, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                case 'P' :

                    siglen++ ;
                    break ;

                default :

                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(info->usage, "\n", info->help) ;

    if ((svc_scandir_ok(info->scandir.s)) !=  1 )
        log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s, " is not running") ;

    for (; n < argc ; n++) {

        r = service_is_g(atree, argv[n], STATE_FLAGS_ISPARSED) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "get information of service: ", argv[n], " -- please make a bug report") ;

        if (!r) {
            /** nothing to do */
            log_warn(argv[n], " is not parsed -- try to start it first") ;
            return 0 ;
        }

        r = service_is_g(atree, argv[n], STATE_FLAGS_ISSUPERVISED) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "get information of service: ", argv[n], " -- please a bug report") ;

        if (!r) {
            /** nothing to do */
            log_1_warn(argv[n], " is not running -- try to start it first") ;
            return 0 ;
        }
    }

    /** build the graph of the entire system */
    graph_build_service(&graph, ares, &areslen, info, flag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- try first to install the corresponding frontend file") ;

    graph_array_init_single(visit, SS_MAX_SERVICE) ;

    for (n = 0 ; n < argc ; n++) {

        int aresid = service_resolve_array_search(ares, areslen, argv[n]) ;
        if (aresid < 0)
            log_die(LOG_EXIT_USER, "service: ", argv[n], " not available -- did you parsed it?") ;

        unsigned int l[graph.mlen], c = 0, pos = 0, idx = 0 ;

        idx = graph_hash_vertex_get_id(&graph, argv[n]) ;

        if (!visit[idx]) {
            list[nservice++] = idx ;
            visit[idx] = 1 ;
        }
        /** find dependencies of the service from the graph, do it recursively */
        c = graph_matrix_get_edge_g_list(l, &graph, argv[n], 1, 1) ;

        /** append to the list to deal with */
        for (; pos < c ; pos++) {
            if (!visit[l[pos]]) {
                list[nservice++] = l[pos] ;
                visit[l[pos]] = 1 ;
            }
        }
    }

    {
        /** stop service and unsupervise it */
        unsigned int m = 0 ;
        int nargc = 2 + argc + siglen ;
        char const *newargv[nargc] ;

        newargv[m++] = "stop" ;
        if (siglen)
            newargv[m++] = "-P" ;
        newargv[m++] = "-u" ;
        for (n = 0 ; n < argc ; n++)
            newargv[m++] = argv[n] ;

        newargv[m++] = 0 ;

        e = ssexec_stop(nargc, newargv, info) ;
        if (e)
            goto freed ;
    }

    /** force to parse again the service */
    for (n = 0 ; n < argc ; n++)
        sanitize_source(argv[n], info, STATE_FLAGS_TORELOAD) ;

    {
        /** start service */
        unsigned int m = 0 ;
        int nargc = 1 + nservice + siglen ;
        char const *newargv[nargc] ;

        newargv[m++] = "start" ;
        if (siglen)
            newargv[m++] = "-P" ;

        for (n = 0 ; n < nservice ; n++) {
            char *name = graph.data.s + genalloc_s(graph_hash_t,&graph.hash)[list[n]].vertex ;
            newargv[m++] = name ;

        }

        newargv[m++] = 0 ;

        e = ssexec_start(nargc, newargv, info) ;

    }

    freed:
        service_resolve_array_free(ares, areslen) ;
        graph_free_all(&graph) ;

    return e ;

}
