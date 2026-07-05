/*
 * resolve_add_uint32.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
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

#include <oblibs/types.h>

uint32_t resolve_add_uint32(char const *data)
{
    uint32_t u ;
    if (!data) data = "0" ;
    if (!u32_scan_strict(data, &u)) return 0 ;
    return u ;
}