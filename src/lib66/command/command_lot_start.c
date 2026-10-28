/*
 * command_lot_start.c
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

#include <oblibs/log.h>
#include <oblibs/bits.h>

#include <66/config.h>
#include <66/ssexec.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/sanitize.h>
#include <66/svc.h>
#include <66/command.h>

int command_lot_start(selection_service_t const *lot, service_graph_resolve_t *store, ssexec_t const *info, uint8_t target, uint8_t stage, bitset32_t *reached)
{
    log_flow() ;

    int r ;
    bool down = svc_target_stops(target) ;
    svc_request_t req = command_request(info, down ? SVC_ACTION_STOP : SVC_ACTION_START, target, down ? "-d" : "-u") ;

    if (reached)
        *reached = bitset32_init(SS_MAX_SERVICE) ;

    if (!lot->member.count)
        return 0 ;

    if (sanitize_init(lot, store, stage, info->who) < 0)
        log_dieusys(LOG_EXIT_SYS, "initiate the services of the selection") ;

    req.wsignal = down ? "-wD" : "-wU" ;

    r = svc_send(lot, store, &req, reached) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "send the order to the services of the selection") ;

    return r ? 0 : LOG_EXIT_SYS ;
}
