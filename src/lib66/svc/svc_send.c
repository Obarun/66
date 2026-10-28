/*
 * svc_send.c
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/bits.h>

#include <66/config.h>
#include <66/svc.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>

int svc_send(selection_service_t const *lot, service_graph_resolve_t *store, svc_request_t const *req, bitset32_t *reached)
{
    log_flow() ;

    int r ;
    uint16_t flag = req->action == SVC_ACTION_STOP ? SVC_FLAGS_DOWN : SVC_FLAGS_UP ;

    if (req->action > SVC_ACTION_SIGNAL || req->target == SVC_TARGET_NONE || !req->signal)
        return (errno = EINVAL, -1) ;

    if (reached)
        *reached = bitset32_init(SS_MAX_SERVICE) ;

    if (!lot->member.count)
        return 1 ;

    svc_ctx_t asvc[lot->member.count] ;

    if (svc_init_ctx(asvc, lot, store, req) < 0)
        return -1 ;

    r = svc_launch(asvc, lot, req) ;

    if (r >= 0 && reached)
        for (uint32_t pos = 0 ; pos < lot->member.count ; pos++)
            if (FLAGS_ISSET(asvc[pos].state, flag))
                bitset32_set(reached, asvc[pos].index) ;

    return r ;
}
