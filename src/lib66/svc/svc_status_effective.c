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

#include <oblibs/log.h>

#include <66/svc.h>
#include <66/status.h>
#include <66/service.h>
#include <66/enum_parser.h>

uint8_t svc_status_effective(resolve_service_t *res, service_status_t const *st)
{
    log_flow() ;

    /* a oneshot/module reactor records its waiting state itself; a classic one
     * cannot -- 66-supervise keeps its status binary up/down -- so the event
     * meaning of a down armed reactor is derived here instead. */
    if (st->state == STATUS_STATE_DOWN && res->type == E_PARSER_TYPE_CLASSIC
        && svc_reactor_armed_idle(res))
            return STATUS_STATE_WAITING ;

    return st->state ;
}
