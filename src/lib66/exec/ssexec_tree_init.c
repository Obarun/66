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
 * except according to the terms contained in the LICENSE file.
 */

#include <errno.h>
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
#include <66/selection.h>
#include <66/command.h>

static char const *tree_init_group = 0 ;

int on_tree_init(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {
        case 'g' : tree_init_group = arg ; break ;
    }

    return 0 ;
}

static int is_enabled(struct resolve_hash_s const *h)
{
    return h->res.enabled ? 1 : 0 ;
}

static void doit(strbuf *sa, ssexec_t *info, uint8_t stage)
{
    log_flow() ;

    size_t pos = 0 ;
    uint32_t index ;
    bitset32_t seeds = bitset32_init(SS_MAX_SERVICE) ;
    command_graph_t d = { .g = SERVICE_GRAPH_ZERO, .store = SERVICE_GRAPH_RESOLVE_ZERO, .reloaded = true } ;
    service_graph_t *g = &d.g ;
    service_graph_resolve_t *store = &d.store ;
    selection_service_t sel = SELECTION_SERVICE_ZERO ;
    selection_service_t enabled = SELECTION_SERVICE_ZERO ;
    selection_service_t lot = SELECTION_SERVICE_ZERO ;
    selection_service_policy_t none = command_policy(SELECTION_SERVICE_PROPAGATE_NONE, false, false, false) ;
    selection_service_policy_t policy = command_policy(SELECTION_SERVICE_PROPAGATE_DEPENDS, false, !info->opt_react, false) ;
    strbuf frontends = STRBUF_ZERO ;

    if (service_graph_load(&d.g, info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "load the graph of the system") ;

    FOREACH_SBL(sa, pos) {

        index = service_graph_find_parsed(g, sa->s + pos) ;

        if (index == SERVICE_GRAPH_NOINDEX) {
            log_trace("ignoring not parsed service: ", sa->s + pos) ;
            continue ;
        }

        bitset32_set(&seeds, index) ;
    }

    if (service_graph_resolve_init(store, g, info->base.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "initialize the store of services") ;

    selection_service_propagate(&sel, g, &seeds, &none) ;

    command_resolve_load(&d, &sel) ;

    if (selection_svc_filter(&enabled, &sel, store, 0, is_enabled) < 0)
        log_dieusys(LOG_EXIT_SYS, "read the services of the trees") ;

    selection_service_propagate(&sel, g, &enabled.member, &policy) ;

    if (service_graph_warn_invalid(g, &frontends, 0) < 0)
        log_dieusys(LOG_EXIT_SYS, "warn about the invalid services") ;

    strbuf_free(&frontends) ;

    FOREACH_SELECTION_MEMBER(&sel, index)
        if (service_graph_invalid(service_graph_state(g, index)))
            selection_service_remove(&sel, index) ;

    command_resolve_load(&d, &sel) ;

    if (svc_lot(&lot, &sel, store, stage) < 0)
        log_dieusys(LOG_EXIT_SYS, "select the services to initiate") ;

    if (!lot.member.count && stage == SVC_LOT_EARLIER)
        log_warn("no earlier service to initiate") ;

    else if (!lot.member.count)
        log_trace("no enabled service to initiate") ;

    else if (sanitize_init(&lot, store, stage, info->who) < 0)
        log_dieusys(LOG_EXIT_SYS, "initiate the services of the trees") ;

    service_graph_resolve_free(&d.store) ;
    service_graph_free(&d.g) ;
}

static void collect_tree_services(strbuf *out, char const *base, char const *treename)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf svc = STRBUF_ZERO ;
    resolve_enum_table_t table = E_TABLE_TREE_ZERO ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_TREE, &tres) ;
    size_t pos = 0 ;

    table.u.tree.id = E_RESOLVE_TREE_CONTENTS ;
    if (resolve_get_field(&svc, &wres, base, treename, table) <= 0)
        log_dieu(LOG_EXIT_SYS, "get services list from tree: ", treename) ;

    resolve_free_obj(&wres) ;

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
    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_TREE_MASTER, &mres) ;
    size_t pos = 0 ;

    table.u.tree.id = E_RESOLVE_TREE_MASTER_CONTENTS ;
    if (resolve_get_field(&master, &wres, info->base.s, SS_MASTER + 1, table) <= 0)
        log_dieu(LOG_EXIT_SYS, "get resolve Master file of trees") ;

    resolve_free_obj(&wres) ;

    FOREACH_SBL(&master, pos) {

        char const *treename = master.s + pos ;

        int r = tree_ongroups(info->base.s, treename, group) ;
        if (r < 0)
            log_dieu(LOG_EXIT_SYS, "read the groups of tree: ", treename) ;
        if (r != 1)
            continue ;

        r = tree_get_permissions(info->base.s, treename) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "read the permissions of tree: ", treename) ;
        if (!r) {
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
    uint8_t stage = SVC_LOT_BOOT ;
    char const *group = tree_init_group ;
    tree_init_group = 0 ;

    if (!argc && !group)
        log_die(LOG_EXIT_USER, "missing tree argument or --groups option") ;

    r = scan_mode(info->scandir.s, FLAGS_DIR) ;
    if (r < 0) log_die(LOG_EXIT_SYS,info->scandir.s, " conflicted format") ;
    if (!r && errno != ENOENT) log_dieusys(LOG_EXIT_SYS, "check scandir: ", info->scandir.s) ;
    if (!r) log_die(LOG_EXIT_USER,"scandir: ", info->scandir.s, " doesn't exist -- please execute \"66 scandir create\" command first") ;

    r = svc_scandir_ok(info->scandir.s) ;
    if (r != 1) stage = SVC_LOT_EARLIER ;

    if (argc) {

        char const *treename = argv[0] ;

        if (tree_isvalid(info->base.s, treename) <= 0)
            log_diesys(LOG_EXIT_USER, "invalid tree name: ", treename) ;

        r = tree_get_permissions(info->base.s, treename) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "read the permissions of tree: ", treename) ;

        if (!r)
            log_die(LOG_EXIT_USER, "You're not allowed to use the tree: ", treename) ;

        collect_tree_services(&sa, info->base.s, treename) ;

        if (sa.len)
            doit(&sa, info, stage) ;
        else
            log_info("Report: no services to initiate at tree: ", treename) ;

    } else {

        collect_group_services(&sa, info, group) ;

        if (sa.len)
            doit(&sa, info, stage) ;
        else
            log_info("Report: no services to initiate for group: ", group) ;
    }

    return 0 ;
}

