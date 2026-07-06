/*
 * service_resolve_sanitize_addon_io.c
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

void service_resolve_sanitize_addon_io(resolve_service_addon_io_t *io)
{
    log_flow() ;

    char stk[io->sa.len + 1] ;

    memcpy(stk, io->sa.s, io->sa.len) ;
    stk[io->sa.len] = 0 ;

    io->sa.len = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_IO, io) ;

    resolve_init(wres) ;

    // string fields only; the fd types are integers
    io->rversion = io->rversion ? resolve_add_string(wres, stk + io->rversion) : 0 ;
    io->fdin.destination = io->fdin.destination ? resolve_add_string(wres, stk + io->fdin.destination) : 0 ;
    io->fdout.destination = io->fdout.destination ? resolve_add_string(wres, stk + io->fdout.destination) : 0 ;
    io->fderr.destination = io->fderr.destination ? resolve_add_string(wres, stk + io->fderr.destination) : 0 ;

    free(wres) ;
}
