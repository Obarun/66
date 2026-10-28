/*
 * svc_isrunning.c
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
#include <66/status.h>
#include <66/service.h>
#include <66/enum_parser.h>

int svc_isrunning_supervised(struct resolve_hash_s const *h)
{
    log_flow() ;

    int r ;
    service_status_t st = STATUS_ZERO ;

    r = svc_reactor_armed_idle(h) ;
    if (r)
        return r ;

    if (svc_status(&h->res, &st) < 0)
        return -1 ;

    if (h->res.type == E_PARSER_TYPE_CLASSIC)
        return st.pid > 0 || st.state == STATUS_STATE_RESTARTING ;

    return st.state == STATUS_STATE_DONE ;
}

int svc_isrunning(struct resolve_hash_s const *h)
{
    log_flow() ;

    int r = svc_issupervised(&h->res) ;
    if (r <= 0)
        return r ;

    return svc_isrunning_supervised(h) ;
}
