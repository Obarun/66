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
#include <oblibs/obgetopt.h>
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

    unsigned int areslen = 0, list[SS_MAX_SERVICE], nservice = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE] ;

    FLAGS_SET(flag, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_ISSUPERVISED|STATE_FLAGS_WANTDOWN) ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = getopt_args(argc,argv, ">" OPTS_STOP, &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;

            switch (opt) {

                case 'u' :

                    FLAGS_SET(flag, STATE_FLAGS_TOUNSUPERVISE|STATE_FLAGS_WANTUP) ;
                    break ;

                case 'X' :

                    log_1_warn("deprecated option -- use 66-svctl -xd instead") ;
                    return 0 ;

                case 'K' :

                    log_1_warn("deprecated option -- use 66-svctl -kd instead") ;
                    return 0 ;

                default :
                    log_usage(usage_stop) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(usage_stop) ;

    if ((svc_scandir_ok(info->scandir.s)) != 1)
        log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s," is not running") ;

    /** build the graph of the entire system */
    graph_build_service(&graph, ares, &areslen, info, flag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- try first to install the corresponding frontend file") ;

    int n = 0 ;
    for (; n < argc ; n++) {

        int aresid = service_resolve_array_search(ares, areslen, argv[n]) ;
        if (aresid < 0)
            log_die(LOG_EXIT_USER, "service: ", *argv, " not available -- did you started it?") ;

        list[nservice++] = aresid ;
        unsigned int l[graph.mlen] ;
        unsigned int c = 0 ;

        /** find requiredby of the service from the graph, do it recursively */
        c = graph_matrix_get_edge_g_list(l, &graph, argv[n], 1, 1) ;

        /** append to the list to deal with */
        for (unsigned int pos = 0 ; pos < c ; pos++)
            list[nservice + pos] = l[pos] ;

        nservice += c ;
    }

    if (FLAGS_ISSET(flag, STATE_FLAGS_TOUNSUPERVISE)) {

        /** we cannot pass through the svc_send() function which
         * call ssexec_svctl(). The ssexec_svctl function is asynchronous.
         * So, when child exist, the svc_send is "unblocked" and the program
         * continue to execute. We need to wait for all services to be brought
         * down before executing the unsupervise.*/
        pid_t pid ;
        int wstat ;

        int nargc = 4 + nservice ;
        char const *newargv[nargc] ;
        unsigned int m = 0, n = 0 ;

        newargv[m++] = "66-svctl" ;
        newargv[m++] = "-wD" ;
        newargv[m++] = "-d" ;

        for (; n < nservice ; n++)
            newargv[m++] = graph.data.s + genalloc_s(graph_hash_t,&graph.hash)[list[n]].vertex ;

        newargv[m++] = 0 ;

        pid = child_spawn0(newargv[0], newargv, (char const *const *) environ) ;

        if (waitpid_nointr(pid, &wstat, 0) < 0)
            log_dieusys(LOG_EXIT_SYS, "wait for 66-svctl") ;

        if (wstat)
            log_dieu(LOG_EXIT_SYS, "stop services selection") ;

        svc_unsupervise(list, nservice, &graph, ares, areslen) ;

    } else {

        unsigned int siglen = 2 ;
        char *sig[siglen + 1] ;

        sig[0] = "-wD" ;
        sig[1] = "-d" ;
        sig[2] = 0 ;

        if (!svc_send(argv, argc, sig, siglen, info))
            log_dieu(LOG_EXIT_SYS, "send -wD -d signal") ;
    }

    service_resolve_array_free(ares, areslen) ;

    graph_free_all(&graph) ;

    return 0 ;
}
