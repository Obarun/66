/*
 * service_graph_dependents.c
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

#include <66/graph.h>

int service_graph_dependents(strbuf *sa, service_graph_t const *g, uint32_t index)
{
    log_flow() ;

    uint32_t const *requiredby = service_graph_requiredby(g, index) ;

    for (uint32_t pos = 0 ; pos < service_graph_vertex(g, index)->nrequiredby ; pos++) {

        if ((pos && !strbuf_cats(sa, " ")) || !strbuf_cats(sa, service_graph_name(g, requiredby[pos])))
            return -1 ;
    }

    return strbuf_uncounted(sa) ? 1 : -1 ;
}
