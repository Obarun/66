/*
 * svc_status_effective.c
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

#include <66/svc.h>
#include <66/status.h>
#include <66/service.h>
#include <66/enum_parser.h>

uint8_t svc_status_effective(struct resolve_hash_s const *h, service_status_t const *st)
{
    log_flow() ;

    /* 66-supervise, 66-oneshotd and the launch of a module record only what the
     * process, the script or the members did, so the event meaning of an armed
     * reactor at rest is derived here: down for a classic, done or down for a
     * oneshot or a module. An event addon not read is no reactor known,
     * a reactor no longer supervised is no longer armed: the state is shown as
     * it is. */
    uint8_t type = h->res.type ;
    bool rest = (type == E_PARSER_TYPE_CLASSIC &&
                st->state == STATUS_STATE_DOWN) ||
                ((type == E_PARSER_TYPE_ONESHOT || type == E_PARSER_TYPE_MODULE) && (st->state == STATUS_STATE_DONE || st->state == STATUS_STATE_DOWN)) ;

    if (rest && svc_reactor_armed_idle(h) > 0 && svc_issupervised(&h->res) > 0)
        return STATUS_STATE_WAITING ;

    return st->state ;
}
