/*
 * ssexec_start.c
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
#include <oblibs/sastr.h>

#include <skalibs/sgetopt.h>

#include <66/ssexec.h>
#include <66/config.h>
#include <66/graph.h>
#include <66/state.h>
#include <66/svc.h>
#include <66/sanitize.h>
#include <66/service.h>
#include <66/enum.h>

int ssexec_start(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    uint32_t flag = 0 ;
    graph_t graph = GRAPH_ZERO ;
    uint8_t siglen = 3 ;

    int n = 0 ;
    unsigned int areslen = 0, list[SS_MAX_SERVICE], visit[SS_MAX_SERVICE], nservice = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE] ;

    FLAGS_SET(flag, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_TOPARSE|STATE_FLAGS_WANTUP) ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = subgetopt_r(argc,argv, OPTS_START, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                case 'P' :

                    FLAGS_CLEAR(flag, STATE_FLAGS_TOPROPAGATE) ;
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

    for (; n < argc ; n++)
        /** If it's the first use of 66, we don't have
         * any resolve files available or the service was
         * never parsed, the graph is empty in the first case or
         * the later call of service_array_search do not found the corresponding
         * resolve file for the second case.
         * Try at least to parse the corresponding frontend file. */
        sanitize_source(argv[n], info, STATE_FLAGS_UNKNOWN) ;

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
        c = graph_matrix_get_edge_g_list(l, &graph, argv[n], 0, 1) ;

        /** append to the list to deal with */
        for (; pos < c ; pos++) {
            if (!visit[l[pos]]) {
                list[nservice++] = l[pos] ;
                visit[l[pos]] = 1 ;
            }
        }
        if (ares[aresid].type == TYPE_MODULE) {

            if (ares[aresid].regex.ncontents) {

                stralloc sa = STRALLOC_ZERO ;
                if (!sastr_clean_string(&sa, ares[aresid].sa.s + ares[aresid].regex.contents))
                    log_dieu(LOG_EXIT_SYS, "clean string") ;

                {
                    size_t idx = 0 ;
                    FOREACH_SASTR(&sa, idx) {


                        /** find dependencies of the service from the graph, do it recursively */
                        c = graph_matrix_get_edge_g_list(l, &graph, sa.s + idx, 0, 1) ;

                        /** append to the list to deal with */
                        for (pos = 0 ; pos < c ; pos++) {
                            if (!visit[l[pos]]) {
                                list[nservice++] = l[pos] ;
                                visit[l[pos]] = 1 ;
                            }
                        }
                    }
                }
            }
        }
    }

    /** initiate services at the corresponding scandir */
    sanitize_init(list, nservice, &graph, ares, areslen, STATE_FLAGS_UNKNOWN) ;

    service_resolve_array_free(ares, areslen) ;

    graph_free_all(&graph) ;

    char *sig[siglen] ;
    if (siglen > 3) {

        sig[0] = "-P" ;
        sig[1] = "-wU" ;
        sig[2] = "-u" ;
        sig[3] = 0 ;

    } else {

        sig[0] = "-wU" ;
        sig[1] = "-u" ;
        sig[2] = 0 ;
    }

    return svc_send_wait(argv, argc, sig, siglen, info) ;
}
