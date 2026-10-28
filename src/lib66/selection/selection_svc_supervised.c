/*
 * selection_svc_supervised.c
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

#include <oblibs/log.h>

#include <66/svc.h>
#include <66/selection.h>
#include <66/service.h>

static int supervised(struct resolve_hash_s const *h)
{
    return svc_issupervised(&h->res) ;
}

int selection_svc_supervised(selection_service_t *out, selection_service_t const *sel, service_graph_resolve_t *store)
{
    log_flow() ;

    return selection_svc_filter(out, sel, store, 0, supervised) ;
}
