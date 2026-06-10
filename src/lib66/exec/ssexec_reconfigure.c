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
#include <stdbool.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
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

static bool on_groups(resolve_service_t *res)
{
    int r = tree_ongroups(res->sa.s + res->path.home, res->sa.s + res->treename, TREE_GROUPS_BOOT) ;

    if (r < 0)
        log_dieu(LOG_EXIT_SYS, "get groups of service: ", res->sa.s + res->name) ;

    if (r)
        return true ;

    return false ;
}

int ssexec_reconfigure(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int rscan, e = 0 ;
    uint8_t siglen = 0 ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t flag = GRAPH_COLLECT_PARSE|GRAPH_WANT_REQUIREDBY, nservice = 0, pos = 0 ;
    resolve_service_t_ref pres = 0 ;
    _alloc_sbl_(tostop, SS_MAX_SERVICE * SS_MAX_SERVICE_NAME) ;
    _alloc_sbl_(toenable, SS_MAX_SERVICE * SS_MAX_SERVICE_NAME) ;
    ss_state_t sta = STATE_ZERO ;

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

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    if (!environ_import_arguments(&sa, argv, argc))
        log_dieusys(LOG_EXIT_SYS, "import arguments") ;

    nservice = service_graph_build_arguments(&graph, argv, argc, info, flag) ;

    if (!nservice) {
        if (errno == EINVAL)
            log_dieusys(LOG_EXIT_SYS, "build the graph") ;

        log_die(LOG_EXIT_USER, "services selection is not available -- have you already parsed a service?") ;
    }

    if (!graph.g.nsort)
        log_warn_return(e,"no services found to handle") ;

    FOREACH_GRAPH_SORT(service_graph_t, &graph, pos) {

        uint32_t index = graph.g.sort[pos] ;
        char *name = graph.g.sindex[index]->name ;

        struct resolve_hash_s *hash = hash_search(&graph.hres, name) ;

        if (hash == NULL)
            log_die(LOG_EXIT_SYS, "get information of service: ", name, " -- please make a bug report") ;

        pres = &hash->res ;

        if (pres->inns) {
            // search first into the user commandline
            if (sbl_search(&sa, pres->sa.s + pres->inns) < 0) {
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

        if (pres->enabled && !pres->inns)
            if (!sbl_add(&toenable, pres->sa.s + pres->name))
                log_die_nomem("stack") ;

        if (!state_write_remote(&sta, status))
            log_dieusys(LOG_EXIT_SYS, "write status file of: ", name) ;

        /** services of group boot cannot be restarted, the changes will appear only at
         * next reboot.*/
        if (on_groups(pres))
            continue ;

        if (sta.issupervised == STATE_FLAGS_TRUE) {
            if (!sbl_add(&tostop, pres->sa.s + pres->name))
                log_die_nomem("strbuf") ;
        }
    }

    if (sbl_count(&tostop) && rscan) {

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

        pos = 0 ;
        FOREACH_SBL(&tostop, pos)
            newargv[m++] = tostop.s + pos ;

        newargv[m] = 0 ;

        PROG = "stop" ;
        e = ssexec_stop(m, newargv, info) ;
        PROG = prog ;
        if (e)
            goto freed ;

        info->help = help ;
        info->usage = usage ;

        info->treename.len = 0 ;
        if (!auto_strbuf(&info->treename, tree))
            log_die_nomem("strbuf") ;

        info->opt_tree = opstree ;
    }

    /** force to parse again the service */
    {
        pos = 0 ;
        FOREACH_GRAPH_SORT(service_graph_t, &graph, pos) {

            uint32_t index = graph.g.sort[pos] ;
            char *name = graph.g.sindex[index]->name ;

            struct resolve_hash_s *hash = hash_search(&graph.hres, name) ;

            if (hash == NULL)
                log_die(LOG_EXIT_SYS, "get information of service: ", name, " -- please make a bug report") ;

            /** only deal with service found in arguments */
            if (!hash->res.inns && sbl_search(&sa, name) >= 0)
                sanitize_source(name, info, flag) ;

            /** need to reverse the previous state change to
             * for current live service.*/
            if (state_read_remote(&sta, hash->res.sa.s + hash->res.live.statedir)) {

                sta.toparse = STATE_FLAGS_FALSE ;

                if (!state_write_remote(&sta, hash->res.sa.s + hash->res.live.statedir))
                    log_warnusys("write status file of: ", hash->res.sa.s + hash->res.live.statedir) ;
            }

        }
    }

    if (sbl_count(&tostop) && rscan) {

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

    if (sbl_count(&toenable)) {

        /** enable again the service if it was enabled */
        unsigned int m = 0 ;
        int nargc = 2 + sbl_count(&toenable) ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;

        char const *help = info->help ;
        char const *usage = info->usage ;

        info->help = help_enable ;
        info->usage = usage_enable ;

        newargv[m++] = "enable" ;

        pos = 0 ;
        FOREACH_SBL(&toenable, pos) {

            char *name = toenable.s + pos ;
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
