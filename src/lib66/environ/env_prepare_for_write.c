/*
 * env_prepare_for_write.c
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
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/environ.h>
#include <66/constants.h>
#include <66/service.h>
#include <66/parse.h>

int env_prepare_for_write(stralloc *dst, stralloc *contents, resolve_service_t *res)
{
    log_flow() ;

    if (!env_compute(contents, res))
        log_warnu_return(LOG_EXIT_ZERO, "compute environment") ;

    if (!env_get_destination(dst, res))
        log_warnu_return(LOG_EXIT_ZERO, "get directory destination for environment") ;

    return 1 ;
}
