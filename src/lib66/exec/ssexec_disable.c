/*
 * ssexec_disable.c
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

#include <oblibs/log.h>
#include <oblibs/hash.h>
#include <oblibs/types.h>
#include <oblibs/environ.h>
#include <oblibs/strbuf.h>

#include <skalibs/sgetopt.h>

#include <66/ssexec.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/config.h>

int ssexec_disable(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    bool stop = false, propagate = true, action = false ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    vertex_t *c, *tmp ;
    int e = 1 ;
    uint32_t flag = GRAPH_WANT_REQUIREDBY, nservice = 0 ;

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

                    stop = true ;
                    break ;

                case 'P' :

                    FLAGS_CLEAR(flag, GRAPH_WANT_REQUIREDBY) ;
                    propagate = false ;
                    break ;

                default :
                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(info->usage, "\n", info->help) ;

    if (!graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the service graph") ;

    if (!environ_import_arguments(&sa, argv, argc))
        log_dieusys(LOG_EXIT_SYS, "import arguments") ;

    nservice = service_graph_build_list(&graph, sa.s, sa.len, info, flag) ;

    if (!nservice)
        log_die(LOG_EXIT_USER, "services selection is not available -- try to parse it first") ;

    hash_reset_visit(graph.hres) ;

    nservice = 0 ;
    FOREACH_GRAPH_SORT(service_graph_t, &graph, nservice) {

        uint32_t index = graph.g.sort[nservice] ;
        vertex_t *v = graph.g.sindex[index] ;
        char *name = v->name ;
        struct resolve_hash_s *hash = hash_search(&graph.hres, name) ;

        if (hash == NULL)
            log_die(LOG_EXIT_SYS, "get information of service: ", name, " -- please make a bug report") ;

        if (!hash->visit)
            service_enable_disable(&graph, hash, action, propagate, info, &sa) ;
    }

    e = 0 ;

    if (stop && graph.g.nvertexes) {

        int nargc = 3 + graph.g.nvertexes ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        char const *help = info->help ;
        char const *usage = info->usage ;

        info->help = help_stop ;
        info->usage = usage_stop ;

        newargv[m++] = "stop" ;
        newargv[m++] = "-u" ;
        HASH_ITER(hh, graph.g.vertexes, c, tmp)
            newargv[m++] = c->name ;
        newargv[m] = 0 ;

        PROG = "stop" ;
        e = ssexec_stop(m, newargv, info) ;
        PROG = prog ;

        info->help = help ;
        info->usage = usage ;
    }

    service_graph_destroy(&graph) ;

    return e ;
}

