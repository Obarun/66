/*
 * state_check.c
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
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/state.h>
#include <66/constants.h>
#include <66/service.h>

int state_check(resolve_service_t *res)
{
    log_flow() ;

    size_t len = strlen(res->sa.s + res->path.servicedir) ;

    char target[len + SS_STATE_LEN + 1 + SS_STATUS_LEN + 1] ;

    auto_strings(target, res->sa.s + res->path.servicedir, SS_STATE, "/", SS_STATUS) ;

    if (access(target, F_OK) < 0)
        return 0 ;

    return 1 ;
}
