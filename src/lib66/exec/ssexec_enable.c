/*
 * ssexec_enable.c
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

#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/types.h> // FLAGS

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
    struct resolve_hash_s *hres = NULL ;
    struct resolve_hash_s tostart[argc] ;

    memset(tostart, 0, sizeof(struct resolve_hash_s) * argc) ;

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

    for(; n < argc ; n++)
        sanitize_source(argv[n], info, flag) ;

    /** build the graph of the entire system */
    graph_build_service(&graph, &hres, info, flag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- please make a bug report") ;

    for (n = 0 ; n < argc ; n++) {

        struct resolve_hash_s *hash = hash_search(&hres, argv[n]) ;
        if (hash == NULL)
            log_die(LOG_EXIT_USER, "service: ", argv[n], " not available -- did you parse it?") ;

        service_enable_disable(&graph, hash, &hres, 1, propagate, info) ;

        if (info->opt_tree) {

            service_switch_tree(&hash->res, info->base.s, info->treename.s, info) ;

            if (hash->res.logger.want && hash->res.type == TYPE_CLASSIC) {

                struct resolve_hash_s *log = hash_search(&hres, hash->res.sa.s + hash->res.logger.name) ;
                if (log == NULL)
                    log_die(LOG_EXIT_USER, "service: ", hash->res.sa.s + hash->res.logger.name, " not available -- please make a bug report") ;

                service_switch_tree(&log->res, info->base.s, info->treename.s, info) ;
            }
        }

        tostart[n] = *hash ;
    }

    graph_free_all(&graph) ;
    e = 0 ;

    if (start && n) {

        int nargc = 2 + n ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        char const *help = info->help ;
        char const *usage = info->usage ;

        info->help = help_start ;
        info->usage = usage_start ;

        newargv[m++] = "start" ;
        for (; pos < n ; pos++)
            newargv[m++] = tostart[pos].name ;
        newargv[m] = 0 ;

        PROG = "start" ;
        e = ssexec_start(m, newargv, info) ;
        PROG = prog ;

        info->help = help ;
        info->usage = usage ;
    }

    hash_free(&hres) ;

    return e ;
}
