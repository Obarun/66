/*
 * parse_dependency_name.c
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
#include <stdint.h>
#include <unistd.h> // getuid

#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/service.h>
#include <66/parse.h>
#include <66/graph.h>
#include <66/config.h>
#include <66/module.h>

int parse_dependency_name(char const *name, char const *service, parse_build_ctx_t *ctx)
{
    log_flow() ;

    int r ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    uint8_t exlen = 3 ;
    char const *exclude[3] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1, SS_MODULE_CONFIG_DIR + 1 } ;
    char held[SS_MAX_SERVICE_NAME + 1] ;
    char const *designated = name ;

    if (!ctx->forced_directory && (ctx->flags & PARSE_NODEPS))
        return 1 ;

    if (strlen(name) > SS_MAX_SERVICE_NAME) {
        log_warn("service: ", service, " names: ", name, " -- the name is too long") ;
        return 0 ;
    }

    r = service_resolve_provide(held, name, ctx->hres, ctx->info->base.s) ;
    if (r < 0)
        return -1 ;

    if (r && !ctx->forced_directory && resolve_hash_search(ctx->hres, held) && service_graph_find_parsed(ctx->g, held) == SERVICE_GRAPH_NOINDEX) {

        int f = service_frontend_path(&sa, name, getuid(), 0, exclude, exlen) ;
        if (f < 0)
            return -1 ;

        if (f)
            r = 0 ;

        sa.len = 0 ;
    }

    if (r)
        designated = held ;

    if (!ctx->forced_directory) {

        // what is parsed already, or in the selection, is left as it is: -f does not walk down
        if (resolve_hash_search(ctx->hres, designated) || service_graph_find_parsed(ctx->g, designated) != SERVICE_GRAPH_NOINDEX)
            return 1 ;
    }

    r = service_frontend_path(&sa, designated, getuid(), ctx->forced_directory, exclude, exlen) ;
    if (r < 0)
        return -1 ;

    if (!r) {

        if ((ctx->flags & PARSE_LEAVEGONE) && !ctx->forced_directory && !strcmp(designated, name))
            return 1 ;

        if (strcmp(designated, name)) {
            log_warnu("get frontend service file of: ", designated, " -- it provides: ", name) ;
            return 0 ;
        }

        if (ctx->forced_directory && ctx->inns) {

            sa.len = 0 ;
            r = service_frontend_path(&sa, name, getuid(), 0, exclude, exlen) ;
            if (r < 0)
                return -1 ;

            if (r)
                return PARSE_REFUSE(ctx, "service: ", service, ", a member of the module: ", ctx->inns, ", depends on: ", name, " outside its module -- a member depends on the members of its module only") ;
        }

        log_warn("no service answers to the name: ", name, " -- is its provider enabled?") ;
        return 0 ;
    }

    parse_build_ctx_t dctx = *ctx ;
    if (!ctx->forced_directory) {
        dctx.inns = 0 ;
        dctx.intree = 0 ;
    }

    return parse_frontend(sa.s, dctx) ;
}

