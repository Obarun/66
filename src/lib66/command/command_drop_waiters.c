/*
 * command_drop_waiters.c
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

#include <oblibs/bits.h>

#include <66/config.h>
#include <66/selection.h>
#include <66/command.h>

void command_drop_waiters(selection_service_t *sel, selection_service_t const *started, bitset32_t const *reached)
{
    uint32_t index ;
    bool none = true ;
    bitset32_t failed = bitset32_init(SS_MAX_SERVICE) ;
    selection_service_t waiters = SELECTION_SERVICE_ZERO ;
    selection_service_policy_t policy = command_policy(SELECTION_SERVICE_PROPAGATE_REQUIREDBY, false, false, false) ;

    if (!started->member.count)
        return ;

    FOREACH_SELECTION_MEMBER(started, index)
        if (!bitset32_isvalid(reached, index)) {
            bitset32_set(&failed, index) ;
            none = false ;
        }

    if (none)
        return ;

    policy.reactors = &failed ;

    selection_service_propagate(&waiters, started->g, &failed, &policy) ;

    FOREACH_SELECTION_MEMBER(&waiters, index)
        selection_service_remove(sel, index) ;
}
