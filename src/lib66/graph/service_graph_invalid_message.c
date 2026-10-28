/*
 * service_graph_invalid_message.c
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/graph.h>

int service_graph_invalid_message(strbuf *sa, service_graph_t const *g, uint32_t index, bool frontend_gone)
{
    log_flow() ;

    char const *name = service_graph_name(g, index) ;
    uint8_t state = service_graph_state(g, index) ;
    int r ;

    if (!service_graph_invalid(state) && !frontend_gone)
        return 0 ;

    sa->len = 0 ;

    switch (state) {

        case SERVICE_GRAPH_STATE_UNREADABLE:

            if (!service_graph_vertex(g, index)->nrequiredby) {
                r = auto_strbuf(sa, "service: ", name, " has an unreadable resolve file -- repair it with 66 reconfigure -c ", name) ;
                break ;
            }

            r = auto_strbuf(sa, "service: ", name, " has an unreadable resolve file, and is required by: ") &&
                service_graph_dependents(sa, g, index) > 0 &&
                auto_strbuf(sa, " -- repair it with 66 reconfigure -c ", name) ;
            break ;

        case SERVICE_GRAPH_STATE_UNORDERED:

            r = auto_strbuf(sa, "service: ", name, " cannot be ordered: it is part of, or depends on, a dependency cycle -- break the cycle in the frontends, then re-parse them with 66 parse -f") ;
            break ;

        case SERVICE_GRAPH_STATE_OK:

            r = auto_strbuf(sa, "service: ", name, " has no frontend anywhere any more -- remove it with 66 remove ", name) ;
            break ;

        case SERVICE_GRAPH_STATE_ABSENT:

            r = auto_strbuf(sa, "service: ", name, " has neither a resolve file nor a frontend -- remove it with 66 remove ", name) ;
            break ;

        default:
            return (errno = EINVAL, -1) ;
    }

    return r ? 1 : -1 ;
}
