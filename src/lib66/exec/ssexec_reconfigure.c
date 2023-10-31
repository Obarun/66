/*
 * ssexec_reconfigure.c
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
#include <oblibs/graph.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>

#include <skalibs/sgetopt.h>
#include <skalibs/genalloc.h>

#include <66/ssexec.h>
#include <66/config.h>
#include <66/graph.h>
#include <66/state.h>
#include <66/svc.h>
#include <66/sanitize.h>
#include <66/service.h>
#include <66/constants.h>
#include <66/tree.h>

int ssexec_reconfigure(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r, e = 0 ;
    uint32_t flag = 0 ;
    uint8_t siglen = 0 ;
    graph_t graph = GRAPH_ZERO ;

    unsigned int areslen = 0, list[SS_MAX_SERVICE + 1], visit[SS_MAX_SERVICE + 1], nservice = 0, n = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE + 1] ;

    memset(list, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    memset(visit, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    FLAGS_SET(flag, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_TOPARSE|STATE_FLAGS_WANTUP) ;

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

    /** build the graph of the entire system */
    graph_build_service(&graph, ares, &areslen, info, flag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- try first to install the corresponding frontend file") ;

    for (; n < argc ; n++) {

        int aresid = service_resolve_array_search(ares, areslen, argv[n]) ;
        if (aresid < 0)
            log_die(LOG_EXIT_USER, "service: ", argv[n], " not available -- please make a bug report") ;

        if (!state_messenger(&ares[aresid], STATE_FLAGS_TOPARSE, STATE_FLAGS_TRUE))
            log_dieusys(LOG_EXIT_SYS, "send message to state of: ", argv[n]) ;

        r = service_is_g(argv[n], STATE_FLAGS_ISSUPERVISED) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "get information of service: ", argv[n], " -- please make a bug report") ;

        if (!r || r == STATE_FLAGS_FALSE) {
            /* only parse it again */
            continue ;

        } else {

            r = tree_ongroups(ares[aresid].sa.s + ares[aresid].path.home, ares[aresid].sa.s + ares[aresid].treename, TREE_GROUPS_BOOT) ;

            if (r < 0)
                log_dieu(LOG_EXIT_SYS, "get groups of service: ", argv[n]) ;

            if (r)
                continue ;

            graph_compute_visit(argv[n], visit, list, &graph, &nservice, 1) ;
        }
    }

    r = svc_scandir_ok(info->scandir.s) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "check: ", info->scandir.s) ;

    if (nservice && r) {

        /** We need to search in every tree to stop the service
         * if the user has specified a specific tree. Therefore, remove
         * the -t option for the stop process and re-enable it
         * for the parse and start processes. */
        char tree[info->treename.len + 1] ;
        auto_strings(tree, info->treename.s) ;

        info->treename.len = 0 ;
        info->opt_tree = 0 ;

        unsigned int m = 0 ;
        int nargc = 3 + nservice + siglen ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;

        char const *help = info->help ;
        char const *usage = info->usage ;

        info->help = help_stop ;
        info->usage = usage_stop ;

        newargv[m++] = "stop" ;
        if (siglen)
            newargv[m++] = "-P" ;
        newargv[m++] = "-u" ;

        for (n = 0 ; n < nservice ; n++) {

            char *name = graph.data.s + genalloc_s(graph_hash_t,&graph.hash)[list[n]].vertex ;
            if (get_rstrlen_until(name,SS_LOG_SUFFIX) < 0)
                newargv[m++] = name ;
        }

        newargv[m] = 0 ;

        PROG = "stop" ;
        e = ssexec_stop(m, newargv, info) ;
        PROG = prog ;
        if (e)
            goto freed ;

        info->help = help ;
        info->usage = usage ;

        info->treename.len = 0 ;
        if (!auto_stra(&info->treename, tree))
            log_die_nomem("stralloc") ;
        info->opt_tree = 1 ;
    }

    /** force to parse again the service */
    for (n = 0 ; n < argc ; n++)
        sanitize_source(argv[n], info) ;

    if (nservice && r) {

        unsigned int m = 0 ;
        int nargc = 2 + nservice + siglen ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;

        char const *help = info->help ;
        char const *usage = info->usage ;

        info->help = help_start ;
        info->usage = usage_start ;

        newargv[m++] = "start" ;
        if (siglen)
            newargv[m++] = "-P" ;

        for (n = 0 ; n < nservice ; n++) {

            char *name = graph.data.s + genalloc_s(graph_hash_t,&graph.hash)[list[n]].vertex ;
            if (get_rstrlen_until(name,SS_LOG_SUFFIX) < 0)
                newargv[m++] = name ;
        }

        newargv[m] = 0 ;

        PROG= "start" ;
        e = ssexec_start(m, newargv, info) ;
        PROG = prog ;

        info->help = help ;
        info->usage = usage ;
    }

    freed:
        service_resolve_array_free(ares, areslen) ;
        graph_free_all(&graph) ;

    return e ;

}
