/*
 * svc_reactor_armed_idle.c
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

#include <errno.h>

#include <oblibs/log.h>

#include <66/svc.h>
#include <66/event_rule.h>
#include <66/service.h>

int svc_reactor_armed_idle(struct resolve_hash_s const *h)
{
    log_flow() ;

    if (!h->res.has_event)
        return 0 ;

    if (!(h->read & SERVICE_READ_EVENT))
        return (errno = EINVAL, -1) ;

    return h->event.docmd == EVENT_DO_START || h->event.docmd == EVENT_DO_RESTART ;
}
