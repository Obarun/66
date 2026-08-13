/*
 * ssexec_tree_init.c
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stddef.h>
#include <sys/stat.h>
#include <string.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/types.h>
#include <oblibs/sbl.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>
#include <oblibs/hash.h>

#include <66/constants.h>
#include <66/config.h>
#include <66/service.h>
#include <66/tree.h>
#include <66/svc.h>
#include <66/ssexec.h>
#include <66/graph.h>
#include <66/sanitize.h>

static char const *tree_init_group = 0 ;

int on_tree_init(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {
        case 'g' : tree_init_group = arg ; break ;
    }

    return 0 ;
}

static void doit(strbuf *sa, ssexec_t *info, uint8_t earlier)
{
    log_flow() ;

    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t flag = GRAPH_WANT_DEPENDS, nservice = 0 ;
    struct resolve_hash_s *c, *tmp ;

    if (earlier)
        FLAGS_SET(flag, GRAPH_WANT_EARLIER) ;

    if (!service_graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
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

    HASH_FOREACH(&graph.hres, c, tmp) {

        if (c->res.enabled) {

            if (!sbl_add(sa, c->name))
                log_die_nomem("strbuf") ;

        } else
            log_trace("ignoring not enabled service: ", c->name) ;
    }

    if (!service_graph_nresolve(&graph, sa->s, sa->len, flag)) {
        errno = EINVAL ;
        log_dieusys(LOG_EXIT_SYS, "resolve the graph") ;
    }

    sanitize_init(&graph, flag, info->who) ;

    service_graph_destroy(&graph) ;
}

static void collect_tree_services(strbuf *out, char const *base, char const *treename)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf svc = STRBUF_ZERO ;
    resolve_enum_table_t table = E_TABLE_TREE_ZERO ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    size_t pos = 0 ;

    table.u.tree.id = E_RESOLVE_TREE_CONTENTS ;
    if (!resolve_get_field(&svc, wres, base, treename, table))
        log_dieu(LOG_EXIT_SYS, "get services list from tree: ", treename) ;

    resolve_free(wres) ;

    FOREACH_SBL(&svc, pos) {
        if (!sbl_add(out, svc.s + pos))
            log_die_nomem("strbuf") ;
    }
}

static void collect_group_services(strbuf *out, ssexec_t *info, char const *group)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf master = STRBUF_ZERO ;
    resolve_enum_table_t table = E_TABLE_TREE_MASTER_ZERO ;
    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;
    size_t pos = 0 ;

    table.u.tree.id = E_RESOLVE_TREE_MASTER_CONTENTS ;
    if (!resolve_get_field(&master, wres, info->base.s, SS_MASTER + 1, table))
        log_dieu(LOG_EXIT_SYS, "get resolve Master file of trees") ;

    resolve_free(wres) ;

    FOREACH_SBL(&master, pos) {

        char const *treename = master.s + pos ;

        int r = tree_ongroups(info->base.s, treename, group) ;
        if (r < 0)
            log_dieu(LOG_EXIT_SYS, "read the groups of tree: ", treename) ;
        if (r != 1)
            continue ;

        if (!tree_get_permissions(info->base.s, treename)) {
            log_trace("not allowed to use the tree, skipping: ", treename) ;
            continue ;
        }

        collect_tree_services(out, info->base.s, treename) ;
    }
}

int ssexec_tree_init(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    int r ;
    uint8_t earlier = 0 ;
    char const *group = tree_init_group ;
    tree_init_group = 0 ;

    if (!argc && !group)
        log_die(LOG_EXIT_USER, "missing tree argument or --groups option") ;

    r = scan_mode(info->scandir.s, S_IFDIR) ;
    if (r < 0) log_die(LOG_EXIT_SYS,info->scandir.s, " conflicted format") ;
    if (!r) log_die(LOG_EXIT_USER,"scandir: ", info->scandir.s, " doesn't exist -- please execute \"66 scandir create\" command first") ;

    r = svc_scandir_ok(info->scandir.s) ;
    if (r != 1) earlier = 1 ;

    if (argc) {

        char const *treename = argv[0] ;

        if (!tree_isvalid(info->base.s, treename))
            log_diesys(LOG_EXIT_USER, "invalid tree name: ", treename) ;

        if (!tree_get_permissions(info->base.s, treename))
            log_die(LOG_EXIT_USER, "You're not allowed to use the tree: ", treename) ;

        collect_tree_services(&sa, info->base.s, treename) ;

        if (sa.len)
            doit(&sa, info, earlier) ;
        else
            log_info("Report: no services to initiate at tree: ", treename) ;

    } else {

        collect_group_services(&sa, info, group) ;

        if (sa.len)
            doit(&sa, info, earlier) ;
        else
            log_info("Report: no services to initiate for group: ", group) ;
    }

    return 0 ;
}

