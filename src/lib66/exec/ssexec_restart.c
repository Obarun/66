/*
 * ssexec_restart.c
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
#include <string.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/types.h>

#include <skalibs/sgetopt.h>

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/svc.h>
#include <66/config.h>
#include <66/sanitize.h>

int ssexec_restart(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r ;
    unsigned int m = 0 ;
    uint8_t siglen = 3 ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t flag = GRAPH_WANT_REQUIREDBY|GRAPH_WANT_SUPERVISED, nservice = 0, pos = 0 ;

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

                    FLAGS_CLEAR(flag, GRAPH_WANT_REQUIREDBY) ;
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
        log_dieusys(LOG_EXIT_SYS, "allocate the service graph") ;

    nservice = service_graph_build_arguments(&graph, argv, argc, info, flag) ;

    if (!nservice) {
        if (errno == EINVAL)
            log_dieusys(LOG_EXIT_SYS, "unable to build service selection graph") ;
        log_die(LOG_EXIT_SYS, "service selection is not supervised -- try to start it first") ;
    }

    sanitize_init(&graph, flag) ;

    char *sig[siglen] ;
    sig[0] = "-wD" ;
    sig[1] = "-D" ;

    if (siglen > 3) {

        sig[2] = "-P" ;
        sig[3] = 0 ;

    } else sig[2] = 0 ;

    r = svc_send_wait(argv, argc, sig, siglen, info) ;

    if (r)
        log_warnusys("stop service selection") ;

    sig[0] = "-wU" ;
    sig[1] = "-U" ;

    {
        /** use ssexec_start here to handle freed
         * services before calling ssexec_signal.
         * For instance, 66 free -P sA, 66 start sB,
         * where sB depends on sA */
        int nargc = 2 + nservice + siglen ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;

        char const *help = info->help ;
        char const *usage = info->usage ;

        info->help = help_start ;
        info->usage = usage_start ;

        newargv[m++] = "start" ;
        if (siglen > 3)
            newargv[m++] = "-P" ;

        pos = 0 ;
        FOREACH_GRAPH_SORT(service_graph_t, &graph, pos) {

            uint32_t index = graph.g.sort[pos] ;
            char *name = graph.g.sindex[index]->name ;
            newargv[m++] = name ;
        }

        newargv[m] = 0 ;

        PROG = "start" ;
        r = ssexec_start(m, newargv, info) ;
        PROG = prog ;

        info->help = help ;
        info->usage = usage ;
    }

    service_graph_destroy(&graph) ;

    return r ;
}



