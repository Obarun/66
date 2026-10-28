/*
 * selection_service_remove.c
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
#include <oblibs/bits.h>

#include <66/selection.h>

void selection_service_remove(selection_service_t *sel, uint32_t index)
{
    log_flow() ;

    if (!bitset32_isvalid(&sel->member, index))
        return ;

    bitset32_clear(&sel->member, index) ;
}
