/*
 * migrate_0920_verbosity.c
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
#include <unistd.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/config.h>
#include <66/graph.h>
#include <66/migrate.h>
#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/utils.h>
#include <66/write.h>

static int migrate_one(char const *base, char const *name)
{
    int r ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_service_addon_execute_t ex = RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
    resolve_wrapper_t w = { .type = DATA_SERVICE, .obj = &res } ;
    resolve_wrapper_t wex = { .type = DATA_SERVICE_EXECUTE, .obj = &ex } ;

    r = resolve_read(&w, base, name) ;

    if (r > 0 && res.has_execute && !res.islog) {

        r = resolve_read(&wex, base, name) ;

        if (r > 0 && (ex.run.run || ex.finish.run)) {

            char const *servicedir = res.sa.s + res.path.servicedir ;

            if (ex.run.run)
                compute_wrapper_scripts(&res, &ex, 1) ;

            if (ex.finish.run)
                compute_wrapper_scripts(&res, &ex, 0) ;

            log_trace("write run and finish scripts of: ", name) ;

            if (!resolve_write(&wex, base, name)
                || (ex.run.run && write_execute_scripts("run", ex.sa.s + ex.run.run, servicedir, 0) < 0)
                || (ex.finish.run && write_execute_scripts("finish", ex.sa.s + ex.finish.run, servicedir, 0) < 0))
                r = -1 ;
        }
    }

    resolve_free_obj(&w) ;
    resolve_free_obj(&wex) ;

    return r < 0 ? -1 : 1 ;
}

int migrate_0920_verbosity(void)
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
