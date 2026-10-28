/*
 * command_down.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
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
#include <oblibs/bits.h>

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/svc.h>
#include <66/enum_parser.h>
#include <66/command.h>

static int is_tick(struct resolve_hash_s const *h)
{
    return h->res.type == E_PARSER_TYPE_EVENT ;
}

static void tick_sources(bitset32_t *sources, selection_service_t const *sel, service_graph_resolve_t *store)
{
    selection_service_t ticks ;

    if (selection_svc_filter(&ticks, sel, store, 0, is_tick) < 0)
        log_dieusys(LOG_EXIT_SYS, "read the services of the selection") ;

    *sources = ticks.member ;
}

static int select_lot(selection_service_t *lot, command_graph_t *d, char const *const *argv, int argc, ssexec_t *info, strbuf *frontends, bool nopropagate, bool unsupervise, bool tree, bool logger, bool *left)
{
    bitset32_t sources ;
    service_graph_t const *g = &d->g ;
    selection_service_t sel ;
    selection_service_policy_t policy = command_policy(SELECTION_SERVICE_PROPAGATE_REQUIREDBY, nopropagate, false, unsupervise && !nopropagate) ;

    if (!command_argv_to_select(&sel, d, argv, argc, info, &policy, frontends, 0))
        return 0 ;

    if (unsupervise && !nopropagate) {

        tick_sources(&sources, &sel, &d->store) ;
        policy.reactors = &sources ;

        selection_service_propagate(&sel, g, &sel.named, &policy) ;

        if (!command_resolve_load(d, &sel))
            return 0 ;
    }

    if (logger)
        command_refuse_logger(g, &sel.named, "free") ;

    if (tree) {

        *left = command_leave_invalid(&sel, frontends, false) ;
        command_refuse_selection(&sel, frontends, SELECTION_SERVICE_REFUSE_NAMED_ABSENT) ;

    } else {

        command_refuse_selection(&sel, frontends, SELECTION_SERVICE_REFUSE_NAMED_ABSENT | SELECTION_SERVICE_REFUSE_NAMED_INVALID | SELECTION_SERVICE_REFUSE_MET_INVALID) ;
    }

    if (service_graph_warn_invalid(g, frontends, &sel.member) < 0)
        log_dieusys(LOG_EXIT_SYS, "warn about the invalid services") ;

    if (svc_lot(lot, &sel, &d->store, SVC_LOT_COMMAND) < 0)
        log_dieusys(LOG_EXIT_SYS, "select the services to stop") ;

    return 1 ;
}

int command_down(char const *const *argv, int argc, ssexec_t *info, strbuf *frontends, bool nopropagate, bool unsupervise, bool tree, bool logger)
{
    log_flow() ;

    int e ;
    bool left = false ;
    command_graph_t d = COMMAND_GRAPH_ZERO ;
    selection_service_t lot = SELECTION_SERVICE_ZERO ;

    command_refuse_scandir(info) ;

    if (service_graph_load(&d.g, info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "load the graph of the system") ;

    while (!select_lot(&lot, &d, argv, argc, info, frontends, nopropagate, unsupervise, tree, logger, &left))
        command_graph_reload(&d, info, !tree) ;

    e = command_lot_stop(&lot, &d.store, info, unsupervise ? SVC_TARGET_FREE : SVC_TARGET_DOWN, "-d", 0, 0) ;

    if (!e && left)
        e = LOG_EXIT_SYS ;

    service_graph_resolve_free(&d.store) ;
    service_graph_free(&d.g) ;

    return e ;
}
