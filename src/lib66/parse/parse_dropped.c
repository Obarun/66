/*
 * parse_dropped.c
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

#include <string.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/hash.h>
#include <oblibs/bits.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/parse.h>
#include <66/graph.h>
#include <66/service.h>

int parse_dropped(bitset32_t *dropped, hash_t *hres, service_graph_t const *g)
{
    log_flow() ;

    struct resolve_hash_s *c, *tmp ;

    *dropped = bitset32_init(SS_MAX_SERVICE) ;

    HASH_FOREACH(hres, c, tmp) {

        uint32_t index = service_graph_find_parsed(g, c->name) ;

        if (index == SERVICE_GRAPH_NOINDEX)
            continue ;

        if (!c->res.islog) {

            size_t len = strlen(c->name) ;

            if (len + SS_LOG_SUFFIX_LEN <= SS_MAX_SERVICE_NAME) {

                char logname[len + SS_LOG_SUFFIX_LEN + 1] ;
                auto_strings(logname, c->name, SS_LOG_SUFFIX) ;

                uint32_t logger = service_graph_find(g, logname) ;

                if (logger != SERVICE_GRAPH_NOINDEX && service_graph_logparent(g, logger) == index && !resolve_hash_search(hres, logname))
                    bitset32_set(dropped, logger) ;
            }
        }

        uint32_t const *dep = service_graph_depends(g, index) ;
        uint8_t const *origin = service_graph_dorigin(g, index) ;

        for (uint32_t pos = 0 ; pos < service_graph_vertex(g, index)->ndepends ; pos++) {

            if (!(origin[pos] & SERVICE_GRAPH_EDGE_CONTENTS))
                continue ;

            if (!resolve_hash_search(hres, service_graph_name(g, dep[pos])))
                bitset32_set(dropped, dep[pos]) ;
        }
    }

    return 1 ;
}
