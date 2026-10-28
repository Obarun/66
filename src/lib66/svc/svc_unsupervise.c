/*
 * svc_unsupervise.c
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
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
#include <stdbool.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/bits.h>

#include <66/fdholder.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/svc.h>

static int one(service_graph_resolve_t *store, uint32_t index, fdholder_client_t *a, bool *started, uint8_t who)
{
    struct resolve_hash_s *h ;

    int r = service_graph_resolve_get(store, index, 0, &h) ;
    if (r < 0)
        return -1 ;
    if (!r)
        return 1 ;

    r = svc_issupervised(&h->res) ;
    if (r <= 0)
        return r ;

    return svc_unsupervise_one(&h->res, a, started, who) ;
}

int svc_unsupervise(selection_service_t const *lot, service_graph_resolve_t *store, uint8_t who)
{
    log_flow() ;

    int r = 1 ;
    uint32_t pos = 0, index = 0 ;
    bool started = false ;
    fdholder_client_t a ;

    FOREACH_SELECTION_UNORDERED(lot, index) {
        r = one(store, index, &a, &started, who) ;
        if (r < 0)
            goto end ;
    }

    FOREACH_SELECTION_REVERSE(lot, pos, index) {
        r = one(store, index, &a, &started, who) ;
        if (r < 0)
            goto end ;
    }

    r = 1 ;

    end:
        if (started) {
            int e = errno ;
            fdholder_client_end(&a) ;
            errno = e ;
        }

        return r ;
}
