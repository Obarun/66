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
#include <66/resolve.h>

int state_check(char const *base, char const *name)
{
    log_flow() ;

    size_t baselen = strlen(base) ;
    size_t namelen = strlen(name) ;
    char target[baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + namelen + SS_SVC_LEN + 1 + namelen + SS_STATE_LEN + 1 + SS_STATUS_LEN + 1] ;

    auto_strings(target, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name, SS_SVC, "/", name, SS_STATE, "/", SS_STATUS) ;

    if (access(target, F_OK) < 0)
        return 0 ;

    return 1 ;
}
