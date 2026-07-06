/*
 * env_prepare_for_write.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/environ.h>
#include <66/service.h>

int env_prepare_for_write(strbuf *dst, strbuf *contents, resolve_service_t *res, resolve_service_addon_environ_t *e)
{
    log_flow() ;

    if (!env_compute(contents, res, e))
        log_warnu_return(LOG_EXIT_ZERO, "compute environment") ;

    if (!env_get_destination(dst, e))
        log_warnu_return(LOG_EXIT_ZERO, "get directory destination for environment") ;

    return 1 ;
}
