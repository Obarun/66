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
 * except according to the terms contained in the LICENSE file.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/bits.h>
#include <oblibs/hash.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/config.h>
#include <66/ssexec.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/parse.h>
#include <66/resolve.h>
#include <66/svc.h>
#include <66/tree.h>
#include <66/utils.h>
#include <66/enum_service.h>
#include <66/command.h>

static uint8_t opt_nopropagate = 0 ;
static uint8_t opt_cleanup = 0 ;

static opt_t const opts_reconfigure[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",         .arg = OPT_NONE, .help = "print this help" },
    { .id = 'P',         .shortname = 'P', .longname = "no-propagate", .arg = OPT_NONE, .help = "do not propagate signal to its dependencies" },
    { .id = 'c',         .shortname = 'c', .longname = "cleanup",      .arg = OPT_NONE, .help = "remove what the services left on disk before parsing them again" },
} ;

static int on_reconfigure(int id, char const *arg, void *data)
{
    (void)arg ;
    (void)data ;

    switch (id) {

        case 'P' :

            opt_nopropagate = 1 ;
            break ;

        case 'c' :

            opt_cleanup = 1 ;
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

typedef struct restore_s restore_t ;
struct restore_s
{
    strbuf up ; // running, put back up
    strbuf down ; // supervised and down, put back supervised and down
    strbuf enabled ; // named and enabled, enabled again after -c
    strbuf unknown ; // named, its activation cannot be read (-c)
    strbuf trees ; // a named unreadable service and its tree, pairs of names
} ;

#define RESTORE_ZERO { STRBUF_ZERO, STRBUF_ZERO, STRBUF_ZERO, STRBUF_ZERO, STRBUF_ZERO }

static void restore_free(restore_t *p)
{
    strbuf_free(&p->up) ;
    strbuf_free(&p->down) ;
    strbuf_free(&p->enabled) ;
    strbuf_free(&p->unknown) ;
    strbuf_free(&p->trees) ;
}

static void add(strbuf *list, char const *name)
{
    if (!sbl_add(list, name))
        log_die_nomem("strbuf") ;
}

static void refuse_operands(bitset32_t *named, service_graph_t const *g, char const *const *argv, int argc, ssexec_t *info, bool cleanup)
{
    int r, unknown ;
    _cleanup_strbuf_ strbuf msg = STRBUF_ZERO ;

    *named = bitset32_init(SS_MAX_SERVICE) ;

    for (int i = 0 ; i < argc ; i++) {

        bitset32_t one ;
        uint32_t index ;

        r = selection_service_names(&one, g, info->base.s, argv + i, 1, true, &unknown) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "find the service: ", argv[i]) ;

        if (!r)
            continue ;

        command_refuse_logger(g, &one, "reconfigure") ;

        for (index = 0 ; !bitset32_isvalid(&one, index) ; index++) ;

        uint8_t state = service_graph_state(g, index) ;

        if (service_graph_invalid(state)) {

            msg.len = 0 ;
            r = service_graph_invalid_message(&msg, g, index, false) ;
            if (r < 0)
                log_dieusys(LOG_EXIT_SYS, "compose the message of service: ", service_graph_name(g, index)) ;

            if (!cleanup)
                log_die(LOG_EXIT_USER, msg.s) ;

            log_warn(msg.s) ;
        }

        bitset32_set(named, index) ;
    }
}

static void restore_unreadable(restore_t *p, selection_service_t const *sel, ssexec_t *info)
{
    uint32_t index ;
    service_graph_t const *g = sel->g ;

    FOREACH_SELECTION_MEMBER(sel, index) {

        if (!bitset32_isvalid(&sel->named, index) || service_graph_state(g, index) != SERVICE_GRAPH_STATE_UNREADABLE)
            continue ;

        int r ;
        char const *name = service_graph_name(g, index) ;
        struct resolve_hash_s h = RESOLVE_HASH_ZERO ;
        _cleanup_strbuf_ strbuf tree = STRBUF_ZERO ;

        r = service_read(&h, info->base.s, name, 0) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "read service: ", name) ;

        if (r) {

            if (h.res.enabled)
                add(&p->enabled, name) ;

            if (h.res.treename && !auto_strbuf(&tree, h.res.sa.s + h.res.treename))
                log_die_nomem("strbuf") ;

        } else {

            add(&p->unknown, name) ;

            r = tree_find_service(&tree, info->base.s, name) ;
            if (r < 0)
                log_dieusys(LOG_EXIT_SYS, "find the tree of service: ", name) ;
        }

        service_read_free(&h) ;

        if (tree.len) {
            add(&p->trees, name) ;
            add(&p->trees, tree.s) ;
        }
    }
}

