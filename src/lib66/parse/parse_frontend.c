/*
 * parse_frontend.c
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

#include <string.h>
#include <unistd.h> // getuid

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/utils.h>
#include <66/constants.h>
#include <66/config.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/parse.h>
#include <66/instance.h>

static int parse_read_instance(strbuf *frontend, char const *svsrc, char const *sv, int insta)
{
    log_flow() ;

    int r ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    uint8_t exlen = 0 ;
    char const *exclude[1] = { 0 } ;

    if (!instance_splitname(&sa, sv, insta, SS_INSTANCE_TEMPLATE))
        return -1 ;

    log_trace("read frontend service at: ", svsrc, sa.s) ;

    r = read_svfile(frontend, sa.s, svsrc) ;
    if (!r)
        return -1 ;

    if (r > 0)
        return 1 ;

    char instaname[sa.len + 1] ;
    auto_strings(instaname, sa.s) ;
    sa.len = 0 ;

    r = service_frontend_path(&sa, sv, getuid(), 0, exclude, exlen) ;
    if (r <= 0)
        return r ;

    char dir[sa.len + 1] ;

    if (!ob_dirname(dir, sa.s))
        return -1 ;

    r = read_svfile(frontend, instaname, dir) ;
    if (!r)
        return -1 ;

    return r > 0 ? 1 : 0 ;
}

int parse_frontend(char const *sv, parse_build_ctx_t ctx)
{
    log_flow() ;

    int insta, r ;
    size_t svlen = strlen(sv) ;
    char svname[svlen + 1], svsrc[svlen + 1], instaname[svlen + 1] ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    struct resolve_hash_s *hash ;

    if (!ob_basename(svname, sv) || !ob_dirname(svsrc, sv))
        return -1 ;

    char known[(ctx.inns ? strlen(ctx.inns) + 1 : 0) + svlen + 1] ;

    hash = parse_get_hashname(known, ctx.hres, svname, ctx.inns) ;
    if (hash != NULL) {
        log_trace("ignoring: ", known, " service -- already appended to the selection") ;
        return 1 ;
    }

    r = name_isvalid(svname, ctx.msg) ;
    if (r <= 0)
        return r ;

    log_trace("parse service: ", sv) ;

    insta = instance_check(svname) ;

    if (insta > 0) {

        auto_strings(instaname, svname) ;
        instaname[insta + 1] = 0 ;

        r = parse_read_instance(&sa, svsrc, svname, insta) ;

    } else {

        log_trace("read frontend service at: ", sv) ;

        r = read_svfile(&sa, svname, svsrc) ;
        // an empty frontend is refused (-1), one that does not open fails (0)
        r = !r ? -1 : r < 0 ? 0 : 1 ;
    }

    if (r < 0)
        return -1 ;

    if (!r)
        return PARSE_REFUSE(&ctx, "unable to parse the frontend of service: ", svname) ;

    if (!identifier_replace(&sa, svname))
        return PARSE_REFUSE(&ctx, "unable to parse the identifiers of service: ", svname) ;

    parse_store_t st ;

    r = parse_store_build(&st, sa.s) ;
    if (r <= 0) {
        parse_store_free(&st) ;
        return r < 0 ? -1 : PARSE_REFUSE(&ctx, "unable to parse service: ", svname) ;
    }

    _cleanup_strbuf_ strbuf copyfrom = STRBUF_ZERO ;

    ctx.st = &st ;
    ctx.copyfrom = &copyfrom ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_SERVICE, &res) ;

    resolve_init(&wres) ;

    r = parse_core(&res, sv, svname, svsrc, svlen, insta, instaname, &ctx) ;
    if (r <= 0 || r == 2) {
        parse_store_free(&st) ;
        resolve_free_obj(&wres) ;
        // a directory that is no module is listed by service_frontend_path(), not parsed
        return r == 2 ? 1 : r < 0 ? -1 : PARSE_REFUSE(&ctx, "unable to parse service: ", svname) ;
    }

    char *name = res.sa.s + res.name ;

    r = name_isvalid(name, ctx.msg) ;
    if (r <= 0) {
        parse_store_free(&st) ;
        resolve_free_obj(&wres) ;
        return r < 0 ? -1 : PARSE_REFUSE(&ctx, "unable to parse service: ", svname) ;
    }

    log_trace("add service: ", name, " to the service selection") ;
    if (!resolve_hash_add(ctx.hres, name, res)) {
        parse_store_free(&st) ;
        resolve_free_obj(&wres) ;
        return -1 ;
    }

    struct resolve_hash_s *c = resolve_hash_search(ctx.hres, name) ;

    c->copyfrom = copyfrom ;
    copyfrom = (strbuf)STRBUF_ZERO ;

    r = parse_build(c, &ctx) ;

    parse_store_free(&st) ;

    return r ;
}
