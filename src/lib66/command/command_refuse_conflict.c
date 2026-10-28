/*
 * command_refuse_conflict.c
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

#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/svc.h>
#include <66/command.h>

int command_refuse_conflict(selection_service_t const *lot, command_graph_t *d, ssexec_t const *info, uint8_t mode)
{
    log_flow() ;

    uint32_t member, other ;
    service_graph_t const *g = lot->g ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    int r = svc_conflict(lot, &d->store, info->base.s, mode, &member, &other) ;

    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "check the conflicts of the selection") ;

    if (!r && d->reloaded)
        log_dieu(LOG_EXIT_SYS, "check the conflicts of the selection -- the system changed under the command") ;

    if (!r || member == SERVICE_GRAPH_NOINDEX)
        return r ;

    if (service_graph_state(g, other) == SERVICE_GRAPH_STATE_UNREADABLE) {

        r = service_graph_invalid_message(&sa, g, other, false) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "compose the message of service: ", service_graph_name(g, other)) ;

        if (r)
            log_die(LOG_EXIT_USER, sa.s) ;
    }

    log_die(LOG_EXIT_USER, "conflicting service for '", service_graph_name(g, member), "' -- please ", mode == SVC_CONFLICT_ENABLED ? "disable" : "stop", " the '", service_graph_name(g, other), "' service first.") ;
}
