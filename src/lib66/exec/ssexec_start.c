/*
 * ssexec_start.c
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
 */

#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/hash.h>

#include <skalibs/sgetopt.h>

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/svc.h>
#include <66/sanitize.h>
#include <66/config.h>

int ssexec_start(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    vertex_t *c, *tmp ;
    uint8_t siglen = 3 ;
    uint32_t flag = GRAPH_WANT_DEPENDS|GRAPH_COLLECT_PARSE|/* sanitize_init */GRAPH_WANT_LOGGER, nservice = 0 ;
    int e = 0 ;

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

                    FLAGS_CLEAR(flag, GRAPH_WANT_DEPENDS) ;
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

    if (!graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the graph") ;

    nservice = service_graph_build_arguments(&graph, argv, argc, info, flag) ;
    if (!nservice)
        log_dieusys(LOG_EXIT_SYS, "build the service selection graph") ;

    if (!graph.g.nsort)
        log_warn_return(e,"no services found to handle") ;

    /** initiate services at the corresponding scandir */
    sanitize_init(&graph, flag) ;

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

    char const *nargv[nservice + 1] ;
    nservice = 0 ;
    HASH_ITER(hh, graph.g.vertexes, c, tmp)
        nargv[nservice++] = c->name ;

    nargv[nservice] = 0 ;

    e = svc_send_wait(nargv, nservice, sig, siglen, info) ;

    service_graph_destroy(&graph) ;

    return e ;
}
