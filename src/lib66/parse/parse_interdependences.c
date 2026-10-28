/*
 * parse_interdependences.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
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
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/parse.h>

int parse_interdependences(struct resolve_hash_s *c, parse_build_ctx_t *ctx)
{
    log_flow() ;

    char const *service = c->res.sa.s + c->res.name ;
    char const *list = c->dependencies.sa.s + c->dependencies.depends ;
    size_t pos = 0 ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    if (!c->dependencies.ndepends) {
        log_trace("no interdependences found for service: ", service) ;
        return 1 ;
    }

    if (!sbl_clean_string(&sa, list))
        return -1 ;

    FOREACH_SBL(&sa, pos) {

        log_trace("parse interdependences ", sa.s + pos, " of service: ", service) ;

        int r = parse_dependency_name(sa.s + pos, service, ctx) ;
        if (r <= 0)
            return r ;
    }

    return 1 ;
}
