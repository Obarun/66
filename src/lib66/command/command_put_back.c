/*
 * command_put_back.c
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

#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/service.h>
#include <66/svc.h>
#include <66/command.h>

static int put(strbuf *list, service_graph_t const *g, ssexec_t *info, strbuf *frontends, bool nopropagate, uint8_t target)
{
    size_t pos = 0 ;
    int n = 0 ;
    char const *argv[sbl_count(list) + 1] ;

    FOREACH_SBL(list, pos) {

        uint32_t index = g ? service_graph_find_parsed(g, list->s + pos) : SERVICE_GRAPH_NOINDEX ;

        if (index != SERVICE_GRAPH_NOINDEX && service_graph_invalid(service_graph_state(g, index)))
            continue ;

        argv[n++] = list->s + pos ;
    }

    if (!n)
        return 0 ;

    argv[n] = 0 ;

    return command_up(argv, n, info, frontends, target, nopropagate, SVC_LOT_COMMAND, false, true) ;
}

int command_put_back(strbuf *up, strbuf *down, service_graph_t const *g, ssexec_t *info, strbuf *frontends, bool nopropagate)
{
    // what ran first, its Depends with it; then what was down, alone, sent down
    int e = put(up, g, info, frontends, nopropagate, SVC_TARGET_NONE) ;
    int r = put(down, g, info, frontends, true, SVC_TARGET_DOWN) ;

    return e ? e : r ;
}
