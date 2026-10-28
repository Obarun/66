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
 * data through the hash entry. A step returns 0 on a fault of the frontend and
 * -1 on a failure of the system: the build names the step that refused, unless
 * the step wrote its own reason. */

#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>
#include <66/event_rule.h>

static void addon_init(uint32_t data, void *addon)
{
    resolve_wrapper_t w = RESOLVE_WRAPPER(data, addon) ;
    resolve_init(&w) ;
}

static void finalize_dependencies(struct resolve_hash_s *c)
{
    c->res.has_dependencies = (c->dependencies.ndepends || c->dependencies.nrequiredby || c->dependencies.noptsdeps ||
                               c->dependencies.ncontents || c->dependencies.nprovide || c->dependencies.nconflict) ? 1 : 0 ;
}

static int check(int r, parse_build_ctx_t *ctx, char const *what, char const *name)
{
    if (r != 0)
        return r ;

    return PARSE_REFUSE(ctx, "unable to parse ", what, " of service: ", name) ;
}

static int build_supervised(parse_build_ctx_t *ctx, struct resolve_hash_s *c)
{
    log_flow() ;

    int r ;
    char const *name = c->res.sa.s + c->res.name ;

    addon_init(DATA_SERVICE_EXECUTE, &c->execute) ;
    addon_init(DATA_SERVICE_DEPENDENCIES, &c->dependencies) ;

    if ((r = check(parse_environ(c, ctx), ctx, "environment", name)) <= 0 ||
        (r = check(parse_logger(c, ctx), ctx, "logger", name)) <= 0 ||
        (r = check(parse_execute(c, ctx), ctx, "execute", name)) <= 0 ||
        (r = check(parse_dependencies(ctx->st, &c->dependencies, name), ctx, "dependencies", name)) <= 0 ||
        (r = check(parse_interdependences(c, ctx), ctx, "dependencies", name)) <= 0 ||
        (r = check(parse_io(c, ctx), ctx, "io", name)) <= 0 ||
        (r = check(parse_event(c, ctx), ctx, "event", name)) <= 0)
            return r ;

    /* logger "effective": keep it only if the resolved io is 66log-bound. */
    if (c->res.logger && c->io.fdin.type != E_PARSER_IO_TYPE_66LOG && c->io.fdout.type != E_PARSER_IO_TYPE_66LOG)
        c->res.logger = 0 ;

    /** a start/restart reactor is armed, not launched: force it down so
     * supervision keeps it offline until an event brings it up */
    if (c->res.has_event && (c->event.docmd == EVENT_DO_START || c->event.docmd == EVENT_DO_RESTART))
        c->execute.down = 1 ;

    return 1 ;
}

static int parse_build_classic(parse_build_ctx_t *ctx, struct resolve_hash_s *c)
{
    log_flow() ;

    int r = build_supervised(ctx, c) ;
    if (r <= 0)
        return r ;

    if (c->res.logger && c->io.fdin.type == E_PARSER_IO_TYPE_66LOG && !c->res.inns && parse_create_logger(ctx->hres, c, ctx->info) < 0)
        return -1 ;

    finalize_dependencies(c) ;

    return 1 ;
}

static int parse_build_oneshot(parse_build_ctx_t *ctx, struct resolve_hash_s *c)
{
    log_flow() ;

    int r = build_supervised(ctx, c) ;
    if (r <= 0)
        return r ;

    c->res.logger = 0 ;

    finalize_dependencies(c) ;

    return 1 ;
}

static int parse_build_event(parse_build_ctx_t *ctx, struct resolve_hash_s *c)
{
    log_flow() ;

    return check(parse_event_source(c, ctx), ctx, "event source", c->res.sa.s + c->res.name) ;
}

static int parse_build_module(parse_build_ctx_t *ctx, struct resolve_hash_s *c)
{
    log_flow() ;

    int r ;
    char const *name = c->res.sa.s + c->res.name ;

    if (c->res.inns)
        return PARSE_REFUSE(ctx, "service: ", name, " is a module inside the module: ", c->res.sa.s + c->res.inns, " -- a module cannot contain a module") ;

    addon_init(DATA_SERVICE_DEPENDENCIES, &c->dependencies) ;

    if ((r = check(parse_environ(c, ctx), ctx, "environment", name)) <= 0 ||
        (r = check(parse_dependencies(ctx->st, &c->dependencies, name), ctx, "dependencies", name)) <= 0 ||
        (r = check(parse_interdependences(c, ctx), ctx, "dependencies", name)) <= 0 ||
        (r = check(parse_regex(c, ctx), ctx, "regex", name)) <= 0 ||
        (r = check(parse_event(c, ctx), ctx, "event", name)) <= 0 ||
        (r = check(parse_module(c, ctx), ctx, "module", name)) <= 0)
            return r ;

    finalize_dependencies(c) ;

    return 1 ;
}

int parse_build(struct resolve_hash_s *c, parse_build_ctx_t *ctx)
{
    log_flow() ;

    switch (c->res.type) {

        case E_PARSER_TYPE_CLASSIC:
            return parse_build_classic(ctx, c) ;

        case E_PARSER_TYPE_ONESHOT:
            return parse_build_oneshot(ctx, c) ;

        case E_PARSER_TYPE_MODULE:
            return parse_build_module(ctx, c) ;

        case E_PARSER_TYPE_EVENT:
            return parse_build_event(ctx, c) ;

        default:
            break ;
    }

    return 1 ;
}