static int select_lot(selection_service_t *sel, restore_t *p, bitset32_t const *named, command_graph_t *d, ssexec_t *info, strbuf *frontends, bool nopropagate)
{
    uint32_t index ;
    service_graph_t const *g = &d->g ;
    service_graph_resolve_t *store = &d->store ;
    // the stop follows the required-by dependencies, a logger is freed with its service
    selection_service_policy_t policy = command_policy(SELECTION_SERVICE_PROPAGATE_REQUIREDBY, nopropagate, false, true) ;

    *sel = (selection_service_t)SELECTION_SERVICE_ZERO ;
    selection_service_propagate(sel, g, named, &policy) ;

    command_refuse_selection(sel, frontends, SELECTION_SERVICE_REFUSE_MET_INVALID) ;

    if (service_graph_resolve_init(store, g, info->base.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "initialize the store of services") ;

    if (!nopropagate) {

        selection_service_t up = SELECTION_SERVICE_ZERO ;
        selection_service_policy_t start = command_policy(SELECTION_SERVICE_PROPAGATE_DEPENDS, false, !info->opt_react, false) ;

        selection_service_propagate(&up, g, &sel->member, &start) ;
        command_refuse_selection(&up, frontends, SELECTION_SERVICE_REFUSE_MET_INVALID) ;

        if (!command_resolve_load(d, &up))
            return 0 ;
    }

    if (service_graph_warn_invalid(g, frontends, &sel->member) < 0)
        log_dieusys(LOG_EXIT_SYS, "warn about the invalid services") ;

    if (!command_resolve_load(d, sel))
        return 0 ;

    FOREACH_SELECTION_MEMBER(sel, index) {

        struct resolve_hash_s *h ;

        if (!bitset32_isvalid(&sel->named, index))
            continue ;

        int r = service_graph_resolve_get(store, index, 0, &h) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "read service: ", service_graph_name(g, index)) ;

        if (r && h->res.enabled)
            add(&p->enabled, service_graph_name(g, index)) ;
    }

    restore_unreadable(p, sel, info) ;

    return 1 ;
}

static void keep_trees(hash_t *hres, restore_t *p, ssexec_t *info)
{
    size_t pos = 0 ;
    struct resolve_hash_s *c, *tmp ;
    resolve_enum_table_t table = E_TABLE_SERVICE_CONFIG_ZERO ;

    if (info->opt_tree)
        return ;

    table.u.service.id = E_RESOLVE_SERVICE_CONFIG_TREENAME ;

    while (pos < p->trees.len) {

        char const *name = p->trees.s + pos ;
        char const *tree = name + strlen(name) + 1 ;

        pos = (size_t)(tree - p->trees.s) + strlen(tree) + 1 ;

        HASH_FOREACH(hres, c, tmp) {

            // a member lives in the tree of its module
            if (strcmp(c->name, name) && (!c->res.inns || strcmp(c->res.sa.s + c->res.inns, name)))
                continue ;

            resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE, &c->res) ;

            if (!resolve_modify_field_by(&w, table, tree))
                log_dieusys(LOG_EXIT_SYS, "set the tree of service: ", c->name) ;
        }
    }
}

static int write_services(hash_t *hres, bitset32_t const *named, bitset32_t const *dropped, service_graph_t *g, ssexec_t *info, bool cleanup, strbuf *msg)
{
    uint32_t index ;
    struct resolve_hash_s *c, *tmp ;

    if (!command_write_begin_checked(hres, info, msg))
        return 0 ;

    for (index = 0 ; cleanup && index < service_graph_nvertex(g) ; index++) {

        if (!bitset32_isvalid(named, index) || service_graph_state(g, index) == SERVICE_GRAPH_STATE_ABSENT)
            continue ;

        if (service_remove_one(info->base.s, info->owner, service_graph_name(g, index)) < 0)
            log_dieusys(LOG_EXIT_SYS, "remove service: ", service_graph_name(g, index)) ;
    }

    if (cleanup) {
        HASH_FOREACH(hres, c, tmp)
            c->beforeread = 0 ;
    }

    if (parse_write(hres, info) <= 0)
        log_dieusys(LOG_EXIT_SYS, "write the services") ;

    for (index = 0 ; index < service_graph_nvertex(g) ; index++) {

        if (!bitset32_isvalid(dropped, index))
            continue ;

        if (service_remove_one(info->base.s, info->owner, service_graph_name(g, index)) < 0)
            log_dieusys(LOG_EXIT_SYS, "remove service: ", service_graph_name(g, index)) ;

        log_info("Removed successfully: ", service_graph_name(g, index)) ;
    }

    command_write_end(g) ;

    return 1 ;
}

static void drop(strbuf *list, service_graph_t const *g)
{
    size_t pos = 0 ;
    _cleanup_strbuf_ strbuf kept = STRBUF_ZERO ;

    FOREACH_SBL(list, pos) {

        if (service_graph_find_parsed(g, list->s + pos) != SERVICE_GRAPH_NOINDEX)
            add(&kept, list->s + pos) ;
    }

    list->len = 0 ;

    if (kept.len && !strbuf_catb(list, kept.s, kept.len))
        log_die_nomem("strbuf") ;
}

