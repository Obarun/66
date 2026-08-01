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

#include <oblibs/log.h>

#include <66/svc.h>
#include <66/event_rule.h>
#include <66/resolve.h>
#include <66/service.h>

int svc_reactor_armed_idle(resolve_service_t *res)
{
    log_flow() ;

    if (!res->has_event)
        return 0 ;

    resolve_service_addon_event_t ev = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    resolve_wrapper_t_ref wev = resolve_set_struct(DATA_SERVICE_EVENT, &ev) ;

    int idle = resolve_read(wev, res->sa.s + res->path.home, res->sa.s + res->name) == 1
            && (ev.docmd == EVENT_DO_START || ev.docmd == EVENT_DO_RESTART) ;

    resolve_free(wev) ;

    return idle ;
}
