/*
 * migrate_0920_logsuffix.c
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

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/constants.h>
#include <66/graph.h>
#include <66/migrate.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/utils.h>

static int is_logger(char const *base, char const *name)
{
    int r ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = { .type = DATA_SERVICE, .obj = &res } ;

    r = resolve_read(&w, base, name) ;
    if (r > 0)
        r = res.islog ? 1 : 0 ;

    resolve_free_obj(&w) ;

    return r ;
}

static int collect(strbuf *names, strbuf *lines, service_graph_t const *g, char const *base)
{
    for (uint32_t index = 0 ; index < service_graph_nvertex(g) ; index++) {

        char const *name = service_graph_name(g, index) ;
        _cleanup_strbuf_ strbuf line = STRBUF_ZERO ;

        if (!service_graph_readable(service_graph_state(g, index)) || get_rstrlen_until(name, SS_LOG_SUFFIX) < 0 ||
            service_graph_module(g, index) != SERVICE_GRAPH_NOINDEX)
                continue ;

        int r = is_logger(base, name) ;
        if (r < 0)
            return -1 ;

        if (r)
            continue ;

        if (!auto_strbuf(&line, "service: ", name, " ends with the suffix " SS_LOG_SUFFIX " reserved to the loggers -- removed by the migration, parse its frontend again under another name"))
            return -1 ;

        if (service_graph_vertex(g, index)->nrequiredby &&
            (!auto_strbuf(&line, "; required by: ") || service_graph_dependents(&line, g, index) < 0))
                return -1 ;

        if (!sbl_add(names, name) || !sbl_add(lines, line.s))
            return -1 ;
    }

    return 1 ;
}

int migrate_0920_logsuffix(void)
{
    log_flow() ;

    int r = 1 ;
    uid_t owner = getuid() ;
    size_t pos = 0, lpos = 0 ;
    service_graph_t g = SERVICE_GRAPH_ZERO ;
    _cleanup_strbuf_ strbuf base = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf names = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf lines = STRBUF_ZERO ;

    if (!set_ownersysdir(&base, owner) ||
        service_graph_load(&g, owner, base.s, SS_LIVE) < 0 ||
        collect(&names, &lines, &g, base.s) < 0)
            r = -1 ;

    int e = errno ;
    service_graph_free(&g) ;
    errno = e ;

    if (r < 0)
        return -1 ;

    FOREACH_SBL(&names, pos) {

        if (service_remove_one(base.s, owner, names.s + pos) < 0)
            return -1 ;

        log_warn(lines.s + lpos) ;
        lpos += strlen(lines.s + lpos) + 1 ;
    }

    return 1 ;
}
