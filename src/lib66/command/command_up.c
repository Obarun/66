/*
 * command_up.c
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
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/bits.h>

#include <66/ssexec.h>
#include <66/parse.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/svc.h>
#include <66/command.h>


static int select_lot(selection_service_t *lot, command_graph_t *d, char const *const *argv, int argc, ssexec_t *info, strbuf *frontends, uint8_t target, bool nopropagate, uint8_t stage, bool tree, bool warned, bool *left)
{
    selection_service_t sel ;
    bool down = target == SVC_TARGET_DOWN ;
    selection_service_policy_t policy = command_policy(SELECTION_SERVICE_PROPAGATE_DEPENDS, nopropagate, !info->opt_react && !down, false) ;

    if (!command_argv_to_select(&sel, d, argv, argc, info, &policy, frontends, tree ? 0 : SELECTION_SERVICE_REFUSE_NAMED_ABSENT | SELECTION_SERVICE_REFUSE_MET_ABSENT | SELECTION_SERVICE_REFUSE_NAMED_INVALID | SELECTION_SERVICE_REFUSE_MET_INVALID))
        return 0 ;

    if (tree)
        *left = command_leave_invalid(&sel, frontends, true) ;

    if (!warned && service_graph_warn_invalid(&d->g, frontends, &sel.member) < 0)
        log_dieusys(LOG_EXIT_SYS, "warn about the invalid services") ;

    if (svc_lot(lot, &sel, &d->store, stage) < 0)
        log_dieusys(LOG_EXIT_SYS, "select the services to start") ;

    // nothing runs: a conflicting service running refuses nothing
    if (down)
        return 1 ;

    return command_refuse_conflict(lot, d, info, SVC_CONFLICT_RUNNING) ;
}

int command_up(char const *const *argv, int argc, ssexec_t *info, strbuf *frontends, uint8_t target, bool nopropagate, uint8_t stage, bool tree, bool warned)
{
    log_flow() ;

    int e ;
    bool left = false ;
    command_graph_t d = COMMAND_GRAPH_ZERO ;
    selection_service_t lot = SELECTION_SERVICE_ZERO ;
    uint32_t flags = (nopropagate ? PARSE_NODEPS : 0) | (info->opt_react ? PARSE_NOFROM : 0) | (tree ? PARSE_LEAVEGONE : 0) ;

    command_refuse_scandir(info) ;

    if (service_graph_load(&d.g, info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "load the graph of the system") ;

    command_parse_missing(&d.g, argv, argc, info, flags) ;

    while (!select_lot(&lot, &d, argv, argc, info, frontends, target, nopropagate, stage, tree, warned, &left))
        command_graph_reload(&d, info, !tree) ;

    e = command_lot_start(&lot, &d.store, info, target ? target : SVC_TARGET_READY, stage, 0) ;

    if (!e && left)
        e = LOG_EXIT_SYS ;

    service_graph_resolve_free(&d.store) ;
    service_graph_free(&d.g) ;

    return e ;
}
