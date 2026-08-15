/*
 * service_resolve_sanitize_addon_regex.c
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

#include <string.h>
#include <stdlib.h> // free

#include <oblibs/log.h>

#include <66/resolve.h>
#include <66/service.h>

void service_resolve_sanitize_addon_regex(resolve_service_addon_regex_t *rx)
{
    log_flow() ;

    char stk[rx->sa.len + 1] ;

    memcpy(stk, rx->sa.s, rx->sa.len) ;
    stk[rx->sa.len] = 0 ;

    rx->sa.len = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_REGEX, rx) ;

    resolve_init(wres) ;

    rx->rversion = 0 ;
    // string fields only; the n* counts are integers
    rx->configure = rx->configure ? resolve_add_string(wres, stk + rx->configure) : 0 ;
    rx->directories = rx->directories ? resolve_add_string(wres, stk + rx->directories) : 0 ;
    rx->files = rx->files ? resolve_add_string(wres, stk + rx->files) : 0 ;
    rx->infiles = rx->infiles ? resolve_add_string(wres, stk + rx->infiles) : 0 ;

    free(wres) ;
}
