/*
 * write_service.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <errno.h>

#include <oblibs/log.h>

#include <66/service.h>
#include <66/enum.h>
#include <66/enum_parser.h>
#include <66/write.h>

int write_services(resolve_service_t *res, resolve_service_addon_execute_t *ex, strbuf const *copyfrom, char const *workdir)
{
    log_flow() ;

    char *name = res->sa.s + res->name ;

    log_trace("write service: ", name) ;

    if (res->islog)
        return write_logger(ex, workdir) ;

    switch(res->type) {

        case E_PARSER_TYPE_MODULE:
        case E_PARSER_TYPE_EVENT:
            return 1 ;

        case E_PARSER_TYPE_CLASSIC:
        case E_PARSER_TYPE_ONESHOT:
            return write_common(ex, copyfrom, workdir) ;

        default:
            break ;
    }

    return (errno = EINVAL, -1) ;
}
