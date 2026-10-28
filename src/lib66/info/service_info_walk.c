/*
 * service_info_walk.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
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
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/bits.h>

#include <66/info.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/config.h>

int service_info_walk(service_graph_t const *g, uint32_t index, uint8_t requiredby, uint8_t reverse, depth_t *depth, int padding, info_graph_style *style)
{
    log_flow() ;

    selection_service_t sel = SELECTION_SERVICE_ZERO ;
    selection_service_policy_t policy = SELECTION_SERVICE_POLICY_ZERO ;
    bitset32_t seed = bitset32_init(SS_MAX_SERVICE) ;

    if ((unsigned int) depth->level > INFO_MAXDEPTH)
        return 1 ;

    /** the level of @index: what it reaches in the direction of the field,
     * without itself */
    policy.propagate = requiredby ? SELECTION_SERVICE_PROPAGATE_REQUIREDBY : SELECTION_SERVICE_PROPAGATE_DEPENDS ;
    bitset32_set(&seed, index) ;

    selection_service_propagate(&sel, g, &seed, &policy) ;
    selection_service_remove(&sel, index) ;

    return service_info_walk_set(g, &sel.member, requiredby, reverse, depth, padding, style) ;
}
