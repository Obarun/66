/*
 * selection_service_carried.c
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

int selection_service_carried(selection_service_t const *sel, uint32_t index)
{
    log_flow() ;

    uint32_t module = service_graph_module(sel->g, index) ;

    return module != SERVICE_GRAPH_NOINDEX && bitset32_isvalid(&sel->member, module) ;
}
