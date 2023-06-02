/*
 * state_messenger.c
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

#include <stdint.h>

#include <oblibs/log.h>
#include <66/state.h>
#include <66/service.h>

int state_messenger(resolve_service_t *res, uint32_t flag, uint32_t value)
{
    log_flow() ;

    ss_state_t sta = STATE_ZERO ;

    if (!state_read(&sta, res))
        log_warnu_return(LOG_EXIT_ZERO, "read status file of: ", res->sa.s + res->name) ;

    state_set_flag(&sta, flag, value) ;

    if (!state_write(&sta, res))
        log_warnusys_return(LOG_EXIT_ZERO, "write status file of: ", res->sa.s + res->name) ;

    return 1 ;
}

