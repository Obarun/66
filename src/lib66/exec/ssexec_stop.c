/*
 * ssexec_stop.c
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
#include <stdbool.h>

#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/hash.h>

#include <skalibs/sgetopt.h>

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/service.h>
#include <66/svc.h>
#include <66/config.h>

int ssexec_stop(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    vertex_t *c, *tmp ;
    uint8_t siglen = 3 ;
    bool unsupervise = false ;
    int e = 0 ;
    uint32_t flag = GRAPH_WANT_SUPERVISED|GRAPH_WANT_REQUIREDBY, nservice = 0 ;

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

                    FLAGS_CLEAR(flag, GRAPH_WANT_REQUIREDBY) ;
                    siglen++ ;
                    break ;

                case 'u' :

                    unsupervise = true ;
                    FLAGS_SET(flag, GRAPH_WANT_LOGGER) ;
                    break ;

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

    if (!graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the graph") ;

    nservice = service_graph_build_arguments(&graph, argv, argc, info, flag) ;
    if (!nservice && errno == EINVAL)
        log_die(LOG_EXIT_USER, "services selection is not available -- did you start it first?") ;

    if (!graph.g.nsort)
        log_warn_return(e,"no services found to handle") ;

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

    char const *nargv[nservice + 1] ;
    nservice = 0 ;
    HASH_ITER(hh, graph.g.vertexes, c, tmp)
        nargv[nservice++] = c->name ;

    nargv[nservice] = 0 ;

    e = svc_send_wait(nargv, nservice, sig, siglen, info) ;

    if (e)
        return e ;

    if (unsupervise)
        svc_unsupervise(&graph) ;

    service_graph_destroy(&graph) ;

    return e ;
}
