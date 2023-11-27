/*
 * ssexec_start.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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
    unsigned int areslen = 0, list[SS_MAX_SERVICE + 1], visit[SS_MAX_SERVICE + 1], nservice = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE + 1] ;

    memset(list, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    memset(visit, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
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
        /** If it's the first use of 66 or we don't have any resolve files available,
         * or the service was never parsed, the graph is empty in the first case,
         * or the later call of service_array_search does not find the corresponding
         * resolve file in the second case.
         * At least try to parse the corresponding frontend file. */
        sanitize_source(argv[n], info, flag) ;

    /** build the graph of the entire system */
    graph_build_service(&graph, ares, &areslen, info, flag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- please make a bug report") ;

    for (n = 0 ; n < argc ; n++) {

        int aresid = service_resolve_array_search(ares, areslen, argv[n]) ;
        if (aresid < 0)
            log_die(LOG_EXIT_USER, "service: ", argv[n], " not available -- did you parse it?") ;

        graph_compute_visit(ares, aresid, visit, list, &graph, &nservice, 0) ;
    }

    /** initiate services at the corresponding scandir */
    sanitize_init(list, nservice, &graph, ares, areslen) ;

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