static int select_names(selection_service_t *sel, command_graph_t *d, strbuf *list, ssexec_t *info, bool logger)
{
    size_t pos = 0 ;
    int n = 0 ;
    char const *argv[sbl_count(list) + 1] ;
    selection_service_policy_t policy = command_policy(SELECTION_SERVICE_PROPAGATE_NONE, false, false, logger) ;

    FOREACH_SBL(list, pos)
        argv[n++] = list->s + pos ;

    argv[n] = 0 ;

    return command_argv_to_select(sel, d, argv, n, info, &policy, 0, 0) ;
}

static void enable_again(restore_t *p, command_graph_t *d, ssexec_t *info)
{
    size_t pos = 0 ;
    selection_service_t sel ;

    FOREACH_SBL(&p->unknown, pos)
        log_warn("service: ", p->unknown.s + pos, " was enabled or not, its resolve file does not tell -- enable it again with 66 enable ", p->unknown.s + pos, " if it was") ;

    drop(&p->enabled, &d->g) ;

    if (!p->enabled.len)
        return ;

    d->reloaded = false ;

    while (!select_names(&sel, d, &p->enabled, info, true))
        command_graph_reload(d, info, false) ;

    if (service_graph_write_begin(info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "lock the system") ;

    if (service_enable_selection(&sel, &d->store, true, 0) < 0)
        log_dieusys(LOG_EXIT_SYS, "enable the services again") ;

    command_write_end(0) ;

    service_graph_resolve_free(&d->store) ;
}

int ssexec_reconfigure(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    bool nopropagate = opt_nopropagate, cleanup = opt_cleanup ;
    opt_nopropagate = 0 ;
    opt_cleanup = 0 ;

    int r, e = 0, scandir ;
    hash_t hres = HASH_ZERO ;
    command_graph_t d = COMMAND_GRAPH_ZERO ;
    selection_service_t sel = SELECTION_SERVICE_ZERO ;
    bitset32_t named ;
    bitset32_t dropped = bitset32_init(SS_MAX_SERVICE) ;
    restore_t restore = RESTORE_ZERO ;
    strbuf frontends = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf msg = STRBUF_ZERO ;
    uint32_t flags = PARSE_FORCE | (nopropagate ? PARSE_NODEPS : 0) ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    command_refuse_member(argv, argc, "reconfigure") ;

    scandir = svc_scandir_ok(info->scandir.s) ;
    if (scandir < 0)
        log_dieusys(LOG_EXIT_SYS, "check the scandir: ", info->scandir.s) ;

    if (!hash_init(&hres, 0, offsetof(struct resolve_hash_s, node)))
        log_dieusys(LOG_EXIT_SYS, "initialize hash table") ;

    if (service_graph_load(&d.g, info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "load the graph of the system") ;

    refuse_operands(&named, &d.g, argv, argc, info, cleanup) ;

    r = parse_names(&hres, &d.g, argv, argc, info, flags, &msg) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "parse the services") ;

    if (!r)
        log_die(LOG_EXIT_USER, msg.s) ;

    r = parse_check(&hres, info, &msg) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "check the services to parse") ;

    if (!r)
        log_die(LOG_EXIT_USER, msg.s) ;

    while (!select_lot(&sel, &restore, &named, &d, info, &frontends, nopropagate)) {
        restore_free(&restore) ;
        restore = (restore_t)RESTORE_ZERO ;
        command_graph_reload(&d, info, true) ;
        refuse_operands(&named, &d.g, argv, argc, info, cleanup) ;
    }

    if (parse_dropped(&dropped, &hres, &d.g) < 0)
        log_dieusys(LOG_EXIT_SYS, "find the services the parse drops") ;

    if (scandir) {

        e = command_free_lot(&sel, &d.store, info, &restore.up, &restore.down) ;

        if (!e)
            e = command_free_unreadable(&sel, info, &restore.up, &restore.down) ;

        if (e) {
            command_put_back(&restore.up, &restore.down, &d.g, info, &frontends, nopropagate) ;
            goto freed ;
        }
    }

    keep_trees(&hres, &restore, info) ;

    service_graph_resolve_free(&d.store) ;

    if (!write_services(&hres, &sel.named, &dropped, &d.g, info, cleanup, &msg)) {

        if (scandir) {

            command_put_back(&restore.up, &restore.down, &d.g, info, &frontends, nopropagate) ;
        }

        log_die(LOG_EXIT_USER, msg.s) ;
    }

    if (scandir) {

        drop(&restore.up, &d.g) ;
        drop(&restore.down, &d.g) ;

        e = command_put_back(&restore.up, &restore.down, 0, info, &frontends, nopropagate) ;
    }

    if (cleanup) {

        if (scandir && (restore.up.len || restore.down.len) && service_graph_reload(&d.g, info->owner, info->base.s, info->live.s) < 0)
            log_dieusys(LOG_EXIT_SYS, "load the graph of the system") ;

        enable_again(&restore, &d, info) ;
    }

    freed:
        service_graph_resolve_free(&d.store) ;
        service_graph_free(&d.g) ;
        resolve_hash_free(&hres) ;
        restore_free(&restore) ;
        strbuf_free(&frontends) ;

    return e ;
}
