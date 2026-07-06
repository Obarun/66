/*
 * service_resolve_get_io_field.c
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

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/strbuf.h>

#include <66/service.h>
#include <66/enum_service.h>

int service_resolve_get_io_field(strbuf *sa, resolve_service_addon_io_t *io, resolve_service_enum_table_t table)
{
    log_flow() ;

    char fmt[U32_FMT] ;
    char const *str = 0 ;

    switch (table.id) {

        case E_RESOLVE_SERVICE_IO_STDIN:
            fmt[u32_fmt(fmt, io->fdin.type)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDINDEST:
            str = io->sa.s + io->fdin.destination ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDOUT:
            fmt[u32_fmt(fmt, io->fdout.type)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDOUTDEST:
            str = io->sa.s + io->fdout.destination ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDERR:
            fmt[u32_fmt(fmt, io->fderr.type)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDERRDEST:
            str = io->sa.s + io->fderr.destination ;
            break ;

        default:
            return 0 ;
    }

    return auto_strbuf(sa, str) ;
}
