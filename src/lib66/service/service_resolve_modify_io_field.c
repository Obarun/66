/*
 * service_resolve_modify_io_field.c
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
#include <stdlib.h> // free

#include <oblibs/log.h>
#include <oblibs/types.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_service.h>

void service_resolve_modify_io_field(resolve_service_addon_io_t *io, resolve_service_enum_table_t table, char const *data)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_IO, io) ;

    switch (table.id) {

        case E_RESOLVE_SERVICE_IO_STDIN:
            io->fdin.type = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDINDEST:
            io->fdin.destination = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDOUT:
            io->fdout.type = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDOUTDEST:
            io->fdout.destination = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDERR:
            io->fderr.type = resolve_add_uint32(data) ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDERRDEST:
            io->fderr.destination = resolve_add_string(wres, data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;

    service_resolve_sanitize_addon_io(io) ;
}
