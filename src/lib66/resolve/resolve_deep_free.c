/*
 * resolve_deep_free.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>
#include <stddef.h>

#include <oblibs/log.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>
#include <66/graph.h>

void resolve_deep_free(uint8_t type, genalloc *g)
{
    log_flow() ;

    size_t pos = 0 ;

    if (type == DATA_SERVICE) {

        for (; pos < genalloc_len(resolve_service_t, g) ; pos++)
            stralloc_free(&genalloc_s(resolve_service_t, g)[pos].sa) ;

        genalloc_free(resolve_service_t, g) ;

    } else if (type == DATA_SERVICE_MASTER) {

        for (; pos < genalloc_len(resolve_service_master_t, g) ; pos++)
            stralloc_free(&genalloc_s(resolve_service_master_t, g)[pos].sa) ;

        genalloc_free(resolve_service_master_t, g) ;

    } else if (type == DATA_TREE) {

        for (; pos < genalloc_len(resolve_tree_t, g) ; pos++)
            stralloc_free(&genalloc_s(resolve_tree_t, g)[pos].sa) ;

         genalloc_free(resolve_tree_t, g) ;

    } else if (type == DATA_TREE_MASTER) {

        for (; pos < genalloc_len(resolve_tree_master_t, g) ; pos++)
            stralloc_free(&genalloc_s(resolve_tree_master_t, g)[pos].sa) ;

         genalloc_free(resolve_tree_master_t, g) ;
    }

}
