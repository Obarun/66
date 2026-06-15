/*
 * ssexec_reconfigure.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <oblibs/opt.h>
#include <oblibs/strbuf.h>
#include <oblibs/sbl.h>
#include <oblibs/types.h>
#include <oblibs/hash.h>
#include <oblibs/environ.h>
#include <oblibs/string.h>

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

static uint8_t opt_nopropagate = 0 ;

static opt_t const opts_reconfigure[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",         .arg = OPT_NONE, .help = "print this help" },
    { .id = 'P',         .shortname = 'P', .longname = "no-propagate", .arg = OPT_NONE, .help = "do not propagate signal to its dependencies" },
} ;

static int on_reconfigure(int id, char const *arg, void *data)
{
    (void)arg ;
    (void)data ;

    switch (id) {

        case 'P' :

            opt_nopropagate = 1 ;
            break ;
    }

    return 0 ;
}

opt_cmd_t const cmd_reconfigure = {
    .name = "66 reconfigure",
    .help = "convenient command to bring down, unsupervise, parse again and bring up services in one pass",
    .operands = "service...",
    .opts = opts_reconfigure,
    .nopts = OPT_COUNT(opts_reconfigure),
    .on_option = &on_reconfigure,
    .fn = &ssexec_reconfigure,
} ;

int ssexec_reconfigure(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    uint8_t nopropagate = opt_nopropagate ;
    opt_nopropagate = 0 ;

    int rscan, e = 0 ;
    uint8_t propagate = 0 ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t flag = GRAPH_COLLECT_PARSE|GRAPH_WANT_REQUIREDBY, nservice = 0, pos = 0 ;
    resolve_service_t_ref pres = 0 ;
    _alloc_sbl_(tostop, SS_MAX_SERVICE * SS_MAX_SERVICE_NAME) ;
    _alloc_sbl_(toenable, SS_MAX_SERVICE * SS_MAX_SERVICE_NAME) ;
    ss_state_t sta = STATE_ZERO ;

    if (nopropagate) {
        FLAGS_CLEAR(flag, GRAPH_WANT_REQUIREDBY) ;
        propagate++ ;
    }

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

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
        int nargc = 3 + nservice + propagate ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;

        newargv[m++] = "stop" ;
        if (propagate)
            newargv[m++] = "-P" ;
        newargv[m++] = "-u" ;

        pos = 0 ;
        FOREACH_SBL(&tostop, pos)
            newargv[m++] = tostop.s + pos ;

        newargv[m] = 0 ;

        PROG = "stop" ;
        e = opt_dispatch(m, newargv, &cmd_stop, info) ;
        PROG = prog ;
        if (e)
            goto freed ;

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
        int nargc = 2 + nservice + propagate ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;

        newargv[m++] = "start" ;
        if (propagate)
            newargv[m++] = "-P" ;

        pos = 0 ;
        FOREACH_GRAPH_SORT(service_graph_t, &graph, pos) {

            uint32_t index = graph.g.sort[pos] ;
            char *name = graph.g.sindex[index]->name ;
            newargv[m++] = name ;
        }

        newargv[m] = 0 ;

        PROG = "start" ;
        e = opt_dispatch(m, newargv, &cmd_start, info) ;
        PROG = prog ;
    }

    if (sbl_count(&toenable)) {

        /** enable again the service if it was enabled */
        unsigned int m = 0 ;
        int nargc = 2 + sbl_count(&toenable) ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;

        newargv[m++] = "enable" ;

        pos = 0 ;
        FOREACH_SBL(&toenable, pos) {

            char *name = toenable.s + pos ;
            if (get_rstrlen_until(name,SS_LOG_SUFFIX) < 0)
                newargv[m++] = name ;
        }

        newargv[m] = 0 ;

        PROG= "enable" ;
        e = opt_dispatch(m, newargv, &cmd_enable, info) ;
        PROG = prog ;
    }

    freed:
        service_graph_destroy(&graph) ;

    return e ;
}
