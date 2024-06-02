/*
 * write_classic.c
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

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/service.h>
#include <66/write.h>
#include <66/parse.h>

/* dst e.g. /var/lib/66/system/service/svc/<name> */

void write_classic(resolve_service_t *res, char const *dst, uint8_t force)
{
    log_flow() ;

    /**notification,timeout, ... */
    if (!write_common(res, dst, force)) {
        parse_cleanup(res, dst, force) ;
        log_dieu(LOG_EXIT_SYS, "write common file of: ", res->sa.s + res->name) ;
    }
}
