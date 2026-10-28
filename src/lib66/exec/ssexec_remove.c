/*
 * ssexec_remove.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
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
#include <stdbool.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/bits.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/svc.h>
#include <66/tree.h>
#include <66/command.h>

static opt_t const opts_remove[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
} ;

opt_cmd_t const cmd_remove = {
    .name = "66 remove",
    .help = "remove services and cleanup all files belong to it from the system",
    .operands = "service...",
    .opts = opts_remove,
    .nopts = OPT_COUNT(opts_remove),
    .on_option = 0,
    .fn = &ssexec_remove,
} ;

static int select_removed(selection_service_t *sel, command_graph_t *d, char const *const *argv, int argc, ssexec_t *info, strbuf *frontends)
{
    int r, unknown = 0 ;
    bitset32_t named ;
    service_graph_t const *g = &d->g ;
    selection_service_policy_t policy = command_policy(SELECTION_SERVICE_PROPAGATE_REQUIREDBY, false, false, true) ;

    r = selection_service_names(&named, g, info->base.s, argv, argc, false, &unknown) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "find the services named") ;

    if (!r)
        command_refuse_unknown(argv[unknown], info->base.s, info->owner, true) ;

    *sel = (selection_service_t)SELECTION_SERVICE_ZERO ;
    selection_service_propagate(sel, g, &named, &policy) ;

    command_refuse_logger(g, &sel->named, "remove") ;

    command_refuse_selection(sel, frontends, SELECTION_SERVICE_REFUSE_MET_INVALID) ;

    if (service_graph_warn_invalid(g, frontends, &sel->member) < 0)
        log_dieusys(LOG_EXIT_SYS, "warn about the invalid services") ;

    if (service_graph_resolve_init(&d->store, g, info->base.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "initialize the store of services") ;

    return command_resolve_load(d, sel) ;
}

static int free_removed(selection_service_t const *sel, service_graph_resolve_t *store, ssexec_t *info, strbuf *up, strbuf *down)
{
    int r, e ;

    r = svc_scandir_ok(info->scandir.s) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "check the scandir: ", info->scandir.s) ;

    if (!r)
        return 0 ;

    e = command_free_lot(sel, store, info, up, down) ;
    if (e)
        return e ;

    return command_free_unreadable(sel, info, up, down) ;
}

static void warn_requiredby(selection_service_t const *sel, uint32_t index)
{
    service_graph_t const *g = sel->g ;
    uint32_t pos = 0, n = service_graph_vertex(g, index)->ndepends ;
    uint32_t const *depends = service_graph_depends(g, index) ;
    uint8_t const *origin = service_graph_dorigin(g, index) ;
    _cleanup_strbuf_ strbuf list = STRBUF_ZERO ;

    for (; pos < n ; pos++) {

        if (!(origin[pos] & SERVICE_GRAPH_EDGE_REQUIREDBY) || bitset32_isvalid(&sel->member, depends[pos]))
            continue ;

        if (!auto_strbuf(&list, list.len ? " " : "", service_graph_name(g, depends[pos])))
            log_die_nomem("strbuf") ;
    }

    if (list.len)
        log_warn("service: ", service_graph_name(g, index), " is removed, the RequiredBy of: ", list.s, " still names it") ;
}

static void remove_one(selection_service_t const *sel, uint32_t index, ssexec_t *info)
{
    service_graph_t const *g = sel->g ;
    uint32_t parent = service_graph_logparent(g, index) ;

    warn_requiredby(sel, index) ;

    if (parent != SERVICE_GRAPH_NOINDEX && bitset32_isvalid(&sel->member, parent))
        return ;

    if (selection_service_carried(sel, index))
        return ;

    if (service_remove_one(info->base.s, info->owner, service_graph_name(g, index)) < 0)
        log_dieusys(LOG_EXIT_SYS, "remove service: ", service_graph_name(g, index)) ;

    log_info("Removed successfully: ", service_graph_name(g, index)) ;
}

int ssexec_remove(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    int e ;
    uint32_t pos, index ;
    command_graph_t d = COMMAND_GRAPH_ZERO ;
    selection_service_t sel = SELECTION_SERVICE_ZERO ;
    strbuf frontends = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf up = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf down = STRBUF_ZERO ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    command_refuse_member(argv, argc, "remove") ;

    if (service_graph_load(&d.g, info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "load the graph of the system") ;

    while (!select_removed(&sel, &d, argv, argc, info, &frontends))
        command_graph_reload(&d, info, true) ;

    e = free_removed(&sel, &d.store, info, &up, &down) ;
    if (e) {
        command_put_back(&up, &down, &d.g, info, &frontends, false) ;
        log_dieu(LOG_EXIT_SYS, "stop the services to remove") ;
    }

    strbuf_free(&frontends) ;

    if (service_graph_write_begin(info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "lock the system") ;

    // in the stop order: what is out of the order first, then what depends before its dependency
    FOREACH_SELECTION_UNORDERED(&sel, index)
        remove_one(&sel, index, info) ;

    FOREACH_SELECTION_REVERSE(&sel, pos, index)
        remove_one(&sel, index, info) ;

    // a named absent service has nothing to remove: what depended on it is gone
    for (index = 0 ; index < service_graph_nvertex(&d.g) ; index++)
        if (bitset32_isvalid(&sel.named, index) && service_graph_state(&d.g, index) == SERVICE_GRAPH_STATE_ABSENT)
            log_info("Removed successfully: ", service_graph_name(&d.g, index)) ;

    command_write_end(0) ;

    service_graph_resolve_free(&d.store) ;
    service_graph_free(&d.g) ;

    return 0 ;
}
