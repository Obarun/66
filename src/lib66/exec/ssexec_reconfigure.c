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
#include <oblibs/stack.h>
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
    uint8_t siglen = 0, issupervised = 0 ;
    graph_t graph = GRAPH_ZERO ;

    unsigned int areslen = 0, list[SS_MAX_SERVICE + 1], visit[SS_MAX_SERVICE + 1], tostate[SS_MAX_SERVICE + 1], ntostate = 0, nservice = 0, n = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE + 1] ;
    ss_state_t sta = STATE_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    _init_stack_(stk, argc * SS_MAX_SERVICE_NAME) ;
    memset(list, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    memset(visit, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    memset(tostate, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
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
        log_die(LOG_EXIT_USER, "services selection is not available -- have you already parsed a service?") ;

    for (; n < argc ; n++) {

        int aresid = service_resolve_array_search(ares, areslen, argv[n]) ;
        if (aresid < 0)
            log_die(LOG_EXIT_USER, "service: ", argv[n], " not available -- did you parse it?") ;

        if (!state_read(&sta, &ares[aresid]))
            log_dieu(LOG_EXIT_SYS, "read state file of: ", argv[n]) ;

        sta.toparse = STATE_FLAGS_TRUE ;

        if (!state_write(&sta, &ares[aresid]))
            log_dieusys(LOG_EXIT_SYS, "write status file of: ", argv[n]) ;

        /** need to reverse the previous state change to
         * for current live service.*/
        tostate[ntostate++] = aresid ;

        issupervised = sta.issupervised == STATE_FLAGS_TRUE ? 1 : 0 ;

        if (ares[aresid].enabled)
            if (!stack_add_g(&stk, argv[n]))
                log_dieu(LOG_EXIT_SYS, "add string") ;

        if (!issupervised) {
            /* only force to parse it again */
            continue ;

        } else {
            /** services of group boot cannot be restarted, the changes will appear only at
             * next reboot.*/
            r = tree_ongroups(ares[aresid].sa.s + ares[aresid].path.home, ares[aresid].sa.s + ares[aresid].treename, TREE_GROUPS_BOOT) ;

            if (r < 0)
                log_dieu(LOG_EXIT_SYS, "get groups of service: ", argv[n]) ;

            if (r)
                continue ;

            graph_compute_visit(ares, aresid, visit, list, &graph, &nservice, 1) ;
        }
    }

    r = svc_scandir_ok(info->scandir.s) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "check: ", info->scandir.s) ;

    if (nservice && r) {

        /** User may request for a specific tree with the -t options.
         * The tree specified may be different from the actual one.
         * So, remove the -t option for the stop process and use it again
         * for the parse and start process. */
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
            /** the stop process will handle logger. Remove those from the list.*/
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
        sanitize_source(argv[n], info, flag) ;

    for (n = 0 ; n < ntostate ; n++) {

        /** live of the service still exist.
         * Reverse to the previous state of the isparsed flag. */
        if (state_read_remote(&sta, ares[tostate[n]].sa.s + ares[tostate[n]].live.statedir)) {

            sta.toparse = STATE_FLAGS_FALSE ;

            if (!state_write_remote(&sta, ares[tostate[n]].sa.s + ares[tostate[n]].live.statedir))
                log_warnusys("write status file of: ", ares[tostate[n]].sa.s + ares[tostate[n]].live.statedir) ;
        }
    }

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

    if (stk.len) {

        /** enable again the service if it was enabled */
        unsigned int m = 0 ;
        int nargc = 2 + stk.count ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;

        char const *help = info->help ;
        char const *usage = info->usage ;

        info->help = help_enable ;
        info->usage = usage_enable ;

        newargv[m++] = "enable" ;

        n = 0 ;
        FOREACH_STK(&stk, n) {

            char *name = stk.s + n ;
            if (get_rstrlen_until(name,SS_LOG_SUFFIX) < 0)
                newargv[m++] = name ;
        }

        newargv[m] = 0 ;

        PROG= "enable" ;
        e = ssexec_enable(m, newargv, info) ;
        PROG = prog ;

        info->help = help ;
        info->usage = usage ;
    }

    freed:
        resolve_free(wres) ;
        service_resolve_array_free(ares, areslen) ;
        graph_free_all(&graph) ;

    return e ;

}
