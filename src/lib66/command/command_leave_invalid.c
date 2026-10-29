/*
 * command_leave_invalid.c
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
#include <oblibs/strbuf.h>

#include <66/config.h>
#include <66/ssexec.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/command.h>

static int bad(strbuf *msg, selection_service_t const *sel, uint32_t index, strbuf *frontends, bool up)
{
    int r, gone = 0 ;
    service_graph_t const *g = sel->g ;
    uint8_t state = service_graph_state(g, index) ;
    char const *name = service_graph_name(g, index) ;

    if (state == SERVICE_GRAPH_STATE_ABSENT) {

        if (!up || !bitset32_isvalid(&sel->met, index))
            return 0 ;

        gone = service_graph_frontend_gone(g, index, frontends) ;
        if (gone < 0)
            log_dieusys(LOG_EXIT_SYS, "find the frontend of: ", name) ;

        if (!gone) {
            msg->len = 0 ;
            if (!auto_strbuf(msg, SSEXEC_MSG_UNPARSED(name)))
                log_die_nomem("strbuf") ;
            return 1 ;
        }

    } else {

        if (!bitset32_isvalid(&sel->member, index) || !service_graph_invalid(state))
            return 0 ;
    }

    r = service_graph_invalid_message(msg, g, index, gone) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "compose the message of service: ", name) ;

    return r ;
}

int command_leave_invalid(selection_service_t *sel, strbuf *frontends, bool up)
{
    log_flow() ;

    int any = 0 ;
    uint32_t index ;
    service_graph_t const *g = sel->g ;
    bitset32_t seeds = bitset32_init(SS_MAX_SERVICE) ;
    selection_service_t drop = SELECTION_SERVICE_ZERO ;
    _cleanup_strbuf_ strbuf msg = STRBUF_ZERO ;

    for (index = 0 ; index < service_graph_nvertex(g) ; index++) {

        if (!bad(&msg, sel, index, frontends, up))
            continue ;

        log_warn(msg.s) ;
        bitset32_set(&seeds, index) ;
        any = 1 ;
    }

    if (!any)
        return 0 ;

    selection_service_policy_t policy = command_policy(up ? SELECTION_SERVICE_PROPAGATE_REQUIREDBY : SELECTION_SERVICE_PROPAGATE_NONE, false, false, false) ;

    selection_service_propagate(&drop, g, &seeds, &policy) ;

    FOREACH_SELECTION_MEMBER(&drop, index) {

        if (!bitset32_isvalid(&sel->member, index))
            continue ;

        log_warn("leaving out service: ", service_graph_name(g, index), bitset32_isvalid(&seeds, index) ? " -- it is invalid" : " -- it depends on an invalid service") ;
        selection_service_remove(sel, index) ;
    }

    return 1 ;
}
