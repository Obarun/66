/*
 * ssexec_tree_init.c
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

#include <string.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/sgetopt.h>

#include <66/constants.h>
#include <66/config.h>
#include <66/service.h>
#include <66/tree.h>
#include <66/svc.h>
#include <66/ssexec.h>
#include <66/state.h>
#include <66/graph.h>
#include <66/sanitize.h>

static void doit(stralloc *sa, ssexec_t *info, uint8_t earlier)
{
    log_flow() ;

    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t flag = GRAPH_WANT_DEPENDS, nservice = 0 ;
    struct resolve_hash_s *c, *tmp ;

    if (earlier)
        FLAGS_SET(flag, GRAPH_WANT_EARLIER) ;

    if (!graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the service graph") ;

    nservice = service_graph_ncollect(&graph, sa->s, sa->len, info, flag) ;

    if (!nservice && earlier) {
        service_graph_destroy(&graph) ;
        log_warn("no earlier service to initiate") ;
        return ;
    }

    if (!nservice)
        log_die(LOG_EXIT_USER, "services selection is not available -- have you already parsed a service?") ;

    sa->len = 0 ;

    HASH_ITER(hh, graph.hres, c, tmp) {

        if (c->res.enabled) {

            if (!sastr_add_string(sa, c->name))
                log_die_nomem("stack") ;

        } else
            log_trace("ignoring not enabled service: ", c->name) ;
    }

    if (!service_graph_nresolve(&graph, sa->s, sa->len, flag)) {
        errno = EINVAL ;
        log_dieusys(LOG_EXIT_SYS, "resolve the graph") ;
    }

    sanitize_init(&graph, flag) ;

    service_graph_destroy(&graph) ;
}

int ssexec_tree_init(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    _alloc_sa_(sa) ;
    int r ;
    uint8_t earlier = 0 ;
    char const *treename = 0 ;
    resolve_enum_table_t table = E_TABLE_TREE_ZERO ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_TREE_INIT, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                default :
                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (!argc)
        log_usage(info->usage, "\n", info->help) ;

    treename = argv[0] ;

    if (!tree_isvalid(info->base.s, treename))
        log_diesys(LOG_EXIT_USER, "invalid tree name: ", treename) ;

    if (!tree_get_permissions(info->base.s, treename))
        log_die(LOG_EXIT_USER, "You're not allowed to use the tree: ", treename) ;

    r = scan_mode(info->scandir.s, S_IFDIR) ;
    if (r < 0) log_die(LOG_EXIT_SYS,info->scandir.s, " conflicted format") ;
    if (!r) log_die(LOG_EXIT_USER,"scandir: ", info->scandir.s, " doesn't exist -- please execute \"66 scandir create\" command first") ;

    r = svc_scandir_ok(info->scandir.s) ;
    if (r != 1) earlier = 1 ;

    table.u.tree.id = E_RESOLVE_TREE_CONTENTS ;
    if (!resolve_get_field_tosa_g(&sa, info->base.s, treename, DATA_TREE, table))
        log_dieu(LOG_EXIT_SYS, "get services list from tree: ", treename) ;

    if (sa.len) {

        doit(&sa, info, earlier) ;

    } else {

        log_info("Report: no services to initiate at tree: ", treename) ;
    }

    return 0 ;
}
