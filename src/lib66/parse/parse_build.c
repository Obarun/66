/*
 * parse_build.c
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

/* Type dispatch of the service build. parse_frontend hands over a hash entry
 * whose core is complete; each parse_build_<type> composes only the addons its
 * type needs, writing straight into the entry (c->execute, c->io, ...). No addon
 * struct is threaded through arguments: the interdependent readers reach the
 * data through the hash entry. */

#include <stdint.h>
#include <stdlib.h> // free

#include <oblibs/log.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>
#include <66/state.h> // STATE_FLAGS_FALSE

static void addon_init(uint32_t data, void *addon)
{
    resolve_wrapper_t_ref w = resolve_set_struct(data, addon) ;
    resolve_init(w) ;
    free(w) ;
}

static void finalize_dependencies(struct resolve_hash_s *c)
{
    c->res.has_dependencies = (c->dependencies.ndepends || c->dependencies.nrequiredby || c->dependencies.noptsdeps ||
                               c->dependencies.ncontents || c->dependencies.nprovide || c->dependencies.nconflict) ? 1 : 0 ;
}

static void build_supervised(parse_build_ctx_t *ctx, struct resolve_hash_s *c)
{
    log_flow() ;

    char const *name = c->res.sa.s + c->res.name ;

    addon_init(DATA_SERVICE_EXECUTE, &c->execute) ;
    addon_init(DATA_SERVICE_DEPENDENCIES, &c->dependencies) ;

    if (!parse_environ(c, ctx))
        log_die(LOG_EXIT_SYS, "parse environment of service: ", name) ;

    if (!parse_logger(c, ctx))
        log_die(LOG_EXIT_SYS, "parse logger of service: ", name) ;

    if (!parse_execute(c, ctx))
        log_die(LOG_EXIT_SYS, "parse execute of service: ", name) ;

    if (!parse_dependencies(ctx->st, &c->dependencies))
        log_die(LOG_EXIT_SYS, "parse dependencies of service: ", name) ;

    if (ctx->isparsed == STATE_FLAGS_FALSE)
        if (!parse_interdependences(c, ctx))
            log_dieu(LOG_EXIT_SYS, "parse dependencies of service: ", name) ;

    if (!parse_io(c, ctx))
        log_die(LOG_EXIT_SYS, "parse io of service: ", name) ;

    /* logger "effective": keep it only if the resolved io is 66log-bound. */
    if (c->res.logger && c->io.fdin.type != E_PARSER_IO_TYPE_66LOG && c->io.fdout.type != E_PARSER_IO_TYPE_66LOG)
        c->res.logger = 0 ;
}

static void parse_build_classic(parse_build_ctx_t *ctx, struct resolve_hash_s *c)
{
    log_flow() ;

    build_supervised(ctx, c) ;

    if (c->res.logger && c->io.fdin.type == E_PARSER_IO_TYPE_66LOG && !c->res.inns)
        parse_create_logger(ctx->hres, c, ctx->info) ;

    finalize_dependencies(c) ;
}

static void parse_build_oneshot(parse_build_ctx_t *ctx, struct resolve_hash_s *c)
{
    log_flow() ;

    build_supervised(ctx, c) ;

    c->res.logger = 0 ;

    finalize_dependencies(c) ;
}

static void parse_build_module(parse_build_ctx_t *ctx, struct resolve_hash_s *c)
{
    log_flow() ;

    char const *name = c->res.sa.s + c->res.name ;

    addon_init(DATA_SERVICE_DEPENDENCIES, &c->dependencies) ;

    if (!parse_environ(c, ctx))
        log_die(LOG_EXIT_SYS, "parse environment of service: ", name) ;

    if (!parse_dependencies(ctx->st, &c->dependencies))
        log_die(LOG_EXIT_SYS, "parse dependencies of service: ", name) ;

    if (ctx->isparsed == STATE_FLAGS_FALSE)
        if (!parse_interdependences(c, ctx))
            log_dieu(LOG_EXIT_SYS, "parse dependencies of service: ", name) ;

    if (!parse_regex(c, ctx))
        log_die(LOG_EXIT_SYS, "parse regex of service: ", name) ;

    parse_module(c, ctx) ;

    finalize_dependencies(c) ;
}

void parse_build(struct resolve_hash_s *c, parse_build_ctx_t *ctx)
{
    log_flow() ;

    switch (c->res.type) {

        case E_PARSER_TYPE_CLASSIC:
            parse_build_classic(ctx, c) ;
            break ;

        case E_PARSER_TYPE_ONESHOT:
            parse_build_oneshot(ctx, c) ;
            break ;

        case E_PARSER_TYPE_MODULE:
            parse_build_module(ctx, c) ;
            break ;

        default:
            break ;
    }
}
