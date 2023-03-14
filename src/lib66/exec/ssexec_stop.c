/*
 * ssexec_stop.c
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
#include <skalibs/djbunix.h>
#include <skalibs/genalloc.h>

#include <66/graph.h>
#include <66/config.h>
#include <66/ssexec.h>
#include <66/state.h>
#include <66/svc.h>
#include <66/service.h>

int ssexec_stop(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    uint32_t flag = 0 ;
    graph_t graph = GRAPH_ZERO ;
    uint8_t siglen = 3 ;
    int e = 0 ;

    unsigned int areslen = 0, list[SS_MAX_SERVICE], visit[SS_MAX_SERVICE], nservice = 0, n = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE] ;

    FLAGS_SET(flag, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_ISSUPERVISED|STATE_FLAGS_WANTDOWN) ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = subgetopt_r(argc,argv, OPTS_STOP, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                case 'P' :

                    FLAGS_CLEAR(flag, STATE_FLAGS_TOPROPAGATE) ;
                    siglen++ ;
                    break ;

                case 'u' :

                    FLAGS_SET(flag, STATE_FLAGS_TOUNSUPERVISE|STATE_FLAGS_WANTUP) ;
                    break ;

                case 'X' :

                    log_1_warn("deprecated option -- use 66 signal -xd instead") ;
                    return 0 ;

                case 'K' :

                    log_1_warn("deprecated option -- use 66 signal -kd instead") ;
                    return 0 ;

                default :
                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(info->usage, "\n", info->help) ;

    if ((svc_scandir_ok(info->scandir.s)) != 1)
        log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s," is not running") ;

    /** build the graph of the entire system */
    graph_build_service(&graph, ares, &areslen, info, flag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- did you started its first?") ;

    graph_array_init_single(visit, SS_MAX_SERVICE) ;

    for (; n < argc ; n++) {

        int aresid = service_resolve_array_search(ares, areslen, argv[n]) ;
        if (aresid < 0)
            log_die(LOG_EXIT_USER, "service: ", argv[n], " not available -- did you started it?") ;

        unsigned int l[graph.mlen], c = 0, pos = 0, idx = 0 ;

        idx = graph_hash_vertex_get_id(&graph, argv[n]) ;

        if (!visit[idx]) {
            list[nservice++] = idx ;
            visit[idx] = 1 ;
        }

        /** find requiredby of the service from the graph, do it recursively */
        c = graph_matrix_get_edge_g_list(l, &graph, argv[n], 1, 1) ;

        /** append to the list to deal with */
        for (; pos < c ; pos++) {
            if (!visit[l[pos]]) {
                list[nservice++] = l[pos] ;
                visit[l[pos]] = 1 ;
            }
        }
    }

    char *sig[siglen] ;
    if (siglen > 3) {

        sig[0] = "-P" ;
        sig[1] = "-wD" ;
        sig[2] = "-d" ;
        sig[3] = 0 ;

    } else {

        sig[0] = "-wD" ;
        sig[1] = "-d" ;
        sig[2] = 0 ;
    }

    e = svc_send_wait(argv, argc, sig, siglen, info) ;

    if (FLAGS_ISSET(flag, STATE_FLAGS_TOUNSUPERVISE))
        svc_unsupervise(list, nservice, &graph, ares, areslen) ;

    service_resolve_array_free(ares, areslen) ;

    graph_free_all(&graph) ;

    return e ;
}
