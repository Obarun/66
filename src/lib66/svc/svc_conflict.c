/*
 * svc_conflict.c
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/bits.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/svc.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>

static int conflicts(service_graph_resolve_t *store, uint32_t other, uint8_t mode, int *yes)
{
    int r ;
    uint8_t state = service_graph_state(store->g, other) ;
    struct resolve_hash_s *h ;

    *yes = 0 ;

    if (state == SERVICE_GRAPH_STATE_ABSENT)
        return 1 ;

    if (state == SERVICE_GRAPH_STATE_UNREADABLE)
        return (*yes = 1, 1) ;

    r = service_graph_resolve_read(store, other, SERVICE_READ_EVENT, &h) ;
    if (r <= 0)
        return r ;

    r = mode == SVC_CONFLICT_ENABLED ? (h->res.enabled ? 1 : 0) : svc_isrunning(h) ;
    if (r < 0)
        return -1 ;

    *yes = r ;

    return 1 ;
}

static int first_conflict(struct resolve_hash_s const *h, service_graph_resolve_t *store, uint32_t self, char const *base, uint8_t mode, uint32_t *other)
{
    size_t pos = 0 ;
    _cleanup_strbuf_ strbuf stk = STRBUF_ZERO ;

    *other = SERVICE_GRAPH_NOINDEX ;

    if (!h->dependencies.nconflict)
        return 1 ;

    if (!sbl_clean_string(&stk, h->dependencies.sa.s + h->dependencies.conflict))
        return -1 ;

    FOREACH_SBL(&stk, pos) {

        int yes ;
        char held[SS_MAX_SERVICE_NAME + 1] ;
        char const *name = stk.s + pos ;

        int r = service_resolve_provide(held, name, 0, base) ;
        if (r < 0)
            return -1 ;

        uint32_t index = service_graph_find(store->g, r ? held : name) ;

        if (index == SERVICE_GRAPH_NOINDEX || index == self)
            continue ;

        r = conflicts(store, index, mode, &yes) ;
        if (r <= 0)
            return r ;

        if (yes) {
            *other = index ;
            return 1 ;
        }
    }

    return 1 ;
}

int svc_conflict(selection_service_t const *sel, service_graph_resolve_t *store, char const *base, uint8_t mode, uint32_t *member, uint32_t *other)
{
    log_flow() ;

    uint32_t index ;

    *member = *other = SERVICE_GRAPH_NOINDEX ;

    if (mode > SVC_CONFLICT_ENABLED)
        return (errno = EINVAL, -1) ;

    FOREACH_SELECTION_MEMBER(sel, index) {

        struct resolve_hash_s *h ;

        int r = service_graph_resolve_get(store, index, SERVICE_READ_DEPENDENCIES, &h) ;
        if (r < 0)
            return -1 ;

        if (!r)
            continue ;

        r = first_conflict(h, store, index, base, mode, other) ;
        if (r <= 0)
            return r ;

        if (*other != SERVICE_GRAPH_NOINDEX) {
            *member = index ;
            return 1 ;
        }
    }

    return 1 ;
}
