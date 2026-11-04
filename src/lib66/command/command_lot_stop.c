/*
 * command_lot_stop.c
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
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/config.h>
#include <66/ssexec.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/svc.h>
#include <66/command.h>

int command_lot_stop(selection_service_t const *lot, service_graph_resolve_t *store, ssexec_t const *info, uint8_t target, char const *signal, strbuf *up, strbuf *down)
{
    log_flow() ;

    int r ;
    uint32_t index ;
    bitset32_t reached ;
    svc_request_t req = command_request(info, SVC_ACTION_STOP, target, signal) ;

    if (!lot->member.count)
        return 0 ;

    if (up && down) {

        selection_service_t run ;

        if (selection_svc_filter(&run, lot, store, SERVICE_READ_EVENT, svc_isrunning_supervised) < 0)
            log_dieusys(LOG_EXIT_SYS, "read the state of the services of the selection") ;

        FOREACH_SELECTION_MEMBER(lot, index)
            if (!sbl_add(bitset32_isvalid(&run.member, index) ? up : down, service_graph_name(lot->g, index)))
                log_die_nomem("strbuf") ;
    }

    req.wsignal = "-wD" ;

    r = svc_send(lot, store, &req, &reached) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "send the order to the services of the selection") ;

    if (target == SVC_TARGET_FREE) {

        selection_service_t out = *lot ;

        FOREACH_SELECTION_MEMBER(lot, index)
            if (!bitset32_isvalid(&reached, index))
                selection_service_remove(&out, index) ;

        if (svc_unsupervise(&out, store, info->who, info->timeout) < 0)
            log_dieusys(LOG_EXIT_SYS, "unsupervise the services of the selection") ;
    }

    return r ? 0 : LOG_EXIT_SYS ;
}
