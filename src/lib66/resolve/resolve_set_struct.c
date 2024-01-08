/*
 * resolve_set_struct.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h>

#include <oblibs/log.h>

#include <66/resolve.h>

resolve_wrapper_t *resolve_set_struct(uint8_t type, void *s)
{
    log_flow() ;

    resolve_wrapper_t *wres = malloc(sizeof(resolve_wrapper_t)) ;

    wres->obj = s ;
    wres->type = type ;
    return wres ;
} ;

