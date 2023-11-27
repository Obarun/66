/*
 * ssexec_enable.c
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
#include <oblibs/types.h> // FLAGS
#include <oblibs/stack.h>

#include <skalibs/sgetopt.h>

#include <66/ssexec.h>
#include <66/service.h>
#include <66/state.h>
#include <66/graph.h>
#include <66/config.h>
#include <66/enum.h>
#include <66/sanitize.h>

int ssexec_enable(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    uint32_t flag = 0 ;
    uint8_t start = 0, propagate = 1 ;
    int n = 0, e = 1 ;
    size_t pos = 0 ;
    graph_t graph = GRAPH_ZERO ;

    unsigned int areslen = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE + 1] ;
    unsigned int visit[SS_MAX_SERVICE + 1] ;

    memset(visit, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    FLAGS_SET(flag, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTUP) ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_ENABLE, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                case 'S' :

                    start = 1 ;
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

    _init_stack_(stk, argc * SS_MAX_TREENAME) ;

    for(; n < argc ; n++)
        sanitize_source(argv[n], info, flag) ;

    /** build the graph of the entire system */
    graph_build_service(&graph, ares, &areslen, info, flag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- please make a bug report") ;

    for (n = 0 ; n < argc ; n++) {

        int aresid = service_resolve_array_search(ares, areslen, argv[n]) ;
        if (aresid < 0)
            log_die(LOG_EXIT_USER, "service: ", argv[n], " not available -- did you parse it?") ;

        service_enable_disable(&graph, aresid, ares, areslen, 1, visit, propagate, info) ;

        if (info->opt_tree) {

            service_switch_tree(&ares[aresid], info->base.s, info->treename.s, info) ;

            if (ares[aresid].logger.want && ares[aresid].type == TYPE_CLASSIC) {

                int logid = service_resolve_array_search(ares, areslen, ares[aresid].sa.s + ares[aresid].logger.name) ;
                if (logid < 0)
                    log_die(LOG_EXIT_USER, "service: ", ares[aresid].sa.s + ares[aresid].logger.name, " not available -- please make a bug report") ;

                service_switch_tree(&ares[logid], info->base.s, info->treename.s, info) ;
            }
        }

        if (!stack_add_g(&stk, argv[n]))
            log_dieu(LOG_EXIT_SYS, "add string") ;
    }

    service_resolve_array_free(ares, areslen) ;
    graph_free_all(&graph) ;

    e = 0 ;

    if (start && stk.len) {

        size_t len = stack_count_element(&stk) ;
        int nargc = 2 + len ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        char const *help = info->help ;
        char const *usage = info->usage ;

        info->help = help_start ;
        info->usage = usage_start ;

        newargv[m++] = "start" ;
        FOREACH_STK(&stk, pos)
            newargv[m++] = stk.s + pos ;
        newargv[m] = 0 ;

        PROG = "start" ;
        e = ssexec_start(m, newargv, info) ;
        PROG = prog ;

        info->help = help ;
        info->usage = usage ;
    }

    return e ;
}
