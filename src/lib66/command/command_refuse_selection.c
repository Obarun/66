/*
 * command_refuse_selection.c
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

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/command.h>

#define GRAPH_REFUSE (SELECTION_SERVICE_REFUSE_NAMED_ABSENT | SELECTION_SERVICE_REFUSE_MET_ABSENT | SELECTION_SERVICE_REFUSE_NAMED_INVALID | SELECTION_SERVICE_REFUSE_MET_INVALID)
#define NAMED_REFUSE (SELECTION_SERVICE_REFUSE_NAMED_ABSENT | SELECTION_SERVICE_REFUSE_NAMED_INVALID)

static uint32_t shows(selection_service_t const *sel, uint32_t index)
{
    service_graph_t const *g = sel->g ;
    uint8_t state = service_graph_state(g, index) ;
    int named = bitset32_isvalid(&sel->named, index) != 0 ;
    int member = bitset32_isvalid(&sel->member, index) != 0 ;
    uint32_t f = 0 ;

    if (named && state == SERVICE_GRAPH_STATE_ABSENT)
        f |= SELECTION_SERVICE_REFUSE_NAMED_ABSENT ;

    if (!named && bitset32_isvalid(&sel->met, index))
        f |= SELECTION_SERVICE_REFUSE_MET_ABSENT ;

    if (named && service_graph_invalid(state))
        f |= SELECTION_SERVICE_REFUSE_NAMED_INVALID ;

    if (!named && member && service_graph_invalid(state))
        f |= SELECTION_SERVICE_REFUSE_MET_INVALID ;

    return f ;
}

static uint32_t first_refused(selection_service_t const *sel, uint32_t refusals)
{
    service_graph_t const *g = sel->g ;
    uint32_t index = 0, nsort = service_graph_nsort(g), n = service_graph_nvertex(g) ;

    for (uint32_t pos = 0 ; pos < nsort ; pos++) {

        index = service_graph_sort(g, pos) ;

        if (shows(sel, index) & refusals)
            return index ;
    }

    for (index = 0 ; index < n ; index++) {

        if (service_graph_state(g, index) == SERVICE_GRAPH_STATE_UNORDERED && (shows(sel, index) & refusals))
            return index ;
    }

    return SERVICE_GRAPH_NOINDEX ;
}

static void refuse(service_graph_t const *g, uint32_t index, strbuf *frontends, bool gone)
{
    int r ;
    char const *name = service_graph_name(g, index) ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    if (service_graph_state(g, index) == SERVICE_GRAPH_STATE_ABSENT) {

        r = service_graph_frontend_gone(g, index, frontends) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "find the frontend of: ", name) ;

        if (!r)
            log_die(LOG_EXIT_USER, SSEXEC_MSG_UNPARSED(name)) ;

        gone = true ;
    }

    r = service_graph_invalid_message(&sa, g, index, gone) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "compose the message of service: ", name) ;

    if (!r)
        log_die(LOG_EXIT_SYS, "service: ", name, " is refused without a cause -- please make a bug report") ;

    log_die(LOG_EXIT_USER, sa.s) ;
}

static void refuse_gone(selection_service_t const *sel, uint32_t index, strbuf *frontends, uint32_t refusals)
{
    service_graph_t const *g = sel->g ;
    uint32_t scope = bitset32_isvalid(&sel->named, index) ? SELECTION_SERVICE_REFUSE_NAMED_INVALID : SELECTION_SERVICE_REFUSE_MET_INVALID ;

    if (!(refusals & scope) || service_graph_state(g, index) != SERVICE_GRAPH_STATE_OK)
        return ;

    int r = service_graph_frontend_gone(g, index, frontends) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "find the frontend of: ", service_graph_name(g, index)) ;

    if (r)
        refuse(g, index, frontends, true) ;
}

static void refuse_scope(selection_service_t const *sel, strbuf *frontends, uint32_t refusals)
{
    uint32_t pos = 0, index = 0, found ;

    if (!refusals)
        return ;

    found = first_refused(sel, refusals & GRAPH_REFUSE) ;

    if (found != SERVICE_GRAPH_NOINDEX)
        refuse(sel->g, found, frontends, false) ;

    if (!(refusals & (SELECTION_SERVICE_REFUSE_NAMED_INVALID | SELECTION_SERVICE_REFUSE_MET_INVALID)))
        return ;

    FOREACH_SELECTION(sel, pos, index)
        refuse_gone(sel, index, frontends, refusals) ;
}

void command_refuse_selection(selection_service_t const *sel, strbuf *frontends, uint32_t refusals)
{
    log_flow() ;

    // the named services first: a refusal names the service named before one it met
    refuse_scope(sel, frontends, refusals & NAMED_REFUSE) ;
    refuse_scope(sel, frontends, refusals & ~NAMED_REFUSE) ;
}
