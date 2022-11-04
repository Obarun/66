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
#include <oblibs/obgetopt.h>
#include <oblibs/graph.h>

#include <skalibs/sgetopt.h>
#include <skalibs/genalloc.h>

#include <66/ssexec.h>
#include <66/config.h>
#include <66/graph.h>
#include <66/state.h>
#include <66/svc.h>
#include <66/sanitize.h>
#include <66/service.h>

int ssexec_start(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    uint32_t flag = 0 ;
    graph_t graph = GRAPH_ZERO ;
    unsigned int siglen = 2 ;
    char *sig[siglen + 1] ;

    unsigned int areslen = 0, list[SS_MAX_SERVICE], nservice = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE] ;

    FLAGS_SET(flag, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_TOINIT|STATE_FLAGS_WANTUP) ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = getopt_args(argc,argv, ">" OPTS_START, &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;

            switch (opt) {

                case 'r' :

                    if (FLAGS_ISSET(flag, STATE_FLAGS_TORESTART))
                        log_usage(usage_start) ;

                    FLAGS_SET(flag, STATE_FLAGS_TORELOAD) ;
                    break ;

                case 'R' :

                    if (FLAGS_ISSET(flag, STATE_FLAGS_TORELOAD))
                        log_usage(usage_start) ;

                    FLAGS_SET(flag, STATE_FLAGS_TORESTART) ;
                    break ;

                default :

                    log_usage(usage_start) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(usage_start) ;

    if ((svc_scandir_ok(info->scandir.s)) !=  1 )
        log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s, " is not running") ;

    int n = 0 ;
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

    for (n = 0 ; n < argc ; n++) {

        int aresid = service_resolve_array_search(ares, areslen, argv[n]) ;
        if (aresid < 0)
            log_die(LOG_EXIT_USER, "service: ", *argv, " not available -- did you parsed it?") ;

        list[nservice++] = aresid ;
        unsigned int l[graph.mlen] ;
        unsigned int c = 0 ;

        /** find dependencies of the service from the graph, do it recursively */
        c = graph_matrix_get_edge_g_list(l, &graph, argv[n], 0, 1) ;

        /** append to the list to deal with */
        for (unsigned int pos = 0 ; pos < c ; pos++)
            list[nservice + pos] = l[pos] ;

        nservice += c ;
    }

    /** initiate services at the corresponding scandir */
    sanitize_init(list, nservice, &graph, ares, areslen, FLAGS_ISSET(flag, STATE_FLAGS_TORESTART) ? STATE_FLAGS_TOINIT : STATE_FLAGS_UNKNOWN) ;

    service_resolve_array_free(ares, areslen) ;

    graph_free_all(&graph) ;

    if (FLAGS_ISSET(flag, STATE_FLAGS_TORELOAD)) {

        sig[0] = "-wU" ;
        sig[1] = "-ru" ;
        sig[2] = 0 ;

        if (!svc_send(argv, argc, sig, siglen, info))
            log_dieu(LOG_EXIT_SYS, "send -wU -ru signal to services selection") ;

    } else if (FLAGS_ISSET(flag, STATE_FLAGS_TORESTART)) {

        sig[0] = "-wD" ;
        sig[1] = "-d" ;
        sig[2] = 0 ;

        if (!svc_send(argv, argc, sig, siglen, info))
            log_dieu(LOG_EXIT_SYS, "send -wD -d signal to services selection") ;

        sig[0] = "-wU" ;
        sig[1] = "-ru" ;
        sig[2] = 0 ;

        if (!svc_send(argv, argc, sig, siglen, info))
            log_dieu(LOG_EXIT_SYS, "send -wU -u signal to services selection") ;

    } else {

        sig[0] = "-wU" ;
        sig[1] = "-u" ;
        sig[2] = 0 ;

        if (!svc_send(argv, argc, sig, siglen, info))
            log_dieu(LOG_EXIT_SYS, "send -wU -u signal to services selection") ;
    }

    return 0 ;
}
