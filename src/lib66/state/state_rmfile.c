/*
 * state_rmfile.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/posixplz.h>

#include <66/state.h>
#include <66/constants.h>
#include <66/service.h>

void state_rmfile(resolve_service_t *res)
{
    log_flow() ;

    unlink_void(res->sa.s + res->path.status) ;
}
