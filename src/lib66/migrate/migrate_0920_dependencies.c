/*
 * migrate_0920_dependencies.c
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
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/hash.h>
#include <oblibs/strbuf.h>

#include <66/graph.h>
#include <66/migrate.h>
#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/ssexec.h>
#include <66/utils.h>

static int write_stop_delay(struct resolve_hash_s *c, char const *base)
{
    resolve_service_addon_execute_t ex = RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
    resolve_wrapper_t wex = RESOLVE_WRAPPER(DATA_SERVICE_EXECUTE, &ex) ;

    int r = resolve_read(&wex, base, c->name) ;

    if (r > 0 && ex.timeout.stop != c->execute.timeout.stop) {

        log_trace("write stop delay of: ", c->name, " from its frontend") ;

        ex.timeout.stop = c->execute.timeout.stop ;

        if (!resolve_write(&wex, base, c->name))
            r = -1 ;
    }

    int e = errno ;
    resolve_free_obj(&wex) ;
    errno = e ;

    return r < 0 ? -1 : 1 ;
}

static int write_addons(hash_t *hres, char const *base)
{
    struct resolve_hash_s *c, *tmp ;

    HASH_FOREACH(hres, c, tmp) {

        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_SERVICE, &res) ;
        resolve_wrapper_t wdep = RESOLVE_WRAPPER(DATA_SERVICE_DEPENDENCIES, &c->dependencies) ;

        int r = resolve_read(&wres, base, c->name) ;

        // nothing declared and nothing on disk: nothing to write
        if (r > 0 && (res.has_dependencies || c->res.has_dependencies)) {

            log_trace("write dependencies addon of: ", c->name, " from its frontend") ;

            if (!resolve_write(&wdep, base, c->name))
                r = -1 ;
        }

        if (r > 0 && res.has_execute && c->res.has_execute)
            r = write_stop_delay(c, base) ;

        int e = errno ;
        resolve_free_obj(&wres) ;
        errno = e ;

        if (r < 0)
            return -1 ;
    }

    return 1 ;
}

static int rewrite(char const *name, service_graph_t const *g, ssexec_t *info)
{
    int r ;
    hash_t hres = HASH_ZERO ;
    _cleanup_strbuf_ strbuf msg = STRBUF_ZERO ;
    char const *argv[2] = { name, 0 } ;

    if (!hash_init(&hres, 0, offsetof(struct resolve_hash_s, node)))
        return -1 ;

    r = parse_names(&hres, g, argv, 1, info, PARSE_FORCE|PARSE_NODEPS|PARSE_NOFROM, &msg) ;

    if (!r)
        log_warn("service: ", name, " -- its frontend is refused, its dependencies and its stop delay stay as the previous release wrote them: ", msg.s) ;
    else if (r > 0)
        r = write_addons(&hres, info->base.s) ;

    // errno is preserved
    resolve_hash_free(&hres) ;

    return r < 0 ? -1 : 1 ;
}

int migrate_0920_dependencies(void)
{
    log_flow() ;

    int r = 1 ;
    ssexec_t info = SSEXEC_ZERO ;
    service_graph_t g = SERVICE_GRAPH_ZERO ;
    _cleanup_strbuf_ strbuf msg = STRBUF_ZERO ;

    info.owner = getuid() ;
    info.ownerlen = uid_format(info.ownerstr, info.owner) ;
    info.ownerstr[info.ownerlen] = 0 ;
    strbuf frontends = STRBUF_ZERO ;
    info.skip_opt_tree = 1 ;

    if (!set_ownersysdir(&info.base, info.owner) ||
        service_graph_load(&g, info.owner, info.base.s, SS_LIVE) < 0 ||
        set_info(&info, &msg) < 0)
            r = -1 ;

    for (uint32_t index = 0 ; index < service_graph_nvertex(&g) && r > 0 ; index++) {

        uint8_t state = service_graph_state(&g, index) ;

        if (!service_graph_readable(state) ||
            service_graph_logparent(&g, index) != SERVICE_GRAPH_NOINDEX ||
            service_graph_module(&g, index) != SERVICE_GRAPH_NOINDEX)
                continue ;

        int gone = service_graph_frontend_gone(&g, index, &frontends) ;
        if (gone < 0) {
            r = -1 ;
            break ;
        }

        if (gone)
            continue ;

        r = rewrite(service_graph_name(&g, index), &g, &info) ;
    }

    int e = errno ;
    service_graph_free(&g) ;
    ssexec_free(&info) ;
    strbuf_free(&frontends) ;
    errno = e ;

    return r < 0 ? -1 : 1 ;
}
