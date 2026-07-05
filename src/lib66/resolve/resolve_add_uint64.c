/*
 * resolve_add_uint64.c
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

uint64_t resolve_add_uint64(char const *data)
{
    uint64_t u ;
    if (!data) data = "0" ;
    if (!u64_scan_strict(data, &u)) return 0 ;
    return u ;
}