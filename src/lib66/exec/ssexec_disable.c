/*
 * ssexec_disable.c
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

#include <string.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/sgetopt.h>
#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/ssexec.h>
#include <66/config.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/resolve.h>
#include <66/utils.h>
#include <66/state.h>

int ssexec_disable(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    uint32_t flag = 0 ;
    uint8_t stop = 0, propagate = 1 ;
    int n = 0, e = 1 ;
    size_t pos = 0 ;
    graph_t graph = GRAPH_ZERO ;
    stralloc sa = STRALLOC_ZERO ;

    unsigned int areslen = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE + 1] ;

    visit_t visit[SS_MAX_SERVICE + 1] ;
    visit_init(visit, SS_MAX_SERVICE) ;

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
    graph_build_service(&graph, ares, &areslen, info, flag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- try to parse it first") ;

    for (; n < argc ; n++) {

        name_isvalid(argv[n]) ;

        int aresid = service_resolve_array_search(ares, areslen, argv[n]) ;
        if (aresid < 0)
            log_die(LOG_EXIT_USER, "service: ", argv[n], " not available -- did you parse it?") ;

        service_enable_disable(&graph, aresid, ares, areslen, 0, visit, propagate) ;

        if (!sastr_add_string(&sa, argv[n]))
            log_dieu(LOG_EXIT_SYS, "add string") ;

    }

    if (stop && sa.len) {

        size_t len = sastr_nelement(&sa) ;
        int nargc = 3 + len ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        char const *help = info->help ;
        char const *usage = info->usage ;

        info->help = help_stop ;
        info->usage = usage_stop ;

        newargv[m++] = "stop" ;
        newargv[m++] = "-u" ;
        FOREACH_SASTR(&sa, pos)
            newargv[m++] = sa.s + pos ;
        newargv[m] = 0 ;

        PROG = "stop" ;
        e = ssexec_stop(m, newargv, info) ;
        PROG = prog ;

        info->help = help ;
        info->usage = usage ;

        goto end ;
    }
    e = 0 ;

    end:
        stralloc_free(&sa) ;
        service_resolve_array_free(ares, areslen) ;
        graph_free_all(&graph) ;

    return e ;
}

