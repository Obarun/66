/*
 * command_graph_reload.c
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
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/constants.h>
#include <66/ssexec.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/command.h>

void command_graph_reload(command_graph_t *d, ssexec_t const *info, bool refuse)
{
    log_flow() ;

    char name[SS_MAX_SERVICE_NAME + 1] = "" ;
    service_graph_t *g = &d->g ;
    service_graph_resolve_t *store = &d->store ;

    if (refuse && store->unreadable != SERVICE_GRAPH_NOINDEX)
        auto_strings(name, service_graph_name(store->g, store->unreadable)) ;

    service_graph_resolve_free(store) ;

    if (service_graph_reload(g, info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "load the graph of the system") ;

    d->reloaded = true ;

    uint32_t index = name[0] ? service_graph_find(g, name) : SERVICE_GRAPH_NOINDEX ;

    if (index != SERVICE_GRAPH_NOINDEX && service_graph_state(g, index) == SERVICE_GRAPH_STATE_UNREADABLE) {

        bitset32_t seed = bitset32_init(SS_MAX_SERVICE) ;
        selection_service_t one ;
        selection_service_policy_t policy = command_policy(SELECTION_SERVICE_PROPAGATE_NONE, false, false, false) ;
        // the unreadable vertex is refused for itself: the refusal exits, no frontend searched
        strbuf frontends = STRBUF_ZERO ;

        bitset32_set(&seed, index) ;
        selection_service_propagate(&one, g, &seed, &policy) ;
        command_refuse_selection(&one, &frontends, SELECTION_SERVICE_REFUSE_NAMED_INVALID) ;
    }
}
