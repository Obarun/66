/*
 * service_enable_selection.c
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

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <oblibs/log.h>
#include <oblibs/bits.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/service.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/resolve.h>
#include <66/tree.h>
#include <66/symlink.h>
#include <66/enum_service.h>
#include <66/svc.h>

static int moves(selection_service_t const *sel, uint32_t index)
{
    uint32_t parent = service_graph_logparent(sel->g, index) ;
    uint32_t v = parent != SERVICE_GRAPH_NOINDEX ? parent : index ;
    uint32_t module = service_graph_module(sel->g, v) ;

    return bitset32_isvalid(&sel->named, v) || (module != SERVICE_GRAPH_NOINDEX && bitset32_isvalid(&sel->named, module)) ;
}

static int write_enabled(resolve_service_t *res, char const *base, bool enable)
{
    int e = 1 ;
    resolve_service_t fresh = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_SERVICE, &fresh) ;
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_MAIN_ZERO ;

    table.u.service.id = E_RESOLVE_SERVICE_CONFIG_ENABLED ;

    if (!resolve_modify_field(&wres, base, res->sa.s + res->name, table, enable ? "1" : "0"))
        e = -1 ;

    resolve_free_obj(&wres) ;

    if (e > 0)
        res->enabled = enable ? 1 : 0 ;

    return e ;
}

static int one(selection_service_t const *sel, service_graph_resolve_t *store, uint32_t index, bool enable, char const *tree, strbuf *adds)
{
    struct resolve_hash_s *h ;
    char const *base = store->base ;

    if (service_graph_resolve_get(store, index, SERVICE_READ_DEPENDENCIES, &h) <= 0)
        return -1 ;

    resolve_service_t *res = &h->res ;
    char name[strlen(res->sa.s + res->name) + 1] ;

    auto_strings(name, res->sa.s + res->name) ;

    if (enable && symlink_provide_enable(base, h) < 0)
        return -1 ;

    if (tree && moves(sel, index)) {

        if (tree_contents_batch(base, adds, true) < 0 || service_switch_tree(res, tree, base) < 0)
            return -1 ;

        adds->len = 0 ;

    } else if (enable && (!sbl_add(adds, res->sa.s + res->treename) || !sbl_add(adds, name)))
        return -1 ;

    if (write_enabled(res, base, enable) < 0)
        return -1 ;

    log_info(enable ? "Enabled" : "Disabled", " successfully: ", name) ;

    return 1 ;
}

int service_enable_selection(selection_service_t const *sel, service_graph_resolve_t *store, bool enable, char const *tree)
{
    log_flow() ;

    uint32_t pos, index ;
    _cleanup_strbuf_ strbuf adds = STRBUF_ZERO ;

    if (enable) {

        FOREACH_SELECTION(sel, pos, index)
            if (one(sel, store, index, enable, tree, &adds) < 0)
                return -1 ;

    } else {

        FOREACH_SELECTION_REVERSE(sel, pos, index)
            if (one(sel, store, index, enable, tree, &adds) < 0)
                return -1 ;
    }

    return tree_contents_batch(store->base, &adds, true) < 0 ? -1 : 1 ;
}
