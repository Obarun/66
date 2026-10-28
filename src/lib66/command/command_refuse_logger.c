/*
 * command_refuse_logger.c
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

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/command.h>

void command_refuse_logger(service_graph_t const *g, bitset32_t const *named, char const *verb)
{
    log_flow() ;

    uint32_t n = service_graph_nvertex(g) ;

    for (uint32_t index = 0 ; index < n ; index++) {

        if (!bitset32_isvalid(named, index))
            continue ;

        uint32_t service = service_graph_logparent(g, index) ;

        if (service != SERVICE_GRAPH_NOINDEX)
            log_die(LOG_EXIT_USER, SSEXEC_MSG_LOGGER(service_graph_name(g, index), service_graph_name(g, service), verb)) ;
    }
}
