/*
 * ssexec_disable.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/string.h>

#include <skalibs/sgetopt.h>

#include <66/constants.h>
#include <66/ssexec.h>
#include <66/config.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/resolve.h>
#include <66/state.h>

int ssexec_disable(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    uint32_t flag = 0 ;
    uint8_t stop = 0, propagate = 1 ;
    int n = 0, e = 1 ;
    size_t pos = 0 ;
    graph_t graph = GRAPH_ZERO ;
    struct resolve_hash_s *hres = NULL ;
    struct resolve_hash_s tostop[argc] ;

    memset(tostop, 0, sizeof(struct resolve_hash_s) * argc) ;

    FLAGS_SET(flag, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTUP) ;
    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_DISABLE, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                case 'S' :

                    stop = 1 ;
                    break ;

                case 'F' :

                    log_1_warn("options -F is dead -- skipping it") ;
                    break ;

                case 'R' :

                    log_1_warn("options -F is deprecated -- use instead 66 remove <service>") ;
                    break ;

                case 'P' :

                    propagate = 0 ;
                    break ;

                default :
                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(info->usage, "\n", info->help) ;

    /** build the graph of the entire system */
    graph_build_service(&graph, &hres, info, flag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- try to parse it first") ;

    for (; n < argc ; n++) {

        struct resolve_hash_s *hash = hash_search(&hres, argv[n]) ;
        if (hash == NULL)
            log_dieu(LOG_EXIT_USER, "find service: ", argv[n], " -- did you parse it?") ;

        service_enable_disable(&graph, hash, &hres, 0, propagate, info) ;

        tostop[n] = *hash ;
    }

    graph_free_all(&graph) ;
    e = 0 ;

    if (stop && n) {

        int nargc = 3 + n ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        char const *help = info->help ;
        char const *usage = info->usage ;

        info->help = help_stop ;
        info->usage = usage_stop ;

        newargv[m++] = "stop" ;
        newargv[m++] = "-u" ;
        for (; pos < n ; pos++)
            newargv[m++] = tostop[pos].name ;
        newargv[m] = 0 ;

        PROG = "stop" ;
        e = ssexec_stop(m, newargv, info) ;
        PROG = prog ;

        info->help = help ;
        info->usage = usage ;
    }

    hash_free(&hres) ;

    return e ;
}

