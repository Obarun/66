/*
 * command_refuse_unsupervised.c
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
#include <oblibs/bits.h>

#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/svc.h>
#include <66/command.h>

static int named(selection_service_t const *lot, uint32_t index)
{
    uint32_t module = service_graph_module(lot->g, index) ;

    return bitset32_isvalid(&lot->named, index) || (module != SERVICE_GRAPH_NOINDEX && bitset32_isvalid(&lot->named, module)) ;
}

void command_refuse_unsupervised(selection_service_t *sup, selection_service_t const *lot, service_graph_resolve_t *store)
{
    log_flow() ;

    uint32_t pos = 0, index = 0 ;

    if (selection_svc_supervised(sup, lot, store) < 0)
        log_dieusys(LOG_EXIT_SYS, "read the state of the selection") ;

    FOREACH_SELECTION(lot, pos, index) {

        if (named(lot, index) && !bitset32_isvalid(&sup->member, index))
            log_die(LOG_EXIT_USER, "service: ", service_graph_name(lot->g, index), " is not supervised -- try to start it first") ;
    }
}
