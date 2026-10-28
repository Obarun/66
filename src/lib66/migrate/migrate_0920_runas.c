/*
 * migrate_0920_runas.c
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

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/config.h>
#include <66/enum_parser.h>
#include <66/graph.h>
#include <66/migrate.h>
#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/utils.h>

static int names_runas(char const *path)
{
    int r ;
    parse_store_t st ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    if (!strbuf_read_file(&sa, path))
        return 1 ;

    r = parse_store_build(&st, sa.s) ;
    if (r > 0)
        r = parse_store_present(&st, E_PARSER_SECTION_LOGGER, E_PARSER_SECTION_LOGGER_RUNAS) ;
    else if (!r)
        r = 1 ;

    parse_store_free(&st) ;

    return r ;
}

static int rewrite(resolve_wrapper_t *w, strbuf const *sa, uint32_t *field, char const *base, char const *name)
{
    ssize_t off ;

    if (!*field || !strcmp(sa->s + *field, SS_LOGGER_RUNNER))
        return 0 ;

    off = resolve_add_string(w, SS_LOGGER_RUNNER) ;
    if (off < 0)
        return -1 ;

    *field = (uint32_t)off ;

    log_trace("write runas of: ", name, " as: ", SS_LOGGER_RUNNER) ;

    return resolve_write(w, base, name) ? 1 : -1 ;
}

static int rewrite_service(resolve_service_t *res, char const *base, char const *name)
{
    int r = 1 ;
    resolve_service_addon_io_t io = RESOLVE_SERVICE_ADDON_IO_ZERO ;
    resolve_service_addon_execute_t ex = RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
    resolve_wrapper_t wio = { .type = DATA_SERVICE_IO, .obj = &io } ;
    resolve_wrapper_t wex = { .type = DATA_SERVICE_EXECUTE, .obj = &ex } ;

    if (res->has_io) {

        r = resolve_read(&wio, base, name) ;
        if (r > 0)
            r = rewrite(&wio, &io.sa, &io.runas, base, name) ;
    }

    if (r >= 0 && res->islog && res->has_execute) {

        r = resolve_read(&wex, base, name) ;
        if (r > 0)
            r = rewrite(&wex, &ex.sa, &ex.run.runas, base, name) ;
    }

    resolve_free_obj(&wio) ;
    resolve_free_obj(&wex) ;

    return r < 0 ? -1 : 1 ;
}

static int migrate_one(char const *base, char const *name)
{
    int r ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = { .type = DATA_SERVICE, .obj = &res } ;

    r = resolve_read(&w, base, name) ;

    if (r > 0 && (res.has_io || res.islog)) {

        r = names_runas(res.sa.s + res.path.frontend) ;
        if (!r)
            r = rewrite_service(&res, base, name) ;
    }

    resolve_free_obj(&w) ;

    return r < 0 ? -1 : 1 ;
}

int migrate_0920_runas(void)
{
    log_flow() ;

    int r = 1 ;
    uid_t owner = getuid() ;
    service_graph_t g = SERVICE_GRAPH_ZERO ;
    _cleanup_strbuf_ strbuf base = STRBUF_ZERO ;

    if (!set_ownersysdir(&base, owner) || service_graph_load(&g, owner, base.s, SS_LIVE) < 0)
        r = -1 ;

    for (uint32_t index = 0 ; index < service_graph_nvertex(&g) && r > 0 ; index++) {

        if (service_graph_readable(service_graph_state(&g, index)))
            r = migrate_one(base.s, service_graph_name(&g, index)) ;
    }

    int e = errno ;
    service_graph_free(&g) ;
    errno = e ;

    return r < 0 ? -1 : 1 ;
}
