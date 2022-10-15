/*
 * resolve_modify_field_g.c
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

#include <66/resolve.h>

int resolve_modify_field_g(resolve_wrapper_t_ref wres, char const *base, char const *name, uint8_t field, char const *value)
{
    log_flow() ;

    if (!resolve_read_g(wres, base, name))
        return 0 ;

    if (!resolve_modify_field(wres, field, value))
        return 0 ;

    if (!resolve_write_g(wres, base, name))
        return 0 ;

    return 1 ;
}
