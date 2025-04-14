/*
 * ssexec_reconfigure.c
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
#include <oblibs/sastr.h>
#include <oblibs/types.h>
#include <oblibs/hash.h>
#include <oblibs/environ.h>
#include <oblibs/string.h>

#include <skalibs/sgetopt.h>

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/service.h>
#include <66/config.h>
#include <66/state.h>
#include <66/svc.h>
#include <66/sanitize.h>
#include <66/tree.h>
#include <66/constants.h>

int ssexec_reconfigure(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r, rscan, e = 0 ;
    uint8_t siglen = 0 ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t flag = GRAPH_COLLECT_PARSE|GRAPH_WANT_REQUIREDBY|GRAPH_WANT_SUPERVISED, nservice = 0, pos = 0 ;
    struct resolve_hash_s *c, *tmp ;
    resolve_service_t_ref pres = 0 ;
    resolve_service_t tostate[SS_MAX_SERVICE], toenable[SS_MAX_SERVICE] ;
    uint32_t ntostate = 0, ntoenable = 0, n = 0 ;
    ss_state_t sta = STATE_ZERO ;

    memset(tostate, 0, SS_MAX_SERVICE * sizeof(resolve_service_t)) ;
    memset(toenable, 0, SS_MAX_SERVICE * sizeof(resolve_service_t)) ;

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

    rscan = svc_scandir_ok(info->scandir.s) ;
    if (rscan < 0)
        log_dieusys(LOG_EXIT_SYS, "check: ", info->scandir.s) ;

    if (!graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the service graph") ;

    _alloc_sa_(sa) ;

    if (!environ_import_arguments(&sa, argv, argc))
        log_dieusys(LOG_EXIT_SYS, "import arguments") ;

    nservice = service_graph_ncollect(&graph, sa.s, sa.len, info, flag) ;

    if (!nservice) {
        if (errno == EINVAL)
            log_dieusys(LOG_EXIT_SYS, "build the graph") ;

        log_die(LOG_EXIT_USER, "services selection is not available -- have you already parsed a service?") ;
    }

    _alloc_stk_(stk, nservice * SS_MAX_SERVICE_NAME) ;

    HASH_ITER(hh, graph.hres, c, tmp) {

        pres = &c->res ;
        char *name = pres->sa.s + pres->name ;

        if (pres->inns) {
            // search first into the user commandline
            if (sastr_cmp(&sa, pres->sa.s + pres->inns) < 0) {
                // it may be a service of a another dependending module
                struct resolve_hash_s *t = hash_search(&graph.hres, pres->sa.s + pres->inns) ;
                if (t == NULL)
                    log_die(LOG_EXIT_USER, "reconfiguring an individual service that is part of a module is not allowed -- please reconfigure the entire module instead using \'66 reconfigure ", pres->sa.s + pres->inns, "\'") ;
            }
        }


        char status[strlen(pres->sa.s + pres->path.servicedir) + SS_STATE_LEN + 1] ;

        auto_strings(status, pres->sa.s + pres->path.servicedir, SS_STATE) ;

        if (!state_read(&sta, pres))
            log_dieu(LOG_EXIT_SYS, "read state file of: ", name) ;

        sta.toparse = STATE_FLAGS_TRUE ;

        if (!state_write(&sta, pres))
            log_dieusys(LOG_EXIT_SYS, "write status file of: ", name) ;

        /** need to reverse the previous state change to
         * for current live service.*/
        tostate[ntostate++] = c->res ;

        if (pres->enabled && !pres->inns)
            toenable[ntoenable++] = c->res ;

        if (!state_write_remote(&sta, status))
            log_dieusys(LOG_EXIT_SYS, "write status file of: ", name) ;

        /** services of group boot cannot be restarted, the changes will appear only at
         * next reboot.*/
        r = tree_ongroups(pres->sa.s + pres->path.home, pres->sa.s + pres->treename, TREE_GROUPS_BOOT) ;

        if (r < 0)
            log_dieu(LOG_EXIT_SYS, "get groups of service: ", name) ;

        if (r)
            continue ;

        if (!stack_add_g(&stk, pres->sa.s + pres->name))
            log_die_nomem("stralloc") ;

    }

    if (!service_graph_nresolve(&graph, stk.s, stk.len, flag))
        log_dieusys(LOG_EXIT_SYS, "build the graph") ;

    nservice = graph.g.nsort ;

    if (nservice && rscan) {

        /** User may request for a specific tree with the -t options.
         * The tree specified may be different from the actual one.
         * So, remove the -t option for the stop process and use it again
         * for the parse and start process. */
        char tree[info->treename.len + 1] ;
        auto_strings(tree, info->treename.s) ;
        uint32_t opstree = info->opt_tree ;
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

        FOREACH_GRAPH_SORT(service_graph_t, &graph, pos) {

            uint32_t index = graph.g.sort[pos] ;
            char *name = graph.g.sindex[index]->name ;
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
        info->opt_tree = opstree ;
    }

    /** force to parse again the service */
    for (n = 0 ; n < argc ; n++)
        sanitize_source(argv[n], info, flag) ;

    for (n = 0 ; n < ntostate ; n++) {

        /** live of the service still exist.
         * Reverse to the previous state of the toparse flag. */
        if (state_read_remote(&sta, tostate[n].sa.s + tostate[n].live.statedir)) {

            sta.toparse = STATE_FLAGS_FALSE ;

            if (!state_write_remote(&sta, tostate[n].sa.s + tostate[n].live.statedir))
                log_warnusys("write status file of: ", tostate[n].sa.s + tostate[n].live.statedir) ;
        }
    }

    if (nservice && rscan) {

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

        pos = 0 ;
        FOREACH_GRAPH_SORT(service_graph_t, &graph, pos) {

            uint32_t index = graph.g.sort[pos] ;
            char *name = graph.g.sindex[index]->name ;
            newargv[m++] = name ;
        }

        newargv[m] = 0 ;

        PROG = "start" ;
        e = ssexec_start(m, newargv, info) ;
        PROG = prog ;

        info->help = help ;
        info->usage = usage ;
    }

    if (ntoenable) {

        /** enable again the service if it was enabled */
        unsigned int m = 0 ;
        int nargc = 2 + ntoenable ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;

        char const *help = info->help ;
        char const *usage = info->usage ;

        info->help = help_enable ;
        info->usage = usage_enable ;

        newargv[m++] = "enable" ;

        n = 0 ;
        for (; n < ntoenable ; n++) {

            char *name = toenable[n].sa.s + toenable[n].name ;
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
        service_graph_destroy(&graph) ;

    return e ;
}
