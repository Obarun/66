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
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>
#include <stdint.h>
#include <unistd.h> // getuid

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/parse.h>
#include <66/ssexec.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/instance.h>
#include <66/state.h>
#include <66/module.h>

int parse_interdependences(struct resolve_hash_s *c, parse_build_ctx_t *ctx)
{
    log_flow() ;

    char const *service = c->res.sa.s + c->res.name ;
    char const *list = c->dependencies.sa.s + c->dependencies.depends ;
    unsigned int listlen = c->dependencies.ndepends ;
    int r, e = 0 ;
    size_t pos = 0, len = 0 ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    uint8_t exlen = 3 ;
    char const *exclude[3] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1, SS_MODULE_CONFIG_DIR } ;
    char owner[SS_MAX_SERVICE_NAME + 1] ;

    if (listlen) {

        if (!sbl_clean_string(&sa, list)) {
            log_warnu("clean the string") ;
            goto freed ;
        }

        char t[sa.len + 1] ;

        sbl_to_char(t, &sa) ;

        len = sa.len ;

        for (; pos < len ; pos += strlen(t + pos) + 1) {

            sa.len = 0 ;
            char const *name = t + pos ;
            char ainsta[strlen(name) + 1] ;
            int insta = -1 ;

            if (!strcmp(name, service))
                log_die(LOG_EXIT_USER, "direct cyclic interdependences detected -- ", name, " depends on: ", service) ;

            log_trace("parse interdependences ", name, " of service: ", service) ;

            insta = instance_check(name) ;

            if (insta > 0) {

                if (!instance_splitname(&sa, name, insta, SS_INSTANCE_NAME))
                    log_die(LOG_EXIT_SYS, "split instance service of: ", name) ;

                auto_strings(ainsta, sa.s) ;
                sa.len = 0 ;
            }

            if (!strcmp(ctx->main, name))
                log_die(LOG_EXIT_USER, "direct cyclic interdependences detected -- ", ctx->main, " depends on: ", service, " which depends on: ", ctx->main) ;

            if (!service_resolve_provide(owner, name, ctx->info->base.s)) {
                log_warnusys("resolve provide alias: ", name) ;
                goto freed ;
            }

            r = service_frontend_path(&sa, owner, getuid(), ctx->forced_directory, exclude, exlen) ;
            if (r < 0) {
                log_warnu( "get frontend service file of: ", owner) ;
                goto freed ;
            }

            if (!r) {

                if (strcmp(owner, name))
                    log_warnu( "get frontend service file of: ", owner, " -- it provides: ", name) ;
                else
                    log_warn("no service answers to the name: ", name, " -- is its provider enabled?") ;

                goto freed ;
            }


            /** nothing to do with the exit code.
             * forced_directory == 0 means that the service
             * comes from an external directory of the module.
             * In this case don't associated it at the module. */
            parse_build_ctx_t dctx = *ctx ;
            if (!ctx->forced_directory) {
                dctx.inns = 0 ;
                dctx.intree = 0 ;
            }

            /** the name is validated on every parse, but a dependency is only
             * pulled in when the service is parsed for the first time: a forced
             * parse must not walk the whole chain down and rewrite it. */
            if (ctx->isparsed == STATE_FLAGS_FALSE)
                parse_frontend(sa.s, dctx) ;
        }

    } else
        log_trace("no interdependences found for service: ", service) ;

    e = 1 ;

    freed:
        return e ;
}
