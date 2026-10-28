/*
 * parse_compute_live.c
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
#include <sys/types.h>

#include <oblibs/log.h>

#include <66/constants.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/ssexec.h>
#include <66/parse.h>

int parse_compute_live(resolve_wrapper_t_ref wres, ssexec_t *info)
{
    log_flow() ;

    resolve_service_t_ref res = (resolve_service_t *)wres->obj ;
    // resolve_add_string() gives -1 when its buffer cannot grow, the uint32_t of compute_*() keeps it
    uint32_t const failed = (uint32_t)-1 ;
    ssize_t livedir = resolve_add_string(wres, info->live.s) ;

    if (livedir < 0)
        return -1 ;

    res->live.livedir = (uint32_t)livedir ;
    res->live.status = compute_status(wres, info) ;
    res->live.servicedir = compute_live_servicedir(wres, info) ;
    res->live.scandir = compute_scan_dir(wres, info) ;
    res->live.statedir = compute_state_dir(wres, info, SS_STATE + 1) ;
    res->live.eventdir = compute_state_dir(wres, info, SS_EVENTDIR + 1) ;
    res->live.supervisedir = compute_state_dir(wres, info, SS_SUPERVISEDIR + 1) ;
    res->live.fdholderdir = compute_pipe_service(wres, info, SS_FDHOLDER) ;
    res->live.oneshotddir = compute_pipe_service(wres, info, SS_ONESHOTD) ;
    res->live.eventddir = compute_pipe_service(wres, info, SS_EVENTD) ;

    if (res->live.status == failed || res->live.servicedir == failed || res->live.scandir == failed ||
        res->live.statedir == failed || res->live.eventdir == failed || res->live.supervisedir == failed ||
        res->live.fdholderdir == failed || res->live.oneshotddir == failed || res->live.eventddir == failed)
            return -1 ;

    return 1 ;
}
