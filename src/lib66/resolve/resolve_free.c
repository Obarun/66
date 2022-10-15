/*
 * resolve_free.c
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

#include <stdlib.h>

#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/resolve.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/service.h>

void resolve_free(resolve_wrapper_t *wres)
{
    log_flow() ;

    RESOLVE_SET_SAWRES(wres) ;

    stralloc_free(sawres) ;

    free(wres) ;
}
